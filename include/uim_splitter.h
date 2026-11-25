#ifndef UIM_SPLITTER_H
#define UIM_SPLITTER_H
/* uim_splitter.h — the sash atom: draggable split ratio with per-pane minimum
 * sizes.
 *
 * What it owns:
 * - split axis, first-pane ratio (0..1), minimum pane sizes in pixels, and
 *   the dragging flag.
 * - ratio conversion from a pointer position, clamped so neither pane drops
 *   below its minimum; clamping returns uim_ERR_RANGE with the ratio applied.
 *
 * What it does not own: the press threshold / gesture (uim_drag owns that),
 * the sash rect hit test, drawing, or the final pane rects
 * (uim_layout_split computes those from the ratio).
 *
 * Contract: pure state + math. No allocation, no globals, no renderer.
 */
#include "uim_layout.h"
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Splitter {
    uim_LayoutAxis axis;
    float ratio;
    float min_first;
    float min_second;
    bool dragging;
} uim_Splitter;

static inline uim_Result uim_splitter_init(uim_Splitter *s, uim_LayoutAxis axis,
                                          float ratio, float min_first, float min_second) {
    if (!s || (axis != uim_LAYOUT_ROW && axis != uim_LAYOUT_COLUMN) ||
        !isfinite(ratio) || !isfinite(min_first) || !isfinite(min_second) ||
        min_first < 0.0f || min_second < 0.0f)
        return uim_ERR_ARGUMENT;
    s->axis = axis;
    s->min_first = min_first;
    s->min_second = min_second;
    s->dragging = false;
    if (ratio < 0.0f) ratio = 0.0f;
    else if (ratio > 1.0f) ratio = 1.0f;
    s->ratio = ratio;
    return uim_OK;
}

static inline uim_Result uim_splitter_begin_drag(uim_Splitter *s) {
    if (!s) return uim_ERR_ARGUMENT;
    s->dragging = true;
    return uim_OK;
}

static inline void uim_splitter_end_drag(uim_Splitter *s) {
    if (s) s->dragging = false;
}

/* Free main-axis length after the sash gap. */
static inline float uim_splitter__free(const uim_Rect *container, uim_LayoutAxis axis, float gap) {
    float main = axis == uim_LAYOUT_ROW ? container->width : container->height;
    float free_len = main - gap;
    return free_len > 0.0f ? free_len : 0.0f;
}

/* Ratio bounds enforced by the minimum pane sizes, for a given free length. */
static inline void uim_splitter__bounds(const uim_Splitter *s, float free_len,
                                       float *lo, float *hi) {
    if (free_len <= 0.0f) { *lo = 0.0f; *hi = 1.0f; return; }
    *lo = s->min_first / free_len;
    *hi = 1.0f - s->min_second / free_len;
    if (*lo < 0.0f) *lo = 0.0f;
    if (*hi > 1.0f) *hi = 1.0f;
    if (*lo > *hi) { *lo = *hi = 0.5f; }
}

/* Convert a pointer position along the split axis to a clamped ratio.
 * position is x for ROW, y for COLUMN, in container coordinates. */
static inline uim_Result uim_splitter_ratio_at(const uim_Splitter *s,
                                              const uim_Rect *container, float gap,
                                              float position, float *out_ratio) {
    float origin, free_len, raw, lo, hi;
    if (!s || !container || !out_ratio || !isfinite(gap) || gap < 0.0f ||
        !isfinite(position) || !isfinite(container->x) || !isfinite(container->y) ||
        !isfinite(container->width) || !isfinite(container->height) ||
        container->width < 0.0f || container->height < 0.0f)
        return uim_ERR_ARGUMENT;
    origin = s->axis == uim_LAYOUT_ROW ? container->x : container->y;
    free_len = uim_splitter__free(container, s->axis, gap);
    if (free_len <= 0.0f) { *out_ratio = s->ratio; return uim_OK; }
    raw = (position - origin) / free_len;
    uim_splitter__bounds(s, free_len, &lo, &hi);
    {
        bool clamped = raw < lo || raw > hi;
        if (raw < lo) raw = lo;
        else if (raw > hi) raw = hi;
        *out_ratio = raw;
        return clamped ? uim_ERR_RANGE : uim_OK;
    }
}

/* Drag the sash to a new pointer position; the ratio is clamped to the
 * minimum-pane bounds. */
static inline uim_Result uim_splitter_drag_to(uim_Splitter *s, const uim_Rect *container,
                                              float gap, float position) {
    float ratio;
    uim_Result rc;
    if (!s) return uim_ERR_ARGUMENT;
    if (!s->dragging) return uim_ERR_STATE;
    rc = uim_splitter_ratio_at(s, container, gap, position, &ratio);
    if (rc == uim_ERR_ARGUMENT) return rc;
    s->ratio = ratio;
    return rc;
}

/* Compute the two pane rects for the current ratio. Geometry delegates to
 * uim_layout_split; this atom only owns the ratio and its constraints. */
static inline uim_Result uim_splitter_pane_rects(const uim_Splitter *s,
                                                 const uim_Rect *container, float gap,
                                                 uim_Rect *first, uim_Rect *second) {
    if (!s) return uim_ERR_ARGUMENT;
    return uim_layout_split(container, s->axis, s->ratio, gap, first, second);
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_SPLITTER_H */
