#ifndef UIM_TREE_H
#define UIM_TREE_H
/* uim_tree.h — the tree atom: expand/collapse set and primary selection for a
 * caller-owned node hierarchy.
 *
 * What it owns:
 * - which caller ids are expanded, and the single primary selection.
 * - expand/collapse/toggle, clear, and hit testing over caller-owned row rects.
 *
 * What it does not own: node data, parent/child traversal, file IO, row text
 * or indentation drawing. The host enumerates visible rows (depth first,
 * skipping collapsed subtrees) and supplies the row ids + rects each frame;
 * this header keeps only the open/selected bookkeeping every tree repeats.
 *
 * Contract: fixed capacity (UIM_TREE_CAPACITY), no allocation, no globals.
 * Adding past capacity is uim_ERR_BUFFER and the set is left unchanged.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UIM_TREE_CAPACITY 64u

typedef struct uim_Tree {
    uint32_t expanded[UIM_TREE_CAPACITY];
    size_t expanded_count;
    uint32_t selected;
    bool has_selection;
} uim_Tree;

static inline uim_Result uim_tree_init(uim_Tree *t) {
    if (!t) return uim_ERR_ARGUMENT;
    t->expanded_count = 0;
    t->selected = 0u;
    t->has_selection = false;
    return uim_OK;
}

static inline bool uim_tree_is_expanded(const uim_Tree *t, uint32_t id) {
    size_t i;
    if (!t || id == 0u) return false;
    for (i = 0; i < t->expanded_count; i++) {
        if (t->expanded[i] == id) return true;
    }
    return false;
}

static inline uim_Result uim_tree_expand(uim_Tree *t, uint32_t id) {
    size_t i;
    if (!t || id == 0u) return uim_ERR_ARGUMENT;
    for (i = 0; i < t->expanded_count; i++) {
        if (t->expanded[i] == id) return uim_ERR_EXISTS;
    }
    if (t->expanded_count >= UIM_TREE_CAPACITY) return uim_ERR_BUFFER;
    t->expanded[t->expanded_count++] = id;
    return uim_OK;
}

static inline uim_Result uim_tree_collapse(uim_Tree *t, uint32_t id) {
    size_t i, j;
    if (!t || id == 0u) return uim_ERR_ARGUMENT;
    for (i = 0; i < t->expanded_count; i++) {
        if (t->expanded[i] == id) {
            for (j = i + 1u; j < t->expanded_count; j++)
                t->expanded[j - 1u] = t->expanded[j];
            t->expanded_count--;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

/* Toggle expansion. out_expanded receives the new state. */
static inline uim_Result uim_tree_toggle(uim_Tree *t, uint32_t id, bool *out_expanded) {
    uim_Result rc;
    if (!t || !out_expanded) return uim_ERR_ARGUMENT;
    if (uim_tree_is_expanded(t, id)) {
        rc = uim_tree_collapse(t, id);
        *out_expanded = false;
    } else {
        rc = uim_tree_expand(t, id);
        *out_expanded = true;
    }
    return rc;
}

static inline void uim_tree_collapse_all(uim_Tree *t) {
    if (!t) return;
    t->expanded_count = 0;
}

static inline uim_Result uim_tree_select(uim_Tree *t, uint32_t id) {
    if (!t) return uim_ERR_ARGUMENT;
    if (id == 0u) {
        t->selected = 0u;
        t->has_selection = false;
        return uim_OK;
    }
    t->selected = id;
    t->has_selection = true;
    return uim_OK;
}

static inline uim_Result uim_tree_get_selected(const uim_Tree *t, uint32_t *out_id) {
    if (!t || !out_id) return uim_ERR_ARGUMENT;
    if (!t->has_selection) return uim_ERR_NOT_FOUND;
    *out_id = t->selected;
    return uim_OK;
}

static inline bool uim_tree_has_selection(const uim_Tree *t) {
    return t && t->has_selection;
}

/* Hit test over row ids + rects in visible-row order. Outside every rect is
 * uim_ERR_NOT_FOUND. */
static inline uim_Result uim_tree_hit(const uim_Tree *t, const uint32_t *ids,
                                     const uim_Rect *rects, size_t count,
                                     float x, float y, uint32_t *out_id) {
    size_t i;
    if (!t || !ids || !rects || !out_id || !isfinite(x) || !isfinite(y))
        return uim_ERR_ARGUMENT;
    for (i = 0; i < count; i++) {
        const uim_Rect *r = &rects[i];
        if (!isfinite(r->x) || !isfinite(r->y) || !isfinite(r->width) || !isfinite(r->height))
            return uim_ERR_ARGUMENT;
        if (x >= r->x && x <= r->x + r->width && y >= r->y && y <= r->y + r->height) {
            *out_id = ids[i];
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_TREE_H */
