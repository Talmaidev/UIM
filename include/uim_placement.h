#ifndef UIM_PLACEMENT_H
#define UIM_PLACEMENT_H
/* uim_placement.h — the placement atom: popups, tooltips, menus.
 *
 * The engine's ui.c hand-computes dropdown geometry (L592-618): try below, flip
 * above, clamp into the window. Every context menu and tooltip then repeats it.
 * This header is the one placement solver: preferred side, flip, clamp, and the
 * arrow offset the host needs to draw the pointer.
 *
 * Contract:
 * - Pure geometry. The popup size is caller-owned and is never resized; only
 *   its position (and the arrow offset) changes.
 * - "Fits" means fully inside bounds with no extra margin. If no side fits,
 *   the side with the most space is used and the rect is clamped into bounds,
 *   reported as uim_ERR_RANGE plus the clamped flag.
 * - arrow is the center offset from the popup's start edge (x for above/below,
 *   y for left/right), clamped so it cannot leave the popup.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uim_PlacementSide {
    uim_PLACE_BELOW = 0,
    uim_PLACE_ABOVE,
    uim_PLACE_RIGHT,
    uim_PLACE_LEFT
} uim_PlacementSide;

typedef struct uim_Placement {
    uim_Rect rect;
    uim_PlacementSide side;
    bool flipped;   /* side != preferred */
    bool clamped;   /* had to be pushed inside bounds */
    float arrow;    /* center offset along the anchor-facing edge */
} uim_Placement;

static inline float uim_placement__clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float uim_placement__right(const uim_Rect *r) { return r->x + r->width; }
static inline float uim_placement__bottom(const uim_Rect *r) { return r->y + r->height; }

static inline bool uim_placement__fits(const uim_Rect *r, const uim_Rect *bounds) {
    return r->x >= bounds->x && r->y >= bounds->y &&
           uim_placement__right(r) <= uim_placement__right(bounds) &&
           uim_placement__bottom(r) <= uim_placement__bottom(bounds);
}

static inline uim_Rect uim_placement__candidate(const uim_Rect *anchor, float w, float h,
                                                float gap, uim_PlacementSide side) {
    uim_Rect r = {0.0f, 0.0f, w, h};
    float acx = anchor->x + anchor->width * 0.5f;
    float acy = anchor->y + anchor->height * 0.5f;
    if (side == uim_PLACE_BELOW) {
        r.x = acx - w * 0.5f;
        r.y = uim_placement__bottom(anchor) + gap;
    } else if (side == uim_PLACE_ABOVE) {
        r.x = acx - w * 0.5f;
        r.y = anchor->y - gap - h;
    } else if (side == uim_PLACE_RIGHT) {
        r.x = uim_placement__right(anchor) + gap;
        r.y = acy - h * 0.5f;
    } else {
        r.x = anchor->x - gap - w;
        r.y = acy - h * 0.5f;
    }
    return r;
}

static inline float uim_placement__space(const uim_Rect *anchor, const uim_Rect *bounds,
                                         float gap, uim_PlacementSide side) {
    if (side == uim_PLACE_BELOW) return uim_placement__bottom(bounds) - (uim_placement__bottom(anchor) + gap);
    if (side == uim_PLACE_ABOVE) return (anchor->y - gap) - bounds->y;
    if (side == uim_PLACE_RIGHT) return uim_placement__right(bounds) - (uim_placement__right(anchor) + gap);
    return (anchor->x - gap) - bounds->x;
}

static inline uim_PlacementSide uim_placement__opposite(uim_PlacementSide s) {
    if (s == uim_PLACE_BELOW) return uim_PLACE_ABOVE;
    if (s == uim_PLACE_ABOVE) return uim_PLACE_BELOW;
    if (s == uim_PLACE_RIGHT) return uim_PLACE_LEFT;
    return uim_PLACE_RIGHT;
}

static inline uim_Result uim_place_popup(const uim_Rect *anchor, float popup_width,
                                         float popup_height, const uim_Rect *bounds,
                                         float gap, uim_PlacementSide preferred,
                                         uim_Placement *out) {
    uim_PlacementSide order[4];
    uim_Rect best;
    uim_PlacementSide chosen;
    float arrow_radius;
    size_t i;
    bool found = false, clamped = false;

    if (!anchor || !bounds || !out || !isfinite(popup_width) || !isfinite(popup_height) ||
        !isfinite(gap) || popup_width < 0.0f || popup_height < 0.0f || gap < 0.0f)
        return uim_ERR_ARGUMENT;
    if (!isfinite(anchor->x) || !isfinite(anchor->y) || !isfinite(anchor->width) ||
        !isfinite(anchor->height) || !isfinite(bounds->x) || !isfinite(bounds->y) ||
        !isfinite(bounds->width) || !isfinite(bounds->height) ||
        anchor->width < 0.0f || anchor->height < 0.0f ||
        bounds->width < 0.0f || bounds->height < 0.0f)
        return uim_ERR_ARGUMENT;
    if (preferred != uim_PLACE_BELOW && preferred != uim_PLACE_ABOVE &&
        preferred != uim_PLACE_RIGHT && preferred != uim_PLACE_LEFT)
        return uim_ERR_ARGUMENT;

    order[0] = preferred;
    order[1] = uim_placement__opposite(preferred);
    order[2] = (preferred == uim_PLACE_LEFT || preferred == uim_PLACE_RIGHT) ? uim_PLACE_BELOW : uim_PLACE_RIGHT;
    order[3] = (preferred == uim_PLACE_LEFT || preferred == uim_PLACE_RIGHT) ? uim_PLACE_ABOVE : uim_PLACE_LEFT;

    for (i = 0; i < 4u; i++) {
        uim_Rect candidate = uim_placement__candidate(anchor, popup_width, popup_height, gap, order[i]);
        if (uim_placement__fits(&candidate, bounds)) {
            best = candidate;
            chosen = order[i];
            found = true;
            break;
        }
    }
    if (!found) {
        float best_space = -1.0f;
        chosen = preferred;
        for (i = 0; i < 4u; i++) {
            float space = uim_placement__space(anchor, bounds, gap, order[i]);
            if (space > best_space) { best_space = space; chosen = order[i]; }
        }
        best = uim_placement__candidate(anchor, popup_width, popup_height, gap, chosen);
        best.x = uim_placement__clampf(best.x, bounds->x,
                                       uim_placement__right(bounds) - popup_width);
        best.y = uim_placement__clampf(best.y, bounds->y,
                                       uim_placement__bottom(bounds) - popup_height);
        if (uim_placement__right(bounds) - popup_width < bounds->x) best.x = bounds->x;
        if (uim_placement__bottom(bounds) - popup_height < bounds->y) best.y = bounds->y;
        clamped = true;
    }

    out->rect = best;
    out->side = chosen;
    out->flipped = chosen != preferred;
    out->clamped = clamped;

    arrow_radius = popup_width * 0.5f;
    if (arrow_radius > 12.0f) arrow_radius = 12.0f;
    if (chosen == uim_PLACE_BELOW || chosen == uim_PLACE_ABOVE) {
        float anchor_center = anchor->x + anchor->width * 0.5f;
        out->arrow = uim_placement__clampf(anchor_center - best.x, arrow_radius,
                                           popup_width - arrow_radius);
    } else {
        float anchor_center = anchor->y + anchor->height * 0.5f;
        arrow_radius = popup_height * 0.5f;
        if (arrow_radius > 12.0f) arrow_radius = 12.0f;
        out->arrow = uim_placement__clampf(anchor_center - best.y, arrow_radius,
                                           popup_height - arrow_radius);
    }
    return clamped ? uim_ERR_RANGE : uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_PLACEMENT_H */
