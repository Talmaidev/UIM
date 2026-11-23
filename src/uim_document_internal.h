#ifndef UIM_DOCUMENT_INTERNAL_H
#define UIM_DOCUMENT_INTERNAL_H
/* Private storage shared by the UIM component registry and the UIM document
 * target.  Both translation units must see exactly the same layout: the
 * manager remains one runtime object, while page/node API symbols live in
 * zcsr_uim_document instead of zcsr_uim_component.
 *
 * This header is never installed and must not be included outside UIM/src. */
#include "uim_component_manager.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { uim_PageId id; char *key; } Page;
typedef struct { uim_NodeId id; uim_NodeDesc d; char *stable_key; void *config; uim_BindingId binding; } Node;
typedef struct { uim_PropertyId id; char *stable_key; uim_Value value; } Property;
typedef struct { uim_BindingId id; uim_BindingDesc d; } Binding;
typedef struct { uim_PropertyId id; uim_Value initial,committed,pending,snapshot; uint64_t revision; bool snapshot_valid; } Setting;
typedef struct { char *name; uim_PageId page; uim_LayoutNode *nodes; size_t count; } Layout;
typedef struct { char *name; uim_PresetEntry *entries; size_t count; } Preset;
typedef struct { uim_PropertyId property_id; uim_Value value; } ComponentProperty;
typedef struct {
    uim_ComponentInstanceInfo info;
    ComponentProperty *properties;
    size_t property_count, property_cap;
} ComponentInstance;
struct UIM_ComponentManager {
    Page *pages; size_t page_count,page_cap;
    Node *nodes; size_t node_count,node_cap;
    Property *props; size_t prop_count,prop_cap;
    Binding *bindings; size_t binding_count,binding_cap;
    Setting *settings; size_t setting_count,setting_cap;
    Layout *layouts; size_t layout_count,layout_cap;
    Preset *presets; size_t preset_count,preset_cap;
    uim_Event *events; size_t event_count,event_cap;
    ComponentInstance *component_instances; size_t component_instance_count,component_instance_cap;
    uint64_t next_page,next_node,next_binding,next_serial,next_event,next_component_instance,active_serial;
    bool apply_pending;
};

#endif /* UIM_DOCUMENT_INTERNAL_H */
