#include "uim_selection.h"

#include <stdlib.h>
#include <string.h>

uim_Result uim_selection_init(uim_Selection *sel) {
    if (!sel) return uim_ERR_ARGUMENT;
    memset(sel, 0, sizeof(*sel));
    return uim_OK;
}

void uim_selection_reset(uim_Selection *sel) {
    if (sel) memset(sel, 0, sizeof(*sel));
}

uim_Selection *uim_selection_create(void) {
    return (uim_Selection *)calloc(1u, sizeof(uim_Selection));
}

void uim_selection_destroy(uim_Selection *sel) {
    free(sel);
}

static int selection_index(const uim_Selection *sel, uim_NodeId id) {
    int i;
    if (!sel || id == uim_NODE_INVALID) return -1;
    for (i = 0; i < sel->count; ++i) {
        if (sel->ids[i] == id) return i;
    }
    return -1;
}

static void selection_remove_at(uim_Selection *sel, int index) {
    if (!sel || index < 0 || index >= sel->count) return;
    if (index + 1 < sel->count) {
        memmove(&sel->ids[index], &sel->ids[index + 1],
                (size_t)(sel->count - index - 1) * sizeof(sel->ids[0]));
    }
    sel->count -= 1;
}

uim_NodeId uim_selection_get(const uim_Selection *sel) {
    if (!sel || sel->count <= 0) return uim_NODE_INVALID;
    return sel->ids[sel->count - 1];
}

void uim_selection_set(uim_Selection *sel, uim_NodeId node_id) {
    if (!sel) return;
    sel->count = 0;
    if (node_id != uim_NODE_INVALID) sel->ids[sel->count++] = node_id;
}

void uim_selection_add(uim_Selection *sel, uim_NodeId node_id) {
    int existing;
    if (!sel || node_id == uim_NODE_INVALID) return;
    existing = selection_index(sel, node_id);
    if (existing >= 0) selection_remove_at(sel, existing);
    if (sel->count >= uim_SELECTION_MAX_NODES) selection_remove_at(sel, 0);
    sel->ids[sel->count++] = node_id;
}

void uim_selection_remove(uim_Selection *sel, uim_NodeId node_id) {
    selection_remove_at(sel, selection_index(sel, node_id));
}

void uim_selection_toggle(uim_Selection *sel, uim_NodeId node_id) {
    if (!sel || node_id == uim_NODE_INVALID) return;
    if (selection_index(sel, node_id) >= 0) {
        uim_selection_remove(sel, node_id);
    } else {
        uim_selection_add(sel, node_id);
    }
}

bool uim_selection_contains(const uim_Selection *sel, uim_NodeId node_id) {
    return selection_index(sel, node_id) >= 0;
}

int uim_selection_count(const uim_Selection *sel) {
    return sel ? sel->count : 0;
}

uim_NodeId uim_selection_get_at(const uim_Selection *sel, int index) {
    if (!sel || index < 0 || index >= sel->count) return uim_NODE_INVALID;
    return sel->ids[index];
}

void uim_selection_clear(uim_Selection *sel) {
    if (sel) sel->count = 0;
}

bool uim_selection_has_selection(const uim_Selection *sel) {
    return sel && sel->count > 0;
}
