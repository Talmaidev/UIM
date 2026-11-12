#include "uim_zcsr_runtime_bridge.h"
#include "zcsr/ui_toggle.h"
#include "zcsr/ui_slider.h"
#include "zcsr/ui_button.h"
#include "zcsr/ui_label.h"
#include <stdio.h>
#include <string.h>

/* UIR ComponentId → ZCSR ComponentId 映射表 */
static ZCSR_Result map_component_type(uint32_t uim_type, ZCSR_ComponentId* out)
{
    /* UIR 用的是简单的 1,2,3... 编号，映射到 ZCSR 的组件类型 */
    switch (uim_type) {
    case 1: *out = ZCSR_COMPONENT_BUTTON; return ZCSR_OK;
    case 2: *out = ZCSR_COMPONENT_TOGGLE; return ZCSR_OK;
    case 3: *out = ZCSR_COMPONENT_SLIDER; return ZCSR_OK;
    case 4: *out = ZCSR_COMPONENT_LABEL; return ZCSR_OK;
    case 5: *out = ZCSR_COMPONENT_PANEL; return ZCSR_OK;
    default: return ZCSR_ERR_UNSUPPORTED;
    }
}

void uim_zcsr_runtime_bridge_create(UIM_ZcsrRuntimeBridge* b, ZCSR_UI* z, UIM_ComponentManager* u)
{
    memset(b, 0, sizeof(*b));
    b->zcsr = z;
    b->uim = u;
}

void uim_zcsr_runtime_bridge_map_node(UIM_ZcsrRuntimeBridge* b, ZCSR_NodeId zn, uim_NodeId un, uim_PropertyId prop)
{
    if (zn < UIM_BRIDGE_MAX_NODES) {
        b->zcsr_to_uim[zn] = un;
        b->property_for_node[zn] = prop;
    }
    if (un < UIM_BRIDGE_MAX_NODES) {
        b->uim_to_zcsr[un] = zn;
    }
}

/* 同步单个节点 */
uim_Result uim_zcsr_runtime_bridge_sync_node(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id, ZCSR_NodeId zcsr_parent)
{
    uim_NodeInfo info;
    uim_Result rc = uim_node_get(b->uim, uim_node_id, &info);
    if (rc != uim_OK) return rc;

    /* 检查是否已经创建过 */
    ZCSR_NodeId zn;
    if (uim_node_id < UIM_BRIDGE_MAX_NODES && b->uim_to_zcsr[uim_node_id] != ZCSR_NODE_INVALID) {
        zn = b->uim_to_zcsr[uim_node_id];
    } else {
        /* 创建新的 ZCSR 节点 */
        ZCSR_ComponentId zcomp;
        /* 两个 Result 枚举数值不兼容：ZCSR_ERR_UNSUPPORTED == 6，而 uim_Result 的 6 是
         * uim_ERR_STATE。直接赋值会把"组件类型不支持"错报成"状态错误"，故显式映射。 */
        ZCSR_Result zmap = map_component_type(info.component_id, &zcomp);
        if (zmap != ZCSR_OK) return uim_ERR_UNSUPPORTED;

        ZCSR_Result zrc = zcsr_node_create(b->zcsr, zcomp, zcsr_parent, &zn);
        if (zrc != ZCSR_OK) return uim_ERR_STATE;

        uim_zcsr_runtime_bridge_map_node(b, zn, uim_node_id, 0);
        b->nodes_created++;
    }

    /* 设置矩形 */
    ZCSR_Rect rect = { info.local_rect.x, info.local_rect.y, info.local_rect.width, info.local_rect.height };
    zcsr_node_set_rect(b->zcsr, zn, &rect);

    /* 设置 flags */
    uint32_t flags = 0;
    if (info.visible) flags |= ZCSR_NODE_VISIBLE;
    if (info.enabled) flags |= ZCSR_NODE_ENABLED;
    zcsr_node_set_flags(b->zcsr, zn, flags);

    /* 递归同步子节点 */
    size_t idx = 0;
    uim_NodeInfo child;
    while (uim_node_get_child(b->uim, uim_node_id, idx, &child) == uim_OK) {
        uim_zcsr_runtime_bridge_sync_node(b, child.id, zn);
        idx++;
    }

    return uim_OK;
}

/* 同步整个页面 */
uim_Result uim_zcsr_runtime_bridge_sync_page(UIM_ZcsrRuntimeBridge* b, uim_PageId page_id)
{
    size_t count = 0;
    uim_Result rc = uim_page_get_node_count(b->uim, page_id, &count);
    if (rc != uim_OK) return rc;

    for (size_t i = 0; i < count; i++) {
        uim_NodeInfo info;
        rc = uim_page_get_node_at(b->uim, page_id, i, &info);
        if (rc != uim_OK) continue;
        /* 只同步根节点（parent_id == 0） */
        if (info.parent_id == uim_NODE_INVALID) {
            uim_zcsr_runtime_bridge_sync_node(b, info.id, ZCSR_NODE_INVALID);
        }
    }
    return uim_OK;
}

/* 应用 UIR 属性变更到 ZCSR */
uim_Result uim_zcsr_runtime_bridge_apply_property(UIM_ZcsrRuntimeBridge* b, uim_NodeId uim_node_id, uim_PropertyId prop_id, uim_Value* value)
{
    if (uim_node_id >= UIM_BRIDGE_MAX_NODES) return uim_ERR_NOT_FOUND;
    ZCSR_NodeId zn = b->uim_to_zcsr[uim_node_id];
    if (zn == ZCSR_NODE_INVALID) return uim_ERR_NOT_FOUND;

    /* 根据属性类型和组件类型调用对应的 setter */
    /* 现在先做简单的：根据 value type 和 property id 做基础映射 */
    switch (value->type) {
    case uim_VALUE_BOOL:
        /* Toggle 的开关状态 */
        zcsr_toggle_set_value(b->zcsr, zn, value->as.boolean);
        break;
    case uim_VALUE_FLOAT64:
        /* Slider 的值 */
        zcsr_slider_set_value(b->zcsr, zn, (float)value->as.real);
        break;
    case uim_VALUE_STRING:
        /* Button/Label 的文本 */
        zcsr_button_set_text(b->zcsr, zn, value->as.string.data, (uint32_t)value->as.string.size);
        break;
    default:
        return uim_ERR_TYPE;
    }
    return uim_OK;
}

void uim_zcsr_runtime_bridge_forward_zcsr_events(UIM_ZcsrRuntimeBridge* b)
{
    ZCSR_UIEvent ev;
    while (zcsr_ui_poll_event(b->zcsr, &ev) == ZCSR_OK) {
        uim_NodeId un = b->zcsr_to_uim[ev.source_node];
        if (un == uim_NODE_INVALID) continue;

        uim_Value v;
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

void uim_zcsr_runtime_bridge_drain_uim_events(UIM_ZcsrRuntimeBridge* b)
{
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
