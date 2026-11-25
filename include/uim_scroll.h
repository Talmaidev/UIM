#ifndef UIM_SCROLL_H
#define UIM_SCROLL_H
/* uim_scroll.h — the scroll atom: offset, clamping, wheel/page, bar geometry.
 *
 * The engine's ui.c currently hand-computes the scrollbar thumb (L571-578) and
 * clamps offsets inline; every panel that scrolls repeats the same three lines.
 * This header centralizes the arithmetic; drawing still belongs to the host.
 *
 * Contract:
 * - Pure state + math. The caller owns uim_ScrollState; no allocation.
 * - Offsets are clamped to [0, content - view] on every mutation. A mutation
 *   that had to clamp returns uim_ERR_RANGE (the new value is still applied),
 *   so overscroll is observable in tests.
 * - Disabled axes ignore wheel/page/drag and report an invisible scrollbar.
 * - Scrollbar metrics assume the track spans the view; thumb size is at least
 *   min_thumb but never fills the whole track (a full track means "not needed").
 */
#include "uim_layout.h"
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_ScrollState {
    float offset_x, offset_y;   /* content pixels scrolled out of view */
    float view_w, view_h;
    float content_w, content_h;
    float wheel_step;           /* pixels per wheel notch, > 0 */
    float min_thumb;            /* pixels, > 0 */
    bool axis_x, axis_y;
} uim_ScrollState;

typedef struct uim_ScrollbarMetrics {
    bool visible;
    float track_length;
    float thumb_length;
    float thumb_offset;         /* from the start of the track */
    float max_offset;
} uim_ScrollbarMetrics;

static inline float uim_scroll__nonneg(float v) { return v > 0.0f ? v : 0.0f; }

static inline uim_Result uim_scroll_init(uim_ScrollState *s, float view_w, float view_h,
                                         float wheel_step) {
    if (!s || !isfinite(view_w) || !isfinite(view_h) || !isfinite(wheel_step) ||
        view_w < 0.0f || view_h < 0.0f || wheel_step <= 0.0f)
        return uim_ERR_ARGUMENT;
    s->offset_x = s->offset_y = 0.0f;
    s->view_w = view_w;
    s->view_h = view_h;
    s->content_w = view_w;
    s->content_h = view_h;
    s->wheel_step = wheel_step;
    s->min_thumb = 24.0f;
    s->axis_x = true;
    s->axis_y = true;
    return uim_OK;
}

static inline float uim_scroll_max_x(const uim_ScrollState *s) {
    if (!s || !s->axis_x) return 0.0f;
    return uim_scroll__nonneg(s->content_w - s->view_w);
}

static inline float uim_scroll_max_y(const uim_ScrollState *s) {
    if (!s || !s->axis_y) return 0.0f;
    return uim_scroll__nonneg(s->content_h - s->view_h);
}

static inline uim_Result uim_scroll_set_view(uim_ScrollState *s, float width, float height) {
    bool clamped;
    if (!s || !isfinite(width) || !isfinite(height) || width < 0.0f || height < 0.0f)
        return uim_ERR_ARGUMENT;
    s->view_w = width;
    s->view_h = height;
    s->offset_x = s->offset_x < 0.0f ? 0.0f : s->offset_x;
    s->offset_y = s->offset_y < 0.0f ? 0.0f : s->offset_y;
    clamped = s->offset_x > uim_scroll_max_x(s) || s->offset_y > uim_scroll_max_y(s);
    if (s->offset_x > uim_scroll_max_x(s)) s->offset_x = uim_scroll_max_x(s);
    if (s->offset_y > uim_scroll_max_y(s)) s->offset_y = uim_scroll_max_y(s);
    return clamped ? uim_ERR_RANGE : uim_OK;
}

static inline uim_Result uim_scroll_set_content(uim_ScrollState *s, float width, float height) {
    bool clamped;
    if (!s || !isfinite(width) || !isfinite(height) || width < 0.0f || height < 0.0f)
        return uim_ERR_ARGUMENT;
    s->content_w = width;
    s->content_h = height;
    clamped = s->offset_x > uim_scroll_max_x(s) || s->offset_y > uim_scroll_max_y(s);
    if (s->offset_x > uim_scroll_max_x(s)) s->offset_x = uim_scroll_max_x(s);
    if (s->offset_y > uim_scroll_max_y(s)) s->offset_y = uim_scroll_max_y(s);
    return clamped ? uim_ERR_RANGE : uim_OK;
}

static inline uim_Result uim_scroll_set_axis(uim_ScrollState *s, bool x, bool y) {
    if (!s) return uim_ERR_ARGUMENT;
    s->axis_x = x;
    s->axis_y = y;
    if (!x) s->offset_x = 0.0f;
    if (!y) s->offset_y = 0.0f;
    return uim_OK;
}

static inline uim_Result uim_scroll_set_offset(uim_ScrollState *s, float x, float y) {
    bool clamped = false;
    if (!s || !isfinite(x) || !isfinite(y)) return uim_ERR_ARGUMENT;
    if (x < 0.0f) { x = 0.0f; clamped = true; }
    if (y < 0.0f) { y = 0.0f; clamped = true; }
    if (x > uim_scroll_max_x(s)) { x = uim_scroll_max_x(s); clamped = true; }
    if (y > uim_scroll_max_y(s)) { y = uim_scroll_max_y(s); clamped = true; }
    s->offset_x = x;
    s->offset_y = y;
    return clamped ? uim_ERR_RANGE : uim_OK;
}

/* Wheel deltas in notches; positive means down / right (content scrolls up /
 * left, i.e. offset grows). */
static inline uim_Result uim_scroll_by_wheel(uim_ScrollState *s, float wheel_y, float wheel_x) {
    float x, y;
    bool clamped = false;
    if (!s || !isfinite(wheel_x) || !isfinite(wheel_y)) return uim_ERR_ARGUMENT;
    x = s->offset_x + (s->axis_x ? wheel_x * s->wheel_step : 0.0f);
    y = s->offset_y + (s->axis_y ? wheel_y * s->wheel_step : 0.0f);
    if (x < 0.0f) { x = 0.0f; clamped = true; }
    if (y < 0.0f) { y = 0.0f; clamped = true; }
    if (x > uim_scroll_max_x(s)) { x = uim_scroll_max_x(s); clamped = true; }
    if (y > uim_scroll_max_y(s)) { y = uim_scroll_max_y(s); clamped = true; }
    s->offset_x = x;
    s->offset_y = y;
    return clamped ? uim_ERR_RANGE : uim_OK;
}

/* One page = 90% of the view, direction -1 (up/left) or +1 (down/right). */
static inline uim_Result uim_scroll_page(uim_ScrollState *s, uim_LayoutAxis axis, int direction) {
    float step, x, y;
    bool clamped = false;
    if (!s || (direction != -1 && direction != 1)) return uim_ERR_ARGUMENT;
    if (axis == uim_LAYOUT_ROW) {
        if (!s->axis_x) return uim_OK;
        step = s->view_w * 0.9f * (float)direction;
        x = s->offset_x + step;
        y = s->offset_y;
    } else if (axis == uim_LAYOUT_COLUMN) {
        if (!s->axis_y) return uim_OK;
        step = s->view_h * 0.9f * (float)direction;
        x = s->offset_x;
        y = s->offset_y + step;
    } else {
        return uim_ERR_ARGUMENT;
    }
    if (x < 0.0f) { x = 0.0f; clamped = true; }
    if (y < 0.0f) { y = 0.0f; clamped = true; }
    if (x > uim_scroll_max_x(s)) { x = uim_scroll_max_x(s); clamped = true; }
    if (y > uim_scroll_max_y(s)) { y = uim_scroll_max_y(s); clamped = true; }
    s->offset_x = x;
    s->offset_y = y;
    return clamped ? uim_ERR_RANGE : uim_OK;
}

static inline uim_Result uim_scrollbar_metrics(const uim_ScrollState *s, uim_LayoutAxis axis,
                                               float track_length,
                                               uim_ScrollbarMetrics *out) {
    float view, content, max_offset, thumb, thumb_offset;
    if (!s || !out || !isfinite(track_length) || track_length < 0.0f) return uim_ERR_ARGUMENT;
    out->visible = false;
    out->track_length = track_length;
    out->thumb_length = 0.0f;
    out->thumb_offset = 0.0f;
    out->max_offset = 0.0f;
    if (axis == uim_LAYOUT_ROW) { view = s->view_w; content = s->content_w; if (!s->axis_x) return uim_OK; }
    else if (axis == uim_LAYOUT_COLUMN) { view = s->view_h; content = s->content_h; if (!s->axis_y) return uim_OK; }
    else return uim_ERR_ARGUMENT;
    max_offset = uim_scroll__nonneg(content - view);
    out->max_offset = max_offset;
    if (track_length <= 0.0f || view <= 0.0f || content <= view) return uim_OK;

    thumb = track_length * (view / content);
    if (thumb < s->min_thumb) thumb = s->min_thumb;
    if (thumb > track_length) thumb = track_length;
    if (thumb >= track_length) return uim_OK;   /* nothing left to drag */
    thumb_offset = (track_length - thumb) * (max_offset > 0.0f ? (axis == uim_LAYOUT_ROW ? s->offset_x : s->offset_y) / max_offset : 0.0f);
    if (thumb_offset < 0.0f) thumb_offset = 0.0f;
    if (thumb_offset > track_length - thumb) thumb_offset = track_length - thumb;
    out->visible = true;
    out->thumb_length = thumb;
    out->thumb_offset = thumb_offset;
    return uim_OK;
}

/* Drag the thumb by delta pixels along the track (positive = towards the end). */
static inline uim_Result uim_scrollbar_drag(uim_ScrollState *s, uim_LayoutAxis axis,
                                            float track_length, float delta_px) {
    uim_ScrollbarMetrics m;
    float ratio, delta_offset, x, y;
    uim_Result rc;
    if (!s || !isfinite(delta_px)) return uim_ERR_ARGUMENT;
    rc = uim_scrollbar_metrics(s, axis, track_length, &m);
    if (rc != uim_OK) return rc;
    if (!m.visible) return uim_OK;
    ratio = (track_length - m.thumb_length) > 0.0f
                ? m.max_offset / (track_length - m.thumb_length) : 0.0f;
    delta_offset = delta_px * ratio;
    x = s->offset_x;
    y = s->offset_y;
    if (axis == uim_LAYOUT_ROW) x += delta_offset;
    else y += delta_offset;
    return uim_scroll_set_offset(s, x, y);
}

/* Scroll just enough to bring an item (content coordinates) fully into view. */
static inline uim_Result uim_scroll_reveal(uim_ScrollState *s, uim_Rect item, float margin) {
    float x, y;
    if (!s || !isfinite(item.x) || !isfinite(item.y) || !isfinite(item.width) ||
        !isfinite(item.height) || !isfinite(margin) || margin < 0.0f ||
        item.width < 0.0f || item.height < 0.0f)
        return uim_ERR_ARGUMENT;
    x = s->offset_x;
    y = s->offset_y;
    if (s->axis_x) {
        if (item.x - margin < x) x = item.x - margin;
        else if (item.x + item.width + margin > x + s->view_w)
            x = item.x + item.width + margin - s->view_w;
        if (x < 0.0f) x = 0.0f;
    }
    if (s->axis_y) {
        if (item.y - margin < y) y = item.y - margin;
        else if (item.y + item.height + margin > y + s->view_h)
            y = item.y + item.height + margin - s->view_h;
        if (y < 0.0f) y = 0.0f;
    }
    return uim_scroll_set_offset(s, x, y);
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_SCROLL_H */
