#ifndef UIM_COMPONENT_REGISTRY_H
#define UIM_COMPONENT_REGISTRY_H

/*
 * UIM Component Descriptor ABI v1.
 * Component type identity and property metadata live in UIM, not the editor.
 * Engine-specific runtime IDs/tokens are separate; bridge maps them explicitly.
 */
#include "uim_component_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define uim_COMPONENT_REGISTRY_ABI_VERSION 1u

/* Reserved Editor primitive IDs. These are NOT ZCSR Engine component IDs. */
enum {
    uim_COMPONENT_PANEL       = 0x00010001u,
    uim_COMPONENT_BUTTON      = 0x00010002u,
    uim_COMPONENT_TOGGLE      = 0x00010003u,
    uim_COMPONENT_SLIDER      = 0x00010004u,
    uim_COMPONENT_DROPDOWN    = 0x00010005u,
    uim_COMPONENT_LABEL       = 0x00010006u,
    uim_COMPONENT_SEPARATOR   = 0x00010007u,
    uim_COMPONENT_IMAGE       = 0x00010008u,
    uim_COMPONENT_TEXT_INPUT  = 0x00010009u,
    uim_COMPONENT_SCROLL_VIEW = 0x0001000Au,
    uim_COMPONENT_MOTION      = 0x0001000Bu,
    uim_COMPONENT_HEALTH      = 0x0001000Cu,
    uim_COMPONENT_RULE        = 0x0001000Du,
    uim_COMPONENT_GAME_DPI    = 0x0001000Eu
};

enum {
    uim_COMPONENT_FLAG_CONTAINER = 1u << 0,
    uim_COMPONENT_FLAG_VISUAL    = 1u << 1,
    uim_COMPONENT_FLAG_INTERACTIVE = 1u << 2,
    uim_COMPONENT_FLAG_SINGLETON_PER_NODE = 1u << 3
};

enum {
    uim_PROPERTY_FLAG_READ_ONLY = 1u << 0,
    uim_PROPERTY_FLAG_REQUIRED  = 1u << 1,
    uim_PROPERTY_FLAG_HIDDEN_IN_INSPECTOR = 1u << 2,
    uim_PROPERTY_FLAG_ASSET_REFERENCE = 1u << 3,
    uim_PROPERTY_FLAG_LOCALIZABLE = 1u << 4
};

typedef enum uim_InspectorControlHint {
    uim_INSPECTOR_AUTO = 0,
    uim_INSPECTOR_TEXT,
    uim_INSPECTOR_MULTILINE_TEXT,
    uim_INSPECTOR_CHECKBOX,
    uim_INSPECTOR_NUMBER,
    uim_INSPECTOR_SLIDER,
    uim_INSPECTOR_DROPDOWN,
    uim_INSPECTOR_COLOR,
    uim_INSPECTOR_ASSET_PICKER,
    uim_INSPECTOR_VECTOR2,
    uim_INSPECTOR_VECTOR3,
    uim_INSPECTOR_READ_ONLY
} uim_InspectorControlHint;

typedef struct uim_ComponentPropertyDesc {
    uint32_t struct_size;
    uim_PropertyId property_id;
    uim_ValueType value_type;
    uint32_t flags;
    uim_InspectorControlHint inspector_control;
    const char *stable_key;       /* serialization key; ASCII recommended */
    const char *display_name_key; /* localization key */
    uim_Value default_value;
    double numeric_min;
    double numeric_max;
    double numeric_step;
    uint32_t max_string_bytes;    /* 0 means no descriptor-level limit */
} uim_ComponentPropertyDesc;

typedef struct uim_ComponentTypeDesc {
    uint32_t struct_size;
    uim_ComponentTypeId type_id;
    uint32_t schema_version;
    uint32_t flags;
    const char *stable_key;       /* e.g. "zcsr.ui.button" */
    const char *display_name_key;
    const char *category_key;
    const uim_ComponentPropertyDesc *properties;
    size_t property_count;
} uim_ComponentTypeDesc;

typedef struct uim_ComponentRegistry uim_ComponentRegistry;
typedef struct uim_ComponentRegistryConfig {
    size_t initial_type_capacity;
} uim_ComponentRegistryConfig;

/* uim_ComponentInstanceInfo 由 uim_component_manager.h 定义（同构）；
 * 这里不再重复声明，否则两个头一起 include 会 struct redefinition。 */

/* Descriptor contract:
 * - type_id and property_id are stable within the Editor ABI namespace.
 * - stable_key is the persistence/migration identity; display strings are not.
 * - registry implementations must copy descriptor strings/property arrays or
 *   clearly document registration-time lifetime. v1 Core implementation will
 *   deep-copy them; callers may release temporary descriptors after register.
 * - schema_version is incremented only when serialized meaning changes.
 * - Engine runtime component IDs must be mapped by Engine Bridge; never cast
 *   uim_ComponentTypeId to a ZCSR component enum or pointer.
 */

/* Registry v1. Registration deep-copies strings, property descriptors and
 * default string values. The pointers returned by get/get_at remain borrowed
 * until that type is unregistered or the registry is destroyed. */
uim_Result uim_component_registry_create(const uim_ComponentRegistryConfig *config,
                                         uim_ComponentRegistry **out_registry);
void uim_component_registry_destroy(uim_ComponentRegistry *registry);
uim_Result uim_component_registry_register(uim_ComponentRegistry *registry,
                                            const uim_ComponentTypeDesc *desc);
uim_Result uim_component_registry_unregister(uim_ComponentRegistry *registry,
                                              uim_ComponentTypeId type_id);
uim_Result uim_component_registry_get(const uim_ComponentRegistry *registry,
                                      uim_ComponentTypeId type_id,
                                      uim_ComponentTypeDesc *out_desc);
uim_Result uim_component_registry_find(const uim_ComponentRegistry *registry,
                                       const char *stable_key,
                                       uim_ComponentTypeId *out_type_id);
size_t uim_component_registry_count(const uim_ComponentRegistry *registry);
uim_Result uim_component_registry_get_at(const uim_ComponentRegistry *registry,
                                         size_t index,
                                         uim_ComponentTypeDesc *out_desc);
uim_Result uim_component_registry_get_property(const uim_ComponentRegistry *registry,
                                               uim_ComponentTypeId type_id,
                                               uim_PropertyId property_id,
                                               uim_ComponentPropertyDesc *out_property);

enum {
    uim_PROPERTY_TEXT        = 1u,
    uim_PROPERTY_COLOR       = 2u,
    uim_PROPERTY_BACKGROUND  = 3u,
    uim_PROPERTY_VALUE       = 4u,
    uim_PROPERTY_CHECKED     = 5u,
    uim_PROPERTY_ASSET       = 6u,
    uim_PROPERTY_FONT_SIZE   = 7u,
    uim_PROPERTY_VX          = 8u,
    uim_PROPERTY_VY          = 9u,
    uim_PROPERTY_HP          = 10u,
    uim_PROPERTY_TRIGGER     = 11u,
    uim_PROPERTY_SELECTOR    = 12u,
    uim_PROPERTY_EFFECT      = 13u,
    uim_PROPERTY_AMOUNT      = 14u,
    uim_PROPERTY_RADIUS      = 15u,
    uim_PROPERTY_TAG         = 16u,
    uim_PROPERTY_TARGET_PAGE = 17u,
    uim_PROPERTY_MULTIPLIER  = 18u
};

/* Rule 数据（M3）：trigger / selector / effect 都由整数枚举承载，
 * 序列化是数字，Inspector 里就是数字单元格 —— 先保证"改数即改行为"，
 * 下拉标签放到 M4 的属性 schema 表里统一做。 */
enum {
    uim_RULE_TRIGGER_CLICK = 0,
    uim_RULE_TRIGGER_TICK  = 1
};
enum {
    uim_RULE_SELECT_SAME_ROW = 0,
    uim_RULE_SELECT_SAME_COL = 1,
    uim_RULE_SELECT_RADIUS   = 2,
    uim_RULE_SELECT_NEAREST  = 3,
    uim_RULE_SELECT_TAG      = 4
};
enum {
    uim_RULE_EFFECT_DAMAGE  = 0,
    uim_RULE_EFFECT_MOVE    = 1,
    uim_RULE_EFFECT_DESTROY = 2,
    uim_RULE_EFFECT_GOTO    = 3
};
/* "同行/同列" 的判定容差（页面坐标像素）：中心线相差不超过该值即视为同一行/列。
 * 用容差而不是绝对相等，是因为拖动产生的浮点误差会让"看起来同一行"的两个方块
 * y 相差零点几像素。 */
#define uim_RULE_ALIGN_TOLERANCE 8.0

/* Register the fourteen builtin component descriptors below (10 visual
 * primitives + Motion/Health/Rule + GameDPI). Idempotent: an already-registered
 * type is left untouched. */
uim_Result uim_component_registry_register_builtins(uim_ComponentRegistry *registry);
const uim_ComponentTypeDesc *uim_component_registry_builtin_desc(uim_ComponentTypeId type_id);

/* Receiver-backed component instances. Nodes can own multiple components.
 * The legacy uim_NodeDesc.component_id/config fields remain available as a
 * single legacy component slot and are not reinterpreted by this API.
 * Component property values are borrowed on read; string bytes remain valid
 * until the same property/instance is mutated or the Receiver is destroyed. */
uim_Result uim_component_instance_attach(UIM_ComponentManager *receiver, uim_NodeId node_id,
                                         uim_ComponentTypeId type_id, uint32_t schema_version,
                                         uim_ComponentInstanceId *out_instance_id);
uim_Result uim_component_instance_attach_registered(
    UIM_ComponentManager *receiver, const uim_ComponentRegistry *registry, uim_NodeId node_id,
    uim_ComponentTypeId type_id, uim_ComponentInstanceId *out_instance_id);
/* Restore preserves a serialized instance identity; IDs are never remapped. */
uim_Result uim_component_instance_restore(UIM_ComponentManager *receiver,
                                          const uim_ComponentInstanceInfo *info);
uim_Result uim_component_instance_detach(UIM_ComponentManager *receiver,
                                         uim_ComponentInstanceId instance_id);
uim_Result uim_component_instance_get(const UIM_ComponentManager *receiver,
                                      uim_ComponentInstanceId instance_id,
                                      uim_ComponentInstanceInfo *out_info);
uim_Result uim_node_component_count(const UIM_ComponentManager *receiver, uim_NodeId node_id,
                                    size_t *out_count);
uim_Result uim_node_component_get_at(const UIM_ComponentManager *receiver, uim_NodeId node_id,
                                     size_t index, uim_ComponentInstanceInfo *out_info);
uim_Result uim_component_instance_set_property(UIM_ComponentManager *receiver,
                                               uim_ComponentInstanceId instance_id,
                                               uim_PropertyId property_id, uim_Value value);
/* Registry-validated setter for Inspector/Editor writes. Enforces value type,
 * read-only flag, numeric range and maximum UTF-8 byte length. */
uim_Result uim_component_instance_set_property_checked(
    UIM_ComponentManager *receiver, const uim_ComponentRegistry *registry,
    uim_ComponentInstanceId instance_id, uim_PropertyId property_id, uim_Value value);
uim_Result uim_component_instance_get_property(const UIM_ComponentManager *receiver,
                                               uim_ComponentInstanceId instance_id,
                                               uim_PropertyId property_id,
                                               uim_Value *out_value);

#ifdef __cplusplus
}
#endif
#endif /* UIM_COMPONENT_REGISTRY_H */
