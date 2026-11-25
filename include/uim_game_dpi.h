#ifndef UIM_GAME_DPI_H
#define UIM_GAME_DPI_H
/* Game DPI component and window-local pointer gain.
 *
 * The component exposes one editable number: `multiplier`.
 * The host supplies the mouse driver's native DPI (counts/inch); the effective
 * game DPI is:
 *
 *     game_dpi = driver_dpi * multiplier
 *
 * Pointer deltas are therefore scaled by `multiplier` (the driver DPI cancels
 * against the same physical device). Absolute point conversion stays in
 * `uim_input_scale` and is never multiplied, so hit points remain exact.
 *
 * `uim_GameDpi` is the component-facing facade for
 * `uim_InputScale.sensitivity`; its delta and clamp implementation delegates to
 * that primitive, so UIM has one pointer-gain formula, not two.
 *
 * `uim_GameDpiCursor` integrates the scaled deltas into a window-local cursor
 * position and can push that position through a host sink (for example
 * SetCursorPos with the window origin) so the OS pointer really moves inside
 * the window rectangle. It does not and cannot change the system-wide pointer
 * speed or the hardware DPI; it changes the effective pointer gain inside one
 * window, which is what a game's "mouse DPI" setting means.
 */
#include "uim_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_GameDpi {
    float driver_dpi;  /* read-only device DPI; set at init */
    float multiplier;  /* the only editable value; same 0.05..20 range as
                          uim_InputScale.sensitivity, which owns the clamp */
} uim_GameDpi;

typedef void (*uim_GameDpiCursorSink)(void *user, float x, float y);

typedef struct uim_GameDpiCursor {
    uim_GameDpi gain;
    float x;
    float y;
    float width;
    float height;
    uim_GameDpiCursorSink sink;
    void *sink_user;
} uim_GameDpiCursor;

uim_Result uim_game_dpi_init(uim_GameDpi *gain, float driver_dpi);
uim_Result uim_game_dpi_set_multiplier(uim_GameDpi *gain, float multiplier);
float uim_game_dpi_effective(const uim_GameDpi *gain);
/* Raw relative counts -> window pixels. out_dx/out_dy may alias raw inputs. */
uim_Result uim_game_dpi_delta(const uim_GameDpi *gain,
                              float raw_dx, float raw_dy,
                              float *out_dx, float *out_dy);

/* Window-local virtual cursor. The rect is the allowed pointer region in
 * window client pixels. move() clamps and invokes the sink when configured. */
uim_Result uim_game_dpi_cursor_init(uim_GameDpiCursor *cursor, float driver_dpi,
                                    float width, float height);
uim_Result uim_game_dpi_cursor_set_sink(uim_GameDpiCursor *cursor,
                                        uim_GameDpiCursorSink sink, void *user);
uim_Result uim_game_dpi_cursor_set_position(uim_GameDpiCursor *cursor,
                                            float x, float y);
uim_Result uim_game_dpi_cursor_move(uim_GameDpiCursor *cursor,
                                    float raw_dx, float raw_dy);

#ifdef __cplusplus
}
#endif
#endif /* UIM_GAME_DPI_H */
