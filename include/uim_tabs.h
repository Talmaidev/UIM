#ifndef UIM_TABS_H
#define UIM_TABS_H
/* uim_tabs.h — the tabs atom: one active index, step, wrap, hit test.
 *
 * The settings page keeps its own tab index and its own "did the click land in
 * a tab" loop; the editor's inspector tabs do the same. This header is the
 * index state machine only — the caller supplies the tab rects and draws them.
 *
 * Contract:
 * - active is always < count (or 0 when count == 0).
 * - select/step wrap when wrap is true; otherwise they clamp and return
 *   uim_ERR_RANGE so the boundary is visible (the clamped value is applied).
 * - Hit testing uses the caller's rects; a point outside every rect is
 *   uim_ERR_NOT_FOUND.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Tabs {
    size_t count;
    size_t active;
    bool wrap;
} uim_Tabs;

static inline uim_Result uim_tabs_init(uim_Tabs *t, size_t count, size_t active) {
    if (!t || (count > 0 && active >= count)) return uim_ERR_ARGUMENT;
    t->count = count;
    t->active = count > 0 ? active : 0;
    t->wrap = true;
    return uim_OK;
}

static inline uim_Result uim_tabs_set_count(uim_Tabs *t, size_t count) {
    if (!t) return uim_ERR_ARGUMENT;
    t->count = count;
    if (count == 0) t->active = 0;
    else if (t->active >= count) t->active = count - 1u;
    return uim_OK;
}

/* Returns uim_ERR_RANGE and leaves active unchanged when index is out of range. */
static inline uim_Result uim_tabs_select(uim_Tabs *t, size_t index) {
    if (!t) return uim_ERR_ARGUMENT;
    if (index >= t->count) return uim_ERR_RANGE;
    t->active = index;
    return uim_OK;
}

static inline uim_Result uim_tabs_step(uim_Tabs *t, int delta) {
    long long target;
    bool clamped = false;
    if (!t) return uim_ERR_ARGUMENT;
    if (t->count == 0) return uim_ERR_NOT_FOUND;
    target = (long long)t->active + (long long)delta;
    if (t->wrap) {
        long long n = (long long)t->count;
        target %= n;
        if (target < 0) target += n;
    } else {
        if (target < 0) { target = 0; clamped = true; }
        if (target >= (long long)t->count) { target = (long long)t->count - 1; clamped = true; }
        if (clamped) {
            t->active = (size_t)target;
            return uim_ERR_RANGE;
        }
    }
    t->active = (size_t)target;
    return uim_OK;
}

static inline uim_Result uim_tabs_hit(const uim_Tabs *t, const uim_Rect *rects,
                                      size_t rects_count, float x, float y,
                                      size_t *out_index) {
    size_t i;
    if (!t || !rects || !out_index || !isfinite(x) || !isfinite(y)) return uim_ERR_ARGUMENT;
    for (i = 0; i < t->count && i < rects_count; i++) {
        const uim_Rect *r = &rects[i];
        if (!isfinite(r->x) || !isfinite(r->y) || !isfinite(r->width) || !isfinite(r->height))
            return uim_ERR_ARGUMENT;
        if (x >= r->x && x <= r->x + r->width && y >= r->y && y <= r->y + r->height) {
            *out_index = i;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_TABS_H */
