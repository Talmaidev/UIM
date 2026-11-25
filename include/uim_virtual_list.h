#ifndef UIM_VIRTUAL_LIST_H
#define UIM_VIRTUAL_LIST_H
/* uim_virtual_list.h — the virtual-list atom: visible range, item rects.
 *
 * The hierarchy panel, the console and the asset list all draw every row and
 * all compute "which rows are on screen" by hand. This header is the one
 * answer: total height, first/last visible index with overscan, the rect of an
 * index, index-at-point and minimal scroll-to-index.
 *
 * Contract:
 * - Fixed item height (variable heights need a measured table; out of scope).
 * - The caller owns the state and draws only [first, last); no allocation.
 * - Offsets are clamped to [0, total - viewport]; clamping returns
 *   uim_ERR_RANGE with the clamped value applied.
 * - Rects are in viewport coordinates; item heights exclude the gap.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_VirtualList {
    size_t count;
    float item_height;   /* > 0 */
    float gap;           /* >= 0 */
    float viewport_h;    /* >= 0 */
    float offset;        /* >= 0, content pixels above the viewport */
    size_t overscan;     /* extra items on each side */
} uim_VirtualList;

static inline float uim_virtual__pitch(const uim_VirtualList *l) {
    return l->item_height + l->gap;
}

static inline float uim_virtual_total_height(const uim_VirtualList *l) {
    if (!l || l->count == 0) return 0.0f;
    return (float)l->count * uim_virtual__pitch(l) - l->gap;
}

static inline float uim_virtual_max_offset(const uim_VirtualList *l) {
    float max_offset;
    if (!l) return 0.0f;
    max_offset = uim_virtual_total_height(l) - l->viewport_h;
    return max_offset > 0.0f ? max_offset : 0.0f;
}

static inline uim_Result uim_virtual_init(uim_VirtualList *l, size_t count, float item_height,
                                          float gap, float viewport_h, size_t overscan) {
    if (!l || !isfinite(item_height) || !isfinite(gap) || !isfinite(viewport_h) ||
        item_height <= 0.0f || gap < 0.0f || viewport_h < 0.0f)
        return uim_ERR_ARGUMENT;
    l->count = count;
    l->item_height = item_height;
    l->gap = gap;
    l->viewport_h = viewport_h;
    l->offset = 0.0f;
    l->overscan = overscan;
    return uim_OK;
}

static inline uim_Result uim_virtual_set_offset(uim_VirtualList *l, float offset) {
    bool clamped = false;
    if (!l || !isfinite(offset)) return uim_ERR_ARGUMENT;
    if (offset < 0.0f) { offset = 0.0f; clamped = true; }
    if (offset > uim_virtual_max_offset(l)) { offset = uim_virtual_max_offset(l); clamped = true; }
    l->offset = offset;
    return clamped ? uim_ERR_RANGE : uim_OK;
}

/* Half-open [first, last); both equal count when nothing is visible. */
static inline uim_Result uim_virtual_range(const uim_VirtualList *l,
                                           size_t *first, size_t *last_exclusive) {
    size_t f, last;
    float pitch, end;
    if (!l || !first || !last_exclusive) return uim_ERR_ARGUMENT;
    if (l->count == 0) { *first = 0; *last_exclusive = 0; return uim_OK; }
    pitch = uim_virtual__pitch(l);
    if (pitch <= 0.0f) return uim_ERR_ARGUMENT;
    if (l->offset >= uim_virtual_total_height(l)) {
        *first = l->count;
        *last_exclusive = l->count;
        return uim_OK;
    }
    f = (size_t)floorf(l->offset / pitch);
    if (f > l->count) f = l->count;
    end = l->offset + l->viewport_h;
    last = (size_t)ceilf(end / pitch);
    if (last > l->count) last = l->count;
    if (l->viewport_h <= 0.0f) last = f;
    if (last < f) last = f;
    if (l->overscan > 0u) {
        f = f > l->overscan ? f - l->overscan : 0u;
        if (last + l->overscan < last) last = l->count; /* overflow guard */
        else if (last + l->overscan > l->count) last = l->count;
        else last += l->overscan;
    }
    *first = f;
    *last_exclusive = last;
    return uim_OK;
}

/* Rect of an item in viewport coordinates (y can be negative above the view). */
static inline uim_Result uim_virtual_item_rect(const uim_VirtualList *l, size_t index,
                                               float viewport_x, float viewport_width,
                                               uim_Rect *out) {
    if (!l || !out || !isfinite(viewport_x) || !isfinite(viewport_width) ||
        viewport_width < 0.0f)
        return uim_ERR_ARGUMENT;
    if (index >= l->count) return uim_ERR_NOT_FOUND;
    out->x = viewport_x;
    out->y = (float)index * uim_virtual__pitch(l) - l->offset;
    out->width = viewport_width;
    out->height = l->item_height;
    return uim_OK;
}

/* align_ratio 0 = top, 0.5 = center, 1 = bottom. */
static inline uim_Result uim_virtual_scroll_to(uim_VirtualList *l, size_t index,
                                               float align_ratio) {
    float target;
    if (!l || !isfinite(align_ratio)) return uim_ERR_ARGUMENT;
    if (index >= l->count) return uim_ERR_NOT_FOUND;
    if (align_ratio < 0.0f) align_ratio = 0.0f;
    if (align_ratio > 1.0f) align_ratio = 1.0f;
    target = (float)index * uim_virtual__pitch(l) -
             (l->viewport_h - l->item_height) * align_ratio;
    return uim_virtual_set_offset(l, target);
}

/* Index under a y coordinate relative to the viewport top. Gaps map to the
 * preceding item. */
static inline uim_Result uim_virtual_index_at(const uim_VirtualList *l, float y,
                                              size_t *out_index) {
    float content_y;
    size_t index;
    if (!l || !out_index || !isfinite(y)) return uim_ERR_ARGUMENT;
    if (l->count == 0) return uim_ERR_NOT_FOUND;
    if (y < 0.0f || y > l->viewport_h) return uim_ERR_RANGE;
    content_y = l->offset + y;
    index = (size_t)floorf(content_y / uim_virtual__pitch(l));
    if (index >= l->count) index = l->count - 1u;
    *out_index = index;
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_VIRTUAL_LIST_H */
