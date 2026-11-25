/* tests/test_atoms_stress.c — deterministic property checks for the atoms.
 *
 * Not an example: every iteration generates hostile-but-valid inputs and
 * asserts the invariants each atom promises (finite geometry, clamped offsets,
 * UTF-8-safe line ranges, bounded queues). A failure prints the section and
 * iteration; the LCG seed is fixed so a failure is reproducible.
 */
#include "uim_anim.h"
#include "uim_drag.h"
#include "uim_focus.h"
#include "uim_format.h"
#include "uim_layout.h"
#include "uim_numeric_field.h"
#include "uim_placement.h"
#include "uim_progress.h"
#include "uim_scroll.h"
#include "uim_splitter.h"
#include "uim_tabs.h"
#include "uim_text_layout.h"
#include "uim_toast.h"
#include "uim_types.h"
#include "uim_virtual_list.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static const char *g_section = "";
static uint32_t g_rng = 0xC0FFEE31u;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL [%s] iter=%u %s:%d: %s\n", g_section, g_iter, __FILE__, __LINE__, #cond); g_failures++; } \
} while (0)
#define NEAR(a, b) (fabsf((a) - (b)) <= 0.001f)

static unsigned g_iter = 0;

static uint32_t rnd_u32(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static float rf(float lo, float hi) {
    return lo + (hi - lo) * (float)(rnd_u32() % 10001u) / 10000.0f;
}

static size_t ri(size_t lo, size_t hi) {
    if (hi <= lo) return lo;
    return lo + (size_t)(rnd_u32() % (uint32_t)(hi - lo + 1u));
}

static float mono10(void *user, uint32_t cp) { (void)user; (void)cp; return 10.0f; }

static void stress_layout(void) {
    size_t iter;
    g_section = "layout-stress";
    for (iter = 0; iter < 2000; iter++) {
        uim_Rect box;
        uim_LayoutSpec spec;
        uim_LayoutItem items[6];
        uim_Rect out[6];
        size_t count, i;
        float main_len;
        uim_Result rc;
        g_iter = (unsigned)iter;

        box.x = rf(-100.0f, 100.0f);
        box.y = rf(-100.0f, 100.0f);
        box.width = rf(0.0f, 500.0f);
        box.height = rf(0.0f, 300.0f);
        memset(&spec, 0, sizeof(spec));
        spec.axis = (rnd_u32() & 1u) ? uim_LAYOUT_COLUMN : uim_LAYOUT_ROW;
        spec.gap = rf(0.0f, 20.0f);
        spec.padding.left = rf(0.0f, 24.0f);
        spec.padding.top = rf(0.0f, 24.0f);
        spec.padding.right = rf(0.0f, 24.0f);
        spec.padding.bottom = rf(0.0f, 24.0f);
        spec.main_align = (uim_LayoutAlign)(rnd_u32() % 4u);
        spec.cross_align = (uim_LayoutAlign)(rnd_u32() % 4u);
        count = ri(1u, 6u);
        for (i = 0; i < count; i++) {
            items[i].basis = rf(0.0f, 120.0f);
            items[i].grow = (rnd_u32() % 3u == 0u) ? 0.0f : rf(0.0f, 3.0f);
            items[i].min_main = 0.0f;
            items[i].max_main = (rnd_u32() & 1u) ? 0.0f : rf(0.0f, 180.0f);
            items[i].cross = (rnd_u32() & 1u) ? 0.0f : rf(0.0f, 120.0f);
        }
        rc = uim_layout_solve(&box, &spec, items, count, out);
        CHECK(rc == uim_OK || rc == uim_ERR_RANGE);
        for (i = 0; i < count; i++) {
            CHECK(isfinite(out[i].x) && isfinite(out[i].y));
            CHECK(isfinite(out[i].width) && isfinite(out[i].height));
            CHECK(out[i].width >= 0.0f && out[i].height >= 0.0f);
        }
        if (rc == uim_OK) {
            uim_Rect content = uim_layout_inset(&box, spec.padding);
            main_len = spec.axis == uim_LAYOUT_ROW ? content.width : content.height;
            {
                float used = spec.gap * (float)(count - 1u);
                for (i = 0; i < count; i++)
                    used += spec.axis == uim_LAYOUT_ROW ? out[i].width : out[i].height;
                CHECK(used <= main_len + 0.01f);
            }
        }

        {
            uim_Rect cells[8];
            size_t cols = ri(1u, 4u);
            size_t n = ri(0u, 8u);
            rc = uim_layout_grid(&box, spec.padding, cols, rf(0.0f, 20.0f),
                                 rf(0.0f, 20.0f), rf(1.0f, 40.0f), n, cells);
            CHECK(rc == uim_OK);
            for (i = 0; i < n; i++) {
                CHECK(isfinite(cells[i].x) && isfinite(cells[i].y));
                CHECK(cells[i].width >= 0.0f && cells[i].height >= 0.0f);
            }
        }
    }
}

static void stress_text(void) {
    static const char *samples[] = {
        "hello world", "a b c d e f", "longwordwithoutspaces",
        "\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95\xE6\x96\x87\xE6\x9C\xAC",
        "ab\ncd", "x\r\ny", "   ", ""
    };
    size_t iter;
    g_section = "text-stress";
    for (iter = 0; iter < 2000; iter++) {
        const char *text = samples[rnd_u32() % (sizeof(samples) / sizeof(samples[0]))];
        size_t bytes = strlen(text);
        uim_TextLine lines[8];
        size_t count = 8, i, previous_end = 0;
        bool truncated = false;
        float max_width = rf(0.0f, 140.0f);
        size_t max_lines = ri(0u, 8u);
        uim_Result rc;
        g_iter = (unsigned)iter;

        rc = uim_text_wrap(text, bytes, max_width, mono10, NULL, max_lines,
                           lines, &count, &truncated);
        CHECK(rc == uim_OK || rc == uim_ERR_BUFFER);
        CHECK(count <= 8u);
        if (max_lines > 0u) CHECK(count <= max_lines);
        for (i = 0; i < count; i++) {
            CHECK(lines[i].byte_start >= previous_end);
            CHECK(lines[i].byte_start + lines[i].byte_length <= bytes);
            CHECK(isfinite(lines[i].width) && lines[i].width >= 0.0f);
            if (lines[i].byte_length > 0u) {
                CHECK(lines[i].width <= max_width + 10.001f);
                CHECK(zcsr_text_utf8_valid(text + lines[i].byte_start,
                                           lines[i].byte_length));
            }
            previous_end = lines[i].byte_start + lines[i].byte_length;
        }
    }
}

static void stress_scroll(void) {
    size_t iter;
    g_section = "scroll-stress";
    for (iter = 0; iter < 1500; iter++) {
        uim_ScrollState s;
        size_t op;
        uim_Result rc;
        g_iter = (unsigned)iter;
        CHECK(uim_scroll_init(&s, rf(0.0f, 300.0f), rf(0.0f, 300.0f),
                              rf(1.0f, 60.0f)) == uim_OK);
        rc = uim_scroll_set_content(&s, rf(0.0f, 1200.0f), rf(0.0f, 1200.0f));
        CHECK(rc == uim_OK || rc == uim_ERR_RANGE);
        for (op = 0; op < 40u; op++) {
            unsigned kind = rnd_u32() % 3u;
            if (kind == 0u) (void)uim_scroll_set_offset(&s, rf(-100.0f, 1400.0f),
                                                        rf(-100.0f, 1400.0f));
            else if (kind == 1u) (void)uim_scroll_by_wheel(&s, rf(-6.0f, 6.0f),
                                                           rf(-6.0f, 6.0f));
            else (void)uim_scroll_page(&s, (rnd_u32() & 1u) ? uim_LAYOUT_ROW : uim_LAYOUT_COLUMN,
                                       (rnd_u32() & 1u) ? 1 : -1);
            CHECK(s.offset_x >= 0.0f && s.offset_x <= uim_scroll_max_x(&s) + 0.001f);
            CHECK(s.offset_y >= 0.0f && s.offset_y <= uim_scroll_max_y(&s) + 0.001f);
            {
                uim_ScrollbarMetrics m;
                float track = rf(0.0f, 250.0f);
                CHECK(uim_scrollbar_metrics(&s, (rnd_u32() & 1u) ? uim_LAYOUT_ROW : uim_LAYOUT_COLUMN,
                                            track, &m) == uim_OK);
                if (m.visible) {
                    CHECK(m.thumb_length >= 0.0f && m.thumb_length <= track + 0.001f);
                    CHECK(m.thumb_offset >= 0.0f);
                    CHECK(m.thumb_offset + m.thumb_length <= track + 0.001f);
                }
            }
        }
        {
            uim_Rect item;
            item.x = rf(-100.0f, 1000.0f);
            item.y = rf(-100.0f, 1000.0f);
            item.width = rf(0.0f, 200.0f);
            item.height = rf(0.0f, 200.0f);
            (void)uim_scroll_reveal(&s, item, rf(0.0f, 20.0f));
            CHECK(s.offset_x >= 0.0f && s.offset_x <= uim_scroll_max_x(&s) + 0.001f);
            CHECK(s.offset_y >= 0.0f && s.offset_y <= uim_scroll_max_y(&s) + 0.001f);
        }
    }
}

static void stress_placement(void) {
    size_t iter;
    g_section = "placement-stress";
    for (iter = 0; iter < 1500; iter++) {
        uim_Rect bounds = {0, 0, rf(80.0f, 300.0f), rf(80.0f, 300.0f)};
        uim_Rect anchor;
        uim_Placement p;
        float pw = rf(1.0f, bounds.width + 60.0f);
        float ph = rf(1.0f, bounds.height + 60.0f);
        uim_Result rc;
        g_iter = (unsigned)iter;
        anchor.width = rf(0.0f, bounds.width);
        anchor.height = rf(0.0f, bounds.height);
        anchor.x = rf(0.0f, bounds.width - anchor.width);
        anchor.y = rf(0.0f, bounds.height - anchor.height);
        rc = uim_place_popup(&anchor, pw, ph, &bounds, rf(0.0f, 20.0f),
                             (uim_PlacementSide)(rnd_u32() % 4u), &p);
        CHECK(rc == uim_OK || rc == uim_ERR_RANGE);
        CHECK(isfinite(p.rect.x) && isfinite(p.rect.y));
        CHECK(isfinite(p.rect.width) && isfinite(p.rect.height));
        CHECK(isfinite(p.arrow) && p.arrow >= 0.0f);
        if (p.side == uim_PLACE_BELOW || p.side == uim_PLACE_ABOVE)
            CHECK(p.arrow <= pw + 0.001f);
        else
            CHECK(p.arrow <= ph + 0.001f);
        if (pw <= bounds.width && ph <= bounds.height) {
            CHECK(p.rect.x >= bounds.x - 0.001f);
            CHECK(p.rect.y >= bounds.y - 0.001f);
            CHECK(p.rect.x + p.rect.width <= bounds.x + bounds.width + 0.001f);
            CHECK(p.rect.y + p.rect.height <= bounds.y + bounds.height + 0.001f);
        }
    }
}

static void stress_focus(void) {
    size_t iter;
    g_section = "focus-stress";
    for (iter = 0; iter < 1000; iter++) {
        uim_FocusRing ring;
        uint32_t ids[8];
        uim_Rect rects[8];
        size_t count, i;
        g_iter = (unsigned)iter;
        CHECK(uim_focus_init(&ring) == uim_OK);
        count = ri(1u, 8u);
        for (i = 0; i < count; i++) {
            ids[i] = (uint32_t)(iter * 16u + i + 1u);
            rects[i].x = rf(0.0f, 500.0f);
            rects[i].y = rf(0.0f, 500.0f);
            rects[i].width = rf(0.0f, 100.0f);
            rects[i].height = rf(0.0f, 100.0f);
        }
        CHECK(uim_focus_set_items(&ring, ids, rects, count) == uim_OK);
        CHECK(uim_focus_set_current(&ring, ids[ri(0u, count - 1u)]) == uim_OK);
        for (i = 0; i < 12u; i++) {
            uim_Result rc = uim_focus_next(&ring, (rnd_u32() & 1u) ? 1 : -1);
            CHECK(rc == uim_OK);
            CHECK(ring.current < ring.count);
            CHECK(ring.ids[ring.current] < count + iter * 16u + 1u);
        }
        {
            uint32_t current = uim_focus_current(&ring);
            uim_Result rc = uim_focus_move(&ring, rf(-1.0f, 1.0f), rf(-1.0f, 1.0f));
            CHECK(rc == uim_OK || rc == uim_ERR_NOT_FOUND);
            if (rc == uim_OK) CHECK(uim_focus_current(&ring) != 0u);
            (void)current;
        }
    }
}

static void stress_tween_toast_drag(void) {
    size_t iter;
    g_section = "state-stress";
    for (iter = 0; iter < 1000; iter++) {
        uim_Tween tw;
        uim_ToastQueue q;
        uim_DragState d;
        size_t i;
        float from = rf(-100.0f, 100.0f), to = rf(-100.0f, 100.0f);
        g_iter = (unsigned)iter;

        CHECK(uim_tween_start(&tw, from, to, rf(0.0f, 2.0f),
                              (uim_Easing)(rnd_u32() % (uint32_t)uim_EASE_COUNT)) == uim_OK);
        for (i = 0; i < 6u; i++) {
            CHECK(uim_tween_tick(&tw, rf(0.0f, 3.0f)) == uim_OK);
            CHECK(isfinite(tw.value));
        }
        if (!tw.loop && !tw.active) CHECK(NEAR(tw.value, to));

        CHECK(uim_toast_init(&q) == uim_OK);
        for (i = 0; i < 40u; i++) {
            uint32_t id = rnd_u32() % 16u;
            CHECK(uim_toast_push(&q, id, rf(0.1f, 3.0f), (int)(rnd_u32() % 8u)) == uim_OK);
            CHECK(q.count <= UIM_TOAST_CAPACITY);
        }
        CHECK(uim_toast_tick(&q, rf(0.0f, 5.0f)) == uim_OK);
        CHECK(q.count <= UIM_TOAST_CAPACITY);

        CHECK(uim_drag_init(&d, rf(0.0f, 10.0f)) == uim_OK);
        CHECK(uim_drag_set_axis_lock(&d, (rnd_u32() & 1u) != 0u) == uim_OK);
        CHECK(uim_drag_begin(&d, rnd_u32(), rf(-100.0f, 100.0f), rf(-100.0f, 100.0f)) == uim_OK);
        for (i = 0; i < 8u; i++)
            CHECK(uim_drag_move(&d, rf(-200.0f, 200.0f), rf(-200.0f, 200.0f)) == uim_OK);
        {
            float dx = 0.0f, dy = 0.0f;
            uim_Result rc = uim_drag_end(&d, rf(-200.0f, 200.0f), rf(-200.0f, 200.0f), &dx, &dy);
            CHECK(rc == uim_OK || rc == uim_ERR_STATE);
            CHECK(!d.pressed && !d.active);
            CHECK(isfinite(dx) && isfinite(dy));
        }
    }
}

static void stress_numeric(void) {
    size_t iter;
    g_section = "numeric-stress";
    for (iter = 0; iter < 1500; iter++) {
        uim_NumericField f;
        double lo = rf(-100.0f, 100.0f), hi = rf(-100.0f, 100.0f);
        double step = rf(0.0f, 10.0f);
        size_t op;
        g_iter = (unsigned)iter;
        if (lo > hi) { double t = lo; lo = hi; hi = t; }
        {
            uim_Result init_rc = uim_numeric_field_init(&f, uim_NUMERIC_FLOAT, lo, hi,
                                                        rf(-200.0f, 200.0f), step);
            CHECK(init_rc == uim_OK || init_rc == uim_ERR_RANGE);
        }
        CHECK(isfinite(f.value) && f.value >= lo - 0.001f && f.value <= hi + 0.001f);
        for (op = 0; op < 20u; op++) {
            unsigned kind = rnd_u32() % 3u;
            if (kind == 0u) (void)uim_numeric_field_step(&f, (int)(rnd_u32() % 5u) - 2);
            else if (kind == 1u) (void)uim_numeric_field_drag_by(&f, rf(-500.0f, 500.0f));
            else (void)uim_numeric_field_set_value(&f, rf(-500.0f, 500.0f));
            CHECK(isfinite(f.value));
            CHECK(f.value >= lo - 0.001f && f.value <= hi + 0.001f);
        }
    }
}

static void stress_splitter(void) {
    size_t iter;
    g_section = "splitter-stress";
    for (iter = 0; iter < 1500; iter++) {
        uim_Splitter s;
        uim_Rect box;
        uim_Rect first, second;
        float gap, min_first, min_second, free_len, lo, hi;
        g_iter = (unsigned)iter;
        box.x = 0.0f; box.y = 0.0f;
        box.width = rf(20.0f, 400.0f);
        box.height = rf(20.0f, 400.0f);
        gap = rf(0.0f, 16.0f);
        min_first = rf(0.0f, 40.0f);
        min_second = rf(0.0f, 40.0f);
        CHECK(uim_splitter_init(&s, (rnd_u32() & 1u) ? uim_LAYOUT_COLUMN : uim_LAYOUT_ROW,
                                rf(0.0f, 1.0f), min_first, min_second) == uim_OK);
        CHECK(uim_splitter_begin_drag(&s) == uim_OK);
        free_len = uim_splitter__free(&box, s.axis, gap);
        uim_splitter__bounds(&s, free_len, &lo, &hi);
        {
            float pos = rf(-50.0f, box.width + 50.0f);
            uim_Result rc = uim_splitter_drag_to(&s, &box, gap, pos);
            CHECK(rc == uim_OK || rc == uim_ERR_RANGE);
        }
        CHECK(s.ratio >= lo - 0.001f && s.ratio <= hi + 0.001f);
        CHECK(uim_splitter_pane_rects(&s, &box, gap, &first, &second) == uim_OK);
        CHECK(first.width >= 0.0f && first.height >= 0.0f);
        CHECK(second.width >= 0.0f && second.height >= 0.0f);
    }
}

int main(void) {
    stress_layout();
    stress_text();
    stress_scroll();
    stress_placement();
    stress_focus();
    stress_tween_toast_drag();
    stress_numeric();
    stress_splitter();
    if (g_failures == 0) {
        printf("uim atoms stress: all invariants passed\n");
        return 0;
    }
    printf("uim atoms stress: %d failure(s)\n", g_failures);
    return 1;
}
