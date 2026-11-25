/* tests/test_atoms.c — one contract test for every header-only atom.
 *
 * Each section is independent; a failure prints the section name and keeps
 * going so one run reports every regression. The numbers are chosen so the
 * expected values are exact (monospace advances, integer geometry). */
#include "uim_anim.h"
#include "uim_button.h"
#include "uim_color.h"
#include "uim_dropdown.h"
#include "uim_drag.h"
#include "uim_focus.h"
#include "uim_format.h"
#include "uim_dialog.h"
#include "uim_layout.h"
#include "uim_menu.h"
#include "uim_numeric_field.h"
#include "uim_placement.h"
#include "uim_property_row.h"
#include "uim_splitter.h"
#include "uim_tooltip.h"
#include "uim_tree.h"
#include "uim_progress.h"
#include "uim_scroll.h"
#include "uim_slider.h"
#include "uim_stepper.h"
#include "uim_tabs.h"
#include "uim_toggle.h"
#include "uim_text_layout.h"
#include "uim_theme.h"
#include "uim_toast.h"
#include "uim_types.h"
#include "uim_virtual_list.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static const char *g_section = "";

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL [%s] %s:%d: %s\n", g_section, __FILE__, __LINE__, #cond); g_failures++; } \
} while (0)

#define NEAR(a, b) (fabsf((a) - (b)) <= 0.001f)

static float mono10(void *user, uint32_t codepoint) {
    (void)user;
    (void)codepoint;
    return 10.0f;
}

static void test_button(void) {
    uim_Button b;
    bool edge = false;
    g_section = "button";
    CHECK(uim_button_init(&b, 0.5f, 0.1f) == uim_OK);
    CHECK(uim_button_init(&b, -1.0f, 0.1f) == uim_ERR_ARGUMENT);
    uim_button_set_hover(&b, true);
    CHECK(uim_button_pointer_down(&b) == uim_OK && b.pressed);
    CHECK(uim_button_tick(&b, 0.49f, &edge) == uim_OK && !edge);
    CHECK(uim_button_tick(&b, 0.01f, &edge) == uim_OK && edge);
    CHECK(uim_button_tick(&b, 0.10f, &edge) == uim_OK && edge);
    CHECK(uim_button_pointer_up(&b, true, &edge) == uim_OK && edge && !b.pressed);
    CHECK(uim_button_pointer_down(&b) == uim_OK);
    CHECK(uim_button_pointer_up(&b, false, &edge) == uim_OK && !edge);
    uim_button_set_focus(&b, true);
    CHECK(uim_button_activate(&b, &edge) == uim_OK && edge);
    uim_button_set_focus(&b, false);
    CHECK(uim_button_activate(&b, &edge) == uim_OK && !edge);
}

static void test_toggle(void) {
    uim_Toggle t;
    bool changed = false;
    g_section = "toggle";
    CHECK(uim_toggle_init(&t, false) == uim_OK && !t.on);
    CHECK(uim_toggle_pointer_down(&t) == uim_OK);
    CHECK(uim_toggle_pointer_up(&t, true, &changed) == uim_OK && changed && t.on);
    CHECK(uim_toggle_pointer_down(&t) == uim_OK);
    CHECK(uim_toggle_pointer_up(&t, false, &changed) == uim_OK && !changed && t.on);
    uim_toggle_set_focus(&t, true);
    CHECK(uim_toggle_activate(&t, &changed) == uim_OK && changed && !t.on);
    CHECK(uim_toggle_set(&t, true) == uim_OK && t.on);
}

static void test_slider(void) {
    uim_Slider s;
    float value = 0.0f, x = 0.0f;
    g_section = "slider";
    CHECK(uim_slider_init(&s, 0.0f, 10.0f, 5.0f, 1.0f) == uim_OK);
    CHECK(NEAR(uim_slider_fraction(&s), 0.5f));
    CHECK(uim_slider_value_at(&s, 25.0f, 0.0f, 100.0f, &value) == uim_OK
          && NEAR(value, 2.5f));
    CHECK(uim_slider_value_at(&s, -10.0f, 0.0f, 100.0f, &value) == uim_OK
          && NEAR(value, 0.0f));
    CHECK(uim_slider_begin_drag(&s, 50.0f, 0.0f, 100.0f) == uim_OK
          && s.dragging && NEAR(s.value, 5.0f));
    CHECK(uim_slider_drag_to(&s, 100.0f, 0.0f, 100.0f) == uim_OK
          && NEAR(s.value, 10.0f));
    CHECK(uim_slider_end_drag(&s) == uim_OK && !s.dragging);
    CHECK(uim_slider_x_for(&s, 0.0f, 100.0f, &x) == uim_OK && NEAR(x, 100.0f));
    CHECK(uim_slider_step(&s, 1) == uim_ERR_RANGE && NEAR(s.value, 10.0f));
    CHECK(uim_slider_set_value(&s, -1.0f) == uim_ERR_RANGE && NEAR(s.value, 0.0f));
    CHECK(uim_slider_drag_to(&s, 1.0f, 0.0f, 100.0f) == uim_ERR_STATE);
}

static void test_dropdown(void) {
    uim_Dropdown d;
    uim_Rect items[3] = {{0, 0, 100, 20}, {0, 20, 100, 20}, {0, 40, 100, 20}};
    size_t hit = 99;
    g_section = "dropdown";
    CHECK(uim_dropdown_init(&d, 3, 2) == uim_OK && d.selected == 2 && !d.open);
    CHECK(uim_dropdown_open(&d) == uim_OK && d.open);
    CHECK(uim_dropdown_step(&d, 1) == uim_OK && d.selected == 0);
    CHECK(uim_dropdown_step(&d, -1) == uim_OK && d.selected == 2);
    d.wrap = false;
    CHECK(uim_dropdown_step(&d, 1) == uim_ERR_RANGE && d.selected == 2);
    CHECK(uim_dropdown_select(&d, 1) == uim_OK && d.selected == 1 && !d.open);
    CHECK(uim_dropdown_select(&d, 9) == uim_ERR_RANGE);
    CHECK(uim_dropdown_hit(&d, items, 3, 50.0f, 25.0f, &hit) == uim_OK && hit == 1);
    CHECK(uim_dropdown_hit(&d, items, 3, 150.0f, 25.0f, &hit) == uim_ERR_NOT_FOUND);
    CHECK(uim_dropdown_set_count(&d, 0, 0) == uim_OK && d.count == 0 && !d.open);
    CHECK(uim_dropdown_open(&d) == uim_ERR_NOT_FOUND);
}

static void test_stepper(void) {
    uim_Stepper s;
    g_section = "stepper";
    CHECK(uim_stepper_init(&s, 0.0, 10.0, 9.5, 1.0) == uim_OK
          && s.value > 9.49 && s.value < 9.51);
    CHECK(uim_stepper_step(&s, 1) == uim_ERR_RANGE && s.value > 9.99);
    CHECK(uim_stepper_step(&s, -20) == uim_ERR_RANGE && s.value < 0.01);
    CHECK(uim_stepper_set_wrap(&s, true) == uim_OK);
    CHECK(uim_stepper_step(&s, -1) == uim_OK && s.value > 8.99 && s.value < 9.01);
    CHECK(uim_stepper_set_value(&s, 4.0) == uim_OK && s.value > 3.99 && s.value < 4.01);
    CHECK(NEAR(uim_stepper_fraction(&s), 0.4f));
    CHECK(uim_stepper_init(&s, 5.0, 5.0, 5.0, 1.0) == uim_OK);
    CHECK(uim_stepper_step(&s, 1) == uim_ERR_RANGE && s.value > 4.99 && s.value < 5.01);
}

static void test_types(void) {
    uim_Rect r = {1.0f, 2.0f, 3.0f, 4.0f};
    uim_Insets in = {5.0f, 6.0f, 7.0f, 8.0f};
    uim_Point p = {9.0f, 10.0f};
    g_section = "types";
    CHECK(uim_OK == 0 && uim_ERR_UNSUPPORTED == 10);
    CHECK(r.x + r.width == 4.0f && p.y == 10.0f && in.bottom == 8.0f);
}

static void test_color(void) {
    uim_Color c, parsed, white = {1.0f, 1.0f, 1.0f, 1.0f};
    char text[16];
    g_section = "color";
    CHECK(uim_color_parse("#336699", &parsed) == uim_OK);
    CHECK(NEAR(parsed.r, 0x33 / 255.0f) && NEAR(parsed.g, 0x66 / 255.0f) && NEAR(parsed.b, 0x99 / 255.0f));
    CHECK(uim_color_parse("336699", &parsed) == uim_ERR_ARGUMENT);
    CHECK(uim_color_parse("#GGGGGG", &parsed) == uim_ERR_ARGUMENT);
    CHECK(uim_color_format(&parsed, false, text, sizeof(text)) == uim_OK);
    CHECK(strcmp(text, "#336699") == 0);
    CHECK(uim_color_format(&parsed, false, text, 7) == uim_ERR_BUFFER);
    c = uim_color_blend(uim_color_rgba(0, 0, 255, 255), uim_color_rgba(255, 0, 0, 128));
    CHECK(NEAR(c.r, 128.0f / 255.0f) && NEAR(c.b, 127.0f / 255.0f) && NEAR(c.a, 1.0f));
    CHECK(NEAR(uim_color_luminance(&white), 1.0f));
    {
        uim_Color black = {0.0f, 0.0f, 0.0f, 1.0f};
        CHECK(uim_color_is_dark(&black));
        CHECK(NEAR(uim_color_contrast_text(&white).r, 0.0f));
    }
}

static void test_layout(void) {
    uim_Rect box = {0.0f, 0.0f, 300.0f, 100.0f};
    uim_Rect out[3];
    uim_LayoutSpec spec;
    uim_LayoutItem items[3];
    g_section = "layout";

    memset(&spec, 0, sizeof(spec));
    spec.axis = uim_LAYOUT_ROW;
    spec.gap = 10.0f;
    spec.padding.left = spec.padding.top = spec.padding.right = spec.padding.bottom = 10.0f;
    spec.main_align = uim_ALIGN_START;
    spec.cross_align = uim_ALIGN_START;
    items[0].basis = 50.0f; items[0].grow = 0.0f; items[0].min_main = 0.0f; items[0].max_main = 0.0f; items[0].cross = 20.0f;
    items[1] = items[0];
    items[2] = items[0]; items[2].grow = 1.0f;
    CHECK(uim_layout_solve(&box, &spec, items, 3, out) == uim_OK);
    CHECK(NEAR(out[0].width, 50.0f) && NEAR(out[1].width, 50.0f) && NEAR(out[2].width, 160.0f));
    CHECK(NEAR(out[0].x, 10.0f) && NEAR(out[1].x, 70.0f) && NEAR(out[2].x, 130.0f));
    CHECK(NEAR(out[0].y, 10.0f) && NEAR(out[0].height, 20.0f));

    spec.padding.left = spec.padding.top = spec.padding.right = spec.padding.bottom = 0.0f;
    spec.gap = 0.0f;
    items[0].basis = 60.0f; items[0].cross = 10.0f;
    items[1].basis = 60.0f; items[1].cross = 10.0f; items[1].grow = 0.0f;
    {
        uim_Rect narrow = {0.0f, 0.0f, 100.0f, 50.0f};
        CHECK(uim_layout_solve(&narrow, &spec, items, 2, out) == uim_ERR_RANGE);
    }

    {
        uim_Rect container = {0.0f, 0.0f, 100.0f, 50.0f};
        CHECK(uim_layout_align(&container, 40, 20, uim_ALIGN_CENTER, uim_ALIGN_CENTER, out) == uim_OK);
    }
    CHECK(NEAR(out[0].x, 30.0f) && NEAR(out[0].y, 15.0f));

    {
        uim_Rect container = {0.0f, 0.0f, 100.0f, 50.0f};
        uim_Rect first, second;
        CHECK(uim_layout_split(&container, uim_LAYOUT_ROW, 0.3f, 10.0f, &first, &second) == uim_OK);
        CHECK(NEAR(first.width, 27.0f) && NEAR(second.x, 37.0f) && NEAR(second.width, 63.0f));
    }
    {
        uim_Rect container = {0.0f, 0.0f, 100.0f, 100.0f};
        uim_Insets padding = {0.0f, 0.0f, 0.0f, 0.0f};
        uim_Rect cells[4];
        CHECK(uim_layout_grid(&container, padding, 3, 10.0f, 0.0f, 20.0f, 4, cells) == uim_OK);
        CHECK(NEAR(cells[1].x, 36.6667f) && NEAR(cells[2].x, 73.3333f) && NEAR(cells[3].x, 0.0f) && NEAR(cells[3].y, 20.0f));
    }
}

static void test_theme(void) {
    uim_Theme t;
    g_section = "theme";
    CHECK(uim_theme_init(&t, uim_THEME_DARK, 2.0f, 16.0f) == uim_OK);
    CHECK(NEAR(t.fonts[uim_THEME_FONT_BODY], 32.0f));
    CHECK(NEAR(t.space[3], 32.0f) && NEAR(t.radius[uim_THEME_RADIUS_MD], 16.0f));
    CHECK(NEAR(uim_theme_color(&t, uim_THEME_SURFACE).r, 0x1E / 255.0f));
    CHECK(uim_theme_init(&t, uim_THEME_LIGHT, 0.1f, 4.0f) == uim_ERR_RANGE);
    CHECK(NEAR(t.dpi_scale, 0.5f) && NEAR(t.base_font_px, 8.0f));
    CHECK(NEAR(uim_theme_space(&t, 99), 0.0f));
}

static void test_text_layout(void) {
    uim_TextLine lines[8];
    size_t count;
    float width;
    bool truncated;
    char out[32];
    size_t out_bytes;
    g_section = "text_layout";

    CHECK(uim_text_measure("abc", 3, mono10, NULL, &width) == uim_OK && NEAR(width, 30.0f));
    CHECK(uim_text_measure("\xFF", 1, mono10, NULL, &width) == uim_ERR_ARGUMENT);

    count = 8; truncated = false;
    CHECK(uim_text_wrap("hello world", 11, 60.0f, mono10, NULL, 0, lines, &count, &truncated) == uim_OK);
    CHECK(count == 2 && lines[0].byte_length == 5 && NEAR(lines[0].width, 50.0f));
    CHECK(lines[1].byte_start == 6 && lines[1].byte_length == 5 && !truncated);

    count = 8; truncated = false;
    CHECK(uim_text_wrap("ab\ncd", 5, 100.0f, mono10, NULL, 0, lines, &count, &truncated) == uim_OK);
    CHECK(count == 2 && lines[0].hard_break && lines[0].byte_length == 2 && lines[1].byte_length == 2);

    count = 8; truncated = false;
    CHECK(uim_text_wrap("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95", 12, 30.0f, mono10, NULL, 0, lines, &count, &truncated) == uim_OK);
    CHECK(count == 2 && lines[0].byte_length == 9 && NEAR(lines[0].width, 30.0f));

    count = 8; truncated = false;
    CHECK(uim_text_wrap("hello world", 11, 60.0f, mono10, NULL, 1, lines, &count, &truncated) == uim_OK);
    CHECK(count == 1 && truncated);

    CHECK(uim_text_ellipsize("abcdef", 6, 45.0f, mono10, NULL, NULL, out, sizeof(out), &out_bytes) == uim_OK);
    CHECK(strcmp(out, "abc\xE2\x80\xA6") == 0 && out_bytes == 6);
    CHECK(uim_text_ellipsize("abcdef", 6, 45.0f, mono10, NULL, NULL, out, 5, &out_bytes) == uim_ERR_BUFFER);
    CHECK(uim_text_ellipsize("abc", 3, 100.0f, mono10, NULL, NULL, out, sizeof(out), &out_bytes) == uim_OK);
    CHECK(strcmp(out, "abc") == 0 && out_bytes == 3);
}

static void test_scroll(void) {
    uim_ScrollState s;
    uim_ScrollbarMetrics m;
    uim_Rect item = {0.0f, 250.0f, 10.0f, 30.0f};
    g_section = "scroll";
    CHECK(uim_scroll_init(&s, 100.0f, 100.0f, 48.0f) == uim_OK);
    CHECK(uim_scroll_set_content(&s, 100.0f, 300.0f) == uim_OK);
    CHECK(NEAR(uim_scroll_max_y(&s), 200.0f));
    CHECK(uim_scroll_by_wheel(&s, 1.0f, 0.0f) == uim_OK && NEAR(s.offset_y, 48.0f));
    CHECK(uim_scroll_set_offset(&s, 0.0f, 999.0f) == uim_ERR_RANGE && NEAR(s.offset_y, 200.0f));
    CHECK(uim_scroll_set_offset(&s, 0.0f, 100.0f) == uim_OK);
    CHECK(uim_scrollbar_metrics(&s, uim_LAYOUT_COLUMN, 100.0f, &m) == uim_OK);
    CHECK(m.visible && NEAR(m.thumb_length, 33.3333f) && NEAR(m.thumb_offset, 33.3333f));
    CHECK(uim_scrollbar_drag(&s, uim_LAYOUT_COLUMN, 100.0f, 10.0f) == uim_OK);
    CHECK(NEAR(s.offset_y, 130.0f));
    CHECK(uim_scroll_reveal(&s, item, 4.0f) == uim_OK);
    CHECK(NEAR(s.offset_y, 184.0f));
    {
        uim_Rect top_item = {0.0f, 0.0f, 10.0f, 30.0f};
        CHECK(uim_scroll_reveal(&s, top_item, 4.0f) == uim_OK);
    }
    CHECK(NEAR(s.offset_y, 0.0f));
}

static void test_focus(void) {
    uim_FocusRing ring;
    uint32_t ids[3] = {10u, 20u, 30u};
    uim_Rect rects[3] = {{0, 0, 90, 20}, {100, 0, 90, 20}, {200, 0, 90, 20}};
    uint32_t hit = 0;
    g_section = "focus";
    CHECK(uim_focus_init(&ring) == uim_OK);
    CHECK(uim_focus_set_items(&ring, ids, rects, 3) == uim_OK);
    CHECK(uim_focus_set_current(&ring, 20u) == uim_OK);
    CHECK(uim_focus_next(&ring, 1) == uim_OK && uim_focus_current(&ring) == 30u);
    CHECK(uim_focus_next(&ring, 1) == uim_OK && uim_focus_current(&ring) == 10u);
    CHECK(uim_focus_move(&ring, 1.0f, 0.0f) == uim_OK && uim_focus_current(&ring) == 20u);
    CHECK(uim_focus_move(&ring, 0.0f, -1.0f) == uim_ERR_NOT_FOUND);
    CHECK(uim_focus_hit(&ring, 150.0f, 10.0f, &hit) == uim_OK && hit == 20u);
    CHECK(uim_focus_set_items(&ring, ids, rects, 3) == uim_OK && uim_focus_current(&ring) == 20u);
    CHECK(uim_focus_set_current(&ring, 99u) == uim_ERR_NOT_FOUND);
    {
        uint32_t dup[2] = {10u, 10u};
        uim_Rect dup_rects[2] = {{0, 0, 20, 20}, {20, 0, 20, 20}};
        CHECK(uim_focus_set_items(&ring, dup, dup_rects, 2) == uim_ERR_EXISTS);
        CHECK(ring.count == 3 && uim_focus_current(&ring) == 20u);
    }
    CHECK(uim_focus_set_items(&ring, ids, NULL, 3) == uim_OK);
    CHECK(uim_focus_move(&ring, 1.0f, 0.0f) == uim_ERR_STATE);
}

static void test_placement(void) {
    uim_Placement p;
    uim_Rect anchors[3] = {{100, 100, 40, 20}, {100, 170, 40, 20}, {0, 0, 10, 10}};
    uim_Rect bounds = {0, 0, 300, 200};
    uim_Rect small_bounds = {0, 0, 100, 100};
    g_section = "placement";
    CHECK(uim_place_popup(&anchors[0], 80, 60, &bounds, 4.0f, uim_PLACE_BELOW, &p) == uim_OK);
    CHECK(p.side == uim_PLACE_BELOW && !p.flipped && !p.clamped);
    CHECK(NEAR(p.rect.x, 80.0f) && NEAR(p.rect.y, 124.0f) && NEAR(p.arrow, 40.0f));
    CHECK(uim_place_popup(&anchors[1], 80, 60, &bounds, 4.0f, uim_PLACE_BELOW, &p) == uim_OK);
    CHECK(p.side == uim_PLACE_ABOVE && p.flipped && NEAR(p.rect.y, 106.0f));
    CHECK(uim_place_popup(&anchors[2], 400, 400, &small_bounds, 0.0f, uim_PLACE_BELOW, &p) == uim_ERR_RANGE);
    CHECK(p.clamped && NEAR(p.rect.x, 0.0f) && NEAR(p.rect.y, 0.0f));
}

static void test_format(void) {
    char out[64];
    g_section = "format";
    CHECK(uim_format_int(1234567, true, out, sizeof(out)) == uim_OK && strcmp(out, "1,234,567") == 0);
    CHECK(uim_format_int(-1000, true, out, sizeof(out)) == uim_OK && strcmp(out, "-1,000") == 0);
    CHECK(uim_format_int(42, false, out, 2) == uim_ERR_BUFFER);
    CHECK(uim_format_fixed(12.346, 2, false, out, sizeof(out)) == uim_OK && strcmp(out, "12.35") == 0);
    CHECK(uim_format_fixed(1234.5, 1, true, out, sizeof(out)) == uim_OK && strcmp(out, "1,234.5") == 0);
    CHECK(uim_format_percent(0.125, 1, out, sizeof(out)) == uim_OK && strcmp(out, "12.5%") == 0);
    CHECK(uim_format_bytes(1536.0, true, out, sizeof(out)) == uim_OK && strcmp(out, "1.5 KB") == 0);
    CHECK(uim_format_duration(3725.0, out, sizeof(out)) == uim_OK && strcmp(out, "1:02:05") == 0);
    CHECK(uim_format_duration(3.4, out, sizeof(out)) == uim_OK && strcmp(out, "3.4s") == 0);
}

static void test_progress(void) {
    uim_Progress p;
    float start = -1.0f;
    g_section = "progress";
    CHECK(uim_progress_init(&p, 0.0f, 100.0f) == uim_OK);
    CHECK(uim_progress_set_value(&p, 150.0f) == uim_ERR_RANGE && NEAR(uim_progress_fraction(&p), 1.0f));
    CHECK(uim_progress_set_value(&p, 40.0f) == uim_OK && NEAR(uim_progress_fraction(&p), 0.4f));
    CHECK(uim_progress_set_indeterminate(&p, true) == uim_OK);
    CHECK(uim_progress_set_cycle(&p, 2.0f) == uim_OK);
    CHECK(uim_progress_advance(&p, 0.5f) == uim_OK && NEAR(p.phase, 0.25f));
    CHECK(uim_progress_highlight(&p, 0.3f, &start) == uim_OK && NEAR(start, 0.025f));
}

static void test_virtual_list(void) {
    uim_VirtualList l;
    size_t first = 99, last = 99, index = 99;
    uim_Rect r;
    g_section = "virtual_list";
    CHECK(uim_virtual_init(&l, 100, 20.0f, 0.0f, 100.0f, 0) == uim_OK);
    CHECK(NEAR(uim_virtual_total_height(&l), 2000.0f));
    CHECK(uim_virtual_range(&l, &first, &last) == uim_OK && first == 0 && last == 5);
    CHECK(uim_virtual_set_offset(&l, 200.0f) == uim_OK);
    CHECK(uim_virtual_range(&l, &first, &last) == uim_OK && first == 10 && last == 15);
    l.overscan = 2;
    CHECK(uim_virtual_range(&l, &first, &last) == uim_OK && first == 8 && last == 17);
    CHECK(uim_virtual_item_rect(&l, 10, 0.0f, 100.0f, &r) == uim_OK && NEAR(r.y, 0.0f));
    CHECK(uim_virtual_index_at(&l, 50.0f, &index) == uim_OK && index == 12);
    CHECK(uim_virtual_scroll_to(&l, 50, 0.5f) == uim_OK && NEAR(l.offset, 960.0f));
}

static void test_anim(void) {
    uim_Tween tw;
    g_section = "anim";
    CHECK(NEAR(uim_ease(uim_EASE_LINEAR, 0.5f), 0.5f));
    CHECK(NEAR(uim_ease(uim_EASE_QUAD_IN, 0.5f), 0.25f));
    CHECK(NEAR(uim_ease(uim_EASE_QUAD_OUT, 0.5f), 0.75f));
    CHECK(NEAR(uim_ease(uim_EASE_BACK_OUT, 0.0f), 0.0f));
    CHECK(NEAR(uim_ease(uim_EASE_BACK_OUT, 1.0f), 1.0f));
    CHECK(NEAR(uim_ease(uim_EASE_LINEAR, 2.0f), 1.0f));
    CHECK(uim_tween_start(&tw, 0.0f, 100.0f, 2.0f, uim_EASE_LINEAR) == uim_OK);
    CHECK(!tw.loop && !tw.ping_pong);
    CHECK(uim_tween_tick(&tw, 1.0f) == uim_OK && NEAR(tw.value, 50.0f) && !uim_tween_done(&tw));
    CHECK(uim_tween_tick(&tw, 1.0f) == uim_OK && NEAR(tw.value, 100.0f) && uim_tween_done(&tw));
    CHECK(uim_tween_start(&tw, 0.0f, 100.0f, 1.0f, uim_EASE_LINEAR) == uim_OK);
    CHECK(uim_tween_set_loop(&tw, true, true) == uim_OK);
    CHECK(uim_tween_tick(&tw, 0.5f) == uim_OK && NEAR(tw.value, 50.0f));
    CHECK(uim_tween_tick(&tw, 1.0f) == uim_OK && NEAR(tw.value, 50.0f));
    CHECK(uim_tween_tick(&tw, 0.5f) == uim_OK && NEAR(tw.value, 0.0f) && !uim_tween_done(&tw));
}

static void test_tabs(void) {
    uim_Tabs t;
    uim_Rect rects[3] = {{0, 0, 50, 20}, {50, 0, 50, 20}, {100, 0, 50, 20}};
    size_t index = 99;
    g_section = "tabs";
    CHECK(uim_tabs_init(&t, 3, 0) == uim_OK);
    CHECK(uim_tabs_step(&t, -1) == uim_OK && t.active == 2);
    CHECK(uim_tabs_select(&t, 5) == uim_ERR_RANGE && t.active == 2);
    CHECK(uim_tabs_step(&t, 1) == uim_OK && t.active == 0);
    t.wrap = false;
    CHECK(uim_tabs_step(&t, -1) == uim_ERR_RANGE && t.active == 0);
    CHECK(uim_tabs_hit(&t, rects, 3, 75.0f, 10.0f, &index) == uim_OK && index == 1);
    CHECK(uim_tabs_hit(&t, rects, 3, 500.0f, 10.0f, &index) == uim_ERR_NOT_FOUND);
}

static void test_drag(void) {
    uim_DragState d;
    float dx = 99.0f, dy = 99.0f;
    g_section = "drag";
    CHECK(uim_drag_init(&d, 3.0f) == uim_OK);
    CHECK(uim_drag_set_axis_lock(&d, true) == uim_OK);
    CHECK(uim_drag_begin(&d, 7u, 0.0f, 0.0f) == uim_OK);
    CHECK(uim_drag_move(&d, 2.0f, 1.0f) == uim_OK && !d.active);
    CHECK(uim_drag_move(&d, 5.0f, 1.0f) == uim_OK && d.active && d.axis == uim_LAYOUT_ROW);
    CHECK(NEAR(uim_drag_dx(&d), 5.0f) && NEAR(uim_drag_dy(&d), 0.0f));
    CHECK(uim_drag_end(&d, 6.0f, 2.0f, &dx, &dy) == uim_OK);
    CHECK(NEAR(dx, 6.0f) && NEAR(dy, 0.0f) && !d.pressed && !d.active);
    CHECK(uim_drag_begin(&d, 8u, 0.0f, 0.0f) == uim_OK);
    CHECK(uim_drag_move(&d, 2.0f, 1.0f) == uim_OK && !d.active);
    CHECK(uim_drag_end(&d, 2.0f, 1.0f, &dx, &dy) == uim_ERR_STATE);
    CHECK(uim_drag_move(&d, 5.0f, 0.0f) == uim_ERR_STATE);
}

static void test_toast(void) {
    uim_ToastQueue q;
    size_t i;
    g_section = "toast";
    CHECK(uim_toast_init(&q) == uim_OK);
    for (i = 0; i < UIM_TOAST_CAPACITY; i++)
        CHECK(uim_toast_push(&q, (uint32_t)i, 3.0f, (int)i) == uim_OK);
    CHECK(q.count == UIM_TOAST_CAPACITY);
    CHECK(uim_toast_push(&q, 99u, 3.0f, 0) == uim_OK && q.count == UIM_TOAST_CAPACITY);
    CHECK(uim_toast_find(&q, 0u, &i) == uim_ERR_NOT_FOUND);
    CHECK(uim_toast_find(&q, 99u, &i) == uim_OK);
    CHECK(uim_toast_push(&q, 1u, 5.0f, 8) == uim_OK);
    CHECK(uim_toast_find(&q, 1u, &i) == uim_OK && NEAR(q.entries[i].remaining, 5.0f));
    CHECK(uim_toast_tick(&q, 1.0f) == uim_OK && NEAR(q.entries[i].remaining, 4.0f));
    CHECK(uim_toast_dismiss(&q, 1u) == uim_OK);
    CHECK(uim_toast_dismiss(&q, 1u) == uim_ERR_NOT_FOUND);
    CHECK(uim_toast_tick(&q, 10.0f) == uim_OK && q.count == 0);
}

static void test_numeric_field(void) {
    uim_NumericField f;
    double dv = 0.0;
    int64_t iv = 0;
    g_section = "numeric_field";
    CHECK(uim_numeric_field_init(&f, uim_NUMERIC_FLOAT, 0.0, 100.0, 50.0, 5.0) == uim_OK);
    CHECK(NEAR(f.value, 50.0) && !uim_numeric_field_is_dirty(&f));
    CHECK(uim_numeric_field_init(&f, uim_NUMERIC_FLOAT, 0.0, 100.0, 200.0, 5.0) == uim_ERR_RANGE);
    CHECK(NEAR(f.value, 100.0));
    CHECK(uim_numeric_field_step(&f, 1) == uim_ERR_RANGE && NEAR(f.value, 100.0));
    CHECK(uim_numeric_field_step(&f, -1) == uim_OK && NEAR(f.value, 95.0) && f.dirty);
    CHECK(uim_numeric_field_drag_by(&f, -200.0) == uim_ERR_RANGE && NEAR(f.value, 0.0));
    CHECK(uim_numeric_field_set_read_only(&f, true) == uim_OK);
    CHECK(uim_numeric_field_set_value(&f, 10.0) == uim_ERR_STATE && NEAR(f.value, 0.0));
    CHECK(uim_numeric_field_set_read_only(&f, false) == uim_OK);
    CHECK(uim_numeric_field_parse(&f, "37.5", 4) == uim_OK && NEAR(f.value, 37.5));
    CHECK(uim_numeric_field_parse(&f, "abc", 3) == uim_ERR_TYPE && NEAR(f.value, 37.5));
    CHECK(uim_numeric_field_parse(&f, " 12", 3) == uim_ERR_TYPE);
    CHECK(uim_numeric_field_parse(&f, "1e", 2) == uim_ERR_TYPE);
    CHECK(uim_numeric_field_parse(&f, "200", 3) == uim_ERR_RANGE && NEAR(f.value, 37.5));
    CHECK(uim_numeric_field_commit(&f) == uim_OK && !f.dirty);
    CHECK(uim_numeric_field_set_value(&f, 12.0) == uim_OK && f.dirty);
    CHECK(uim_numeric_field_cancel(&f) == uim_OK && NEAR(f.value, 37.5) && !f.dirty);
    {
        uim_NumericField fi;
        CHECK(uim_numeric_field_init(&fi, uim_NUMERIC_INT, -10, 10, 0, 1.0) == uim_OK);
        CHECK(uim_numeric_field_parse(&fi, "12.5", 4) == uim_ERR_TYPE);
        CHECK(uim_numeric_field_parse(&fi, "-7", 2) == uim_OK);
        CHECK(uim_numeric_field_get_int64(&fi, &iv) == uim_OK && iv == -7);
        CHECK(uim_numeric_field_get_double(&fi, &dv) == uim_OK && NEAR(dv, -7.0));
    }
    CHECK(uim_numeric_field_init(NULL, uim_NUMERIC_FLOAT, 0, 1, 0, 1.0) == uim_ERR_ARGUMENT);
}

static void test_dialog(void) {
    uim_DialogStack st;
    const uim_DialogFrame *top = NULL;
    uim_DialogResult res = uim_DIALOG_RESULT_NONE;
    uint32_t restored = 0u;
    size_t i;
    g_section = "dialog";
    CHECK(uim_dialog_init(&st) == uim_OK);
    CHECK(uim_dialog_open(&st, 1u, true, 5u, NULL) == uim_OK);
    CHECK(uim_dialog_modal_blocked(&st));
    CHECK(uim_dialog_open(&st, 1u, true, 5u, NULL) == uim_ERR_EXISTS);
    CHECK(uim_dialog_open(&st, 2u, false, 0u, NULL) == uim_OK);
    CHECK(uim_dialog_top(&st, &top) == uim_OK && top->id == 2u);
    CHECK(uim_dialog_resolve(&st, 2u, uim_DIALOG_RESULT_CANCELLED) == uim_OK);
    CHECK(uim_dialog_dismiss_closed(&st) == uim_OK);
    CHECK(uim_dialog_top(&st, &top) == uim_OK && top->id == 1u && top->modal);
    CHECK(uim_dialog_result(&st, 1u, &res) == uim_OK && res == uim_DIALOG_RESULT_NONE);
    CHECK(uim_dialog_accept_top(&st) == uim_OK);
    CHECK(uim_dialog_result(&st, 1u, &res) == uim_OK && res == uim_DIALOG_RESULT_ACCEPTED);
    CHECK(uim_dialog_dismiss_closed(&st) == uim_OK);
    CHECK(uim_dialog_focus_to_restore(&st, &restored) == uim_OK && restored == 5u);
    CHECK(uim_dialog_cancel_top(&st) == uim_ERR_NOT_FOUND);
    uim_dialog_clear(&st);
    for (i = 1u; i <= UIM_DIALOG_CAPACITY; i++)
        CHECK(uim_dialog_open(&st, i, false, 0u, NULL) == uim_OK);
    CHECK(uim_dialog_open(&st, 99u, false, 0u, NULL) == uim_ERR_BUFFER);
    CHECK(uim_dialog_init(NULL) == uim_ERR_ARGUMENT);
}

static void test_menu(void) {
    uim_Menu m;
    uim_MenuItem items[5];
    uim_Rect rects[5] = {{0, 0, 60, 20}, {0, 20, 60, 20}, {0, 40, 60, 20},
                         {0, 60, 60, 20}, {0, 80, 60, 20}};
    size_t idx = 99;
    uint32_t cmd = 0u;
    g_section = "menu";
    items[0].id = 1u; items[0].parent = 0u; items[0].submenu = 10u;
    items[0].label = "File"; items[0].flags = uim_MENU_ITEM_ENABLED | uim_MENU_ITEM_SUBMENU;
    items[1].id = 2u; items[1].parent = 1u; items[1].submenu = 0u;
    items[1].label = "New"; items[1].flags = uim_MENU_ITEM_ENABLED;
    items[2].id = 3u; items[2].parent = 1u; items[2].submenu = 0u;
    items[2].label = "Open"; items[2].flags = uim_MENU_ITEM_ENABLED;
    items[3].id = 4u; items[3].parent = 1u; items[3].submenu = 0u;
    items[3].label = NULL; items[3].flags = uim_MENU_ITEM_SEPARATOR;
    items[4].id = 5u; items[4].parent = 1u; items[4].submenu = 0u;
    items[4].label = "Quit"; items[4].flags = 0u;
    CHECK(uim_menu_init(&m) == uim_OK);
    CHECK(uim_menu_set_items(&m, items, 5) == uim_OK);
    CHECK(uim_menu_open(&m, 99u) == uim_ERR_NOT_FOUND);
    CHECK(uim_menu_open(&m, 1u) == uim_OK);
    CHECK(m.active_index == 1u); /* first child selected on open */
    CHECK(uim_menu_step(&m, 1) == uim_OK && m.active_index == 2u);
    CHECK(uim_menu_step(&m, 1) == uim_OK && m.active_index == 1u); /* wrap, skip sep/disabled */
    CHECK(uim_menu_hit(&m, rects, 5, 10.0f, 105.0f, &idx) == uim_ERR_NOT_FOUND);
    CHECK(uim_menu_hit(&m, rects, 5, 10.0f, 50.0f, &idx) == uim_OK && idx == 2u);
    CHECK(uim_menu_activate(&m, &cmd) == uim_OK && cmd == 3u && !uim_menu_is_open(&m));
    {
        uim_MenuItem root[2];
        root[0].id = 10u; root[0].parent = 0u; root[0].submenu = 0u;
        root[0].label = "Copy"; root[0].flags = uim_MENU_ITEM_ENABLED;
        root[1].id = 11u; root[1].parent = 0u; root[1].submenu = 0u;
        root[1].label = "Paste"; root[1].flags = uim_MENU_ITEM_ENABLED;
        CHECK(uim_menu_set_items(&m, root, 2) == uim_OK);
        CHECK(uim_menu_open(&m, 0u) == uim_OK); /* context menu at root */
        CHECK(uim_menu_step(&m, 1) == uim_OK);
        CHECK(uim_menu_activate(&m, &cmd) == uim_OK && cmd == 11u);
    }
    CHECK(uim_menu_set_items(&m, items, 5) == uim_OK);
    items[2].id = 2u;
    CHECK(uim_menu_set_items(&m, items, 5) == uim_ERR_EXISTS);
}

static void test_tree(void) {
    uim_Tree t;
    uint32_t ids[3] = {1u, 2u, 3u};
    uim_Rect rects[3] = {{0, 0, 80, 20}, {0, 20, 80, 20}, {0, 40, 80, 20}};
    uint32_t sel = 0u, hit = 0u;
    bool expanded = false;
    g_section = "tree";
    CHECK(uim_tree_init(&t) == uim_OK);
    CHECK(uim_tree_expand(&t, 1u) == uim_OK);
    CHECK(uim_tree_expand(&t, 1u) == uim_ERR_EXISTS);
    CHECK(uim_tree_is_expanded(&t, 1u));
    CHECK(uim_tree_toggle(&t, 1u, &expanded) == uim_OK && !expanded);
    CHECK(uim_tree_toggle(&t, 1u, &expanded) == uim_OK && expanded);
    CHECK(uim_tree_select(&t, 2u) == uim_OK);
    CHECK(uim_tree_get_selected(&t, &sel) == uim_OK && sel == 2u);
    CHECK(uim_tree_hit(&t, ids, rects, 3, 10.0f, 50.0f, &hit) == uim_OK && hit == 3u);
    CHECK(uim_tree_select(&t, 0u) == uim_OK);
    CHECK(uim_tree_get_selected(&t, &sel) == uim_ERR_NOT_FOUND);
    CHECK(uim_tree_collapse(&t, 9u) == uim_ERR_NOT_FOUND);
    uim_tree_collapse_all(&t);
    CHECK(!uim_tree_is_expanded(&t, 1u));
    CHECK(uim_tree_init(NULL) == uim_ERR_ARGUMENT);
}

static void test_tooltip(void) {
    uim_Tooltip tip;
    g_section = "tooltip";
    CHECK(uim_tooltip_init(&tip, 0.25f) == uim_OK);
    CHECK(uim_tooltip_init(&tip, -1.0f) == uim_ERR_ARGUMENT);
    CHECK(uim_tooltip_set_hover(&tip, 1u, true) == uim_OK);
    CHECK(uim_tooltip_tick(&tip, 0.10f) == uim_OK && !uim_tooltip_is_visible(&tip));
    CHECK(uim_tooltip_tick(&tip, 0.20f) == uim_OK && uim_tooltip_is_visible(&tip));
    CHECK(uim_tooltip_set_hover(&tip, 2u, true) == uim_OK && !uim_tooltip_is_visible(&tip));
    CHECK(uim_tooltip_tick(&tip, 0.05f) == uim_OK && !uim_tooltip_is_visible(&tip));
    uim_tooltip_dismiss(&tip);
    CHECK(!uim_tooltip_is_visible(&tip));
    CHECK(uim_tooltip_set_hover(&tip, 3u, false) == uim_OK);
    CHECK(uim_tooltip_tick(&tip, 1.0f) == uim_OK && !uim_tooltip_is_visible(&tip));
}

static void test_property_row(void) {
    uim_PropertyRow row;
    uim_Rect container = {0.0f, 0.0f, 200.0f, 24.0f};
    uim_Rect label, editor;
    g_section = "property_row";
    CHECK(uim_property_row_init(&row, 80.0f) == uim_OK);
    CHECK(uim_property_row_layout(&row, &container, 8.0f, &label, &editor) == uim_OK);
    CHECK(NEAR(label.width, 80.0f) && NEAR(editor.x, 88.0f) && NEAR(editor.width, 112.0f));
    CHECK(uim_property_row_interactive(&row));
    CHECK(uim_property_row_set_read_only(&row, true) == uim_OK);
    CHECK(!uim_property_row_interactive(&row));
    CHECK(uim_property_row_set_read_only(&row, false) == uim_OK);
    CHECK(uim_property_row_set_enabled(&row, false) == uim_OK);
    CHECK(!uim_property_row_interactive(&row));
    {
        uim_Rect narrow = {0.0f, 0.0f, 60.0f, 24.0f};
        CHECK(uim_property_row_layout(&row, &narrow, 8.0f, &label, &editor) == uim_OK);
        CHECK(NEAR(editor.width, 0.0f));
    }
    CHECK(uim_property_row_init(NULL, 80.0f) == uim_ERR_ARGUMENT);
}

static void test_splitter(void) {
    uim_Splitter s;
    uim_Rect container = {0.0f, 0.0f, 220.0f, 100.0f};
    uim_Rect first, second;
    float ratio = 0.0f;
    g_section = "splitter";
    CHECK(uim_splitter_init(&s, uim_LAYOUT_ROW, 0.25f, 20.0f, 20.0f) == uim_OK);
    CHECK(uim_splitter_ratio_at(&s, &container, 4.0f, 0.0f, &ratio) == uim_ERR_RANGE);
    CHECK(NEAR(ratio, 20.0f / 216.0f));
    CHECK(uim_splitter_begin_drag(&s) == uim_OK);
    CHECK(uim_splitter_drag_to(&s, &container, 4.0f, 110.0f) == uim_OK);
    CHECK(NEAR(s.ratio, 110.0f / 216.0f));
    CHECK(uim_splitter_pane_rects(&s, &container, 4.0f, &first, &second) == uim_OK);
    CHECK(NEAR(first.width, 110.0f) && NEAR(second.x, 114.0f));
    uim_splitter_end_drag(&s);
    CHECK(uim_splitter_drag_to(&s, &container, 4.0f, 50.0f) == uim_ERR_STATE);
    CHECK(uim_splitter_init(&s, uim_LAYOUT_COLUMN, 0.5f, 10.0f, 10.0f) == uim_OK);
    CHECK(uim_splitter_init(&s, uim_LAYOUT_ROW, 0.5f, -1.0f, 10.0f) == uim_ERR_ARGUMENT);
}

int main(void) {
    test_button();
    test_toggle();
    test_slider();
    test_dropdown();
    test_stepper();
    test_types();
    test_color();
    test_layout();
    test_theme();
    test_text_layout();
    test_scroll();
    test_focus();
    test_placement();
    test_format();
    test_progress();
    test_virtual_list();
    test_anim();
    test_tabs();
    test_drag();
    test_toast();
    test_numeric_field();
    test_dialog();
    test_menu();
    test_tree();
    test_tooltip();
    test_property_row();
    test_splitter();
    if (g_failures == 0) { printf("uim atoms: all contracts passed\n"); return 0; }
    printf("uim atoms: %d failure(s)\n", g_failures);
    return 1;
}
