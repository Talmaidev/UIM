/* ui_editor_component_registry.c — 组件类型注册表
 *
 * 职责：
 * - 维护「组件类型元数据」表：type_id → schema / flags / properties
 * - 把「按类型挂载实例」转发到 UIM 的 instance 层（uim_component_instance_attach）
 *
 * 不负责：
 * - 实例存储、节点归属、属性值存储 —— 全在 UIM/uim_component_manager.c
 * - 渲染、布局
 *
 * 生命周期：register 时深拷贝 properties 数组，registry 自持；destroy 全部释放。
 * 类型 desc 里的 stable_key / display_name_key / category_key 仍由调用方持有
 * （它们是字面量或长生命周期字符串）。
 */
#include "uim_component_registry.h"

#include <stdlib.h>
#include <string.h>

#define uim_REGISTRY_DEFAULT_CAPACITY 16u

typedef struct {
    uim_ComponentTypeDesc desc;
    uim_ComponentPropertyDesc *owned_props;
} registry_entry;

struct uim_ComponentRegistry {
    registry_entry *entries;
    size_t count;
    size_t capacity;
};

static bool key_nonempty(const char *s) { return s != NULL && s[0] != '\0'; }

static int find_index(const uim_ComponentRegistry *registry, uim_ComponentTypeId type_id)
{
    size_t i;
    for (i = 0; i < registry->count; ++i) {
        if (registry->entries[i].desc.type_id == type_id) return (int)i;
    }
    return -1;
}

/* ---- 类型表生命周期 ---- */

uim_Result uim_component_registry_create(const uim_ComponentRegistryConfig *config,
                                         uim_ComponentRegistry **out_registry)
{
    uim_ComponentRegistry *registry;
    size_t capacity;

    if (out_registry == NULL) return uim_ERR_ARGUMENT;
    *out_registry = NULL;

    capacity = (config != NULL && config->initial_type_capacity > 0u)
             ? config->initial_type_capacity
             : uim_REGISTRY_DEFAULT_CAPACITY;

    registry = (uim_ComponentRegistry *)calloc(1, sizeof(*registry));
    if (registry == NULL) return uim_ERR_NOMEM;

    registry->entries = (registry_entry *)calloc(capacity, sizeof(*registry->entries));
    if (registry->entries == NULL) {
        free(registry);
        return uim_ERR_NOMEM;
    }
    registry->capacity = capacity;
    registry->count = 0;
    *out_registry = registry;
    return uim_OK;
}

void uim_component_registry_destroy(uim_ComponentRegistry *registry)
{
    size_t i;
    if (registry == NULL) return;
    for (i = 0; i < registry->count; ++i) {
        free(registry->entries[i].owned_props);
    }
    free(registry->entries);
    free(registry);
}

uim_Result uim_component_registry_register(uim_ComponentRegistry *registry,
                                            const uim_ComponentTypeDesc *desc)
{
    registry_entry entry;
    uim_ComponentPropertyDesc *props = NULL;

    if (registry == NULL || desc == NULL) return uim_ERR_ARGUMENT;
    if (desc->type_id == 0u) return uim_ERR_ARGUMENT;
    if (!key_nonempty(desc->stable_key)) return uim_ERR_ARGUMENT;
    if (desc->struct_size != 0u && desc->struct_size < sizeof(uim_ComponentTypeDesc)) {
        return uim_ERR_ARGUMENT;
    }
    if (desc->property_count > 0u && desc->properties == NULL) return uim_ERR_ARGUMENT;
    if (find_index(registry, desc->type_id) >= 0) return uim_ERR_EXISTS;

    if (registry->count == registry->capacity) {
        size_t next = registry->capacity * 2u;
        registry_entry *grown;
        if (next < registry->capacity) return uim_ERR_NOMEM;   /* size_t 溢出 */
        grown = (registry_entry *)realloc(registry->entries, next * sizeof(*grown));
        if (grown == NULL) return uim_ERR_NOMEM;
        registry->entries = grown;
        registry->capacity = next;
    }

    memset(&entry, 0, sizeof(entry));
    if (desc->property_count > 0u) {
        props = (uim_ComponentPropertyDesc *)malloc(
            desc->property_count * sizeof(*props));
        if (props == NULL) return uim_ERR_NOMEM;
        memcpy(props, desc->properties, desc->property_count * sizeof(*props));
    }
    entry.desc = *desc;
    entry.desc.properties = props;      /* 指向自己拷贝的副本 */
    entry.owned_props = props;

    registry->entries[registry->count] = entry;
    registry->count += 1u;
    return uim_OK;
}

uim_Result uim_component_registry_unregister(uim_ComponentRegistry *registry,
                                              uim_ComponentTypeId type_id)
{
    int index;
    size_t tail;

    if (registry == NULL || type_id == 0u) return uim_ERR_ARGUMENT;
    index = find_index(registry, type_id);
    if (index < 0) return uim_ERR_NOT_FOUND;

    free(registry->entries[index].owned_props);
    tail = registry->count - (size_t)index - 1u;
    if (tail > 0u) {
        memmove(&registry->entries[index], &registry->entries[index + 1],
                tail * sizeof(registry->entries[0]));
    }
    registry->count -= 1u;
    memset(&registry->entries[registry->count], 0, sizeof(registry->entries[0]));
    return uim_OK;
}

uim_Result uim_component_registry_get(const uim_ComponentRegistry *registry,
                                      uim_ComponentTypeId type_id,
                                      uim_ComponentTypeDesc *out_desc)
{
    int index;
    if (registry == NULL || out_desc == NULL) return uim_ERR_ARGUMENT;
    index = find_index(registry, type_id);
    if (index < 0) return uim_ERR_NOT_FOUND;
    *out_desc = registry->entries[index].desc;
    if (out_desc->struct_size == 0u) out_desc->struct_size = sizeof(uim_ComponentTypeDesc);
    return uim_OK;
}

uim_Result uim_component_registry_find(const uim_ComponentRegistry *registry,
                                       const char *stable_key,
                                       uim_ComponentTypeId *out_type_id)
{
    size_t i;
    if (registry == NULL || stable_key == NULL || out_type_id == NULL) return uim_ERR_ARGUMENT;
    for (i = 0; i < registry->count; ++i) {
        const char *key = registry->entries[i].desc.stable_key;
        if (key != NULL && strcmp(key, stable_key) == 0) {
            *out_type_id = registry->entries[i].desc.type_id;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

size_t uim_component_registry_count(const uim_ComponentRegistry *registry)
{
    return (registry != NULL) ? registry->count : 0u;
}

uim_Result uim_component_registry_get_at(const uim_ComponentRegistry *registry,
                                         size_t index,
                                         uim_ComponentTypeDesc *out_desc)
{
    if (registry == NULL || out_desc == NULL) return uim_ERR_ARGUMENT;
    if (index >= registry->count) return uim_ERR_RANGE;
    *out_desc = registry->entries[index].desc;
    if (out_desc->struct_size == 0u) out_desc->struct_size = sizeof(uim_ComponentTypeDesc);
    return uim_OK;
}

uim_Result uim_component_registry_get_property(const uim_ComponentRegistry *registry,
                                               uim_ComponentTypeId type_id,
                                               uim_PropertyId property_id,
                                               uim_ComponentPropertyDesc *out_property)
{
    int index;
    size_t i;
    const uim_ComponentTypeDesc *desc;

    if (registry == NULL || out_property == NULL) return uim_ERR_ARGUMENT;
    index = find_index(registry, type_id);
    if (index < 0) return uim_ERR_NOT_FOUND;

    desc = &registry->entries[index].desc;
    for (i = 0; i < desc->property_count; ++i) {
        if (desc->properties[i].property_id == property_id) {
            *out_property = desc->properties[i];
            if (out_property->struct_size == 0u) {
                out_property->struct_size = sizeof(uim_ComponentPropertyDesc);
            }
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

/* ---- 转发到 UIM 实例层 ---- */

uim_Result uim_component_instance_attach_registered(
    UIM_ComponentManager *manager, const uim_ComponentRegistry *registry, uim_NodeId node_id,
    uim_ComponentTypeId type_id, uim_ComponentInstanceId *out_instance_id)
{
    uim_ComponentTypeDesc desc;
    uim_Result rc;

    if (manager == NULL || registry == NULL || out_instance_id == NULL) return uim_ERR_ARGUMENT;
    if (node_id == 0u) return uim_ERR_ARGUMENT;

    rc = uim_component_registry_get(registry, type_id, &desc);
    if (rc != uim_OK) return rc;

    /* SINGLETON_PER_NODE：同节点同类型只允许一个实例，先查再挂 */
    if ((desc.flags & uim_COMPONENT_FLAG_SINGLETON_PER_NODE) != 0u) {
        size_t count = 0u;
        size_t i;
        uim_ComponentInstanceInfo info;
        if (uim_node_component_count(manager, node_id, &count) == uim_OK) {
            for (i = 0; i < count; ++i) {
                if (uim_node_component_get_at(manager, node_id, i, &info) == uim_OK &&
                    info.type_id == type_id) {
                    return uim_ERR_EXISTS;
                }
            }
        }
    }

    rc = uim_component_instance_attach(manager, node_id, type_id,
                                       desc.schema_version != 0u ? desc.schema_version : 1u,
                                       out_instance_id);
    if (rc != uim_OK) return rc;

    /* New instances start from the registry defaults, not from an empty property set.
       Without this an added Motion/Health/Rule shows no editable fields in the inspector. */
    for (size_t i = 0; i < desc.property_count; ++i) {
        uim_Result prc = uim_component_instance_set_property(
            manager, *out_instance_id, desc.properties[i].property_id,
            desc.properties[i].default_value);
        if (prc != uim_OK) {
            (void)uim_component_instance_detach(manager, *out_instance_id);
            *out_instance_id = 0u;
            return prc;
        }
    }
    return uim_OK;
}

uim_Result uim_component_instance_set_property_checked(
    UIM_ComponentManager *manager, const uim_ComponentRegistry *registry,
    uim_ComponentInstanceId instance_id, uim_PropertyId property_id, uim_Value value)
{
    uim_ComponentInstanceInfo info;
    uim_ComponentPropertyDesc prop;
    uim_Result rc;

    if (manager == NULL || registry == NULL) return uim_ERR_ARGUMENT;

    rc = uim_component_instance_get(manager, instance_id, &info);
    if (rc != uim_OK) return rc;

    rc = uim_component_registry_get_property(registry, info.type_id, property_id, &prop);
    if (rc != uim_OK) return rc;   /* 类型没声明这个属性：拒绝写入 */

    if ((prop.flags & uim_PROPERTY_FLAG_READ_ONLY) != 0u) return uim_ERR_STATE;
    if (prop.value_type != uim_VALUE_NONE && prop.value_type != value.type) return uim_ERR_TYPE;

    return uim_component_instance_set_property(manager, instance_id, property_id, value);
}

#define PROP_INT(id, key, name, value, flags, hint, lo, hi, step) \
    { (uint32_t)sizeof(uim_ComponentPropertyDesc), (id), uim_VALUE_INT64, (flags), (hint), \
      (key), (name), { uim_VALUE_INT64, { .integer = (value) } }, (lo), (hi), (step), 0u }
#define PROP_FLOAT(id, key, name, value, flags, hint, lo, hi, step) \
    { (uint32_t)sizeof(uim_ComponentPropertyDesc), (id), uim_VALUE_FLOAT64, (flags), (hint), \
      (key), (name), { uim_VALUE_FLOAT64, { .real = (value) } }, (lo), (hi), (step), 0u }
#define PROP_BOOL(id, key, name, value, flags, hint) \
    { (uint32_t)sizeof(uim_ComponentPropertyDesc), (id), uim_VALUE_BOOL, (flags), (hint), \
      (key), (name), { uim_VALUE_BOOL, { .boolean = (value) } }, 0.0, 1.0, 1.0, 0u }
#define PROP_STR(id, key, name, text, flags, hint, maxb) \
    { (uint32_t)sizeof(uim_ComponentPropertyDesc), (id), uim_VALUE_STRING, (flags), (hint), \
      (key), (name), { uim_VALUE_STRING, { .string = { (text), sizeof(text) - 1u } } }, \
      0.0, 0.0, 0.0, (maxb) }

static const uim_ComponentPropertyDesc k_panel_props[] = {
    PROP_INT(uim_PROPERTY_BACKGROUND, "background", "Background", 0x2b3440ff, 0u,
             uim_INSPECTOR_COLOR, 0.0, 4294967295.0, 1.0)
};
static const uim_ComponentPropertyDesc k_button_props[] = {
    PROP_STR(uim_PROPERTY_TEXT, "text", "Text", "Button", 0u, uim_INSPECTOR_TEXT, 1024u),
    PROP_INT(uim_PROPERTY_COLOR, "color", "Color", 0xffffffff, 0u,
             uim_INSPECTOR_COLOR, 0.0, 4294967295.0, 1.0)
};
static const uim_ComponentPropertyDesc k_toggle_props[] = {
    PROP_STR(uim_PROPERTY_TEXT, "text", "Text", "Toggle", 0u, uim_INSPECTOR_TEXT, 1024u),
    PROP_BOOL(uim_PROPERTY_CHECKED, "checked", "Checked", false, 0u, uim_INSPECTOR_CHECKBOX)
};
static const uim_ComponentPropertyDesc k_slider_props[] = {
    PROP_FLOAT(uim_PROPERTY_VALUE, "value", "Value", 50.0, 0u, uim_INSPECTOR_SLIDER, 0.0, 100.0, 0.1)
};
static const uim_ComponentPropertyDesc k_dropdown_props[] = {
    PROP_STR(uim_PROPERTY_TEXT, "text", "Text", "Option", 0u, uim_INSPECTOR_TEXT, 1024u)
};
static const uim_ComponentPropertyDesc k_label_props[] = {
    PROP_STR(uim_PROPERTY_TEXT, "text", "Text", "Label", 0u, uim_INSPECTOR_TEXT, 4096u),
    PROP_INT(uim_PROPERTY_COLOR, "color", "Color", 0xffffffff, 0u,
             uim_INSPECTOR_COLOR, 0.0, 4294967295.0, 1.0),
    PROP_FLOAT(uim_PROPERTY_FONT_SIZE, "font_size", "Font Size", 14.0, 0u,
               uim_INSPECTOR_NUMBER, 6.0, 96.0, 1.0)
};
static const uim_ComponentPropertyDesc k_separator_props[] = {
    PROP_INT(uim_PROPERTY_COLOR, "color", "Color", 0x808080ff, 0u,
             uim_INSPECTOR_COLOR, 0.0, 4294967295.0, 1.0)
};
static const uim_ComponentPropertyDesc k_image_props[] = {
    PROP_STR(uim_PROPERTY_ASSET, "asset", "Asset", "", 0u, uim_INSPECTOR_ASSET_PICKER, 2048u),
    PROP_INT(uim_PROPERTY_COLOR, "color", "Tint", 0xffffffff, 0u,
             uim_INSPECTOR_COLOR, 0.0, 4294967295.0, 1.0)
};
static const uim_ComponentPropertyDesc k_input_props[] = {
    PROP_STR(uim_PROPERTY_TEXT, "text", "Text", "Input", 0u, uim_INSPECTOR_TEXT, 4096u)
};
static const uim_ComponentPropertyDesc k_scroll_props[] = {
    PROP_INT(uim_PROPERTY_BACKGROUND, "background", "Background", 0x1e242cff, 0u,
             uim_INSPECTOR_COLOR, 0.0, 4294967295.0, 1.0)
};
/* 行为组件：运行时每帧把 vx/vy 积分到节点矩形上（不写盘，退出恢复）。 */
static const uim_ComponentPropertyDesc k_motion_props[] = {
    PROP_FLOAT(uim_PROPERTY_VX, "vx", "VX", 0.0, 0u, uim_INSPECTOR_NUMBER,
               -4000.0, 4000.0, 1.0),
    PROP_FLOAT(uim_PROPERTY_VY, "vy", "VY", 0.0, 0u, uim_INSPECTOR_NUMBER,
               -4000.0, 4000.0, 1.0)
};
/* Health：回合制/动作游戏最小生命值。hp<=0 时运行时把节点隐藏。 */
static const uim_ComponentPropertyDesc k_health_props[] = {
    PROP_INT(uim_PROPERTY_HP, "hp", "HP", 10, 0u, uim_INSPECTOR_NUMBER,
             0.0, 100000.0, 1.0)
};
/* Rule：when / where / do 三段纯数据。
 * trigger 0=click 1=tick；selector 0=same_row 1=same_col 2=radius 3=nearest 4=tag；
 * effect 0=damage 1=move 2=destroy 3=goto。
 * move 在 click 触发时是瞬间位移(px)，tick 触发时是速度(px/s)。 */
static const uim_ComponentPropertyDesc k_rule_props[] = {
    PROP_INT(uim_PROPERTY_TRIGGER, "trigger", "Trigger", 0, 0u, uim_INSPECTOR_NUMBER,
             0.0, 1.0, 1.0),
    PROP_INT(uim_PROPERTY_SELECTOR, "selector", "Selector", 0, 0u, uim_INSPECTOR_NUMBER,
             0.0, 4.0, 1.0),
    PROP_INT(uim_PROPERTY_EFFECT, "effect", "Effect", 0, 0u, uim_INSPECTOR_NUMBER,
             0.0, 3.0, 1.0),
    PROP_INT(uim_PROPERTY_AMOUNT, "amount", "Amount", 1, 0u, uim_INSPECTOR_NUMBER,
             -100000.0, 100000.0, 1.0),
    PROP_FLOAT(uim_PROPERTY_RADIUS, "radius", "Radius", 200.0, 0u, uim_INSPECTOR_NUMBER,
               0.0, 100000.0, 1.0),
    PROP_STR(uim_PROPERTY_TAG, "tag", "Tag", "enemy", 0u, uim_INSPECTOR_TEXT, 128u),
    PROP_INT(uim_PROPERTY_TARGET_PAGE, "target_page", "Target Page", 0, 0u,
             uim_INSPECTOR_NUMBER, 0.0, 1000.0, 1.0)
};

#define COMPONENT(id, key, name, category, flags, props) \
    { (uint32_t)sizeof(uim_ComponentTypeDesc), (id), 1u, (flags), (key), (name), (category), \
      (props), sizeof(props) / sizeof((props)[0]) }

static const uim_ComponentTypeDesc k_builtin_descs[] = {
    COMPONENT(uim_COMPONENT_PANEL, "builtin.panel", "Panel", "Layout",
              uim_COMPONENT_FLAG_CONTAINER | uim_COMPONENT_FLAG_VISUAL, k_panel_props),
    COMPONENT(uim_COMPONENT_BUTTON, "builtin.button", "Button", "Input",
              uim_COMPONENT_FLAG_VISUAL | uim_COMPONENT_FLAG_INTERACTIVE, k_button_props),
    COMPONENT(uim_COMPONENT_TOGGLE, "builtin.toggle", "Toggle", "Input",
              uim_COMPONENT_FLAG_VISUAL | uim_COMPONENT_FLAG_INTERACTIVE, k_toggle_props),
    COMPONENT(uim_COMPONENT_SLIDER, "builtin.slider", "Slider", "Input",
              uim_COMPONENT_FLAG_VISUAL | uim_COMPONENT_FLAG_INTERACTIVE, k_slider_props),
    COMPONENT(uim_COMPONENT_DROPDOWN, "builtin.dropdown", "Dropdown", "Input",
              uim_COMPONENT_FLAG_VISUAL | uim_COMPONENT_FLAG_INTERACTIVE, k_dropdown_props),
    COMPONENT(uim_COMPONENT_LABEL, "builtin.label", "Label", "Text",
              uim_COMPONENT_FLAG_VISUAL, k_label_props),
    COMPONENT(uim_COMPONENT_SEPARATOR, "builtin.separator", "Separator", "Layout",
              uim_COMPONENT_FLAG_VISUAL, k_separator_props),
    COMPONENT(uim_COMPONENT_IMAGE, "builtin.image", "Image", "Media",
              uim_COMPONENT_FLAG_VISUAL, k_image_props),
    COMPONENT(uim_COMPONENT_TEXT_INPUT, "builtin.text_input", "Text Input", "Input",
              uim_COMPONENT_FLAG_VISUAL | uim_COMPONENT_FLAG_INTERACTIVE, k_input_props),
    COMPONENT(uim_COMPONENT_SCROLL_VIEW, "builtin.scroll_view", "Scroll View", "Layout",
              uim_COMPONENT_FLAG_CONTAINER | uim_COMPONENT_FLAG_VISUAL, k_scroll_props),
    COMPONENT(uim_COMPONENT_MOTION, "builtin.motion", "Motion", "Behavior",
              uim_COMPONENT_FLAG_SINGLETON_PER_NODE, k_motion_props),
    COMPONENT(uim_COMPONENT_HEALTH, "builtin.health", "Health", "Behavior",
              uim_COMPONENT_FLAG_SINGLETON_PER_NODE, k_health_props),
    COMPONENT(uim_COMPONENT_RULE, "builtin.rule", "Rule", "Behavior",
              uim_COMPONENT_FLAG_SINGLETON_PER_NODE, k_rule_props)
};

uim_Result uim_component_registry_register_builtins(uim_ComponentRegistry *registry) {
    size_t i;
    if (!registry) return uim_ERR_ARGUMENT;
    for (i = 0; i < sizeof(k_builtin_descs) / sizeof(k_builtin_descs[0]); ++i) {
        uim_ComponentTypeDesc existing;
        uim_Result rc = uim_component_registry_get(registry, k_builtin_descs[i].type_id,
                                                   &existing);
        if (rc == uim_OK) continue;
        if (rc != uim_ERR_NOT_FOUND) return rc;
        rc = uim_component_registry_register(registry, &k_builtin_descs[i]);
        if (rc != uim_OK) return rc;
    }
    return uim_OK;
}

const uim_ComponentTypeDesc *uim_component_registry_builtin_desc(uim_ComponentTypeId type_id) {
    size_t i;
    for (i = 0; i < sizeof(k_builtin_descs) / sizeof(k_builtin_descs[0]); ++i) {
        if (k_builtin_descs[i].type_id == type_id) return &k_builtin_descs[i];
    }
    return NULL;
}
