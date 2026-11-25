#ifndef UIM_INPUT_SCALE_H
#define UIM_INPUT_SCALE_H
/* UIM input scale: window-API pointer values -> UI logical units.
 *
 * Contract (v1):
 * - This module never touches the OS mouse driver, the system pointer speed,
 *   the process DPI awareness, or any global cursor setting. The host passes
 *   the window's own DPI ratio and the raw pointer values that the window API
 *   already delivered (WM_MOUSEMOVE / XQueryPointer / NSEvent locationInWindow).
 *   Nothing here calls SetProcessDpiAwareness, SetCursorPos, SystemParametersInfo
 *   or their platform equivalents.
 * - dpi_scale is physical pixels per logical unit: window_dpi / 96.0.
 *   1.0 means one logical unit equals one physical client pixel.
 * - sensitivity is the game-input gain applied to pointer *deltas* only - the
 *   classic FPS aim multiplier. It is never applied to absolute positions, so
 *   the caret / hit point still lands exactly where the user points.
 * - point : logical = raw / dpi_scale
 *   delta : logical = raw_delta / dpi_scale * sensitivity
 * - Pure value type: no allocation, no globals, no hidden state. One instance
 *   must not be shared across threads without external synchronization.
 *
 * Values outside the documented range are clamped and reported as
 * uim_ERR_RANGE; non-finite or non-positive input is rejected with
 * uim_ERR_ARGUMENT and the stored value is left unchanged.
 */
#include "uim_component_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define uim_INPUT_SCALE_DPI_MIN        0.25f
#define uim_INPUT_SCALE_DPI_MAX        8.0f
#define uim_INPUT_SCALE_SENSITIVITY_MIN 0.05f
#define uim_INPUT_SCALE_SENSITIVITY_MAX 20.0f

typedef struct uim_InputScale {
    float dpi_scale;   /* read-only: use the setters below */
    float sensitivity; /* read-only: use the setters below */
} uim_InputScale;

/* Both values are clamped to their documented range. Returns uim_ERR_RANGE
 * when a clamp happened, uim_ERR_ARGUMENT when a value was rejected. */
uim_Result uim_input_scale_init(uim_InputScale *scale, float dpi_scale, float sensitivity);
/* dpi_scale = 1.0, sensitivity = 1.0 (no scaling, no gain). */
uim_Result uim_input_scale_init_default(uim_InputScale *scale);

uim_Result uim_input_scale_set_dpi_scale(uim_InputScale *scale, float dpi_scale);
uim_Result uim_input_scale_set_sensitivity(uim_InputScale *scale, float sensitivity);

/* Absolute pointer position in physical client pixels -> UI logical units.
 * Used for hit testing, caret placement and hover, never for aim. */
uim_Result uim_input_scale_point(const uim_InputScale *scale,
                                 float raw_x, float raw_y,
                                 float *out_x, float *out_y);
/* UI logical units -> physical client pixels (renderer / OS cursor feedback). */
uim_Result uim_input_scale_point_inverse(const uim_InputScale *scale,
                                         float logical_x, float logical_y,
                                         float *out_x, float *out_y);
/* Relative pointer motion in physical pixels -> logical motion with the game
 * input sensitivity applied. out_dx/out_dy may alias each other's inputs but
 * must not alias out with the same pointer. */
uim_Result uim_input_scale_delta(const uim_InputScale *scale,
                                 float raw_dx, float raw_dy,
                                 float *out_dx, float *out_dy);

#ifdef __cplusplus
}
#endif
#endif
