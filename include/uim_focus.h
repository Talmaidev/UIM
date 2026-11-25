#ifndef UIM_FOCUS_H
#define UIM_FOCUS_H
/* uim_focus.h — the focus atom: tab ring + directional navigation.
 *
 * The editor currently walks its own widget list for Tab and arrow keys, and
 * each dialog invents its own "focus the first field" rule. This header is the
 * one focus ring: stable ids across rebuilds, wrap-around Tab/Shift+Tab,
 * nearest-in-direction arrow navigation and mouse hit testing.
 *
 * Contract:
 * - Fixed capacity (UIM_FOCUS_CAPACITY), no allocation. Overflow is reported as
 *   uim_ERR_BUFFER and the ring is left unchanged.
 * - Items are addressed by caller ids, not pointers, so rebuilding the widget
 *   list every frame keeps the current item selected.
 * - Rects are optional; directional navigation and hit testing require them and
 *   return uim_ERR_STATE when absent.
 * - Tab with no current item selects the first (or last when going backwards);
 *   it never returns "no focus" while the ring is non-empty.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UIM_FOCUS_CAPACITY 64u

typedef struct uim_FocusRing {
    uint32_t ids[UIM_FOCUS_CAPACITY];
    uim_Rect rects[UIM_FOCUS_CAPACITY];
    size_t count;
    size_t current;       /* index into ids; valid only when has_current */
    bool has_current;
    bool has_rects;
} uim_FocusRing;

static inline uim_Result uim_focus_init(uim_FocusRing *ring) {
    if (!ring) return uim_ERR_ARGUMENT;
    ring->count = 0;
    ring->current = 0;
    ring->has_current = false;
    ring->has_rects = false;
    return uim_OK;
}

/* Rebuild the ring. Ids must be unique; a duplicate is uim_ERR_EXISTS and the
 * ring is left unchanged. If the previously focused id is still present it
 * stays focused; otherwise focus falls back to "none" until next/click/set. */
static inline uim_Result uim_focus_set_items(uim_FocusRing *ring, const uint32_t *ids,
                                             const uim_Rect *rects, size_t count) {
    uint32_t previous = 0;
    bool had = false, kept = false;
    size_t i, j;
    if (!ring || (count > 0 && !ids)) return uim_ERR_ARGUMENT;
    if (count > UIM_FOCUS_CAPACITY) return uim_ERR_BUFFER;
    for (i = 0; i < count; i++) {
        if (!isfinite(rects ? rects[i].x : 0.0f) || !isfinite(rects ? rects[i].y : 0.0f) ||
            !isfinite(rects ? rects[i].width : 0.0f) || !isfinite(rects ? rects[i].height : 0.0f) ||
            (rects && (rects[i].width < 0.0f || rects[i].height < 0.0f)))
            return uim_ERR_ARGUMENT;
        for (j = 0; j < i; j++) {
            if (ids[j] == ids[i]) return uim_ERR_EXISTS;
        }
    }
    if (ring->has_current && ring->count > 0) { previous = ring->ids[ring->current]; had = true; }
    for (i = 0; i < count; i++) {
        ring->ids[i] = ids[i];
        if (rects) ring->rects[i] = rects[i];
        else { ring->rects[i].x = ring->rects[i].y = ring->rects[i].width = ring->rects[i].height = 0.0f; }
    }
    ring->count = count;
    ring->has_rects = rects != NULL && count > 0;
    if (had) {
        for (i = 0; i < count; i++) {
            if (ring->ids[i] == previous) { ring->current = i; kept = true; break; }
        }
    }
    if (!kept) { ring->current = 0; ring->has_current = false; }
    return uim_OK;
}

static inline void uim_focus_clear(uim_FocusRing *ring) {
    if (!ring) return;
    ring->count = 0;
    ring->current = 0;
    ring->has_current = false;
    ring->has_rects = false;
}

static inline uim_Result uim_focus_set_current(uim_FocusRing *ring, uint32_t id) {
    size_t i;
    if (!ring) return uim_ERR_ARGUMENT;
    for (i = 0; i < ring->count; i++) {
        if (ring->ids[i] == id) {
            ring->current = i;
            ring->has_current = true;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

static inline uint32_t uim_focus_current(const uim_FocusRing *ring) {
    if (!ring || !ring->has_current || ring->count == 0) return 0u;
    return ring->ids[ring->current];
}

/* direction +1 = Tab, -1 = Shift+Tab. Wraps. */
static inline uim_Result uim_focus_next(uim_FocusRing *ring, int direction) {
    if (!ring || (direction != 1 && direction != -1)) return uim_ERR_ARGUMENT;
    if (ring->count == 0) return uim_ERR_NOT_FOUND;
    if (!ring->has_current) {
        ring->current = direction > 0 ? 0u : ring->count - 1u;
    } else if (direction > 0) {
        ring->current = (ring->current + 1u) % ring->count;
    } else {
        ring->current = (ring->current + ring->count - 1u) % ring->count;
    }
    ring->has_current = true;
    return uim_OK;
}

/* Nearest candidate whose center lies in the half-plane of (dx,dy). */
static inline uim_Result uim_focus_move(uim_FocusRing *ring, float dx, float dy) {
    float ux, uy, len;
    float best_score = 0.0f;
    size_t best = 0, i;
    bool found = false;
    if (!ring || !isfinite(dx) || !isfinite(dy)) return uim_ERR_ARGUMENT;
    len = hypotf(dx, dy);
    if (len <= 0.000001f) return uim_ERR_ARGUMENT;
    if (ring->count == 0) return uim_ERR_NOT_FOUND;
    if (!ring->has_rects) return uim_ERR_STATE;
    if (!ring->has_current) {
        ring->current = 0;
        ring->has_current = true;
    }
    ux = dx / len;
    uy = dy / len;
    {
        const uim_Rect *from = &ring->rects[ring->current];
        float cx = from->x + from->width * 0.5f;
        float cy = from->y + from->height * 0.5f;
        for (i = 0; i < ring->count; i++) {
            float tx, ty, ox, oy, primary, cross, score;
            if (i == ring->current) continue;
            tx = ring->rects[i].x + ring->rects[i].width * 0.5f;
            ty = ring->rects[i].y + ring->rects[i].height * 0.5f;
            ox = tx - cx;
            oy = ty - cy;
            primary = ox * ux + oy * uy;
            if (primary <= 0.000001f) continue;
            cross = fabsf(ox * uy - oy * ux);
            score = primary + cross * 2.0f;
            if (!found || score < best_score) { found = true; best = i; best_score = score; }
        }
    }
    if (!found) return uim_ERR_NOT_FOUND;
    ring->current = best;
    ring->has_current = true;
    return uim_OK;
}

static inline uim_Result uim_focus_hit(const uim_FocusRing *ring, float x, float y,
                                       uint32_t *out_id) {
    size_t i;
    if (!ring || !out_id || !isfinite(x) || !isfinite(y)) return uim_ERR_ARGUMENT;
    if (!ring->has_rects) return uim_ERR_STATE;
    for (i = 0; i < ring->count; i++) {
        const uim_Rect *r = &ring->rects[i];
        if (x >= r->x && x <= r->x + r->width && y >= r->y && y <= r->y + r->height) {
            *out_id = ring->ids[i];
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_FOCUS_H */
