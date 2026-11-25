#include "uim_game_dpi.h"
#include "uim_input_scale.h"

#include <math.h>

static uim_Result game_dpi_validate(const uim_GameDpi *gain) {
    if (!gain) return uim_ERR_ARGUMENT;
    if (!isfinite(gain->driver_dpi) || gain->driver_dpi <= 0.0f) return uim_ERR_STATE;
    if (!isfinite(gain->multiplier) || gain->multiplier <= 0.0f) return uim_ERR_STATE;
    return uim_OK;
}

uim_Result uim_game_dpi_init(uim_GameDpi *gain, float driver_dpi) {
    if (!gain) return uim_ERR_ARGUMENT;
    if (!isfinite(driver_dpi) || driver_dpi <= 0.0f) return uim_ERR_ARGUMENT;
    gain->driver_dpi = driver_dpi;
    gain->multiplier = 1.0f;
    return uim_OK;
}

/* GameDPI is the component-facing name for uim_InputScale.sensitivity.
 * Clamping and validation live in that primitive, so there is only one gain
 * range and one delta formula in UIM. */
uim_Result uim_game_dpi_set_multiplier(uim_GameDpi *gain, float multiplier) {
    uim_InputScale scale;
    uim_Result rc;
    if (!gain) return uim_ERR_ARGUMENT;
    rc = uim_input_scale_init_default(&scale);
    if (rc == uim_OK) rc = uim_input_scale_set_sensitivity(&scale, multiplier);
    if (rc != uim_OK && rc != uim_ERR_RANGE) return rc;
    gain->multiplier = scale.sensitivity;
    return rc;
}

float uim_game_dpi_effective(const uim_GameDpi *gain) {
    if (game_dpi_validate(gain) != uim_OK) return 0.0f;
    return gain->driver_dpi * gain->multiplier;
}

uim_Result uim_game_dpi_delta(const uim_GameDpi *gain,
                              float raw_dx, float raw_dy,
                              float *out_dx, float *out_dy) {
    uim_InputScale scale;
    uim_Result rc;
    rc = game_dpi_validate(gain);
    if (rc != uim_OK) return rc;
    /* dpi_scale = 1: GameDPI consumes already-normalized window deltas. */
    rc = uim_input_scale_init(&scale, 1.0f, gain->multiplier);
    if (rc != uim_OK && rc != uim_ERR_RANGE) return rc;
    return uim_input_scale_delta(&scale, raw_dx, raw_dy, out_dx, out_dy);
}

uim_Result uim_game_dpi_cursor_init(uim_GameDpiCursor *cursor, float driver_dpi,
                                    float width, float height) {
    uim_Result rc;
    if (!cursor) return uim_ERR_ARGUMENT;
    if (!isfinite(width) || !isfinite(height) || width <= 0.0f || height <= 0.0f)
        return uim_ERR_ARGUMENT;
    rc = uim_game_dpi_init(&cursor->gain, driver_dpi);
    if (rc != uim_OK) return rc;
    cursor->x = width * 0.5f;
    cursor->y = height * 0.5f;
    cursor->width = width;
    cursor->height = height;
    cursor->sink = NULL;
    cursor->sink_user = NULL;
    return uim_OK;
}

uim_Result uim_game_dpi_cursor_set_sink(uim_GameDpiCursor *cursor,
                                        uim_GameDpiCursorSink sink, void *user) {
    if (!cursor) return uim_ERR_ARGUMENT;
    cursor->sink = sink;
    cursor->sink_user = user;
    return uim_OK;
}

uim_Result uim_game_dpi_cursor_set_position(uim_GameDpiCursor *cursor,
                                            float x, float y) {
    if (!cursor) return uim_ERR_ARGUMENT;
    if (!isfinite(x) || !isfinite(y)) return uim_ERR_ARGUMENT;
    if (cursor->width <= 0.0f || cursor->height <= 0.0f) return uim_ERR_STATE;
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x > cursor->width - 1.0f) x = cursor->width - 1.0f;
    if (y > cursor->height - 1.0f) y = cursor->height - 1.0f;
    cursor->x = x;
    cursor->y = y;
    if (cursor->sink) cursor->sink(cursor->sink_user, cursor->x, cursor->y);
    return uim_OK;
}

uim_Result uim_game_dpi_cursor_move(uim_GameDpiCursor *cursor,
                                    float raw_dx, float raw_dy) {
    float dx, dy;
    uim_Result rc;
    if (!cursor) return uim_ERR_ARGUMENT;
    rc = uim_game_dpi_delta(&cursor->gain, raw_dx, raw_dy, &dx, &dy);
    if (rc != uim_OK) return rc;
    return uim_game_dpi_cursor_set_position(cursor, cursor->x + dx, cursor->y + dy);
}
