#ifndef UIM_TOAST_H
#define UIM_TOAST_H
/* uim_toast.h — the toast atom: a bounded, deduplicated notification queue.
 *
 * Every transient message in the workspace currently owns its own timer and
 * its own "don't stack more than N" rule. This header is the one queue:
 * bounded capacity, refresh on duplicate id, priority-based eviction, and
 * caller-driven ticking.
 *
 * Contract:
 * - Fixed capacity (UIM_TOAST_CAPACITY); pushing into a full queue evicts the
 *   lowest-priority entry (oldest wins ties), then appends.
 * - Duration is seconds, > 0; priority is caller-defined (higher = keep longer).
 * - A repeated id refreshes duration/priority in place and keeps its position.
 * - uim_toast_tick removes entries whose remaining time reached 0; the caller
 *   owns the clock.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UIM_TOAST_CAPACITY 8u

typedef struct uim_ToastEntry {
    uint32_t id;
    float remaining;
    float duration;
    int priority;
} uim_ToastEntry;

typedef struct uim_ToastQueue {
    uim_ToastEntry entries[UIM_TOAST_CAPACITY];
    size_t count;
} uim_ToastQueue;

static inline uim_Result uim_toast_init(uim_ToastQueue *q) {
    if (!q) return uim_ERR_ARGUMENT;
    q->count = 0;
    return uim_OK;
}

static inline uim_Result uim_toast_push(uim_ToastQueue *q, uint32_t id, float duration,
                                        int priority) {
    size_t i, victim = 0;
    if (!q || !isfinite(duration) || duration <= 0.0f) return uim_ERR_ARGUMENT;
    for (i = 0; i < q->count; i++) {
        if (q->entries[i].id == id) {
            q->entries[i].remaining = duration;
            q->entries[i].duration = duration;
            q->entries[i].priority = priority;
            return uim_OK;
        }
    }
    if (q->count < UIM_TOAST_CAPACITY) {
        q->entries[q->count].id = id;
        q->entries[q->count].remaining = duration;
        q->entries[q->count].duration = duration;
        q->entries[q->count].priority = priority;
        q->count++;
        return uim_OK;
    }
    for (i = 1; i < q->count; i++) {
        if (q->entries[i].priority < q->entries[victim].priority) victim = i;
    }
    for (i = victim + 1u; i < q->count; i++) q->entries[i - 1u] = q->entries[i];
    q->count--;
    q->entries[q->count].id = id;
    q->entries[q->count].remaining = duration;
    q->entries[q->count].duration = duration;
    q->entries[q->count].priority = priority;
    q->count++;
    return uim_OK;
}

static inline uim_Result uim_toast_tick(uim_ToastQueue *q, float dt) {
    size_t i = 0;
    if (!q || !isfinite(dt) || dt < 0.0f) return uim_ERR_ARGUMENT;
    while (i < q->count) {
        if (q->entries[i].remaining <= dt) {
            size_t j;
            for (j = i + 1u; j < q->count; j++) q->entries[j - 1u] = q->entries[j];
            q->count--;
        } else {
            q->entries[i].remaining -= dt;
            i++;
        }
    }
    return uim_OK;
}

static inline uim_Result uim_toast_dismiss(uim_ToastQueue *q, uint32_t id) {
    size_t i;
    if (!q) return uim_ERR_ARGUMENT;
    for (i = 0; i < q->count; i++) {
        if (q->entries[i].id == id) {
            size_t j;
            for (j = i + 1u; j < q->count; j++) q->entries[j - 1u] = q->entries[j];
            q->count--;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

static inline uim_Result uim_toast_find(const uim_ToastQueue *q, uint32_t id, size_t *out_index) {
    size_t i;
    if (!q || !out_index) return uim_ERR_ARGUMENT;
    for (i = 0; i < q->count; i++) {
        if (q->entries[i].id == id) { *out_index = i; return uim_OK; }
    }
    return uim_ERR_NOT_FOUND;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_TOAST_H */
