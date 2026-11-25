#ifndef UIM_SLIDER_H
#define UIM_SLIDER_H
/* uim_slider.h — the slider atom: clamped value, track mapping and drag.
 *
 * Coordinates are caller-space pixels. The atom does not know the theme, the
 * track rect or the thumb size; it only maps a position on a one-dimensional
 * track to a value and owns the dragging flag.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Slider {
    float min_value;
    float max_value;
    float value;
    float step;
    bool dragging;
} uim_Slider;

static inline uim_Result uim_slider_init(uim_Slider *s, float min_value,
                                         float max_value, float value,
                                         float step) {
    if (!s || !isfinite(min_value) || !isfinite(max_value)
        || !isfinite(value) || !isfinite(step) || step < 0.0f
        || min_value > max_value)
        return uim_ERR_ARGUMENT;
    s->min_value = min_value;
    s->max_value = max_value;
    s->step = step;
    s->dragging = false;
    if (value < min_value) s->value = min_value;
    else if (value > max_value) s->value = max_value;
    else s->value = value;
    return value < min_value || value > max_value ? uim_ERR_RANGE : uim_OK;
}

static inline uim_Result uim_slider_set_value(uim_Slider *s, float value) {
    if (!s || !isfinite(value)) return uim_ERR_ARGUMENT;
    if (value < s->min_value) { s->value = s->min_value; return uim_ERR_RANGE; }
    if (value > s->max_value) { s->value = s->max_value; return uim_ERR_RANGE; }
    s->value = value;
    return uim_OK;
}

static inline float uim_slider_fraction(const uim_Slider *s) {
    float span;
    if (!s) return 0.0f;
    span = s->max_value - s->min_value;
    if (span <= 0.0f) return 0.0f;
    return (s->value - s->min_value) / span;
}

/* Map an x position on [track_x, track_x + track_width] to a clamped value.
 * A zero/negative width is an argument error. */
static inline uim_Result uim_slider_value_at(const uim_Slider *s, float x,
                                             float track_x, float track_width,
                                             float *out_value) {
    float f;
    if (!s || !out_value || !isfinite(x) || !isfinite(track_x)
        || !isfinite(track_width) || track_width <= 0.0f)
        return uim_ERR_ARGUMENT;
    f = (x - track_x) / track_width;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    *out_value = s->min_value + f * (s->max_value - s->min_value);
    return uim_OK;
}

static inline uim_Result uim_slider_x_for(const uim_Slider *s, float track_x,
                                          float track_width, float *out_x) {
    if (!s || !out_x || !isfinite(track_x) || !isfinite(track_width)
        || track_width <= 0.0f)
        return uim_ERR_ARGUMENT;
    *out_x = track_x + uim_slider_fraction(s) * track_width;
    return uim_OK;
}

static inline uim_Result uim_slider_begin_drag(uim_Slider *s, float x,
                                               float track_x, float track_width) {
    float value;
    uim_Result r;
    if (!s) return uim_ERR_ARGUMENT;
    r = uim_slider_value_at(s, x, track_x, track_width, &value);
    if (r != uim_OK) return r;
    s->dragging = true;
    s->value = value;
    return uim_OK;
}

static inline uim_Result uim_slider_drag_to(uim_Slider *s, float x,
                                            float track_x, float track_width) {
    float value;
    uim_Result r;
    if (!s) return uim_ERR_ARGUMENT;
    if (!s->dragging) return uim_ERR_STATE;
    r = uim_slider_value_at(s, x, track_x, track_width, &value);
    if (r != uim_OK) return r;
    s->value = value;
    return uim_OK;
}

static inline uim_Result uim_slider_end_drag(uim_Slider *s) {
    if (!s) return uim_ERR_ARGUMENT;
    s->dragging = false;
    return uim_OK;
}

/* Keyboard or stepper nudge. A positive step larger than the range is clamped. */
static inline uim_Result uim_slider_step(uim_Slider *s, int steps) {
    double target;
    if (!s) return uim_ERR_ARGUMENT;
    if (s->step <= 0.0f) return uim_slider_set_value(s, s->value);
    target = (double)s->value + (double)steps * (double)s->step;
    if (target < (double)s->min_value) { s->value = s->min_value; return uim_ERR_RANGE; }
    if (target > (double)s->max_value) { s->value = s->max_value; return uim_ERR_RANGE; }
    s->value = (float)target;
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_SLIDER_H */
