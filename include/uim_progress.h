#ifndef UIM_PROGRESS_H
#define UIM_PROGRESS_H
/* uim_progress.h — the progress atom: value, fraction, indeterminate phase.
 *
 * Neither UIM nor the engine has a progress primitive; each panel that shows
 * load state keeps its own clamped float and its own marquee phase. This header
 * is the state half (drawing stays in the host): range, clamped value, fraction
 * and the moving window of an indeterminate bar.
 *
 * Contract:
 * - value is always inside [min, max]; a write that clamps returns
 *   uim_ERR_RANGE (the clamped value is applied).
 * - fraction is 0 when the range is degenerate (min == max); such a range is
 *   rejected at init/set_range with uim_ERR_ARGUMENT.
 * - phase advances only while indeterminate; cycle_seconds must be > 0.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Progress {
    float min, max, value;
    bool indeterminate;
    float phase;          /* 0..1, marquee position */
    float cycle_seconds;
} uim_Progress;

static inline uim_Result uim_progress_init(uim_Progress *p, float min_value, float max_value) {
    if (!p || !isfinite(min_value) || !isfinite(max_value) || min_value >= max_value)
        return uim_ERR_ARGUMENT;
    p->min = min_value;
    p->max = max_value;
    p->value = min_value;
    p->indeterminate = false;
    p->phase = 0.0f;
    p->cycle_seconds = 1.4f;
    return uim_OK;
}

static inline uim_Result uim_progress_set_range(uim_Progress *p, float min_value, float max_value) {
    if (!p || !isfinite(min_value) || !isfinite(max_value) || min_value >= max_value)
        return uim_ERR_ARGUMENT;
    p->min = min_value;
    p->max = max_value;
    if (p->value < min_value) p->value = min_value;
    if (p->value > max_value) p->value = max_value;
    return uim_OK;
}

static inline float uim_progress_fraction(const uim_Progress *p) {
    float f;
    if (!p || p->max <= p->min) return 0.0f;
    f = (p->value - p->min) / (p->max - p->min);
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return f;
}

static inline uim_Result uim_progress_set_value(uim_Progress *p, float value) {
    bool clamped = false;
    if (!p || !isfinite(value)) return uim_ERR_ARGUMENT;
    if (value < p->min) { value = p->min; clamped = true; }
    if (value > p->max) { value = p->max; clamped = true; }
    p->value = value;
    return clamped ? uim_ERR_RANGE : uim_OK;
}

static inline uim_Result uim_progress_set_indeterminate(uim_Progress *p, bool on) {
    if (!p) return uim_ERR_ARGUMENT;
    p->indeterminate = on;
    if (on) p->phase = 0.0f;
    return uim_OK;
}

static inline uim_Result uim_progress_set_cycle(uim_Progress *p, float seconds) {
    if (!p || !isfinite(seconds) || seconds <= 0.0f) return uim_ERR_ARGUMENT;
    p->cycle_seconds = seconds;
    return uim_OK;
}

static inline uim_Result uim_progress_advance(uim_Progress *p, float dt) {
    if (!p || !isfinite(dt) || dt < 0.0f) return uim_ERR_ARGUMENT;
    if (!p->indeterminate || p->cycle_seconds <= 0.0f) return uim_OK;
    p->phase += dt / p->cycle_seconds;
    p->phase = fmodf(p->phase, 1.0f);
    if (p->phase < 0.0f) p->phase += 1.0f;
    return uim_OK;
}

/* Start of the moving highlight window (width = span, 0..1 of the track).
 * At phase 0 it starts fully off the left edge and ends at phase 1 off the
 * right edge, matching the usual indeterminate bar. */
static inline uim_Result uim_progress_highlight(const uim_Progress *p, float span,
                                                float *out_start) {
    if (!p || !out_start || !isfinite(span) || span < 0.0f || span > 1.0f)
        return uim_ERR_ARGUMENT;
    *out_start = p->phase * (1.0f + span) - span;
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_PROGRESS_H */
