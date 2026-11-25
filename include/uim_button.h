#ifndef UIM_BUTTON_H
#define UIM_BUTTON_H
/* uim_button.h — the button atom: hover, press, release, click and repeat.
 *
 * What it owns:
 * - pointer press/release edge semantics; a click is a release inside the
 *   button after a press, never a bare "hover + down".
 * - keyboard activation (Space/Enter) as a separate edge.
 * - press-and-hold repeat timing for tiny +/- steppers.
 *
 * What it does not own: drawing, hit rectangles, focus traversal. The caller
 * supplies hover/focus and draws; this header only keeps the state machine.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Button {
    bool hovered;
    bool pressed;
    bool focused;
    float repeat_delay;
    float repeat_interval;
    float held_time;
    float next_repeat;
} uim_Button;

static inline uim_Result uim_button_init(uim_Button *b, float repeat_delay,
                                         float repeat_interval) {
    if (!b || !isfinite(repeat_delay) || !isfinite(repeat_interval)
        || repeat_delay < 0.0f || repeat_interval < 0.0f)
        return uim_ERR_ARGUMENT;
    b->hovered = false;
    b->pressed = false;
    b->focused = false;
    b->repeat_delay = repeat_delay;
    b->repeat_interval = repeat_interval;
    b->held_time = 0.0f;
    b->next_repeat = repeat_delay;
    return uim_OK;
}

static inline void uim_button_set_hover(uim_Button *b, bool hovered) {
    if (b) b->hovered = hovered;
}

static inline void uim_button_set_focus(uim_Button *b, bool focused) {
    if (b) b->focused = focused;
}

/* Pointer went down. Starts a press and restarts repeat timing. */
static inline uim_Result uim_button_pointer_down(uim_Button *b) {
    if (!b) return uim_ERR_ARGUMENT;
    b->pressed = true;
    b->held_time = 0.0f;
    b->next_repeat = b->repeat_delay;
    return uim_OK;
}

/* Pointer went up. The click edge is true only when the press started on this
 * button and the release happened inside. Always clears pressed. */
static inline uim_Result uim_button_pointer_up(uim_Button *b, bool released_inside,
                                               bool *out_clicked) {
    if (!b || !out_clicked) return uim_ERR_ARGUMENT;
    *out_clicked = b->pressed && released_inside;
    b->pressed = false;
    b->held_time = 0.0f;
    b->next_repeat = b->repeat_delay;
    return uim_OK;
}

/* Cancel without a click (Escape, pointer capture lost, window deactivate). */
static inline void uim_button_cancel(uim_Button *b) {
    if (!b) return;
    b->pressed = false;
    b->held_time = 0.0f;
    b->next_repeat = b->repeat_delay;
}

/* Keyboard activation. Returns a click edge only when the button is focused. */
static inline uim_Result uim_button_activate(uim_Button *b, bool *out_clicked) {
    if (!b || !out_clicked) return uim_ERR_ARGUMENT;
    *out_clicked = b->focused;
    return uim_OK;
}

/* Advance repeat timing. Emits at most one repeat per tick so a long frame
 * cannot flood the caller. A zero interval disables repeat. */
static inline uim_Result uim_button_tick(uim_Button *b, float dt, bool *out_repeat) {
    if (!b || !out_repeat || !isfinite(dt) || dt < 0.0f) return uim_ERR_ARGUMENT;
    *out_repeat = false;
    if (!b->pressed || !b->hovered || b->repeat_interval <= 0.0f) return uim_OK;
    b->held_time += dt;
    if (b->held_time >= b->next_repeat) {
        *out_repeat = true;
        b->next_repeat += b->repeat_interval;
    }
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_BUTTON_H */
