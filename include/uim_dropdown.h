#ifndef UIM_DROPDOWN_H
#define UIM_DROPDOWN_H
/* uim_dropdown.h — the select/dropdown atom: open state, selected index,
 * keyboard step and item hit testing.
 *
 * Popup placement is uim_placement; scrolling is uim_scroll; focus is
 * uim_focus. This header only owns the index and open flag so every page does
 * not invent its own "which item is active" bookkeeping.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Dropdown {
    size_t count;
    size_t selected;
    bool open;
    bool wrap;
} uim_Dropdown;

static inline uim_Result uim_dropdown_init(uim_Dropdown *d, size_t count,
                                           size_t selected) {
    if (!d || (count > 0 && selected >= count)) return uim_ERR_ARGUMENT;
    d->count = count;
    d->selected = count > 0 ? selected : 0;
    d->open = false;
    d->wrap = true;
    return uim_OK;
}

static inline uim_Result uim_dropdown_set_count(uim_Dropdown *d, size_t count,
                                                size_t selected) {
    if (!d || (count > 0 && selected >= count)) return uim_ERR_ARGUMENT;
    d->count = count;
    d->selected = count > 0 ? selected : 0;
    if (count == 0) d->open = false;
    return uim_OK;
}

static inline uim_Result uim_dropdown_open(uim_Dropdown *d) {
    if (!d) return uim_ERR_ARGUMENT;
    if (d->count == 0) return uim_ERR_NOT_FOUND;
    d->open = true;
    return uim_OK;
}

static inline uim_Result uim_dropdown_close(uim_Dropdown *d) {
    if (!d) return uim_ERR_ARGUMENT;
    d->open = false;
    return uim_OK;
}

static inline uim_Result uim_dropdown_toggle(uim_Dropdown *d) {
    if (!d) return uim_ERR_ARGUMENT;
    if (d->open) { d->open = false; return uim_OK; }
    return uim_dropdown_open(d);
}

static inline uim_Result uim_dropdown_select(uim_Dropdown *d, size_t index) {
    if (!d) return uim_ERR_ARGUMENT;
    if (index >= d->count) return uim_ERR_RANGE;
    d->selected = index;
    d->open = false;
    return uim_OK;
}

static inline uim_Result uim_dropdown_step(uim_Dropdown *d, int delta) {
    long long target;
    bool clamped = false;
    if (!d) return uim_ERR_ARGUMENT;
    if (d->count == 0) return uim_ERR_NOT_FOUND;
    target = (long long)d->selected + (long long)delta;
    if (d->wrap) {
        long long n = (long long)d->count;
        target %= n;
        if (target < 0) target += n;
    } else {
        if (target < 0) { target = 0; clamped = true; }
        if (target >= (long long)d->count) {
            target = (long long)d->count - 1;
            clamped = true;
        }
    }
    d->selected = (size_t)target;
    return clamped ? uim_ERR_RANGE : uim_OK;
}

/* Hit test over caller-owned item rects. Returns uim_ERR_NOT_FOUND outside. */
static inline uim_Result uim_dropdown_hit(const uim_Dropdown *d,
                                          const uim_Rect *rects,
                                          size_t rects_count,
                                          float x, float y,
                                          size_t *out_index) {
    size_t i;
    if (!d || !rects || !out_index || !isfinite(x) || !isfinite(y)
        || rects_count == 0)
        return uim_ERR_ARGUMENT;
    for (i = 0; i < d->count && i < rects_count; i++) {
        const uim_Rect *r = &rects[i];
        if (!isfinite(r->x) || !isfinite(r->y)
            || !isfinite(r->width) || !isfinite(r->height))
            return uim_ERR_ARGUMENT;
        if (x >= r->x && x <= r->x + r->width
            && y >= r->y && y <= r->y + r->height) {
            *out_index = i;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_DROPDOWN_H */
