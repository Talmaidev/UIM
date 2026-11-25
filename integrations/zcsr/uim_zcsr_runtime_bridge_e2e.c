/* End-to-end UIM_ZcsrRuntimeBridge test: ZCSR Primitive → UIM Component Manager → Apply flow. */
#include "zcsr/ui.h"
#include "zcsr/ui_input.h"
#include "zcsr/ui_button.h"
#include "zcsr/ui_toggle.h"
#include "zcsr/ui_slider.h"
#include "zcsr/ui_text_input.h"
#include "uim_component_manager.h"
#include "uim_component_registry.h"
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
    uim_property_register(ur, 3, uim_value_string("", 0)); /* text input */

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

    /* UIM node ids are 64-bit and can exceed the old 2048-entry array. */
    {
        struct UIM_ZcsrRuntimeBridge high_bridge;
        uim_NodeId high_uim = ((uim_NodeId)1u << 40) | 17u;
        ZCSR_NodeId high_zcsr = 1500u;
        uim_zcsr_runtime_bridge_create(&high_bridge, zui, ur);
        uim_zcsr_runtime_bridge_map_node(&high_bridge, high_zcsr, high_uim, 0);
        CHECK(uim_zcsr_runtime_bridge_find_zcsr(&high_bridge, high_uim) == high_zcsr);
        uim_zcsr_runtime_bridge_destroy(&high_bridge);
    }

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

    printf("=== Test 5: all 10 UIM component types map to ZCSR components ===\n");
    {
        uim_PageId page2;
        uim_NodeId root2, nodes[10];
        uim_NodeDesc d;
        size_t i;
        CHECK(uim_page_create(ur, "components", &page2) == uim_OK);
        memset(&d, 0, sizeof(d));
        d.page_id = page2; d.parent_id = 0; d.visible = true; d.enabled = true;
        d.stable_key = "root"; d.component_id = 1;
        d.local_rect.width = 800; d.local_rect.height = 600;
        CHECK(uim_node_create(ur, &d, &root2) == uim_OK);
        d.parent_id = root2;
        for (i = 0; i < 10; ++i) {
            char key[16];
            snprintf(key, sizeof(key), "c%u", (unsigned)i);
            d.stable_key = key;
            d.component_id = 0x00010001u + (uint32_t)i;   /* PANEL .. SCROLL_VIEW */
            d.local_rect.x = 10.0f + (float)i * 12.0f;
            d.local_rect.y = 10.0f;
            d.local_rect.width = 100.0f; d.local_rect.height = 20.0f;
            CHECK(uim_node_create(ur, &d, &nodes[i]) == uim_OK);
        }
        CHECK(uim_zcsr_runtime_bridge_sync_page(&UIM_ZcsrRuntimeBridge, page2) == uim_OK);
        for (i = 0; i < 10; ++i) {
            ZCSR_NodeId zn;
            ZCSR_ComponentId zc;
            CHECK(nodes[i] < UIM_BRIDGE_MAX_NODES);
            zn = uim_zcsr_runtime_bridge_find_zcsr(&UIM_ZcsrRuntimeBridge, nodes[i]);
            CHECK(zn != ZCSR_NODE_INVALID);
            CHECK(zcsr_node_get_component_id(zui, zn, &zc) == ZCSR_OK);
            CHECK(zc == (ZCSR_ComponentId)(ZCSR_COMPONENT_PANEL + (ZCSR_ComponentId)i));
        }

        /* Text input: the engine emits TEXT_CHANGED for typed bytes; the bridge
         * must turn that into a UIM string value, not swallow the event. */
        {
            uim_NodeDesc td = d;
            uim_NodeId uti = uim_NODE_INVALID;
            uim_BindingId text_binding;
            uim_BindingDesc tb;
            uim_SettingState text_state;
            ZCSR_NodeId zti;
            ZCSR_Input text_in;
            td.stable_key = "text_input";
            td.component_id = uim_COMPONENT_TEXT_INPUT;
            td.local_rect.x = 10.0f;
            td.local_rect.y = 300.0f;
            td.local_rect.width = 160.0f;
            td.local_rect.height = 24.0f;
            CHECK(uim_node_create(ur, &td, &uti) == uim_OK);
            memset(&tb, 0, sizeof(tb));
            tb.node_id = uti;
            tb.property_id = 3;
            tb.value_type = uim_VALUE_STRING;
            tb.direction = uim_BIND_READ_WRITE;
            tb.initial_sync = uim_SYNC_INITIAL_ONLY;
            tb.state = uim_BINDING_ACTIVE;
            CHECK(uim_binding_create(ur, &tb, &text_binding) == uim_OK);
            CHECK(uim_zcsr_runtime_bridge_sync_page(&UIM_ZcsrRuntimeBridge, page2) == uim_OK);
            zti = uim_zcsr_runtime_bridge_find_zcsr(&UIM_ZcsrRuntimeBridge, uti);
            CHECK(zti != ZCSR_NODE_INVALID);
            CHECK(zcsr_ui_set_focus(zui, zti) == ZCSR_OK);
            memset(&text_in, 0, sizeof(text_in));
            text_in.type = ZCSR_INPUT_TEXT;
            text_in.text_utf8 = "42";
            text_in.text_bytes = 2u;
            CHECK(zcsr_ui_process_input(zui, &text_in) == ZCSR_OK);
            UIM_ZcsrRuntimeBridge.events_forwarded = 0;
            uim_zcsr_runtime_bridge_forward_zcsr_events(&UIM_ZcsrRuntimeBridge);
            CHECK(UIM_ZcsrRuntimeBridge.events_forwarded == 1);
            CHECK(uim_setting_get_state(ur, 3, &text_state) == uim_OK);
            CHECK(text_state.dirty == true);
            CHECK(text_state.pending.type == uim_VALUE_STRING);
            CHECK(text_state.pending.as.string.size == 2u);
            CHECK(text_state.pending.as.string.data != NULL);
            CHECK(memcmp(text_state.pending.as.string.data, "42", 2u) == 0);
        }

        /* A 3D node must not become a 2D ZCSR widget. The editor draws it with
         * camera3d; the bridge deliberately leaves it out of the 2D tree. */
        {
            uim_NodeDesc d3d = d;
            uim_NodeId n3d = uim_NODE_INVALID;
            d3d.parent_id = root2;
            d3d.stable_key = "node3d";
            d3d.component_id = uim_COMPONENT_PANEL;
            d3d.space = uim_SPACE_3D;
            d3d.transform.position[0] = 0.0f;
            d3d.transform.position[1] = 0.0f;
            d3d.transform.position[2] = 0.0f;
            d3d.transform.scale[0] = 1.0f;
            d3d.transform.scale[1] = 1.0f;
            d3d.transform.scale[2] = 1.0f;
            CHECK(uim_node_create(ur, &d3d, &n3d) == uim_OK);
            CHECK(uim_zcsr_runtime_bridge_sync_page(&UIM_ZcsrRuntimeBridge, page2) == uim_OK);
            CHECK(n3d < UIM_BRIDGE_MAX_NODES);
            {
                ZCSR_NodeId zn = uim_zcsr_runtime_bridge_find_zcsr(&UIM_ZcsrRuntimeBridge, n3d);
                ZCSR_Rect projected;
                CHECK(zn != ZCSR_NODE_INVALID);
                CHECK(zcsr_node_get_rect(zui, zn, &projected) == ZCSR_OK);
                CHECK(projected.width > 0.0f);
                CHECK(projected.height > 0.0f);
            }
        }
    }

    printf("\n%d failures\n", failures);
    if (failures == 0) printf("UIM_ZcsrRuntimeBridge ALL PASS\n");

    uim_component_manager_destroy(ur);
    zcsr_ui_destroy(zui);
    return failures;
}
