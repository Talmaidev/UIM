/* Contract test for UIM selection: one owner for node-id selection state,
 * shared by editor, runtime and tools, with no allocation in the stack form. */
#include "uim_selection.h"

#include <stdio.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    uim_Selection selection;
    uim_Selection *heap;
    int i;

    CHECK(uim_selection_init(&selection) == uim_OK);
    CHECK(uim_selection_init(NULL) == uim_ERR_ARGUMENT);
    CHECK(uim_selection_count(&selection) == 0);
    CHECK(uim_selection_get(&selection) == uim_NODE_INVALID);
    CHECK(!uim_selection_has_selection(&selection));

    uim_selection_set(&selection, 7u);
    CHECK(uim_selection_count(&selection) == 1);
    CHECK(uim_selection_get(&selection) == 7u);
    CHECK(uim_selection_contains(&selection, 7u));

    /* set replaces, never accumulates. */
    uim_selection_set(&selection, 9u);
    CHECK(uim_selection_count(&selection) == 1);
    CHECK(!uim_selection_contains(&selection, 7u));
    CHECK(uim_selection_get(&selection) == 9u);

    /* add: new id becomes primary; duplicate re-add moves to the end. */
    uim_selection_add(&selection, 11u);
    CHECK(uim_selection_count(&selection) == 2);
    CHECK(uim_selection_get(&selection) == 11u);
    uim_selection_add(&selection, 9u);
    CHECK(uim_selection_count(&selection) == 2);
    CHECK(uim_selection_get(&selection) == 9u);
    CHECK(uim_selection_get_at(&selection, 0) == 11u);
    CHECK(uim_selection_get_at(&selection, 1) == 9u);

    /* remove the primary: primary falls back to the last remaining id. */
    uim_selection_remove(&selection, 9u);
    CHECK(uim_selection_count(&selection) == 1);
    CHECK(uim_selection_get(&selection) == 11u);

    /* toggle removes then re-adds. */
    uim_selection_toggle(&selection, 11u);
    CHECK(!uim_selection_has_selection(&selection));
    uim_selection_toggle(&selection, 11u);
    CHECK(uim_selection_get(&selection) == 11u);

    /* invalid ids are ignored. */
    uim_selection_add(&selection, uim_NODE_INVALID);
    CHECK(uim_selection_count(&selection) == 1);
    uim_selection_set(&selection, uim_NODE_INVALID);
    CHECK(uim_selection_count(&selection) == 0);

    /* out-of-range get_at. */
    CHECK(uim_selection_get_at(&selection, -1) == uim_NODE_INVALID);
    CHECK(uim_selection_get_at(&selection, 0) == uim_NODE_INVALID);
    CHECK(uim_selection_get_at(NULL, 0) == uim_NODE_INVALID);

    /* capacity: adding past the cap evicts the oldest id, primary stays newest. */
    for (i = 0; i < uim_SELECTION_MAX_NODES; ++i) {
        uim_selection_add(&selection, (uim_NodeId)(100 + i));
    }
    CHECK(uim_selection_count(&selection) == uim_SELECTION_MAX_NODES);
    CHECK(uim_selection_get(&selection) == (uim_NodeId)(100 + uim_SELECTION_MAX_NODES - 1));
    uim_selection_add(&selection, 999u);
    CHECK(uim_selection_count(&selection) == uim_SELECTION_MAX_NODES);
    CHECK(uim_selection_get(&selection) == 999u);
    CHECK(!uim_selection_contains(&selection, 100u));

    /* clear/reset. */
    uim_selection_clear(&selection);
    CHECK(uim_selection_count(&selection) == 0);
    uim_selection_add(&selection, 5u);
    uim_selection_reset(&selection);
    CHECK(uim_selection_count(&selection) == 0);

    /* heap form stays compatible with legacy callers. */
    heap = uim_selection_create();
    CHECK(heap != NULL);
    uim_selection_add(heap, 3u);
    uim_selection_add(heap, 4u);
    CHECK(uim_selection_count(heap) == 2);
    CHECK(uim_selection_get(heap) == 4u);
    uim_selection_destroy(heap);

    /* NULL safety for every accessor. */
    CHECK(uim_selection_count(NULL) == 0);
    CHECK(!uim_selection_contains(NULL, 1u));
    CHECK(!uim_selection_has_selection(NULL));
    uim_selection_clear(NULL);
    uim_selection_set(NULL, 1u);
    uim_selection_add(NULL, 1u);
    uim_selection_remove(NULL, 1u);
    uim_selection_toggle(NULL, 1u);

    if (failures == 0) printf("UIM selection contract: ALL PASS\n");
    else printf("%d failures\n", failures);
    return failures;
}
