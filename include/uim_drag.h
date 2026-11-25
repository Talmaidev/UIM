#ifndef UIM_DRAG_H
#define UIM_DRAG_H
/* uim_drag.h — the drag atom: press, threshold, axis lock, end/cancel.
 *
 * Sliders, scrollbar thumbs, splitters, window move and node drag all repeat
 * the same three decisions: when does a press become a drag, which axis wins,
 * and what delta is delivered. This header is that state machine; the caller
 * still owns capture, cursor and hit testing.
 *
 * Contract:
 * - One active gesture at a time, identified by a caller id.
 * - A press becomes a drag only after the pointer travels `threshold` pixels;
 *   below that it is still a click (uim_drag_active stays false).
 * - With axis_lock on, the dominant direction at activation wins and the
 *   reported delta is zero on the other axis.
 * - end() reports uim_OK only if the gesture actually became a drag; a plain
 *   click reports uim_ERR_STATE. Both reset the state.
 */
#include "uim_layout.h"
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_DragState {
    bool pressed;
    bool active;
    uint32_t id;
    float start_x, start_y;
    float current_x, current_y;
    float threshold;
    bool axis_lock;
    bool axis_locked;
    uim_LayoutAxis axis;
} uim_DragState;

static inline uim_Result uim_drag_init(uim_DragState *d, float threshold) {
    if (!d || !isfinite(threshold) || threshold < 0.0f) return uim_ERR_ARGUMENT;
    d->pressed = false;
    d->active = false;
    d->id = 0u;
    d->start_x = d->start_y = d->current_x = d->current_y = 0.0f;
    d->threshold = threshold;
    d->axis_lock = false;
    d->axis_locked = false;
    d->axis = uim_LAYOUT_ROW;
    return uim_OK;
}

static inline uim_Result uim_drag_set_axis_lock(uim_DragState *d, bool on) {
    if (!d) return uim_ERR_ARGUMENT;
    d->axis_lock = on;
    d->axis_locked = false;
    return uim_OK;
}

static inline uim_Result uim_drag_begin(uim_DragState *d, uint32_t id, float x, float y) {
    if (!d || !isfinite(x) || !isfinite(y)) return uim_ERR_ARGUMENT;
    if (d->pressed) return uim_ERR_STATE;
    d->pressed = true;
    d->active = false;
    d->axis_locked = false;
    d->id = id;
    d->start_x = d->current_x = x;
    d->start_y = d->current_y = y;
    return uim_OK;
}

static inline uim_Result uim_drag_move(uim_DragState *d, float x, float y) {
    float dx, dy, distance;
    if (!d || !isfinite(x) || !isfinite(y)) return uim_ERR_ARGUMENT;
    if (!d->pressed) return uim_ERR_STATE;
    d->current_x = x;
    d->current_y = y;
    if (d->active) return uim_OK;
    dx = x - d->start_x;
    dy = y - d->start_y;
    distance = hypotf(dx, dy);
    if (distance >= d->threshold) {
        d->active = true;
        if (d->axis_lock) {
            d->axis_locked = true;
            d->axis = fabsf(dx) >= fabsf(dy) ? uim_LAYOUT_ROW : uim_LAYOUT_COLUMN;
        }
    }
    return uim_OK;
}

static inline float uim_drag_dx(const uim_DragState *d) {
    float dx;
    if (!d || !d->pressed) return 0.0f;
    dx = d->current_x - d->start_x;
    if (d->axis_locked && d->axis == uim_LAYOUT_COLUMN) return 0.0f;
    return dx;
}

static inline float uim_drag_dy(const uim_DragState *d) {
    float dy;
    if (!d || !d->pressed) return 0.0f;
    dy = d->current_y - d->start_y;
    if (d->axis_locked && d->axis == uim_LAYOUT_ROW) return 0.0f;
    return dy;
}

/* Ends the gesture; returns uim_OK when it was a drag, uim_ERR_STATE when it
 * never crossed the threshold (a click). Outputs are optional. */
static inline uim_Result uim_drag_end(uim_DragState *d, float x, float y,
                                      float *out_dx, float *out_dy) {
    bool was_active;
    float dx, dy;
    if (!d || !isfinite(x) || !isfinite(y)) return uim_ERR_ARGUMENT;
    if (!d->pressed) return uim_ERR_STATE;
    d->current_x = x;
    d->current_y = y;
    dx = uim_drag_dx(d);
    dy = uim_drag_dy(d);
    was_active = d->active;
    d->pressed = false;
    d->active = false;
    d->axis_locked = false;
    if (out_dx) *out_dx = dx;
    if (out_dy) *out_dy = dy;
    return was_active ? uim_OK : uim_ERR_STATE;
}

static inline void uim_drag_cancel(uim_DragState *d) {
    if (!d) return;
    d->pressed = false;
    d->active = false;
    d->axis_locked = false;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_DRAG_H */
