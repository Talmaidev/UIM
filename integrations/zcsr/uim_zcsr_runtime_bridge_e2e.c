/* End-to-end UIM_ZcsrRuntimeBridge test: ZCSR Primitive → UIM Component Manager → Apply flow. */
#include "zcsr/ui.h"
#include "zcsr/ui_input.h"
#include "zcsr/ui_button.h"
#include "zcsr/ui_toggle.h"
#include "zcsr/ui_slider.h"
#include "uim_component_manager.h"
#include "uim_zcsr_runtime_bridge.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } } while(0)

int main(void)
{
    /* --- ZCSR side --- */
    ZCSR_UIConfig zcfg = { 32, 16, 64, 800.0f, 600.0f };
    ZCSR_UI* zui = NULL;
    ZCSR_NodeId btn, tgl, sld;
    zcsr_ui_create(&zcfg, &zui);

    zcsr_node_create(zui, ZCSR_COMPONENT_BUTTON, ZCSR_NODE_INVALID, &btn);
    ZCSR_Rect br = { 10, 10, 100, 30 };
    zcsr_node_set_rect(zui, btn, &br);
    zcsr_button_set_text(zui, btn, "Apply", 5);

    zcsr_node_create(zui, ZCSR_COMPONENT_TOGGLE, ZCSR_NODE_INVALID, &tgl);
    ZCSR_Rect tr = { 10, 50, 100, 24 };
    zcsr_node_set_rect(zui, tgl, &tr);

    zcsr_node_create(zui, ZCSR_COMPONENT_SLIDER, ZCSR_NODE_INVALID, &sld);
    ZCSR_Rect sr = { 10, 90, 200, 20 };
    zcsr_node_set_rect(zui, sld, &sr);
    zcsr_slider_set_range(zui, sld, 0.0f, 100.0f);

    /* --- UIR side --- */
    UIM_ComponentManagerConfig ucfg = { 32, 8 };
    UIM_ComponentManager* ur = NULL;
    uim_component_manager_create(&ucfg, &ur);

    uim_PageId page;
    uim_page_create(ur, "main", &page);

    /* Register properties. */
    uim_property_register(ur, 1, uim_value_bool(false));   /* shadows */
    uim_property_register(ur, 2, uim_value_float(50.0));   /* volume */

    /* Create UIR nodes matching ZCSR. */
    uim_NodeDesc nd;
    uim_NodeId ubtn, utgl, usld;
    memset(&nd, 0, sizeof(nd));
    nd.page_id = page; nd.parent_id = 0; nd.visible = true; nd.enabled = true;

    nd.stable_key = "btn_apply"; nd.component_id = 1; nd.local_rect.x=10; nd.local_rect.y=10;
    nd.local_rect.width=100; nd.local_rect.height=30; nd.action_id = 1; /* APPLY action */
    uim_node_create(ur, &nd, &ubtn);

    nd.stable_key = "tgl_shadows"; nd.component_id = 2; nd.local_rect.x=10; nd.local_rect.y=50;
    nd.local_rect.width=100; nd.local_rect.height=24; nd.action_id = 0;
    uim_node_create(ur, &nd, &utgl);

    nd.stable_key = "sld_volume"; nd.component_id = 3; nd.local_rect.x=10; nd.local_rect.y=90;
    nd.local_rect.width=200; nd.local_rect.height=20; nd.action_id = 0;
    uim_node_create(ur, &nd, &usld);

    /* Bindings: toggle → prop 1, slider → prop 2. */
    uim_BindingDesc bd;
    memset(&bd, 0, sizeof(bd));
    bd.node_id = utgl; bd.property_id = 1; bd.value_type = uim_VALUE_BOOL;
    bd.direction = uim_BIND_READ_WRITE; bd.initial_sync = uim_SYNC_INITIAL_ONLY;
    bd.state = uim_BINDING_ACTIVE;
    uim_BindingId b1;
    uim_binding_create(ur, &bd, &b1);

    bd.node_id = usld; bd.property_id = 2; bd.value_type = uim_VALUE_FLOAT64;
    uim_BindingId b2;
    uim_binding_create(ur, &bd, &b2);

    /* --- UIM_ZcsrRuntimeBridge --- */
    UIM_ZcsrRuntimeBridge UIM_ZcsrRuntimeBridge;
    uim_zcsr_runtime_bridge_create(&UIM_ZcsrRuntimeBridge, zui, ur);
    uim_zcsr_runtime_bridge_map_node(&UIM_ZcsrRuntimeBridge, btn, ubtn, 0);       /* button: action, no property */
    uim_zcsr_runtime_bridge_map_node(&UIM_ZcsrRuntimeBridge, tgl, utgl, 1);        /* toggle: shadows */
    uim_zcsr_runtime_bridge_map_node(&UIM_ZcsrRuntimeBridge, sld, usld, 2);        /* slider: volume */

    printf("=== Test 1: Toggle click → UIR property change ===\n");
    {
        ZCSR_Input in;
        memset(&in, 0, sizeof(in));
        in.type = ZCSR_INPUT_POINTER_DOWN;
        in.pointer_position.x = 50; in.pointer_position.y = 60;
        in.pointer_button = ZCSR_POINTER_LEFT;
        zcsr_ui_process_input(zui, &in);
        in.type = ZCSR_INPUT_POINTER_UP;
        zcsr_ui_process_input(zui, &in);
    }
    uim_zcsr_runtime_bridge_forward_zcsr_events(&UIM_ZcsrRuntimeBridge);
    uim_zcsr_runtime_bridge_drain_uim_events(&UIM_ZcsrRuntimeBridge);
    CHECK(UIM_ZcsrRuntimeBridge.events_forwarded == 1);

    /* Check UIR pending value. */
    uim_SettingState ss;
    uim_setting_get_state(ur, 1, &ss);
    printf("  [DBG] prop1: dirty=%d pending_type=%d pending_bool=%d committed_bool=%d\n",
           (int)ss.dirty, (int)ss.pending.type, (int)ss.pending.as.boolean, (int)ss.committed.as.boolean);
    CHECK(ss.dirty == true);
    CHECK(ss.pending.as.boolean == true);

    printf("=== Test 2: Apply request → confirm → committed ===\n");
    {
        uint64_t serial = 0;
        uim_apply_request(ur, &serial);
        CHECK(serial > 0);
        uim_apply_confirm(ur, serial, true);
    }
    uim_setting_get_state(ur, 1, &ss);
    CHECK(ss.dirty == false);
    CHECK(ss.committed.as.boolean == true);

    printf("=== Test 3: Slider drag → UIR property change ===\n");
    {
        ZCSR_Input in;
        memset(&in, 0, sizeof(in));
        in.type = ZCSR_INPUT_POINTER_DOWN;
        in.pointer_position.x = 150; in.pointer_position.y = 100;
        in.pointer_button = ZCSR_POINTER_LEFT;
        zcsr_ui_process_input(zui, &in);
        in.type = ZCSR_INPUT_POINTER_UP;
        zcsr_ui_process_input(zui, &in);
    }
    UIM_ZcsrRuntimeBridge.events_forwarded = 0;
    uim_zcsr_runtime_bridge_forward_zcsr_events(&UIM_ZcsrRuntimeBridge);
    uim_zcsr_runtime_bridge_drain_uim_events(&UIM_ZcsrRuntimeBridge);
    CHECK(UIM_ZcsrRuntimeBridge.events_forwarded == 1);
    uim_setting_get_state(ur, 2, &ss);
    printf("  [DBG] prop2: dirty=%d pending_type=%d pending_real=%f\n",
           (int)ss.dirty, (int)ss.pending.type, ss.pending.as.real);
    CHECK(ss.dirty == true);
    CHECK(ss.pending.as.real > 60.0); /* ~70% of 200px = 70 */

    printf("=== Test 4: Button click → UIR action ===\n");
    {
        ZCSR_Input in;
        memset(&in, 0, sizeof(in));
        in.type = ZCSR_INPUT_POINTER_DOWN;
        in.pointer_position.x = 50; in.pointer_position.y = 25;
        in.pointer_button = ZCSR_POINTER_LEFT;
        zcsr_ui_process_input(zui, &in);
        in.type = ZCSR_INPUT_POINTER_UP;
        zcsr_ui_process_input(zui, &in);
    }
    UIM_ZcsrRuntimeBridge.events_forwarded = 0;
    uim_zcsr_runtime_bridge_forward_zcsr_events(&UIM_ZcsrRuntimeBridge);
    uim_zcsr_runtime_bridge_drain_uim_events(&UIM_ZcsrRuntimeBridge);
    CHECK(UIM_ZcsrRuntimeBridge.events_forwarded == 1); /* action routed */

    printf("\n%d failures\n", failures);
    if (failures == 0) printf("UIM_ZcsrRuntimeBridge ALL PASS\n");

    uim_component_manager_destroy(ur);
    zcsr_ui_destroy(zui);
    return failures;
}
