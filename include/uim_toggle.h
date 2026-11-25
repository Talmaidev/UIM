#ifndef UIM_TOGGLE_H
#define UIM_TOGGLE_H
/* uim_toggle.h — the checkbox/switch atom: one boolean with click semantics.
 *
 * The state and the click edge live here. Drawing, focus and hit testing stay
 * with the caller, exactly like uim_button.
 */
#include "uim_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Toggle {
    bool on;
    bool hovered;
    bool pressed;
    bool focused;
} uim_Toggle;

static inline uim_Result uim_toggle_init(uim_Toggle *t, bool initial_on) {
    if (!t) return uim_ERR_ARGUMENT;
    t->on = initial_on;
    t->hovered = false;
    t->pressed = false;
    t->focused = false;
    return uim_OK;
}

static inline void uim_toggle_set_hover(uim_Toggle *t, bool hovered) {
    if (t) t->hovered = hovered;
}

static inline void uim_toggle_set_focus(uim_Toggle *t, bool focused) {
    if (t) t->focused = focused;
}

static inline uim_Result uim_toggle_set(uim_Toggle *t, bool on) {
    if (!t) return uim_ERR_ARGUMENT;
    t->on = on;
    return uim_OK;
}

static inline uim_Result uim_toggle_flip(uim_Toggle *t) {
    if (!t) return uim_ERR_ARGUMENT;
    t->on = !t->on;
    return uim_OK;
}

static inline uim_Result uim_toggle_pointer_down(uim_Toggle *t) {
    if (!t) return uim_ERR_ARGUMENT;
    t->pressed = true;
    return uim_OK;
}

/* A toggle flips only on release inside after a press. */
static inline uim_Result uim_toggle_pointer_up(uim_Toggle *t, bool released_inside,
                                               bool *out_changed) {
    if (!t || !out_changed) return uim_ERR_ARGUMENT;
    *out_changed = false;
    if (t->pressed && released_inside) {
        t->on = !t->on;
        *out_changed = true;
    }
    t->pressed = false;
    return uim_OK;
}

static inline uim_Result uim_toggle_activate(uim_Toggle *t, bool *out_changed) {
    if (!t || !out_changed) return uim_ERR_ARGUMENT;
    *out_changed = false;
    if (t->focused) {
        t->on = !t->on;
        *out_changed = true;
    }
    return uim_OK;
}

static inline void uim_toggle_cancel(uim_Toggle *t) {
    if (t) t->pressed = false;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_TOGGLE_H */
