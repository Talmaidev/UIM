#ifndef UIM_STEPPER_H
#define UIM_STEPPER_H
/* uim_stepper.h — the numeric stepper atom: clamped double, step and wrap.
 *
 * It is deliberately format-free. A host that needs display text uses
 * uim_format_fixed/int on the value. Keeping formatting out of this atom lets
 * integer row/column fields and float time fields share one state machine.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Stepper {
    double min_value;
    double max_value;
    double value;
    double step;
    bool wrap;
} uim_Stepper;

static inline uim_Result uim_stepper_init(uim_Stepper *s, double min_value,
                                          double max_value, double value,
                                          double step) {
    if (!s || !isfinite(min_value) || !isfinite(max_value)
        || !isfinite(value) || !isfinite(step) || step <= 0.0
        || min_value > max_value)
        return uim_ERR_ARGUMENT;
    s->min_value = min_value;
    s->max_value = max_value;
    s->step = step;
    s->wrap = false;
    if (value < min_value) s->value = min_value;
    else if (value > max_value) s->value = max_value;
    else s->value = value;
    return value < min_value || value > max_value ? uim_ERR_RANGE : uim_OK;
}

static inline uim_Result uim_stepper_set_value(uim_Stepper *s, double value) {
    if (!s || !isfinite(value)) return uim_ERR_ARGUMENT;
    if (value < s->min_value) { s->value = s->min_value; return uim_ERR_RANGE; }
    if (value > s->max_value) { s->value = s->max_value; return uim_ERR_RANGE; }
    s->value = value;
    return uim_OK;
}

static inline uim_Result uim_stepper_set_wrap(uim_Stepper *s, bool wrap) {
    if (!s) return uim_ERR_ARGUMENT;
    s->wrap = wrap;
    return uim_OK;
}

/* Step by delta units. With wrap, crossing an edge wraps by one range; with
 * clamp, the edge value is applied and uim_ERR_RANGE is returned. */
static inline uim_Result uim_stepper_step(uim_Stepper *s, int delta) {
    double target, span;
    if (!s) return uim_ERR_ARGUMENT;
    if (delta == 0) return uim_OK;
    target = s->value + (double)delta * s->step;
    span = s->max_value - s->min_value;
    if (span <= 0.0) { s->value = s->min_value; return uim_ERR_RANGE; }
    if (s->wrap) {
        target = s->min_value + fmod(target - s->min_value, span);
        if (target < s->min_value) target += span;
        s->value = target;
        return uim_OK;
    }
    if (target < s->min_value) { s->value = s->min_value; return uim_ERR_RANGE; }
    if (target > s->max_value) { s->value = s->max_value; return uim_ERR_RANGE; }
    s->value = target;
    return uim_OK;
}

static inline float uim_stepper_fraction(const uim_Stepper *s) {
    double span;
    if (!s) return 0.0f;
    span = s->max_value - s->min_value;
    if (span <= 0.0) return 0.0f;
    return (float)((s->value - s->min_value) / span);
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_STEPPER_H */
