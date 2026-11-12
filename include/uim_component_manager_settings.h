#ifndef UIM_COMPONENT_MANAGER_SETTINGS_H
#define UIM_COMPONENT_MANAGER_SETTINGS_H

#include "uim_component_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uim_SettingsAction {
    uim_SETTINGS_RESET = 0x55495201u,
    uim_SETTINGS_CANCEL = 0x55495202u,
    uim_SETTINGS_APPLY = 0x55495203u
} uim_SettingsAction;

typedef struct uim_SettingsCategoryDesc {
    const char *stable_key;
    const void *component_config;
    size_t component_config_size;
} uim_SettingsCategoryDesc;

typedef struct uim_SettingsItemDesc {
    const char *stable_key;       /* unique within this settings page */
    uim_PropertyId property_id;   /* property must already be registered */
    uim_ComponentId label_component_id;
    uim_ComponentId control_component_id;
    uim_BindingDirection direction;
    uim_SyncPolicy initial_sync;
    const void *label_config;
    size_t label_config_size;
    const void *control_config;
    size_t control_config_size;
    uint32_t category_index;       /* ignored when no categories are declared */
} uim_SettingsItemDesc;

typedef struct uim_SettingsWindowDesc {
    const char *page_key;
    uim_ComponentId window_component_id;
    uim_ComponentId panel_component_id;
    uim_ComponentId scroll_component_id;
    uim_ComponentId label_component_id;
    uim_ComponentId button_component_id;
    float width, height;
    float header_height;
    float sidebar_width;
    float footer_height;
    float padding;
    float row_height;
    float row_gap;
    float action_button_width;
    float action_button_height;
    const uim_SettingsItemDesc *items;
    size_t item_count;
    uim_ComponentId category_component_id;
    const uim_SettingsCategoryDesc *categories;
    size_t category_count;
} uim_SettingsWindowDesc;

typedef struct uim_SettingsWindow {
    uim_PageId page_id;
    uim_NodeId window_node;
    uim_NodeId header_node;
    uim_NodeId sidebar_node;
    uim_NodeId content_node;
    uim_NodeId scroll_node;
    uim_NodeId footer_node;
    uim_NodeId reset_button;
    uim_NodeId cancel_button;
    uim_NodeId apply_button;
    uim_NodeId *row_nodes;
    uim_NodeId *label_nodes;
    uim_NodeId *control_nodes;
    uim_PropertyId *property_ids;
    uint32_t *row_category_indices;
    uim_NodeId *category_nodes;
    size_t item_count;
    size_t category_count;
    uint32_t active_category;
} uim_SettingsWindow;

/* Builds the neutral default commercial settings composition. All Component IDs,
 * control config, labels/translations and Property IDs are supplied by the host.
 * This constructs only Manager data; it does not draw or instantiate primitives. */
uim_Result uim_settings_window_build(UIM_ComponentManager *manager,
                                    const uim_SettingsWindowDesc *desc,
                                    uim_SettingsWindow *out_window);
void uim_settings_window_dispose(uim_SettingsWindow *window);
uim_Result uim_settings_window_destroy(UIM_ComponentManager *manager, uim_SettingsWindow *window);
/* Invoke after the adapter reports activation of one of the three action nodes.
 * Apply returns its serial; host must confirm it with uim_apply_confirm(). */
uim_Result uim_settings_window_activate(UIM_ComponentManager *manager,
                                        const uim_SettingsWindow *window,
                                        uim_NodeId activated_node,
                                        uint64_t *out_apply_serial);
/* Adapter-facing path: dispatches a polled uim_EVENT_ACTION for one of this Window's
 * Reset/Cancel/Apply buttons. Apply returns a serial that the host must confirm. */
uim_Result uim_settings_window_dispatch_event(UIM_ComponentManager *manager,
                                              const uim_SettingsWindow *window,
                                              const uim_Event *event,
                                              uint64_t *out_apply_serial);
uim_Result uim_settings_window_select_category(UIM_ComponentManager *manager,
                                               uim_SettingsWindow *window,
                                               uint32_t category_index);
uim_Result uim_settings_window_select_category_node(UIM_ComponentManager *manager,
                                                    uim_SettingsWindow *window,
                                                    uim_NodeId category_node,
                                                    uint32_t *out_category_index);

#ifdef __cplusplus
}
#endif
#endif /* UIM_COMPONENT_MANAGER_SETTINGS_H */
