#include "uim_component_runtime.h"
#include "uim_component_registry.h"



#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct RuntimePropSnapshot {
    uim_PropertyId id;
    uim_Value value; /* deep-owned copy */
} RuntimePropSnapshot;

typedef struct RuntimeComponentSnapshot {
    uim_ComponentInstanceInfo info;
    size_t property_count;
    RuntimePropSnapshot *properties;
} RuntimeComponentSnapshot;

typedef struct PageNodeSnapshot {
    uim_NodeId id;
    uim_NodeId parent_id;
    uim_Rect rect;
    uim_LayoutConstraints layout;
    bool visible;
    bool enabled;
    int32_t z_order;
    uim_ActionId action_id;
    uint32_t child_order;
    uim_Space space;
    uim_Transform3 transform;
    void *component_config;
    size_t component_config_size;
    size_t component_count;
    RuntimeComponentSnapshot *components;
} PageNodeSnapshot;

typedef struct {
    int64_t trigger;
    int64_t selector;
    int64_t effect;
    int64_t amount;
    double radius;
    char tag[64];
    int64_t target_page;
} PageRule;

static PageNodeSnapshot *g_snapshots = NULL;
static size_t g_snapshot_count = 0u;
static UIM_ComponentManager *g_snapshot_manager = NULL;
static bool g_paused = false;
/* 只有 begin() 建立、且 manager/page 匹配的会话才会被 tick 推进。
 * 这样“退出播放后残留的 tick 调用”或“切页后的旧 tick”不会偷偷改文档。 */
static bool g_active = false;
static UIM_ComponentManager *g_active_manager = NULL;
static uim_PageId g_active_page = uim_PAGE_INVALID;
/* goto 效果不能由 runtime 直接切页（切页是宿主的事），
 * 这里只登记一次请求，宿主每帧取走。1-based，0 表示没有请求。 */
static int g_goto_request = 0;

#define uim_runtime_MAX_NODES 100000u
#define uim_runtime_MAX_TARGETS 256u

static double page_clamp_dt(double dt) {
    if (dt < 0.0) dt = 0.0;
    if (dt > 0.05) dt = 0.05;
    return dt;
}

/* ---------- 属性读取 ---------- */

static bool page_prop_int(UIM_ComponentManager *manager, uim_ComponentInstanceId instance,
                          uim_PropertyId property, int64_t *out) {
    uim_Value value;
    if (instance == uim_COMPONENT_INSTANCE_INVALID) return false;
    if (uim_component_instance_get_property(manager, instance, property, &value) != uim_OK)
        return false;
    if (value.type == uim_VALUE_INT64) { *out = value.as.integer; return true; }
    if (value.type == uim_VALUE_FLOAT64) { *out = (int64_t)value.as.real; return true; }
    if (value.type == uim_VALUE_BOOL) { *out = value.as.boolean ? 1 : 0; return true; }
    return false;
}

static bool page_prop_float(UIM_ComponentManager *manager, uim_ComponentInstanceId instance,
                            uim_PropertyId property, double *out) {
    uim_Value value;
    if (instance == uim_COMPONENT_INSTANCE_INVALID) return false;
    if (uim_component_instance_get_property(manager, instance, property, &value) != uim_OK)
        return false;
    if (value.type == uim_VALUE_FLOAT64) { *out = value.as.real; return true; }
    if (value.type == uim_VALUE_INT64) { *out = (double)value.as.integer; return true; }
    return false;
}

static void page_prop_string(UIM_ComponentManager *manager, uim_ComponentInstanceId instance,
                             uim_PropertyId property, char *out, size_t cap) {
    uim_Value value;
    size_t n;
    if (!out || cap == 0u) return;
    out[0] = '\0';
    if (instance == uim_COMPONENT_INSTANCE_INVALID) return;
    if (uim_component_instance_get_property(manager, instance, property, &value) != uim_OK)
        return;
    if (value.type != uim_VALUE_STRING || !value.as.string.data) return;
    n = value.as.string.size;
    if (n >= cap) n = cap - 1u;
    memcpy(out, value.as.string.data, n);
    out[n] = '\0';
}

static uim_ComponentInstanceId page_find_component(UIM_ComponentManager *manager,
                                                  uim_NodeId node_id,
                                                  uim_ComponentTypeId type_id) {
    size_t count = 0u;
    size_t i;
    if (uim_node_component_count(manager, node_id, &count) != uim_OK) return 0u;
    for (i = 0; i < count; ++i) {
        uim_ComponentInstanceInfo info;
        if (uim_node_component_get_at(manager, node_id, i, &info) != uim_OK) continue;
        if (info.type_id == type_id) return info.instance_id;
    }
    return 0u;
}

static bool page_node_rule(UIM_ComponentManager *manager, uim_NodeId node_id, PageRule *out) {
    uim_ComponentInstanceId instance;
    int64_t v;
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->amount = 1;
    out->radius = 200.0;
    strcpy(out->tag, "enemy");
    instance = page_find_component(manager, node_id, uim_COMPONENT_RULE);
    if (instance == 0u) return false;
    if (page_prop_int(manager, instance, uim_PROPERTY_TRIGGER, &v)) out->trigger = v;
    if (page_prop_int(manager, instance, uim_PROPERTY_SELECTOR, &v)) out->selector = v;
    if (page_prop_int(manager, instance, uim_PROPERTY_EFFECT, &v)) out->effect = v;
    if (page_prop_int(manager, instance, uim_PROPERTY_AMOUNT, &v)) out->amount = v;
    if (page_prop_float(manager, instance, uim_PROPERTY_RADIUS, &out->radius)) { }
    if (page_prop_int(manager, instance, uim_PROPERTY_TARGET_PAGE, &v)) out->target_page = v;
    page_prop_string(manager, instance, uim_PROPERTY_TAG, out->tag, sizeof(out->tag));
    return true;
}

/* ---------- 几何 ---------- */

static void page_center(const uim_Rect *r, double *cx, double *cy) {
    *cx = (double)r->x + (double)r->width * 0.5;
    *cy = (double)r->y + (double)r->height * 0.5;
}

/* ---------- 快照 ---------- */

static void runtime_value_free(uim_Value *value) {
    if (value == NULL) return;
    if (value->type == uim_VALUE_STRING) free((void *)value->as.string.data);
    memset(value, 0, sizeof(*value));
}

static uim_Result runtime_value_copy(uim_Value *dst, uim_Value src) {
    memset(dst, 0, sizeof(*dst));
    *dst = src;
    if (src.type == uim_VALUE_STRING) {
        char *copy;
        if (src.as.string.data == NULL || src.as.string.size == 0u) {
            dst->as.string.data = NULL;
            dst->as.string.size = 0u;
            return uim_OK;
        }
        copy = (char *)malloc(src.as.string.size);
        if (copy == NULL) return uim_ERR_NOMEM;
        memcpy(copy, src.as.string.data, src.as.string.size);
        dst->as.string.data = copy;
        dst->as.string.size = src.as.string.size;
    }
    return uim_OK;
}

static void node_snapshot_free(PageNodeSnapshot *snap) {
    size_t i, j;
    if (snap == NULL) return;
    free(snap->component_config);
    snap->component_config = NULL;
    for (i = 0; i < snap->component_count; ++i) {
        for (j = 0; j < snap->components[i].property_count; ++j)
            runtime_value_free(&snap->components[i].properties[j].value);
        free(snap->components[i].properties);
    }
    free(snap->components);
    memset(snap, 0, sizeof(*snap));
}

static bool node_snapshot_capture(UIM_ComponentManager *manager, const uim_NodeInfo *info,
                                  PageNodeSnapshot *snap) {
    size_t i, j;
    memset(snap, 0, sizeof(*snap));
    snap->id = info->id;
    snap->parent_id = info->parent_id;
    snap->rect = info->local_rect;
    snap->layout = info->layout;
    snap->visible = info->visible;
    snap->enabled = info->enabled;
    snap->z_order = info->z_order;
    snap->action_id = info->action_id;
    snap->child_order = info->child_order;
    snap->space = info->space;
    snap->transform = info->transform;
    if (info->component_config != NULL && info->component_config_size > 0u) {
        snap->component_config = malloc(info->component_config_size);
        if (snap->component_config == NULL) return false;
        memcpy(snap->component_config, info->component_config, info->component_config_size);
        snap->component_config_size = info->component_config_size;
    }
    if (uim_node_component_count(manager, info->id, &snap->component_count) != uim_OK)
        return false;
    if (snap->component_count > 0u) {
        snap->components = (RuntimeComponentSnapshot *)calloc(
            snap->component_count, sizeof(*snap->components));
        if (snap->components == NULL) return false;
    }
    for (i = 0; i < snap->component_count; ++i) {
        RuntimeComponentSnapshot *comp = &snap->components[i];
        if (uim_node_component_get_at(manager, info->id, i, &comp->info) != uim_OK)
            return false;
        if (uim_component_instance_property_count(manager, comp->info.instance_id,
                                                  &comp->property_count) != uim_OK)
            return false;
        if (comp->property_count > 0u) {
            comp->properties = (RuntimePropSnapshot *)calloc(
                comp->property_count, sizeof(*comp->properties));
            if (comp->properties == NULL) return false;
        }
        for (j = 0; j < comp->property_count; ++j) {
            if (uim_component_instance_property_get_at(
                    manager, comp->info.instance_id, j, &comp->properties[j].id,
                    &comp->properties[j].value) != uim_OK)
                return false;
            /* Re-read into an owned copy: the manager value may be replaced by play. */
            {
                uim_Value owned = {0};
                if (runtime_value_copy(&owned, comp->properties[j].value) != uim_OK)
                    return false;
                comp->properties[j].value = owned;
            }
        }
    }
    return true;
}

static void node_snapshot_restore(UIM_ComponentManager *manager, const PageNodeSnapshot *snap) {
    size_t i, j;
    uim_NodeInfo info;
    if (uim_node_get(manager, snap->id, &info) != uim_OK) return;
    if (info.parent_id != snap->parent_id)
        (void)uim_node_set_parent(manager, snap->id, snap->parent_id);
    if (info.space != snap->space) (void)uim_node_set_space(manager, snap->id, snap->space);
    if (snap->space == uim_SPACE_3D)
        (void)uim_node_set_transform3(manager, snap->id, &snap->transform);
    else
        (void)uim_node_set_rect(manager, snap->id, snap->rect);
    (void)uim_node_set_layout_constraints(manager, snap->id, snap->layout);
    (void)uim_node_set_visible(manager, snap->id, snap->visible);
    (void)uim_node_set_enabled(manager, snap->id, snap->enabled);
    (void)uim_node_set_z_order(manager, snap->id, snap->z_order);
    (void)uim_node_set_action_id(manager, snap->id, snap->action_id);
    (void)uim_node_set_child_order(manager, snap->id, snap->child_order);
    (void)uim_node_set_component_config(manager, snap->id, snap->component_config,
                                        snap->component_config_size);
    for (i = 0; i < snap->component_count; ++i) {
        const RuntimeComponentSnapshot *comp = &snap->components[i];
        uim_ComponentInstanceInfo current;
        if (uim_component_instance_get(manager, comp->info.instance_id, &current) != uim_OK)
            (void)uim_component_instance_restore(manager, &comp->info);
        for (j = 0; j < comp->property_count; ++j)
            (void)uim_component_instance_set_property(manager, comp->info.instance_id,
                                                      comp->properties[j].id,
                                                      comp->properties[j].value);
    }
}

bool uim_runtime_begin(UIM_ComponentManager *manager, uim_PageId page) {
    size_t count = 0u;
    size_t i;
    uim_runtime_end();
    g_paused = false;
    g_goto_request = 0;
    if (manager == NULL || page == uim_PAGE_INVALID) return false;
    if (uim_page_get_node_count(manager, page, &count) != uim_OK) return false;
    if (count > uim_runtime_MAX_NODES) {
        printf("[play] page too large for snapshot: %u\n", (unsigned)count);
        return false;
    }
    if (count > 0u) {
        g_snapshots = (PageNodeSnapshot *)calloc(count, sizeof(*g_snapshots));
        if (g_snapshots == NULL) return false;
    }
    g_snapshot_count = 0u;
    for (i = 0; i < count; ++i) {
        uim_NodeInfo info;
        PageNodeSnapshot *snap;
        if (uim_page_get_node_at(manager, page, i, &info) != uim_OK) continue;
        snap = &g_snapshots[g_snapshot_count];
        if (!node_snapshot_capture(manager, &info, snap)) {
            node_snapshot_free(snap);
            for (size_t j = 0; j < g_snapshot_count; ++j)
                node_snapshot_free(&g_snapshots[j]);
            free(g_snapshots);
            g_snapshots = NULL;
            g_snapshot_count = 0u;
            return false;
        }
        g_snapshot_count++;
    }
    g_snapshot_manager = manager;
    g_active = true;
    g_active_manager = manager;
    g_active_page = page;
    return true;
}

void uim_runtime_end(void) {
    size_t i;
    if (g_snapshot_manager != NULL) {
        for (i = 0; i < g_snapshot_count; ++i)
            node_snapshot_restore(g_snapshot_manager, &g_snapshots[i]);
    }
    if (g_snapshots != NULL) {
        for (i = 0; i < g_snapshot_count; ++i) node_snapshot_free(&g_snapshots[i]);
        free(g_snapshots);
    }
    g_snapshots = NULL;
    g_snapshot_count = 0u;
    g_snapshot_manager = NULL;
    g_active = false;
    g_active_manager = NULL;
    g_active_page = uim_PAGE_INVALID;
    g_paused = false;
    g_goto_request = 0;
}

/* ---------- 选择器 ---------- */

static bool page_starts_with(const char *text, const char *prefix) {
    if (!text || !prefix) return false;
    while (*prefix) {
        if (*text != *prefix) return false;
        ++text;
        ++prefix;
    }
    return true;
}

/* 收集 rule 的选择器命中的目标（不含自己）。返回数量。 */
static size_t page_select_targets(UIM_ComponentManager *manager, uim_PageId page,
                                  uim_NodeId self, const PageRule *rule,
                                  uim_NodeId *out, size_t cap) {
    size_t count = 0u;
    size_t i;
    uim_NodeInfo self_info;
    double self_cx = 0.0, self_cy = 0.0;
    size_t hits = 0u;
    if (cap == 0u || !out) return 0u;
    if (uim_node_get(manager, self, &self_info) != uim_OK) return 0u;
    page_center(&self_info.local_rect, &self_cx, &self_cy);
    if (uim_page_get_node_count(manager, page, &count) != uim_OK) return 0u;
    if (count > uim_runtime_MAX_NODES) return 0u;

    if (rule->selector == uim_RULE_SELECT_NEAREST) {
        uim_NodeId best = uim_NODE_INVALID;
        double best_d2 = 0.0;
        for (i = 0; i < count; ++i) {
            uim_NodeInfo info;
            double cx, cy, d2;
            if (uim_page_get_node_at(manager, page, i, &info) != uim_OK) continue;
            if (info.id == self || !info.visible || !info.enabled) continue;
            page_center(&info.local_rect, &cx, &cy);
            d2 = (cx - self_cx) * (cx - self_cx) + (cy - self_cy) * (cy - self_cy);
            if (best == uim_NODE_INVALID || d2 < best_d2) { best = info.id; best_d2 = d2; }
        }
        if (best != uim_NODE_INVALID) { out[0] = best; return 1u; }
        return 0u;
    }

    for (i = 0; i < count && hits < cap; ++i) {
        uim_NodeInfo info;
        double cx, cy;
        if (uim_page_get_node_at(manager, page, i, &info) != uim_OK) continue;
        if (info.id == self || !info.visible || !info.enabled) continue;
        page_center(&info.local_rect, &cx, &cy);
        switch (rule->selector) {
            case uim_RULE_SELECT_SAME_ROW:
                if (fabs(cy - self_cy) <= uim_RULE_ALIGN_TOLERANCE) out[hits++] = info.id;
                break;
            case uim_RULE_SELECT_SAME_COL:
                if (fabs(cx - self_cx) <= uim_RULE_ALIGN_TOLERANCE) out[hits++] = info.id;
                break;
            case uim_RULE_SELECT_RADIUS: {
                double d2 = (cx - self_cx) * (cx - self_cx) + (cy - self_cy) * (cy - self_cy);
                if (d2 <= rule->radius * rule->radius) out[hits++] = info.id;
                break;
            }
            case uim_RULE_SELECT_TAG:
                if (info.stable_key && page_starts_with(info.stable_key, rule->tag))
                    out[hits++] = info.id;
                break;
            default:
                break;
        }
    }
    return hits;
}

/* ---------- 效果 ---------- */

static void page_apply_damage(UIM_ComponentManager *manager, uim_NodeId target,
                              const PageRule *rule) {
    uim_ComponentInstanceId hp = page_find_component(manager, target, uim_COMPONENT_HEALTH);
    int64_t current = 0;
    uim_Value value;
    uim_NodeInfo info;
    if (hp == 0u) return;                       /* 没有 Health 的目标不掉血 */
    if (!page_prop_int(manager, hp, uim_PROPERTY_HP, &current)) return;
    current -= rule->amount;
    if (current < 0) current = 0;
    memset(&value, 0, sizeof(value));
    value.type = uim_VALUE_INT64;
    value.as.integer = current;
    (void)uim_component_instance_set_property(manager, hp, uim_PROPERTY_HP, value);
    if (current <= 0 && uim_node_get(manager, target, &info) == uim_OK && info.visible) {
        (void)uim_node_set_visible(manager, target, false);
    }
}

static void page_apply_move(UIM_ComponentManager *manager, uim_NodeId target,
                            const PageRule *rule, double dt) {
    uim_NodeInfo info;
    double delta = (double)rule->amount * dt;   /* tick：px/s；click 时 dt=0 → 见下 */
    if (uim_node_get(manager, target, &info) != uim_OK) return;
    info.local_rect.x = (float)((double)info.local_rect.x + delta);
    (void)uim_node_set_rect(manager, target, info.local_rect);
}

static void page_apply_move_instant(UIM_ComponentManager *manager, uim_NodeId target,
                                    const PageRule *rule) {
    uim_NodeInfo info;
    if (uim_node_get(manager, target, &info) != uim_OK) return;
    info.local_rect.x = (float)((double)info.local_rect.x + (double)rule->amount);
    (void)uim_node_set_rect(manager, target, info.local_rect);
}

static void page_apply_effect(UIM_ComponentManager *manager, uim_PageId page,
                              uim_NodeId self, const PageRule *rule, double dt) {
    uim_NodeId targets[uim_runtime_MAX_TARGETS];
    size_t n;
    size_t i;
    (void)page;
    n = page_select_targets(manager, page, self, rule, targets, uim_runtime_MAX_TARGETS);
    for (i = 0; i < n; ++i) {
        switch (rule->effect) {
            case uim_RULE_EFFECT_DAMAGE:
                page_apply_damage(manager, targets[i], rule);
                break;
            case uim_RULE_EFFECT_MOVE:
                if (dt > 0.0) page_apply_move(manager, targets[i], rule, dt);
                else page_apply_move_instant(manager, targets[i], rule);
                break;
            case uim_RULE_EFFECT_DESTROY:
                (void)uim_node_set_visible(manager, targets[i], false);
                break;
            case uim_RULE_EFFECT_GOTO:
                if (rule->target_page > 0) g_goto_request = (int)rule->target_page;
                break;
            default:
                break;
        }
    }
}

/* ---------- tick ---------- */

/* 对一个节点累加一次 Motion 的位移；没有 Motion 组件则什么都不做。 */
static void page_tick_motion(UIM_ComponentManager *manager, uim_NodeId node_id, double dt,
                             const uim_NodeInfo *info) {
    size_t component_count = 0u;
    size_t i;
    double dx = 0.0;
    double dy = 0.0;
    if (uim_node_component_count(manager, node_id, &component_count) != uim_OK) return;
    for (i = 0; i < component_count; ++i) {
        uim_ComponentInstanceInfo instance;
        uim_Value value;
        if (uim_node_component_get_at(manager, node_id, i, &instance) != uim_OK) continue;
        if (instance.type_id != uim_COMPONENT_MOTION) continue;
        if (uim_component_instance_get_property(manager, instance.instance_id,
                                                uim_PROPERTY_VX, &value) == uim_OK &&
            value.type == uim_VALUE_FLOAT64) {
            dx += value.as.real;
        }
        if (uim_component_instance_get_property(manager, instance.instance_id,
                                                uim_PROPERTY_VY, &value) == uim_OK &&
            value.type == uim_VALUE_FLOAT64) {
            dy += value.as.real;
        }
    }
    if (dx == 0.0 && dy == 0.0) return;
    {
        uim_NodeInfo moved = *info;
        moved.local_rect.x += (float)(dx * dt);
        moved.local_rect.y += (float)(dy * dt);
        (void)uim_node_set_rect(manager, node_id, moved.local_rect);
    }
}

static void page_tick_node(UIM_ComponentManager *manager, uim_PageId page, uim_NodeId node_id,
                           double dt) {
    uim_NodeInfo info;
    PageRule rule;
    if (uim_node_get(manager, node_id, &info) != uim_OK) return;
    if (!info.enabled || !info.visible) return;
    page_tick_motion(manager, node_id, dt, &info);
    if (page_node_rule(manager, node_id, &rule) && rule.trigger == uim_RULE_TRIGGER_TICK) {
        page_apply_effect(manager, page, node_id, &rule, dt);
    }
}

void uim_runtime_tick(UIM_ComponentManager *manager, uim_PageId page, double dt) {
    size_t count = 0u;
    if (!g_active || g_paused) return;
    if (manager == NULL || manager != g_active_manager || page != g_active_page) return;
    dt = page_clamp_dt(dt);
    if (dt <= 0.0) return;
    if (uim_page_get_node_count(manager, page, &count) != uim_OK) return;
    if (count > uim_runtime_MAX_NODES) return;
    for (size_t i = 0; i < count; ++i) {
        uim_NodeInfo info;
        if (uim_page_get_node_at(manager, page, i, &info) != uim_OK) continue;
        page_tick_node(manager, page, info.id, dt);
    }
}

/* ---------- 命中与点击 ---------- */

uim_NodeId uim_runtime_pick_ex(UIM_ComponentManager *manager, uim_PageId page,
                                float x, float y,
                                uim_RuntimeProjectNodeFn project, void *project_user) {
    size_t count = 0u;
    uim_NodeId best = uim_NODE_INVALID;
    int32_t best_z = 0;
    size_t i;
    if (manager == NULL || page == uim_PAGE_INVALID) return uim_NODE_INVALID;
    if (uim_page_get_node_count(manager, page, &count) != uim_OK) return uim_NODE_INVALID;
    if (count > uim_runtime_MAX_NODES) return uim_NODE_INVALID;
    for (i = 0; i < count; ++i) {
        uim_NodeInfo info;
        uim_Rect hit_rect;
        if (uim_page_get_node_at(manager, page, i, &info) != uim_OK) continue;
        if (!info.visible || !info.enabled) continue;
        if (info.space == uim_SPACE_3D) {
            if (project == NULL || !project(project_user, manager, &info, &hit_rect))
                continue;
            if (hit_rect.width <= 0.0f || hit_rect.height <= 0.0f) continue;
        } else {
            hit_rect = info.local_rect;
            if (hit_rect.width <= 0.0f || hit_rect.height <= 0.0f) continue;
        }
        if (x < hit_rect.x || y < hit_rect.y ||
            x > hit_rect.x + hit_rect.width ||
            y > hit_rect.y + hit_rect.height) continue;
        /* 后绘制（z 更大 / id 更大）的在上层，优先命中 */
        if (best == uim_NODE_INVALID || info.z_order > best_z ||
            (info.z_order == best_z && info.id > best)) {
            best = info.id;
            best_z = info.z_order;
        }
    }
    return best;
}

uim_NodeId uim_runtime_pick(UIM_ComponentManager *manager, uim_PageId page,
                             float x, float y) {
    return uim_runtime_pick_ex(manager, page, x, y, NULL, NULL);
}

int uim_runtime_fire_click(UIM_ComponentManager *manager, uim_PageId page,
                            uim_NodeId node_id) {
    PageRule rule;
    if (!g_active || manager != g_active_manager || page != g_active_page) return 0;
    if (node_id == uim_NODE_INVALID) return 0;
    if (!page_node_rule(manager, node_id, &rule)) return 0;
    if (rule.trigger != uim_RULE_TRIGGER_CLICK) return 0;
    page_apply_effect(manager, page, node_id, &rule, 0.0);
    return 1;
}

uim_NodeId uim_runtime_click_ex(UIM_ComponentManager *manager, uim_PageId page,
                                 float x, float y,
                                 uim_RuntimeProjectNodeFn project, void *project_user) {
    uim_NodeId hit = uim_runtime_pick_ex(manager, page, x, y, project, project_user);
    if (hit != uim_NODE_INVALID) (void)uim_runtime_fire_click(manager, page, hit);
    return hit;
}

uim_NodeId uim_runtime_click(UIM_ComponentManager *manager, uim_PageId page,
                              float x, float y) {
    return uim_runtime_click_ex(manager, page, x, y, NULL, NULL);
}

int uim_runtime_take_goto_request(void) {
    int request = g_goto_request;
    g_goto_request = 0;
    return request;
}

void uim_runtime_set_paused(bool paused) { g_paused = paused; }
bool uim_runtime_paused(void) { return g_paused; }

void uim_runtime_step(UIM_ComponentManager *manager, uim_PageId page) {
    bool was_paused = g_paused;
    g_paused = false;
    uim_runtime_tick(manager, page, 1.0 / 60.0);
    g_paused = was_paused;
}
