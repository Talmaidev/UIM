#ifndef UIM_COMPONENT_MANAGER_H
#define UIM_COMPONENT_MANAGER_H
/* UIM Component Manager.
 *
 * Canonical UIM contract: this module owns the smallest UI component registry,
 * component instances and property values. It does not own the OS window, the
 * renderer, the scene, the setting page, or the editor workspace.
 *
 * Migration status: Page/Node document and layout APIs still live in this
 * header for source compatibility, but their symbols are compiled only into
 * the zcsr_uim_document target. zcsr_uim is a compatibility facade that links
 * both zcsr_uim_component and zcsr_uim_document. Do not add new page/node
 * document APIs here; extend the document target instead.
 *
 * This module does not include or implement ZCSR Primitive runtime or any
 * graphics API. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct UIM_ComponentManager UIM_ComponentManager;
typedef uint64_t uim_PageId;
typedef uint64_t uim_NodeId;
typedef uint64_t uim_BindingId;
typedef uint32_t uim_ComponentId; /* opaque external component identity; never redefined here */
typedef uint32_t uim_ActionId;    /* host-defined semantic action identity */
typedef uint32_t uim_PropertyId;
#define uim_PAGE_INVALID ((uim_PageId)0)
#define uim_NODE_INVALID ((uim_NodeId)0)
#define uim_BINDING_INVALID ((uim_BindingId)0)
#define uim_ACTION_INVALID ((uim_ActionId)0)

typedef enum uim_Result {
    uim_OK=0, uim_ERR_ARGUMENT, uim_ERR_NOMEM, uim_ERR_NOT_FOUND,
    uim_ERR_EXISTS, uim_ERR_TYPE, uim_ERR_STATE, uim_ERR_RANGE,
    uim_ERR_STALE, uim_ERR_BUFFER, uim_ERR_UNSUPPORTED
} uim_Result;

typedef enum uim_ValueType { uim_VALUE_NONE=0, uim_VALUE_BOOL, uim_VALUE_INT64, uim_VALUE_FLOAT64, uim_VALUE_STRING } uim_ValueType;
typedef struct uim_Value {
    uim_ValueType type;
    union { bool boolean; int64_t integer; double real; struct { const char *data; size_t size; } string; } as;
} uim_Value;
uim_Value uim_value_bool(bool v);
uim_Value uim_value_int(int64_t v);
uim_Value uim_value_float(double v);
uim_Value uim_value_string(const char *v, size_t n);
bool uim_value_equal(uim_Value a, uim_Value b);
/* Values returned by get/state APIs are borrowed views. Copy string bytes before
 * mutating the manager or destroying it. uim_Event string payloads are owned by
 * the event and must be released with uim_event_dispose(). */

typedef struct uim_Rect { float x,y,width,height; } uim_Rect;
/* Legacy document placement space. A node may carry a projected placement
 * transform for editing a 2.5D document; this is not a 3D scene model and it
 * must not be used to create a 3D renderer. Existing 2D callers leave space
 * zero and keep using local_rect exactly as before. */
typedef enum uim_Space { uim_SPACE_2D = 0, uim_SPACE_3D = 1 } uim_Space;
typedef struct uim_Transform3 {
    float position[3];
    float rotation[3]; /* Euler degrees; quaternion can be added without changing 2D */
    float scale[3];
} uim_Transform3;
/* Optional responsive anchors. Offsets are measured inward from each anchor:
 * left=x(parent)*anchor_min_x+offset_left; right=x(parent)*anchor_max_x-offset_right,
 * with the equivalent top/bottom equations. Root nodes anchor to the page viewport. */
typedef struct uim_LayoutConstraints {
    bool enabled;
    float anchor_min_x, anchor_min_y, anchor_max_x, anchor_max_y;
    float offset_left, offset_top, offset_right, offset_bottom;
} uim_LayoutConstraints;
typedef struct UIM_ComponentManagerConfig { size_t initial_node_capacity; size_t initial_page_capacity; } UIM_ComponentManagerConfig;

typedef enum uim_BindingDirection { uim_BIND_READ=1, uim_BIND_WRITE=2, uim_BIND_READ_WRITE=3 } uim_BindingDirection;
typedef enum uim_SyncPolicy { uim_SYNC_EXTERNAL_WINS=0, uim_SYNC_PENDING_WINS, uim_SYNC_INITIAL_ONLY } uim_SyncPolicy;
typedef enum uim_BindingState { uim_BINDING_ACTIVE=0, uim_BINDING_SUSPENDED, uim_BINDING_BROKEN } uim_BindingState;
typedef struct uim_BindingDesc {
    uim_NodeId node_id; uim_PropertyId property_id; uim_ValueType value_type;
    uim_BindingDirection direction; uim_SyncPolicy initial_sync; uim_BindingState state;
} uim_BindingDesc;

typedef struct uim_NodeDesc {
    const char *stable_key; /* persistent editor/layout identity; unique within a page */
    uim_PageId page_id; uim_ComponentId component_id; uim_NodeId parent_id;
    uim_Rect local_rect; uim_LayoutConstraints layout; bool visible; bool enabled; int32_t z_order;
    uim_ActionId action_id; /* 0 means this node has no semantic action */
    uint32_t child_order; const void *component_config; size_t component_config_size;
    /* 2D layout stays in local_rect/layout; 3D nodes use space+transform.
     * The fields are appended so existing zero-initialized 2D callers are unchanged. */
    uim_Space space;
    uim_Transform3 transform;
} uim_NodeDesc;
typedef struct uim_NodeInfo {
    const char *stable_key; /* borrowed until node mutation/destruction */
    uim_NodeId id; uim_PageId page_id; uim_ComponentId component_id; uim_NodeId parent_id;
    uim_Rect local_rect; uim_LayoutConstraints layout; bool visible; bool enabled; int32_t z_order; uim_ActionId action_id; uint32_t child_order;
    uim_BindingId binding_id; const void *component_config; size_t component_config_size;
    uim_Space space;
    uim_Transform3 transform;
} uim_NodeInfo;
typedef struct uim_SettingState { uim_Value initial, committed, pending; bool dirty; uint64_t revision; } uim_SettingState;
typedef enum uim_EventType { uim_EVENT_NONE=0, uim_EVENT_USER_VALUE_CHANGED, uim_EVENT_ACTION } uim_EventType;
typedef struct uim_Event { uim_EventType type; uim_NodeId node_id; uim_PropertyId property_id; uim_ActionId action_id; uim_Value value; uint64_t sequence; } uim_Event;

typedef struct uim_LayoutNode { const char *stable_key; const char *parent_stable_key; uim_NodeId node_id; uim_NodeId parent_id; uim_Rect rect; uim_LayoutConstraints layout; bool visible; bool enabled; int32_t z_order; uint32_t child_order; } uim_LayoutNode;
typedef struct uim_LayoutSnapshot { uim_PageId page_id; const uim_LayoutNode *nodes; size_t node_count; } uim_LayoutSnapshot;
typedef struct uim_PresetEntry { uim_PropertyId property_id; uim_Value value; } uim_PresetEntry;
typedef struct uim_PresetSnapshot { const uim_PresetEntry *entries; size_t entry_count; } uim_PresetSnapshot;

uim_Result uim_component_manager_create(const UIM_ComponentManagerConfig *config, UIM_ComponentManager **out_manager);
void uim_component_manager_destroy(UIM_ComponentManager *manager);
uim_Result uim_page_create(UIM_ComponentManager *manager, const char *key, uim_PageId *out_page_id);
uim_Result uim_page_destroy(UIM_ComponentManager *manager, uim_PageId page_id);
uim_Result uim_page_find(const UIM_ComponentManager *manager, const char *key, uim_PageId *out_page_id);
uim_Result uim_page_get_key(const UIM_ComponentManager *manager, uim_PageId page_id, const char **out_key);
uim_Result uim_page_count(const UIM_ComponentManager *manager, size_t *out_count);
uim_Result uim_page_get_at(const UIM_ComponentManager *manager, size_t index, uim_PageId *out_page_id);
uim_Result uim_page_get_node_count(const UIM_ComponentManager *manager, uim_PageId page_id, size_t *out_count);
uim_Result uim_page_get_node_at(const UIM_ComponentManager *manager, uim_PageId page_id, size_t index, uim_NodeInfo *out_info);
uim_Result uim_node_find_stable_key(const UIM_ComponentManager *manager, uim_PageId page_id, const char *stable_key, uim_NodeId *out_node_id);
uim_Result uim_node_create(UIM_ComponentManager *manager, const uim_NodeDesc *desc, uim_NodeId *out_node_id);
/* Rename a node's persistent identity. The key must be unique within its page. */
uim_Result uim_node_set_stable_key(UIM_ComponentManager *manager, uim_NodeId node_id, const char *stable_key);
uim_Result uim_node_destroy(UIM_ComponentManager *manager, uim_NodeId node_id);
uim_Result uim_node_get(const UIM_ComponentManager *manager, uim_NodeId node_id, uim_NodeInfo *out_info);
uim_Result uim_node_get_child(const UIM_ComponentManager *manager, uim_NodeId parent_id, size_t index, uim_NodeInfo *out_info);
uim_Result uim_node_set_parent(UIM_ComponentManager *manager, uim_NodeId node_id, uim_NodeId parent_id);
uim_Result uim_node_set_rect(UIM_ComponentManager *manager, uim_NodeId node_id, uim_Rect rect);
/* Space is a document property, not an editor toggle. Switching does not
 * silently reinterpret rect <-> transform; the caller must issue an explicit
 * conversion command so undo/diff stay predictable. */
uim_Result uim_node_set_space(UIM_ComponentManager *manager, uim_NodeId node_id, uim_Space space);
uim_Result uim_node_set_transform3(UIM_ComponentManager *manager, uim_NodeId node_id,
                                   const uim_Transform3 *transform);
uim_Result uim_node_get_transform3(const UIM_ComponentManager *manager, uim_NodeId node_id,
                                   uim_Transform3 *out_transform);
uim_Result uim_node_set_layout_constraints(UIM_ComponentManager *manager, uim_NodeId node_id, uim_LayoutConstraints constraints);
uim_Result uim_node_get_layout_constraints(const UIM_ComponentManager *manager, uim_NodeId node_id, uim_LayoutConstraints *out_constraints);
/* Recalculate anchored nodes for a viewport size. Changes are prevalidated and applied atomically. */
uim_Result uim_page_resolve_layout(UIM_ComponentManager *manager, uim_PageId page_id, float viewport_width, float viewport_height);
uim_Result uim_node_set_visible(UIM_ComponentManager *manager, uim_NodeId node_id, bool visible);
uim_Result uim_node_set_enabled(UIM_ComponentManager *manager, uim_NodeId node_id, bool enabled);
uim_Result uim_node_set_z_order(UIM_ComponentManager *manager, uim_NodeId node_id, int32_t z_order);
uim_Result uim_node_set_action_id(UIM_ComponentManager *manager, uim_NodeId node_id, uim_ActionId action_id);
uim_Result uim_node_set_child_order(UIM_ComponentManager *manager, uim_NodeId node_id, uint32_t child_order);
uim_Result uim_node_set_component_config(UIM_ComponentManager *manager, uim_NodeId node_id, const void *data, size_t size);

uim_Result uim_binding_create(UIM_ComponentManager *manager, const uim_BindingDesc *desc, uim_BindingId *out_binding_id);
/* Restore an existing binding definition without applying its initial sync policy. */
uim_Result uim_binding_restore(UIM_ComponentManager *manager, const uim_BindingDesc *desc, uim_BindingId *out_binding_id);
uim_Result uim_binding_remove(UIM_ComponentManager *manager, uim_BindingId binding_id);
uim_Result uim_binding_set_state(UIM_ComponentManager *manager, uim_BindingId binding_id, uim_BindingState state);
uim_Result uim_binding_get(const UIM_ComponentManager *manager, uim_BindingId binding_id, uim_BindingDesc *out_desc);
uim_Result uim_property_register(UIM_ComponentManager *manager, uim_PropertyId property_id, uim_Value initial_value);
uim_Result uim_property_register_named(UIM_ComponentManager *manager, uim_PropertyId property_id, const char *stable_key, uim_Value initial_value);
uim_Result uim_property_get_stable_key(const UIM_ComponentManager *manager, uim_PropertyId property_id, const char **out_key);
uim_Result uim_property_find_stable_key(const UIM_ComponentManager *manager, const char *stable_key, uim_PropertyId *out_property_id);
uim_Result uim_setting_count(const UIM_ComponentManager *manager, size_t *out_count);
uim_Result uim_setting_get_at(const UIM_ComponentManager *manager, size_t index, uim_PropertyId *out_property_id, uim_SettingState *out_state);
uim_Result uim_property_update(UIM_ComponentManager *manager, uim_PropertyId property_id, uim_Value external_value);
uim_Result uim_property_get(const UIM_ComponentManager *manager, uim_PropertyId property_id, uim_Value *out_value);
uim_Result uim_user_value_changed(UIM_ComponentManager *manager, uim_NodeId node_id, uim_Value value);
/* Submit an action already emitted by a real UI adapter. The node must carry a nonzero
 * host-defined action_id. This never performs game/business work; it only queues an event. */
uim_Result uim_user_action(UIM_ComponentManager *manager, uim_NodeId node_id, uim_Value payload);
uim_Result uim_setting_set_pending(UIM_ComponentManager *manager, uim_PropertyId property_id, uim_Value value);
uim_Result uim_setting_set_pending_batch(UIM_ComponentManager *manager, const uim_PropertyId *property_ids, const uim_Value *values, size_t count);
uim_Result uim_setting_get_state(const UIM_ComponentManager *manager, uim_PropertyId property_id, uim_SettingState *out_state);
uim_Result uim_setting_reset(UIM_ComponentManager *manager, uim_PropertyId property_id);
uim_Result uim_setting_cancel(UIM_ComponentManager *manager, uim_PropertyId property_id);
uim_Result uim_apply_request(UIM_ComponentManager *manager, uint64_t *out_serial);
uim_Result uim_apply_request_for(UIM_ComponentManager *manager, const uim_PropertyId *property_ids, size_t count, uint64_t *out_serial);
uim_Result uim_apply_confirm(UIM_ComponentManager *manager, uint64_t serial, bool success);
uim_Result uim_apply_is_pending(const UIM_ComponentManager *manager, uint64_t *out_serial);
uim_Result uim_event_poll(UIM_ComponentManager *manager, uim_Event *out_event);
void uim_event_dispose(uim_Event *event); /* release copied string payload, then zero event */
uim_Result uim_event_clear(UIM_ComponentManager *manager);
uim_Result uim_layout_save(UIM_ComponentManager *manager, const char *name, uim_PageId page_id);
uim_Result uim_layout_load(UIM_ComponentManager *manager, const char *name);
uim_Result uim_layout_get(const UIM_ComponentManager *manager, const char *name, uim_LayoutSnapshot *out_snapshot);
uim_Result uim_layout_save_file(const UIM_ComponentManager *manager, const char *path, uim_PageId page_id);
uim_Result uim_layout_load_file(UIM_ComponentManager *manager, const char *path, uim_PageId target_page_id);
uim_Result uim_preset_save(UIM_ComponentManager *manager, const char *name);
uim_Result uim_preset_load(UIM_ComponentManager *manager, const char *name);
uim_Result uim_preset_get(const UIM_ComponentManager *manager, const char *name, uim_PresetSnapshot *out_snapshot);
uim_Result uim_preset_save_file(const UIM_ComponentManager *manager, const char *path);
uim_Result uim_preset_load_file(UIM_ComponentManager *manager, const char *path);

/* Multi-component document instances. A Node may own multiple component
 * instances; each instance is a stable document identity (separate from the
 * legacy single-slot uim_NodeDesc.component_id). Type identity lives in the
 * Editor ABI layer; here it is an opaque 32-bit id. */
typedef uint32_t uim_ComponentTypeId;
typedef uint64_t uim_ComponentInstanceId;
#define uim_COMPONENT_INSTANCE_INVALID ((uim_ComponentInstanceId)0)
typedef struct uim_ComponentInstanceInfo {
    uim_ComponentInstanceId instance_id;
    uim_NodeId node_id;
    uim_ComponentTypeId type_id;
    uint32_t schema_version;
} uim_ComponentInstanceInfo;

uim_Result uim_component_instance_attach(UIM_ComponentManager *manager, uim_NodeId node_id,
                                         uim_ComponentTypeId type_id, uint32_t schema_version,
                                         uim_ComponentInstanceId *out_instance_id);
uim_Result uim_component_instance_restore(UIM_ComponentManager *manager,
                                          const uim_ComponentInstanceInfo *info);
uim_Result uim_component_instance_detach(UIM_ComponentManager *manager,
                                         uim_ComponentInstanceId instance_id);
uim_Result uim_component_instance_get(const UIM_ComponentManager *manager,
                                      uim_ComponentInstanceId instance_id,
                                      uim_ComponentInstanceInfo *out_info);
uim_Result uim_node_component_count(const UIM_ComponentManager *manager, uim_NodeId node_id,
                                    size_t *out_count);
uim_Result uim_node_component_get_at(const UIM_ComponentManager *manager, uim_NodeId node_id,
                                     size_t index, uim_ComponentInstanceInfo *out_info);
uim_Result uim_component_instance_set_property(UIM_ComponentManager *manager,
                                               uim_ComponentInstanceId instance_id,
                                               uim_PropertyId property_id, uim_Value value);
uim_Result uim_component_instance_get_property(const UIM_ComponentManager *manager,
                                               uim_ComponentInstanceId instance_id,
                                               uim_PropertyId property_id,
                                               uim_Value *out_value);
/* Property enumeration for serialization and diagnostics. Values are borrowed
 * until the same instance is mutated or the receiver is destroyed. */
uim_Result uim_component_instance_property_count(const UIM_ComponentManager *manager,
                                                 uim_ComponentInstanceId instance_id,
                                                 size_t *out_count);
uim_Result uim_component_instance_property_get_at(const UIM_ComponentManager *manager,
                                                  uim_ComponentInstanceId instance_id,
                                                  size_t index,
                                                  uim_PropertyId *out_property_id,
                                                  uim_Value *out_value);

#ifdef __cplusplus
}
#endif
#endif
