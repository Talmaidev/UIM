#include "uim_zcsr_runtime_bridge.h"
#include "zcsr/camera3d.h"
#include "zcsr/ui_toggle.h"
#include "zcsr/ui_slider.h"
#include "zcsr/ui_button.h"
#include "zcsr/ui_label.h"
#include "zcsr/ui_dropdown.h"
#include "zcsr/ui_separator.h"
#include "zcsr/ui_text_input.h"
#include "zcsr/ui_scroll_view.h"
#include "zcsr/ui_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct UIM_ZcsrNodeMapEntry {
    uim_NodeId                  uim;
    ZCSR_NodeId                 zcsr;
    struct UIM_ZcsrNodeMapEntry* next;
};

static size_t bridge_hash(uim_NodeId id) {
    uint64_t x = (uint64_t)id;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (size_t)(x & (UIM_BRIDGE_HASH_BUCKETS - 1u));
}

static ZCSR_NodeId bridge_lookup(const UIM_ZcsrRuntimeBridge* b, uim_NodeId un) {
    UIM_ZcsrNodeMapEntry* entry;
    if (!b) return ZCSR_NODE_INVALID;
    entry = b->uim_to_zcsr[bridge_hash(un)];
    while (entry) {
        if (entry->uim == un) return entry->zcsr;
        entry = entry->next;
    }
    return ZCSR_NODE_INVALID;
}

static void bridge_store(UIM_ZcsrRuntimeBridge* b, uim_NodeId un, ZCSR_NodeId zn) {
    UIM_ZcsrNodeMapEntry* entry;
    size_t bucket;
    if (!b) return;
    bucket = bridge_hash(un);
    entry = b->uim_to_zcsr[bucket];
    while (entry) {
        if (entry->uim == un) {
            entry->zcsr = zn;
            return;
        }
        entry = entry->next;
    }
    entry = (UIM_ZcsrNodeMapEntry*)malloc(sizeof *entry);
    if (!entry) return;
    entry->uim = un;
    entry->zcsr = zn;
    entry->next = b->uim_to_zcsr[bucket];
    b->uim_to_zcsr[bucket] = entry;
}

static void bridge_free_map(UIM_ZcsrRuntimeBridge* b) {
    size_t i;
    if (!b) return;
    for (i = 0; i < UIM_BRIDGE_HASH_BUCKETS; ++i) {
        UIM_ZcsrNodeMapEntry* entry = b->uim_to_zcsr[i];
        while (entry) {
            UIM_ZcsrNodeMapEntry* next = entry->next;
            free(entry);
            entry = next;
        }
        b->uim_to_zcsr[i] = NULL;
    }
}

/* UIR editor component ids (same numeric domain as zcsr_editor's
 * ui_editor_component_registry.h; duplicated here because the bridge is built
 * by UIM and must stay independent of the editor target). */
#define UIR_C_PANEL       0x00010001u
#define UIR_C_BUTTON      0x00010002u
#define UIR_C_TOGGLE      0x00010003u
#define UIR_C_SLIDER      0x00010004u
#define UIR_C_DROPDOWN    0x00010005u
#define UIR_C_LABEL       0x00010006u
#define UIR_C_SEPARATOR   0x00010007u
#define UIR_C_IMAGE       0x00010008u
#define UIR_C_TEXT_INPUT  0x00010009u
#define UIR_C_SCROLL_VIEW 0x0001000Au

/* Property ids used by the editor registry. */
#define UIR_PROP_TEXT       1u
#define UIR_PROP_COLOR      2u
#define UIR_PROP_BACKGROUND 3u
#define UIR_PROP_VALUE      4u
#define UIR_PROP_CHECKED    5u
#define UIR_PROP_ASSET      6u
#define UIR_PROP_FONT_SIZE  7u

static bool is_visual_type(uint32_t type) {
    return type >= UIR_C_PANEL && type <= UIR_C_SCROLL_VIEW;
}

/* Legacy 1..5 ids still exist in uim_NodeDesc.component_id and in older
 * .uireproj v1 files; keep accepting them, but the editor writes real type ids. */
static ZCSR_Result map_component_type(uint32_t uim_type, ZCSR_ComponentId* out) {
    if (uim_type >= UIR_C_PANEL && uim_type <= UIR_C_SCROLL_VIEW) {
        *out = (ZCSR_ComponentId)(uim_type + 0x5A420000u);
        return ZCSR_OK;
    }
    switch (uim_type) {
    case 1: *out = ZCSR_COMPONENT_BUTTON; return ZCSR_OK;
    case 2: *out = ZCSR_COMPONENT_TOGGLE; return ZCSR_OK;
    case 3: *out = ZCSR_COMPONENT_SLIDER; return ZCSR_OK;
    case 4: *out = ZCSR_COMPONENT_LABEL; return ZCSR_OK;
    case 5: *out = ZCSR_COMPONENT_PANEL; return ZCSR_OK;
    case 9: *out = ZCSR_COMPONENT_TEXT_INPUT; return ZCSR_OK;
    default: return ZCSR_ERR_UNSUPPORTED;
    }
}

/* Pick the node's visual component. A node may also carry behavior components
 * (Rule/Health/Motion); those must not decide the ZCSR widget type. */
static uint32_t node_visual_type(const UIM_ComponentManager* uim, uim_NodeId node,
                                 uim_ComponentInstanceId* out_instance) {
    size_t count = 0;
    size_t i;
    if (out_instance) *out_instance = uim_COMPONENT_INSTANCE_INVALID;
    if (uim_node_component_count(uim, node, &count) == uim_OK) {
        for (i = 0; i < count; ++i) {
            uim_ComponentInstanceInfo info;
            if (uim_node_component_get_at(uim, node, i, &info) == uim_OK &&
                is_visual_type(info.type_id)) {
                if (out_instance) *out_instance = info.instance_id;
                return info.type_id;
            }
        }
    }
    return 0u;
}

static uint32_t node_component_type(const UIM_ComponentManager* uim, uim_NodeId node,
                                    uim_ComponentInstanceId* out_instance) {
    uim_NodeInfo info;
    uint32_t type = node_visual_type(uim, node, out_instance);
    if (type != 0u) return type;
    if (uim_node_get(uim, node, &info) == uim_OK) return info.component_id;
    return 0u;
}

static bool get_prop(const UIM_ComponentManager* uim, uim_ComponentInstanceId instance,
                     uim_PropertyId prop, uim_Value* out) {
    if (instance == uim_COMPONENT_INSTANCE_INVALID) return false;
    return uim_component_instance_get_property(uim, instance, prop, out) == uim_OK;
}

static ZCSR_Color color_from_u32(int64_t packed) {
    ZCSR_Color c;
    uint32_t v = (uint32_t)packed;
    /* UIM stores colors as 0xAARRGGBB; ZCSR_Color is RGBA floats. */
    c.a = (float)((v >> 24) & 0xFFu) / 255.0f;
    c.r = (float)((v >> 16) & 0xFFu) / 255.0f;
    c.g = (float)((v >> 8) & 0xFFu) / 255.0f;
    c.b = (float)(v & 0xFFu) / 255.0f;
    return c;
}

static void set_text(ZCSR_UI* z, ZCSR_NodeId zn, ZCSR_ComponentId comp,
                     const char* text, size_t size) {
    switch (comp) {
    case ZCSR_COMPONENT_LABEL:
        (void)zcsr_label_set_text(z, zn, text, (uint32_t)size);
        break;
    case ZCSR_COMPONENT_TOGGLE:
        (void)zcsr_toggle_set_text(z, zn, text, (uint32_t)size);
        break;
    case ZCSR_COMPONENT_TEXT_INPUT:
        (void)zcsr_text_input_set_text(z, zn, text, (uint32_t)size);
        break;
    case ZCSR_COMPONENT_DROPDOWN:
        (void)zcsr_dropdown_clear_items(z, zn);
        if (size > 0u) (void)zcsr_dropdown_add_item(z, zn, text, (uint32_t)size);
        break;
    case ZCSR_COMPONENT_BUTTON:
    default:
        (void)zcsr_button_set_text(z, zn, text, (uint32_t)size);
        break;
    }
}

static void apply_node_style(UIM_ZcsrRuntimeBridge* b, uim_NodeId un, ZCSR_NodeId zn,
                             uint32_t uim_type, uim_ComponentInstanceId instance) {
    ZCSR_ComponentId comp = 0;
    uim_Value v;
    ZCSR_Color color;
    (void)un;
    if (map_component_type(uim_type, &comp) != ZCSR_OK) return;

    if (get_prop(b->uim, instance, UIR_PROP_TEXT, &v) &&
        v.type == uim_VALUE_STRING && v.as.string.data) {
        set_text(b->zcsr, zn, comp, v.as.string.data, v.as.string.size);
    }
    if (get_prop(b->uim, instance, UIR_PROP_BACKGROUND, &v) && v.type == uim_VALUE_INT64) {
        color = color_from_u32(v.as.integer);
        (void)zcsr_node_set_color(b->zcsr, zn, ZCSR_COLOR_BG, color);
    }
    if (get_prop(b->uim, instance, UIR_PROP_COLOR, &v) && v.type == uim_VALUE_INT64) {
        color = color_from_u32(v.as.integer);
        if (comp == ZCSR_COMPONENT_LABEL) {
            (void)zcsr_label_set_color(b->zcsr, zn, color);
        } else if (comp == ZCSR_COMPONENT_SEPARATOR) {
            (void)zcsr_separator_set_color(b->zcsr, zn, color);
        } else if (comp == ZCSR_COMPONENT_IMAGE) {
            (void)zcsr_image_set_tint(b->zcsr, zn, color);
        } else {
            (void)zcsr_node_set_color(b->zcsr, zn, ZCSR_COLOR_BG, color);
            (void)zcsr_node_set_color(b->zcsr, zn, ZCSR_COLOR_TEXT, color);
        }
    }
    if (comp == ZCSR_COMPONENT_IMAGE) {
        if (b->resource_resolve &&
            get_prop(b->uim, instance, UIR_PROP_ASSET, &v) &&
            v.type == uim_VALUE_STRING && v.as.string.data != NULL &&
            v.as.string.size > 0u) {
            uint32_t key = b->resource_resolve(b->resource_user, v.as.string.data);
            (void)zcsr_image_set_resource(b->zcsr, zn, key);
        }
    }
    if (comp == ZCSR_COMPONENT_TOGGLE &&
        get_prop(b->uim, instance, UIR_PROP_CHECKED, &v) && v.type == uim_VALUE_BOOL) {
        (void)zcsr_toggle_set_value(b->zcsr, zn, v.as.boolean);
    }
    if (comp == ZCSR_COMPONENT_SLIDER &&
        get_prop(b->uim, instance, UIR_PROP_VALUE, &v) && v.type == uim_VALUE_FLOAT64) {
        (void)zcsr_slider_set_range(b->zcsr, zn, 0.0f, 100.0f);
        (void)zcsr_slider_set_value(b->zcsr, zn, (float)v.as.real);
    }
}

void uim_zcsr_runtime_bridge_create(UIM_ZcsrRuntimeBridge* b, ZCSR_UI* z, UIM_ComponentManager* u) {
    size_t i;
    if (!b) return;
    memset(b, 0, sizeof(*b));
    b->zcsr = z;
    b->uim = u;
    b->view_scale = 1.0f;
    for (i = 0; i < UIM_BRIDGE_MAX_NODES; ++i) {
        b->zcsr_to_uim[i] = uim_NODE_INVALID;
        b->property_for_node[i] = 0u;
    }
}

void uim_zcsr_runtime_bridge_destroy(UIM_ZcsrRuntimeBridge* b) {
    bridge_free_map(b);
}

void uim_zcsr_runtime_bridge_reset(UIM_ZcsrRuntimeBridge* b) {
    size_t i;
    if (!b) return;
    bridge_free_map(b);
    for (i = 0; i < UIM_BRIDGE_MAX_NODES; ++i) {
        b->zcsr_to_uim[i] = uim_NODE_INVALID;
        b->property_for_node[i] = 0u;
    }
    b->nodes_created = 0;
}

void uim_zcsr_runtime_bridge_set_view(UIM_ZcsrRuntimeBridge* b, float scale,
                                      float offset_x, float offset_y) {
    if (!b) return;
    b->view_scale = scale > 0.0f ? scale : 1.0f;
    b->view_offset_x = offset_x;
    b->view_offset_y = offset_y;
}

void uim_zcsr_runtime_bridge_set_resource_resolver(UIM_ZcsrRuntimeBridge* b,
                                                   UIM_ZcsrResourceResolveFn resolve,
                                                   void* user) {
    if (!b) return;
    b->resource_resolve = resolve;
    b->resource_user = user;
}

void uim_zcsr_runtime_bridge_map_node(UIM_ZcsrRuntimeBridge* b, ZCSR_NodeId zn, uim_NodeId un, uim_PropertyId prop) {
    if (!b) return;
    if (zn < UIM_BRIDGE_MAX_NODES) {
        b->zcsr_to_uim[zn] = un;
        b->property_for_node[zn] = prop;
    }
    if (un != uim_NODE_INVALID) {
        bridge_store(b, un, zn);
    }
}

ZCSR_NodeId uim_zcsr_runtime_bridge_find_zcsr(const UIM_ZcsrRuntimeBridge* b,
                                              uim_NodeId un) {
    if (!b || un == uim_NODE_INVALID) return ZCSR_NODE_INVALID;
    return bridge_lookup(b, un);
}

uim_Result uim_zcsr_runtime_bridge_sync_node(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id, ZCSR_NodeId zcsr_parent) {
    return uim_zcsr_runtime_bridge_sync_node_ex(b, uim_node_id, zcsr_parent, false);
}

/* Page/camera basis for 3D billboard projection. The root node's document rect
 * is the camera viewport; this keeps a project's authored resolution stable
 * regardless of the OS window size. */
static void bridge_page_view_size(UIM_ComponentManager* manager, uim_PageId page,
                                  float* out_w, float* out_h) {
    size_t count = 0u;
    size_t i;
    *out_w = 960.0f;
    *out_h = 600.0f;
    if (!manager || page == uim_PAGE_INVALID) return;
    if (uim_page_get_node_count(manager, page, &count) != uim_OK) return;
    for (i = 0u; i < count; ++i) {
        uim_NodeInfo info;
        if (uim_page_get_node_at(manager, page, i, &info) != uim_OK) continue;
        if (info.parent_id != uim_NODE_INVALID && info.parent_id != 0u) continue;
        if (info.local_rect.width > 0.0f && info.local_rect.height > 0.0f) {
            *out_w = info.local_rect.width;
            *out_h = info.local_rect.height;
        }
        return;
    }
}

static bool bridge_project_3d_page_rect(const UIM_ComponentManager* manager, uim_PageId page,
                                        const uim_Transform3* transform,
                                        uim_Rect* out_rect) {
    float page_w = 960.0f, page_h = 600.0f;
    float bx = 0.0f, by = 0.0f, bw = 0.0f, bh = 0.0f;
    zcsr_camera3d camera;
    float world_w, world_h;
    if (out_rect == NULL || transform == NULL) return false;
    world_w = fabsf(transform->scale[0]) * 120.0f;
    world_h = fabsf(transform->scale[1]) * 60.0f;
    bridge_page_view_size((UIM_ComponentManager*)manager, page, &page_w, &page_h);
    zcsr_camera3d_init(&camera, (int)page_w, (int)page_h,
                       0.0f, 0.0f, 600.0f, 0.0f, 0.0f, 60.0f);
    if (world_w < 1.0f) world_w = 1.0f;
    if (world_h < 1.0f) world_h = 1.0f;
    if (!zcsr_camera3d_billboard(&camera, transform->position[0], transform->position[1],
                                 transform->position[2], world_w, world_h,
                                 &bx, &by, &bw, &bh))
        return false;
    *out_rect = (uim_Rect){ bx, by, bw, bh };
    return true;
}

bool uim_zcsr_runtime_bridge_project_node_rect(const UIM_ComponentManager* uim,
                                               const uim_NodeInfo* info,
                                               uim_Rect* out_rect) {
    if (uim == NULL || info == NULL || info->space != uim_SPACE_3D) return false;
    return bridge_project_3d_page_rect(uim, info->page_id, &info->transform, out_rect);
}

uim_Result uim_zcsr_runtime_bridge_sync_node_ex(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id,
                                                ZCSR_NodeId zcsr_parent, bool is_root) {
    uim_NodeInfo info;
    uim_ComponentInstanceId instance = uim_COMPONENT_INSTANCE_INVALID;
    uint32_t uim_type;
    uim_Result rc = uim_node_get(b->uim, uim_node_id, &info);
    ZCSR_NodeId zn = ZCSR_NODE_INVALID;
    if (rc != uim_OK) return rc;

    uim_type = node_component_type(b->uim, uim_node_id, &instance);
    if (uim_type == 0u) uim_type = UIR_C_PANEL;

    if (uim_node_id != uim_NODE_INVALID) {
        zn = bridge_lookup(b, uim_node_id);
    }
    if (zn == ZCSR_NODE_INVALID) {
        ZCSR_ComponentId zcomp;
        ZCSR_Result zmap = map_component_type(uim_type, &zcomp);
        if (zmap != ZCSR_OK) return uim_ERR_UNSUPPORTED;
        if (zcsr_node_create(b->zcsr, zcomp, zcsr_parent, &zn) != ZCSR_OK)
            return uim_ERR_STATE;
        uim_zcsr_runtime_bridge_map_node(b, zn, uim_node_id, 0);
        b->nodes_created++;
    }

    {
        float s = b->view_scale > 0.0f ? b->view_scale : 1.0f;
        float ox = is_root ? b->view_offset_x : 0.0f;
        float oy = is_root ? b->view_offset_y : 0.0f;
        ZCSR_Rect rect;
        bool visible = info.visible;
        if (info.space == uim_SPACE_3D) {
            /* 3D UI nodes are camera-facing billboards, not document-space
             * rectangles. camera3d projects them into the same 2D quad batcher
             * that 2D nodes use; player and editor therefore share this path. */
            uim_Rect page_rect;
            if (bridge_project_3d_page_rect(b->uim, info.page_id, &info.transform,
                                            &page_rect)) {
                rect = (ZCSR_Rect){ page_rect.x * s + ox, page_rect.y * s + oy,
                                    page_rect.width * s, page_rect.height * s };
            } else {
                visible = false;
                rect = (ZCSR_Rect){ 0.0f, 0.0f, 0.0f, 0.0f };
            }
        } else {
            rect = (ZCSR_Rect){ info.local_rect.x * s + ox,
                                info.local_rect.y * s + oy,
                                info.local_rect.width * s,
                                info.local_rect.height * s };
        }
        (void)zcsr_node_set_rect(b->zcsr, zn, &rect);

        {
            uint32_t flags = 0;
            if (visible) flags |= ZCSR_NODE_VISIBLE;
            if (info.enabled) flags |= ZCSR_NODE_ENABLED;
            (void)zcsr_node_set_flags(b->zcsr, zn, flags);
            (void)zcsr_node_set_z_order(b->zcsr, zn, info.z_order);
        }
    }
    apply_node_style(b, uim_node_id, zn, uim_type, instance);

    {
        size_t idx = 0;
        uim_NodeInfo child;
        while (uim_node_get_child(b->uim, uim_node_id, idx, &child) == uim_OK) {
            (void)uim_zcsr_runtime_bridge_sync_node_ex(b, child.id, zn, false);
            idx++;
        }
    }
    return uim_OK;
}

/* Sync entire UIM page → ZCSR UI node tree. Root nodes are the only nodes with
 * parent_id == INVALID; children are reached recursively. */
uim_Result uim_zcsr_runtime_bridge_sync_page(UIM_ZcsrRuntimeBridge* b, uim_PageId page_id) {
    size_t count = 0;
    size_t i;
    uim_Result rc = uim_page_get_node_count(b->uim, page_id, &count);
    if (rc != uim_OK) return rc;
    for (i = 0; i < count; i++) {
        uim_NodeInfo info;
        rc = uim_page_get_node_at(b->uim, page_id, i, &info);
        if (rc != uim_OK) continue;
        if (info.parent_id == uim_NODE_INVALID || info.parent_id == 0u) {
            (void)uim_zcsr_runtime_bridge_sync_node_ex(b, info.id, ZCSR_NODE_INVALID, true);
        }
    }
    return uim_OK;
}

uim_Result uim_zcsr_runtime_bridge_apply_property(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id, uim_PropertyId prop_id, uim_Value* value) {
    ZCSR_NodeId zn;
    ZCSR_ComponentId comp = 0;
    if (!b || !value || uim_node_id == uim_NODE_INVALID) return uim_ERR_NOT_FOUND;
    zn = bridge_lookup(b, uim_node_id);
    if (zn == ZCSR_NODE_INVALID) return uim_ERR_NOT_FOUND;
    (void)zcsr_node_get_component_id(b->zcsr, zn, &comp);

    if (prop_id == UIR_PROP_COLOR && value->type == uim_VALUE_INT64) {
        ZCSR_Color c = color_from_u32(value->as.integer);
        if (comp == ZCSR_COMPONENT_LABEL) (void)zcsr_label_set_color(b->zcsr, zn, c);
        else if (comp == ZCSR_COMPONENT_SEPARATOR) (void)zcsr_separator_set_color(b->zcsr, zn, c);
        else if (comp == ZCSR_COMPONENT_IMAGE) (void)zcsr_image_set_tint(b->zcsr, zn, c);
        else (void)zcsr_node_set_color(b->zcsr, zn, ZCSR_COLOR_BG, c);
        return uim_OK;
    }
    if (prop_id == UIR_PROP_BACKGROUND && value->type == uim_VALUE_INT64) {
        (void)zcsr_node_set_color(b->zcsr, zn, ZCSR_COLOR_BG,
                                  color_from_u32(value->as.integer));
        return uim_OK;
    }

    switch (value->type) {
    case uim_VALUE_BOOL:
        if (comp == ZCSR_COMPONENT_TOGGLE) (void)zcsr_toggle_set_value(b->zcsr, zn, value->as.boolean);
        break;
    case uim_VALUE_FLOAT64:
        if (comp == ZCSR_COMPONENT_SLIDER) (void)zcsr_slider_set_value(b->zcsr, zn, (float)value->as.real);
        break;
    case uim_VALUE_INT64:
        if (comp == ZCSR_COMPONENT_SLIDER) (void)zcsr_slider_set_value(b->zcsr, zn, (float)value->as.integer);
        break;
    case uim_VALUE_STRING:
        if (value->as.string.data) set_text(b->zcsr, zn, comp, value->as.string.data, value->as.string.size);
        break;
    default:
        return uim_ERR_TYPE;
    }
    return uim_OK;
}

void uim_zcsr_runtime_bridge_forward_zcsr_events(UIM_ZcsrRuntimeBridge* b) {
    ZCSR_UIEvent ev;
    while (zcsr_ui_poll_event(b->zcsr, &ev) == ZCSR_OK) {
        uim_NodeId un = uim_NODE_INVALID;
        uim_Value v;
        if (ev.source_node < UIM_BRIDGE_MAX_NODES) {
            un = b->zcsr_to_uim[ev.source_node];
        }
        if (un == uim_NODE_INVALID) continue;

        switch (ev.type) {
        case ZCSR_EVENT_TOGGLE_CHANGED: {
            bool tval = false;
            zcsr_toggle_get_value(b->zcsr, ev.source_node, &tval);
            v = uim_value_bool(tval);
            break;
        }
        case ZCSR_EVENT_SLIDER_CHANGED: {
            float sval = 0.0f;
            zcsr_slider_get_value(b->zcsr, ev.source_node, &sval);
            v = uim_value_float((double)sval);
            break;
        }
        case ZCSR_EVENT_TEXT_CHANGED:
        case ZCSR_EVENT_TEXT_SUBMITTED: {
            char text[512];
            uint32_t bytes = 0;
            text[0] = '\0';
            (void)zcsr_text_input_get_text(b->zcsr, ev.source_node, text,
                                           (uint32_t)sizeof text, &bytes);
            v = uim_value_string(text, (size_t)bytes);
            break;
        }
        case ZCSR_EVENT_BUTTON_CLICKED:
            v = uim_value_int(1);
            uim_user_action(b->uim, un, v);
            b->events_forwarded++;
            continue;
        default:
            continue;
        }
        uim_user_value_changed(b->uim, un, v);
        b->events_forwarded++;
    }
}

void uim_zcsr_runtime_bridge_drain_uim_events(UIM_ZcsrRuntimeBridge* b) {
    uim_Event uev;
    while (uim_event_poll(b->uim, &uev) == uim_OK) {
        b->events_received++;
        switch (uev.type) {
        case uim_EVENT_USER_VALUE_CHANGED:
            printf("  [UIR] node=%llu prop=%llu value_type=%d\n",
                   (unsigned long long)uev.node_id,
                   (unsigned long long)uev.property_id,
                   (int)uev.value.type);
            break;
        case uim_EVENT_ACTION:
            printf("  [UIR] ACTION node=%llu action=%llu\n",
                   (unsigned long long)uev.node_id,
                   (unsigned long long)uev.action_id);
            break;
        default: break;
        }
        uim_event_dispose(&uev);
    }
}
