#ifndef UIM_ANIM_H
#define UIM_ANIM_H
/* uim_anim.h — the animation atom: easing curves and one tween.
 *
 * The editor animates panel collapse and hover states with hand-written
 * interpolations; the runtime has its own fade math. This header fixes the
 * curve set and a single tween state machine (start / tick / loop / ping-pong /
 * stop) so timing feels the same everywhere.
 *
 * Contract:
 * - Time is seconds, supplied by the caller; no clock is read here.
 * - uim_ease clamps t to [0,1] (NaN -> 0) and never returns NaN; unknown
 *   easing ids fall back to linear.
 * - A non-looping tween deactivates at the end and pins value to `to`;
 *   a looping tween never deactivates. duration == 0 completes immediately.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uim_Easing {
    uim_EASE_LINEAR = 0,
    uim_EASE_QUAD_IN,
    uim_EASE_QUAD_OUT,
    uim_EASE_QUAD_IN_OUT,
    uim_EASE_CUBIC_IN,
    uim_EASE_CUBIC_OUT,
    uim_EASE_CUBIC_IN_OUT,
    uim_EASE_BACK_OUT,
    uim_EASE_COUNT
} uim_Easing;

static inline float uim_ease(uim_Easing easing, float t) {
    if (!isfinite(t)) t = 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    switch (easing) {
    case uim_EASE_QUAD_IN: return t * t;
    case uim_EASE_QUAD_OUT: { float u = 1.0f - t; return 1.0f - u * u; }
    case uim_EASE_QUAD_IN_OUT:
        return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    case uim_EASE_CUBIC_IN: return t * t * t;
    case uim_EASE_CUBIC_OUT: { float u = 1.0f - t; return 1.0f - u * u * u; }
    case uim_EASE_CUBIC_IN_OUT: {
        float u = 1.0f - t;
        return t < 0.5f ? 4.0f * t * t * t : 1.0f - 4.0f * u * u * u;
    }
    case uim_EASE_BACK_OUT: {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        float u = t - 1.0f;
        return 1.0f + c3 * u * u * u + c1 * u * u;
    }
    case uim_EASE_LINEAR:
    default:
        return t;
    }
}

typedef struct uim_Tween {
    float from, to;
    float duration;   /* seconds, >= 0 */
    float elapsed;
    float value;
    uim_Easing easing;
    bool loop;
    bool ping_pong;
    bool active;
} uim_Tween;

static inline uim_Result uim_tween_start(uim_Tween *tw, float from, float to,
                                         float duration, uim_Easing easing) {
    if (!tw || !isfinite(from) || !isfinite(to) || !isfinite(duration) || duration < 0.0f)
        return uim_ERR_ARGUMENT;
    if ((unsigned)easing >= (unsigned)uim_EASE_COUNT) return uim_ERR_ARGUMENT;
    tw->from = from;
    tw->to = to;
    tw->duration = duration;
    tw->elapsed = 0.0f;
    tw->value = from;
    tw->easing = easing;
    tw->active = duration > 0.0f;
    tw->loop = false;
    tw->ping_pong = false;
    if (duration <= 0.0f) tw->value = to;
    return uim_OK;
}

static inline uim_Result uim_tween_set_loop(uim_Tween *tw, bool loop, bool ping_pong) {
    if (!tw) return uim_ERR_ARGUMENT;
    tw->loop = loop;
    tw->ping_pong = loop && ping_pong;
    return uim_OK;
}

static inline uim_Result uim_tween_tick(uim_Tween *tw, float dt) {
    float t;
    if (!tw || !isfinite(dt) || dt < 0.0f) return uim_ERR_ARGUMENT;
    if (!tw->active) return uim_OK;
    if (tw->duration <= 0.0f) { tw->value = tw->to; tw->active = false; return uim_OK; }
    tw->elapsed += dt;
    if (tw->loop) {
        float cycle = tw->ping_pong ? tw->duration * 2.0f : tw->duration;
        tw->elapsed = fmodf(tw->elapsed, cycle);
        if (tw->elapsed < 0.0f) tw->elapsed += cycle;
        if (tw->ping_pong && tw->elapsed > tw->duration)
            t = 2.0f - tw->elapsed / tw->duration;
        else
            t = tw->elapsed / tw->duration;
    } else {
        if (tw->elapsed >= tw->duration) {
            tw->elapsed = tw->duration;
            tw->value = tw->to;
            tw->active = false;
            return uim_OK;
        }
        t = tw->elapsed / tw->duration;
    }
    tw->value = tw->from + (tw->to - tw->from) * uim_ease(tw->easing, t);
    return uim_OK;
}

static inline uim_Result uim_tween_stop(uim_Tween *tw, bool snap_to_end) {
    if (!tw) return uim_ERR_ARGUMENT;
    if (snap_to_end) tw->value = tw->to;
    tw->active = false;
    return uim_OK;
}

static inline bool uim_tween_done(const uim_Tween *tw) {
    return tw ? !tw->active : true;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_ANIM_H */
