/* UIM-ZCSR Runtime Bridge: translates UIM instance data to ZCSR Runtime nodes.
 * No rendering, no GL. Pure mapping and event-flow verification. */
#ifndef UIM_ZCSR_RUNTIME_BRIDGE_H
#define UIM_ZCSR_RUNTIME_BRIDGE_H
#include "zcsr/ui.h"
#include "zcsr/ui_event.h"
#include "uim_component_manager.h"

#define UIM_BRIDGE_MAX_NODES 128

typedef struct UIM_ZcsrRuntimeBridge {
    ZCSR_UI*                zcsr;
    UIM_ComponentManager*   uim;

    /* 1:1 mapping arrays (fast lookup) */
    uim_NodeId              zcsr_to_uim[UIM_BRIDGE_MAX_NODES];
    uim_PropertyId          property_for_node[UIM_BRIDGE_MAX_NODES]; /* 0 = no binding */

    /* Reverse mapping: UIM node → ZCSR node */
    ZCSR_NodeId             uim_to_zcsr[UIM_BRIDGE_MAX_NODES];

    int                     events_forwarded;
    int                     events_received;
    int                     nodes_created;
} UIM_ZcsrRuntimeBridge;

void uim_zcsr_runtime_bridge_create(UIM_ZcsrRuntimeBridge* b, ZCSR_UI* z, UIM_ComponentManager* u);
void uim_zcsr_runtime_bridge_map_node(UIM_ZcsrRuntimeBridge* b, ZCSR_NodeId zn, uim_NodeId un, uim_PropertyId prop);

/* Sync entire UIM page → ZCSR UI node tree */
uim_Result uim_zcsr_runtime_bridge_sync_page(UIM_ZcsrRuntimeBridge* b, uim_PageId page_id);

/* Sync a single UIM node → ZCSR node (create if needed) */
uim_Result uim_zcsr_runtime_bridge_sync_node(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id, ZCSR_NodeId zcsr_parent);

/* Apply UIM property change → ZCSR setter */
uim_Result uim_zcsr_runtime_bridge_apply_property(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id, uim_PropertyId prop_id, uim_Value* value);

/* Drain ZCSR event queue and forward each to UIM. */
void uim_zcsr_runtime_bridge_forward_zcsr_events(UIM_ZcsrRuntimeBridge* b);
/* Drain UIM event queue and print. */
void uim_zcsr_runtime_bridge_drain_uim_events(UIM_ZcsrRuntimeBridge* b);

#endif /* UIM_ZCSR_RUNTIME_BRIDGE_H */
