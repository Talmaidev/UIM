/* UIM-ZCSR Runtime Bridge: translates UIM instance data to ZCSR Runtime nodes.
 * No rendering, no GL. Pure mapping and event-flow verification. */
#ifndef UIM_ZCSR_RUNTIME_BRIDGE_H
#define UIM_ZCSR_RUNTIME_BRIDGE_H
#include "zcsr/ui.h"
#include "zcsr/ui_event.h"
#include "uim_component_manager.h"

/* ZCSR node ids are small integers, so the reverse side can stay indexed.
 * UIM node ids are 64-bit and monotonic across pages; they are stored in a
 * chained hash map instead of a fixed 2048-entry array. */
#define UIM_BRIDGE_MAX_NODES 2048
#define UIM_BRIDGE_HASH_BUCKETS 4096

typedef struct UIM_ZcsrNodeMapEntry UIM_ZcsrNodeMapEntry;

/* Host-owned image resource resolver. UIM stores an asset path; the host maps it
 * to a zcsr_ui_key (0 = missing). The bridge never decodes files or owns textures. */
typedef uint32_t (*UIM_ZcsrResourceResolveFn)(void* user, const char* asset_path);

typedef struct UIM_ZcsrRuntimeBridge {
    ZCSR_UI*                zcsr;
    UIM_ComponentManager*   uim;

    /* ZCSR -> UIM mapping, indexed by the small ZCSR node id. */
    uim_NodeId              zcsr_to_uim[UIM_BRIDGE_MAX_NODES];
    uim_PropertyId          property_for_node[UIM_BRIDGE_MAX_NODES]; /* 0 = no binding */

    /* UIM node -> ZCSR node mapping. UIM ids are 64-bit and may exceed the
     * fixed ZCSR range, so this is a dynamically allocated hash chain. */
    UIM_ZcsrNodeMapEntry*   uim_to_zcsr[UIM_BRIDGE_HASH_BUCKETS];

    int                     events_forwarded;
    int                     events_received;
    int                     nodes_created;

    /* Document page -> window viewport. The bridge keeps UIM rects in document
     * units; player/editor pass scale+offset so ZCSR_UI draws at the real client
     * size without a second renderer. */
    float                   view_scale;
    float                   view_offset_x;
    float                   view_offset_y;

    UIM_ZcsrResourceResolveFn resource_resolve;
    void*                   resource_user;
} UIM_ZcsrRuntimeBridge;

void uim_zcsr_runtime_bridge_create(UIM_ZcsrRuntimeBridge* b, ZCSR_UI* z, UIM_ComponentManager* u);

/* Free hash entries. Safe to call after reset; does not destroy ZCSR nodes. */
void uim_zcsr_runtime_bridge_destroy(UIM_ZcsrRuntimeBridge* b);

/* Drop mappings before switching pages / rebuilding a document. ZCSR nodes are
 * destroyed by the caller's ZCSR_UI instance. */
void uim_zcsr_runtime_bridge_reset(UIM_ZcsrRuntimeBridge* b);

/* Set the document->window transform used by sync_page/sync_node. */
void uim_zcsr_runtime_bridge_set_view(UIM_ZcsrRuntimeBridge* b, float scale,
                                      float offset_x, float offset_y);

/* Optional: resolve UIM Image `asset` strings to host texture keys. */
void uim_zcsr_runtime_bridge_set_resource_resolver(UIM_ZcsrRuntimeBridge* b,
                                                   UIM_ZcsrResourceResolveFn resolve,
                                                   void* user);

/* Project one 3D UIM node to page coordinates using the same camera and world
 * unit contract as bridge rendering. Returns false for 2D nodes or when the
 * billboard is behind the camera. This is the UIM-side 3D hit-test input. */
bool uim_zcsr_runtime_bridge_project_node_rect(const UIM_ComponentManager* uim,
                                               const uim_NodeInfo* info,
                                               uim_Rect* out_rect);

/* Register a 1:1 ZCSR node <-> UIM node binding. Used by sync_node and by
 * integration tests that build a node tree manually. */
void uim_zcsr_runtime_bridge_map_node(UIM_ZcsrRuntimeBridge* b, ZCSR_NodeId zn,
                                      uim_NodeId un, uim_PropertyId prop);

/* Return the ZCSR node bound to a UIM node, or ZCSR_NODE_INVALID. */
ZCSR_NodeId uim_zcsr_runtime_bridge_find_zcsr(const UIM_ZcsrRuntimeBridge* b,
                                              uim_NodeId un);

/* Sync entire UIM page → ZCSR UI node tree */
uim_Result uim_zcsr_runtime_bridge_sync_page(UIM_ZcsrRuntimeBridge* b, uim_PageId page_id);

/* Sync a single UIM node → ZCSR node (create if needed) */
uim_Result uim_zcsr_runtime_bridge_sync_node(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id, ZCSR_NodeId zcsr_parent);
uim_Result uim_zcsr_runtime_bridge_sync_node_ex(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id,
                                                ZCSR_NodeId zcsr_parent, bool is_root);

/* Apply UIM property change → ZCSR setter */
uim_Result uim_zcsr_runtime_bridge_apply_property(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id, uim_PropertyId prop_id, uim_Value* value);

/* Drain ZCSR event queue and forward each to UIM. */
void uim_zcsr_runtime_bridge_forward_zcsr_events(UIM_ZcsrRuntimeBridge* b);
/* Drain UIM event queue and print. */
void uim_zcsr_runtime_bridge_drain_uim_events(UIM_ZcsrRuntimeBridge* b);

#endif /* UIM_ZCSR_RUNTIME_BRIDGE_H */
