#ifndef UIM_SELECTION_H
#define UIM_SELECTION_H
/* UIM selection: the smallest multi-selection state for UI documents.
 *
 * Contract (v1):
 * - Stores node ids only. It never copies node data and never touches a
 *   manager, renderer or window, so editor, runtime and tools can share it.
 * - One instance may live on the stack (uim_selection_init/reset, no
 *   allocation) or on the heap (uim_selection_create/destroy). The heap
 *   functions exist for legacy callers; new code should prefer the stack form.
 * - The last added id is the primary selection (uim_selection_get). Capacity is
 *   uim_SELECTION_MAX_NODES; adding past capacity evicts the oldest id.
 * - Passing NULL to any function is a safe no-op / INVALID / false / 0.
 */
#include "uim_component_manager.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define uim_SELECTION_MAX_NODES 64

typedef struct uim_Selection {
    uim_NodeId ids[uim_SELECTION_MAX_NODES];
    int count;
} uim_Selection;

/* Stack form: zeroes the selection. Returns uim_ERR_ARGUMENT on NULL. */
uim_Result uim_selection_init(uim_Selection *sel);
void uim_selection_reset(uim_Selection *sel);

/* Heap form (legacy compatibility). */
uim_Selection *uim_selection_create(void);
void uim_selection_destroy(uim_Selection *sel);

/* Primary selection: the last added id, uim_NODE_INVALID when empty. */
uim_NodeId uim_selection_get(const uim_Selection *sel);

void uim_selection_set(uim_Selection *sel, uim_NodeId node_id);
void uim_selection_add(uim_Selection *sel, uim_NodeId node_id);
void uim_selection_remove(uim_Selection *sel, uim_NodeId node_id);
void uim_selection_toggle(uim_Selection *sel, uim_NodeId node_id);

bool uim_selection_contains(const uim_Selection *sel, uim_NodeId node_id);
int uim_selection_count(const uim_Selection *sel);
uim_NodeId uim_selection_get_at(const uim_Selection *sel, int index);
void uim_selection_clear(uim_Selection *sel);
bool uim_selection_has_selection(const uim_Selection *sel);

#ifdef __cplusplus
}
#endif
#endif
