#ifndef UIM_TOOLTIP_H
#define UIM_TOOLTIP_H
/* uim_tooltip.h — the tooltip atom: hover delay, show/hide and re-trigger.
 *
 * What it owns:
 * - which trigger is hovered, how long it has been hovered, and when the
 *   tooltip becomes visible after the configured delay.
 * - re-trigger: moving from one trigger to another restarts the timer;
 *   leaving the widget hides the tooltip immediately.
 *
 * What it does not own: tooltip text, the popup rect or flip/clamp geometry
 * (uim_placement owns that), drawing, or hover hit testing (the host reports
 * hovered + id). A scroll, click or popup opening calls dismiss().
 *
 * Contract: pure timing state. No allocation, no globals, no clock source —
 * the host drives tick(dt). delay must be > 0; dt must be finite and >= 0.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Tooltip {
    uint32_t trigger_id;
    float delay;
    float hover_time;
    bool hovered;
    bool visible;
} uim_Tooltip;

static inline uim_Result uim_tooltip_init(uim_Tooltip *tip, float delay_seconds) {
    if (!tip || !isfinite(delay_seconds) || delay_seconds < 0.0f) return uim_ERR_ARGUMENT;
    tip->trigger_id = 0u;
    tip->delay = delay_seconds;
    tip->hover_time = 0.0f;
    tip->hovered = false;
    tip->visible = false;
    return uim_OK;
}

/* Report hover state for one trigger. A new trigger id, or leaving and
 * re-entering, resets the dwell timer. */
static inline uim_Result uim_tooltip_set_hover(uim_Tooltip *tip, uint32_t trigger_id, bool hovered) {
    if (!tip) return uim_ERR_ARGUMENT;
    if (!hovered) {
        tip->hovered = false;
        tip->visible = false;
        tip->hover_time = 0.0f;
        return uim_OK;
    }
    if (!tip->hovered || tip->trigger_id != trigger_id) {
        tip->trigger_id = trigger_id;
        tip->hover_time = 0.0f;
        tip->visible = false;
    }
    tip->hovered = true;
    return uim_OK;
}

/* Advance dwell time. Becomes visible once the delay has elapsed; stays
 * visible while hovered. */
static inline uim_Result uim_tooltip_tick(uim_Tooltip *tip, float dt) {
    if (!tip || !isfinite(dt) || dt < 0.0f) return uim_ERR_ARGUMENT;
    if (!tip->hovered) return uim_OK;
    tip->hover_time += dt;
    if (tip->delay <= 0.0f || tip->hover_time >= tip->delay) tip->visible = true;
    return uim_OK;
}

/* Hide immediately (click, scroll, another popup opened, Escape). */
static inline void uim_tooltip_dismiss(uim_Tooltip *tip) {
    if (!tip) return;
    tip->hovered = false;
    tip->visible = false;
    tip->hover_time = 0.0f;
}

static inline bool uim_tooltip_is_visible(const uim_Tooltip *tip) {
    return tip && tip->visible;
}

static inline uint32_t uim_tooltip_trigger(const uim_Tooltip *tip) {
    return tip ? tip->trigger_id : 0u;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_TOOLTIP_H */
