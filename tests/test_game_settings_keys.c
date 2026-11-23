/* Real key-injection contract for the UIM settings page.
 *
 * This does not test a mapping table: it calls the same
 * zcsr_game_settings_handle_key() the runtime shell calls, with ZCSR_KEY_*
 * values, and asserts tab/selection/apply/cancel/reset behavior. Native
 * Windows VK values must no longer drive the page. */
#include "zcsr_ui_page_manager/ui_game_settings.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char g_buf[64 * 1024];

static zcsr_game_settings* make_panel(zcsr_game_settings_item* items,
                                      zcsr_game_settings_tab* tabs) {
    static const char* options[] = { "Windowed", "Fullscreen" };
    zcsr_game_settings_config cfg;

    memset(items, 0, sizeof(*items) * 6);
    items[0].label = "Display";
    items[0].type = ZCSR_GAME_SETTINGS_DROPDOWN;
    items[0].options = options;
    items[0].option_count = 2;
    items[0].value = 0;
    items[0].default_value = 0;

    items[1].label = "Brightness";
    items[1].type = ZCSR_GAME_SETTINGS_SLIDER;
    items[1].min = 0.0f;
    items[1].max = 100.0f;
    items[1].step = 10.0f;
    items[1].slider_value = 50.0f;

    items[2].label = "VSync";
    items[2].type = ZCSR_GAME_SETTINGS_TOGGLE;
    items[2].toggle_value = true;

    items[3].label = "separator";
    items[3].type = ZCSR_GAME_SETTINGS_SEPARATOR;

    /* Audio tab fixture: item 4 was left zero-initialized, which is enum value
     * DROPDOWN with no options. That is an invalid item, not a valid empty tab. */
    items[4].label = "Audio";
    items[4].type = ZCSR_GAME_SETTINGS_SEPARATOR;

    memset(tabs, 0, sizeof(*tabs) * 2);
    tabs[0].name = "Video";
    tabs[0].items = items;
    tabs[0].count = 4;
    tabs[1].name = "Audio";
    tabs[1].items = &items[4];
    tabs[1].count = 1;

    memset(&cfg, 0, sizeof(cfg));
    cfg.tabs = tabs;
    cfg.tab_count = 2;
    cfg.x = 10;
    cfg.y = 10;
    cfg.width = 960;
    cfg.height = 540;
    cfg.item_height = 32;
    cfg.text_scale = 1.4f;
    return zcsr_game_settings_create(g_buf, sizeof(g_buf), &cfg);
}

static void press(zcsr_game_settings* s, int key) {
    assert(zcsr_game_settings_handle_key(s, (zcsr_keycode)key, ZCSR_KEY_DOWN) == 0);
}

int main(void) {
    zcsr_game_settings_item items[6];
    zcsr_game_settings_tab tabs[2];
    zcsr_game_settings* s;
    int value = -1;
    float slider = 0.0f;
    bool toggle = true;

    s = make_panel(items, tabs);
    assert(s);
    assert(zcsr_game_settings_active_tab(s) == 0);

    /* Q/E switch tabs; old native VK 81 (Q) must not. */
    press(s, ZCSR_KEY_E);
    assert(zcsr_game_settings_active_tab(s) == 1);
    press(s, ZCSR_KEY_Q);
    assert(zcsr_game_settings_active_tab(s) == 0);
    press(s, 81);
    assert(zcsr_game_settings_active_tab(s) == 0);

    /* Dropdown: Enter opens, Down moves hover, Enter commits. */
    press(s, ZCSR_KEY_ENTER);
    press(s, ZCSR_KEY_ARROW_DOWN);
    press(s, ZCSR_KEY_ENTER);
    zcsr_game_settings_apply(s);
    assert(zcsr_game_settings_get_item_value(s, 0, 0, &value, NULL, NULL));
    assert(value == 1);

    /* Down to slider; Right increases the pending value. */
    press(s, ZCSR_KEY_ARROW_DOWN);
    press(s, ZCSR_KEY_ARROW_RIGHT);
    zcsr_game_settings_apply(s);
    assert(zcsr_game_settings_get_item_value(s, 0, 1, NULL, &slider, NULL));
    assert(slider > 50.0f && slider <= 100.0f);

    /* Down to toggle; Space flips it. */
    press(s, ZCSR_KEY_ARROW_DOWN);
    press(s, ZCSR_KEY_SPACE);
    zcsr_game_settings_apply(s);
    assert(zcsr_game_settings_get_item_value(s, 0, 2, NULL, NULL, &toggle));
    assert(!toggle);

    /* Tab to buttons; Enter activates Apply (1). */
    press(s, ZCSR_KEY_TAB);
    assert(zcsr_game_settings_handle_key(s, ZCSR_KEY_ENTER, ZCSR_KEY_DOWN) == 1);
    zcsr_game_settings_apply(s);
    zcsr_game_settings_destroy(s);

    /* Esc in item mode is Cancel (-1). */
    s = make_panel(items, tabs);
    assert(s);
    assert(zcsr_game_settings_handle_key(s, ZCSR_KEY_ESCAPE, ZCSR_KEY_DOWN) == -1);
    zcsr_game_settings_destroy(s);

    /* Tab -> Right -> Cancel button; Enter must return -1. */
    s = make_panel(items, tabs);
    assert(s);
    press(s, ZCSR_KEY_TAB);
    press(s, ZCSR_KEY_ARROW_RIGHT);
    assert(zcsr_game_settings_handle_key(s, ZCSR_KEY_ENTER, ZCSR_KEY_DOWN) == -1);
    zcsr_game_settings_destroy(s);

    /* Tab -> Left wraps to Reset; Enter resets to defaults (0). */
    s = make_panel(items, tabs);
    assert(s);
    press(s, ZCSR_KEY_TAB);
    press(s, ZCSR_KEY_ARROW_LEFT);
    assert(zcsr_game_settings_handle_key(s, ZCSR_KEY_ENTER, ZCSR_KEY_DOWN) == 0);
    assert(zcsr_game_settings_get_item_value(s, 0, 0, &value, NULL, NULL));
    assert(value == 0);
    assert(zcsr_game_settings_get_item_value(s, 0, 2, NULL, NULL, &toggle));
    assert(toggle);
    zcsr_game_settings_destroy(s);

    /* W/S aliases move through the list; Right then edits the slider. */
    s = make_panel(items, tabs);
    assert(s);
    press(s, ZCSR_KEY_S);
    press(s, ZCSR_KEY_ARROW_RIGHT);
    zcsr_game_settings_apply(s);
    assert(zcsr_game_settings_get_item_value(s, 0, 1, NULL, &slider, NULL));
    assert(slider > 50.0f);
    zcsr_game_settings_destroy(s);

    printf("game_settings_keys: PASS\n");
    return 0;
}
