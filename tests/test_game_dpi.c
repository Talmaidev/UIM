/* Game DPI contract: the component stores one multiplier, the host supplies
 * the driver DPI, and the window cursor really moves through the sink. */
#include "uim_game_dpi.h"
#include "uim_component_registry.h"
#include "uim_input_scale.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

static float sink_x, sink_y;
static int sink_calls;

static void record_sink(void *user, float x, float y) {
    (void)user;
    sink_x = x;
    sink_y = y;
    sink_calls++;
}

int main(void) {
    uim_GameDpi gain;
    uim_GameDpiCursor cursor;
    float dx, dy;

    CHECK(uim_game_dpi_init(&gain, 800.0f) == uim_OK);
    CHECK(gain.driver_dpi == 800.0f);
    CHECK(gain.multiplier == 1.0f);
    CHECK(uim_game_dpi_effective(&gain) == 800.0f);

    /* Only the multiplier is input; effective DPI follows the driver DPI. */
    CHECK(uim_game_dpi_set_multiplier(&gain, 2.0f) == uim_OK);
    CHECK(uim_game_dpi_effective(&gain) == 1600.0f);
    CHECK(uim_game_dpi_delta(&gain, 10.0f, -5.0f, &dx, &dy) == uim_OK);
    CHECK(dx == 20.0f && dy == -10.0f);

    /* Range clamps instead of silently accepting impossible values. */
    CHECK(uim_game_dpi_set_multiplier(&gain, 0.001f) == uim_ERR_RANGE);
    CHECK(gain.multiplier == uim_INPUT_SCALE_SENSITIVITY_MIN);
    CHECK(uim_game_dpi_set_multiplier(&gain, 50.0f) == uim_ERR_RANGE);
    CHECK(gain.multiplier == uim_INPUT_SCALE_SENSITIVITY_MAX);
    CHECK(uim_game_dpi_set_multiplier(&gain, 1.5f) == uim_OK);

    /* Window-local cursor: scaled moves, clamped to the window rectangle. */
    CHECK(uim_game_dpi_cursor_init(&cursor, 800.0f, 200.0f, 100.0f) == uim_OK);
    CHECK(cursor.x == 100.0f && cursor.y == 50.0f);
    CHECK(uim_game_dpi_set_multiplier(&cursor.gain, 1.5f) == uim_OK);
    CHECK(uim_game_dpi_cursor_set_sink(&cursor, record_sink, NULL) == uim_OK);
    CHECK(uim_game_dpi_cursor_move(&cursor, 20.0f, 10.0f) == uim_OK);
    CHECK(cursor.x == 130.0f && cursor.y == 65.0f);
    CHECK(sink_calls == 1 && sink_x == 130.0f && sink_y == 65.0f);
    CHECK(uim_game_dpi_cursor_move(&cursor, 1000.0f, 1000.0f) == uim_OK);
    CHECK(cursor.x == 199.0f && cursor.y == 99.0f);
    CHECK(sink_calls == 2 && sink_x == 199.0f && sink_y == 99.0f);
    CHECK(uim_game_dpi_cursor_set_position(&cursor, -5.0f, 200.0f) == uim_OK);
    CHECK(cursor.x == 0.0f && cursor.y == 99.0f);

    /* Component identity is stable and exposes only the multiplier. */
    {
        const uim_ComponentTypeDesc *desc =
            uim_component_registry_builtin_desc(uim_COMPONENT_GAME_DPI);
        CHECK(desc != NULL);
        if (desc) {
            CHECK(strcmp(desc->stable_key, "builtin.game_dpi") == 0);
            CHECK(desc->property_count == 1u);
            CHECK(desc->properties[0].property_id == uim_PROPERTY_MULTIPLIER);
        }
    }

    if (failures == 0) printf("UIM game dpi contract: ALL PASS\n");
    else printf("%d failures\n", failures);
    return failures;
}
