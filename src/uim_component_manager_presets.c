/* uim_component_manager_presets.c — Component Manager 内置预设数据模型
 *
 * 这里定义游戏常用的页面和节点预设。
 * game_test / editor 都可以直接调用这些 API 创建 Component Manager 实例，
 * 不需要自己手动写 Page/Node 初始化代码。
 */

#include "uim_component_manager_presets.h"
#include <string.h>
#include <stdio.h>

/* Component IDs — 对应 ZCSR UI Primitive */
#define CID_PANEL     1
#define CID_LABEL     2
#define CID_BUTTON    3
#define CID_SLIDER    4
#define CID_DROPDOWN  5
#define CID_TOGGLE    6
#define CID_SEPARATOR 7

/* Action IDs */
#define ACTION_OPEN_SETTINGS  1
#define ACTION_EXIT           2
#define ACTION_BACK           3

/* 组件配置结构 — 对应 ZCSR 的组件数据 */
typedef struct {
    char text[64];
} HostLabelConfig;

typedef struct {
    char text[64];
} HostButtonConfig;

typedef struct {
    float min, max, value;
} HostSliderConfig;

typedef struct {
    const char* items[16];
    int item_count;
    int selected;
} HostDropdownConfig;

typedef struct {
    bool value;
} HostToggleConfig;

/* 创建完整的游戏 Component Manager 数据模型 */
uim_Result uim_component_manager_create_game_preset(UIM_ComponentManager** out_rec) {
    uim_Result rc;
    UIM_ComponentManagerConfig cfg = { 256, 8 };
    rc = uim_component_manager_create(&cfg, out_rec);
    if (rc != uim_OK) return rc;

    UIM_ComponentManager* rec = *out_rec;
    uim_PageId page_main, page_settings;
    uim_NodeId nid;
    uim_NodeDesc nd;

    /* ---- MainWindow Page ---- */
    rc = uim_page_create(rec, "MainWindow", &page_main);
    if (rc != uim_OK) return rc;

    memset(&nd, 0, sizeof(nd));
    nd.page_id = page_main;
    nd.component_id = CID_PANEL;
    nd.parent_id = 0;
    nd.local_rect = (uim_Rect){0, 0, 1280, 720};
    nd.visible = true;
    nd.enabled = true;
    nd.z_order = 0;
    nd.stable_key = "main_bg";
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    /* Title */
    static HostLabelConfig cfg_title = { "DARK REALMS" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){440, 120, 400, 40};
    nd.z_order = 1;
    nd.stable_key = "title";
    nd.component_config = &cfg_title;
    nd.component_config_size = sizeof(cfg_title);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    /* Settings button */
    static HostButtonConfig cfg_btn_settings = { "SETTINGS" };
    nd.component_id = CID_BUTTON;
    nd.local_rect = (uim_Rect){520, 280, 240, 44};
    nd.z_order = 2;
    nd.stable_key = "btn_settings";
    nd.action_id = ACTION_OPEN_SETTINGS;
    nd.component_config = &cfg_btn_settings;
    nd.component_config_size = sizeof(cfg_btn_settings);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    /* Exit button */
    static HostButtonConfig cfg_btn_exit = { "EXIT" };
    nd.component_id = CID_BUTTON;
    nd.local_rect = (uim_Rect){520, 340, 240, 44};
    nd.z_order = 3;
    nd.stable_key = "btn_exit";
    nd.action_id = ACTION_EXIT;
    nd.component_config = &cfg_btn_exit;
    nd.component_config_size = sizeof(cfg_btn_exit);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    /* ---- Settings Page ---- */
    rc = uim_page_create(rec, "Settings", &page_settings);
    if (rc != uim_OK) return rc;

    memset(&nd, 0, sizeof(nd));
    nd.page_id = page_settings;
    nd.component_id = CID_PANEL;
    nd.parent_id = 0;
    nd.local_rect = (uim_Rect){0, 0, 1280, 720};
    nd.visible = true;
    nd.enabled = true;
    nd.z_order = 0;
    nd.stable_key = "settings_bg";
    nd.component_config = NULL;
    nd.component_config_size = 0;
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    /* === Video Tab (10 items) === */
    static HostLabelConfig cfg_tab_video = { "Video" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){20, 20, 100, 24};
    nd.z_order = 1;
    nd.stable_key = "tab_video";
    nd.component_config = &cfg_tab_video;
    nd.component_config_size = sizeof(cfg_tab_video);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_v_display = { "Display" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 60, 150, 24};
    nd.z_order = 2;
    nd.stable_key = "label_display";
    nd.component_config = &cfg_v_display;
    nd.component_config_size = sizeof(cfg_v_display);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostDropdownConfig cfg_v_mode = {
        .items = { "Windowed", "Borderless", "Fullscreen" },
        .item_count = 3, .selected = 1,
    };
    nd.component_id = CID_DROPDOWN;
    nd.local_rect = (uim_Rect){420, 60, 180, 28};
    nd.z_order = 3;
    nd.stable_key = "dropdown_display_mode";
    nd.component_config = &cfg_v_mode;
    nd.component_config_size = sizeof(cfg_v_mode);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_v_res = { "Resolution" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 100, 150, 24};
    nd.z_order = 4;
    nd.stable_key = "label_resolution";
    nd.component_config = &cfg_v_res;
    nd.component_config_size = sizeof(cfg_v_res);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostDropdownConfig cfg_v_res_dropdown = {
        .items = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160" },
        .item_count = 5, .selected = 2,
    };
    nd.component_id = CID_DROPDOWN;
    nd.local_rect = (uim_Rect){420, 100, 180, 28};
    nd.z_order = 5;
    nd.stable_key = "dropdown_resolution";
    nd.component_config = &cfg_v_res_dropdown;
    nd.component_config_size = sizeof(cfg_v_res_dropdown);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_v_refresh = { "Refresh Rate" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 140, 150, 24};
    nd.z_order = 6;
    nd.stable_key = "label_refresh";
    nd.component_config = &cfg_v_refresh;
    nd.component_config_size = sizeof(cfg_v_refresh);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostDropdownConfig cfg_v_refresh_dropdown = {
        .items = { "60Hz", "75Hz", "120Hz", "144Hz", "165Hz", "240Hz" },
        .item_count = 6, .selected = 0,
    };
    nd.component_id = CID_DROPDOWN;
    nd.local_rect = (uim_Rect){420, 140, 180, 28};
    nd.z_order = 7;
    nd.stable_key = "dropdown_refresh";
    nd.component_config = &cfg_v_refresh_dropdown;
    nd.component_config_size = sizeof(cfg_v_refresh_dropdown);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_v_vsync = { "Vertical Sync" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 180, 150, 24};
    nd.z_order = 8;
    nd.stable_key = "label_vsync";
    nd.component_config = &cfg_v_vsync;
    nd.component_config_size = sizeof(cfg_v_vsync);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostToggleConfig cfg_v_vsync_toggle = { true };
    nd.component_id = CID_TOGGLE;
    nd.local_rect = (uim_Rect){420, 180, 60, 24};
    nd.z_order = 9;
    nd.stable_key = "toggle_vsync";
    nd.component_config = &cfg_v_vsync_toggle;
    nd.component_config_size = sizeof(cfg_v_vsync_toggle);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_v_framelimit = { "Frame Limit" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 220, 150, 24};
    nd.z_order = 10;
    nd.stable_key = "label_framelimit";
    nd.component_config = &cfg_v_framelimit;
    nd.component_config_size = sizeof(cfg_v_framelimit);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostDropdownConfig cfg_v_framelimit_dropdown = {
        .items = { "30", "60", "120", "144", "Unlimited" },
        .item_count = 5, .selected = 1,
    };
    nd.component_id = CID_DROPDOWN;
    nd.local_rect = (uim_Rect){420, 220, 180, 28};
    nd.z_order = 11;
    nd.stable_key = "dropdown_framelimit";
    nd.component_config = &cfg_v_framelimit_dropdown;
    nd.component_config_size = sizeof(cfg_v_framelimit_dropdown);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_v_brightness = { "Brightness" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 260, 150, 24};
    nd.z_order = 12;
    nd.stable_key = "label_brightness";
    nd.component_config = &cfg_v_brightness;
    nd.component_config_size = sizeof(cfg_v_brightness);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostSliderConfig cfg_v_brightness_slider = { 0.0f, 100.0f, 50.0f };
    nd.component_id = CID_SLIDER;
    nd.local_rect = (uim_Rect){420, 260, 200, 24};
    nd.z_order = 13;
    nd.stable_key = "slider_brightness";
    nd.component_config = &cfg_v_brightness_slider;
    nd.component_config_size = sizeof(cfg_v_brightness_slider);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    /* === Graphics Tab (7 items) === */
    static HostLabelConfig cfg_tab_graphics = { "Graphics" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){130, 20, 100, 24};
    nd.z_order = 14;
    nd.stable_key = "tab_graphics";
    nd.component_config = &cfg_tab_graphics;
    nd.component_config_size = sizeof(cfg_tab_graphics);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_g_preset = { "Quality Preset" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 300, 150, 24};
    nd.z_order = 15;
    nd.stable_key = "label_preset";
    nd.component_config = &cfg_g_preset;
    nd.component_config_size = sizeof(cfg_g_preset);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostDropdownConfig cfg_g_preset_dropdown = {
        .items = { "Low", "Medium", "High", "Ultra" },
        .item_count = 4, .selected = 1,
    };
    nd.component_id = CID_DROPDOWN;
    nd.local_rect = (uim_Rect){420, 300, 180, 28};
    nd.z_order = 16;
    nd.stable_key = "dropdown_preset";
    nd.component_config = &cfg_g_preset_dropdown;
    nd.component_config_size = sizeof(cfg_g_preset_dropdown);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_g_texture = { "Texture Quality" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 340, 150, 24};
    nd.z_order = 17;
    nd.stable_key = "label_texture";
    nd.component_config = &cfg_g_texture;
    nd.component_config_size = sizeof(cfg_g_texture);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostDropdownConfig cfg_g_texture_dropdown = {
        .items = { "Low", "Medium", "High", "Ultra" },
        .item_count = 4, .selected = 1,
    };
    nd.component_id = CID_DROPDOWN;
    nd.local_rect = (uim_Rect){420, 340, 180, 28};
    nd.z_order = 18;
    nd.stable_key = "dropdown_texture";
    nd.component_config = &cfg_g_texture_dropdown;
    nd.component_config_size = sizeof(cfg_g_texture_dropdown);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_g_shadow = { "Shadow Quality" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 380, 150, 24};
    nd.z_order = 19;
    nd.stable_key = "label_shadow";
    nd.component_config = &cfg_g_shadow;
    nd.component_config_size = sizeof(cfg_g_shadow);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostDropdownConfig cfg_g_shadow_dropdown = {
        .items = { "Off", "Low", "Medium", "High" },
        .item_count = 4, .selected = 2,
    };
    nd.component_id = CID_DROPDOWN;
    nd.local_rect = (uim_Rect){420, 380, 180, 28};
    nd.z_order = 20;
    nd.stable_key = "dropdown_shadow";
    nd.component_config = &cfg_g_shadow_dropdown;
    nd.component_config_size = sizeof(cfg_g_shadow_dropdown);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_g_aa = { "Anti-Aliasing" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 420, 150, 24};
    nd.z_order = 21;
    nd.stable_key = "label_aa";
    nd.component_config = &cfg_g_aa;
    nd.component_config_size = sizeof(cfg_g_aa);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostDropdownConfig cfg_g_aa_dropdown = {
        .items = { "Off", "FXAA", "TAA", "MSAA 2x", "MSAA 4x" },
        .item_count = 5, .selected = 1,
    };
    nd.component_id = CID_DROPDOWN;
    nd.local_rect = (uim_Rect){420, 420, 180, 28};
    nd.z_order = 22;
    nd.stable_key = "dropdown_aa";
    nd.component_config = &cfg_g_aa_dropdown;
    nd.component_config_size = sizeof(cfg_g_aa_dropdown);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostLabelConfig cfg_g_viewdist = { "View Distance" };
    nd.component_id = CID_LABEL;
    nd.local_rect = (uim_Rect){250, 460, 150, 24};
    nd.z_order = 23;
    nd.stable_key = "label_viewdist";
    nd.component_config = &cfg_g_viewdist;
    nd.component_config_size = sizeof(cfg_g_viewdist);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    static HostSliderConfig cfg_g_viewdist_slider = { 0.0f, 100.0f, 70.0f };
    nd.component_id = CID_SLIDER;
    nd.local_rect = (uim_Rect){420, 460, 200, 24};
    nd.z_order = 24;
    nd.stable_key = "slider_viewdist";
    nd.component_config = &cfg_g_viewdist_slider;
    nd.component_config_size = sizeof(cfg_g_viewdist_slider);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    /* Back button */
    static HostButtonConfig cfg_btn_back = { "< Back" };
    nd.component_id = CID_BUTTON;
    nd.local_rect = (uim_Rect){20, 650, 120, 36};
    nd.z_order = 100;
    nd.stable_key = "btn_back";
    nd.action_id = ACTION_BACK;
    nd.component_config = &cfg_btn_back;
    nd.component_config_size = sizeof(cfg_btn_back);
    rc = uim_node_create(rec, &nd, &nid);
    if (rc != uim_OK) return rc;

    printf("[uim] data model created: MainWindow + Settings pages (26 nodes)\n");
    return uim_OK;
}
