#ifndef UIM_LAYOUT_H
#define UIM_LAYOUT_H
/* uim_layout.h — the layout atom: rect math only.
 *
 * Today three places lay widgets out by hand (the settings page's MS_* formulas,
 * the menu page, the editor panels) and every new panel invents its own
 * padding/gap/grow rules. This header is the one solver: padding, gap, main-axis
 * grow with min/max, cross-axis alignment, grid cells, splits and insets.
 *
 * Contract:
 * - Pure geometry, caller-owned output array (one uim_Rect per item), no
 *   allocation, no globals, no renderer, no font metrics.
 * - Overflow is reported, never silently shrunk: when the fixed sizes exceed
 *   the container the item rects overflow past the end (main_align still
 *   applies), and uim_ERR_RANGE is returned so the caller can wrap or scroll.
 * - Negative padding/gap/container sizes and non-finite numbers are rejected
 *   with uim_ERR_ARGUMENT and the output is left untouched.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uim_LayoutAxis { uim_LAYOUT_ROW = 0, uim_LAYOUT_COLUMN = 1 } uim_LayoutAxis;
typedef enum uim_LayoutAlign {
    uim_ALIGN_START = 0,
    uim_ALIGN_CENTER,
    uim_ALIGN_END,
    uim_ALIGN_STRETCH   /* fill the cross axis (main axis: behaves like START) */
} uim_LayoutAlign;

typedef struct uim_LayoutSpec {
    uim_LayoutAxis axis;
    float gap;              /* between items, >= 0 */
    uim_Insets padding;     /* inside the container */
    uim_LayoutAlign main_align;
    uim_LayoutAlign cross_align;
} uim_LayoutSpec;

typedef struct uim_LayoutItem {
    float basis;      /* preferred main-axis size; <= 0 with grow > 0 means "from 0" */
    float grow;       /* extra-space weight; 0 = fixed */
    float min_main;   /* lower bound, >= 0 (0 = none) */
    float max_main;   /* upper bound; <= 0 = none */
    float cross;      /* preferred cross size; <= 0 = fill the cross axis */
} uim_LayoutItem;

static inline bool uim_layout__finite(float v) { return isfinite(v); }

static inline bool uim_layout__align_ok(uim_LayoutAlign a) {
    return a == uim_ALIGN_START || a == uim_ALIGN_CENTER ||
           a == uim_ALIGN_END || a == uim_ALIGN_STRETCH;
}

static inline uim_Rect uim_layout_inset(const uim_Rect *outer, uim_Insets insets) {
    uim_Rect r = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!outer) return r;
    r.x = outer->x + insets.left;
    r.y = outer->y + insets.top;
    r.width = outer->width - insets.left - insets.right;
    r.height = outer->height - insets.top - insets.bottom;
    if (r.width < 0.0f) r.width = 0.0f;
    if (r.height < 0.0f) r.height = 0.0f;
    return r;
}

/* Position one box inside a container. STRETCH fills the corresponding axis. */
static inline uim_Result uim_layout_align(const uim_Rect *container,
                                          float width, float height,
                                          uim_LayoutAlign horizontal,
                                          uim_LayoutAlign vertical,
                                          uim_Rect *out) {
    if (!container || !out || !uim_layout__finite(width) || !uim_layout__finite(height) ||
        width < 0.0f || height < 0.0f ||
        !uim_layout__finite(container->x) || !uim_layout__finite(container->y) ||
        !uim_layout__finite(container->width) || !uim_layout__finite(container->height) ||
        container->width < 0.0f || container->height < 0.0f ||
        !uim_layout__align_ok(horizontal) || !uim_layout__align_ok(vertical))
        return uim_ERR_ARGUMENT;
    if (horizontal == uim_ALIGN_STRETCH) width = container->width;
    if (vertical == uim_ALIGN_STRETCH) height = container->height;
    out->x = container->x;
    if (horizontal == uim_ALIGN_CENTER) out->x += (container->width - width) * 0.5f;
    else if (horizontal == uim_ALIGN_END) out->x += container->width - width;
    out->y = container->y;
    if (vertical == uim_ALIGN_CENTER) out->y += (container->height - height) * 0.5f;
    else if (vertical == uim_ALIGN_END) out->y += container->height - height;
    out->width = width;
    out->height = height;
    return uim_OK;
}

/* Two panes with a fixed gap; ratio is the first pane's share of the free
 * space (0..1). Used by splitters and sidebars. */
static inline uim_Result uim_layout_split(const uim_Rect *container,
                                          uim_LayoutAxis axis, float ratio, float gap,
                                          uim_Rect *first, uim_Rect *second) {
    float free_main;
    if (!container || !first || !second || !uim_layout__finite(ratio) ||
        !uim_layout__finite(gap) || gap < 0.0f ||
        !uim_layout__finite(container->x) || !uim_layout__finite(container->y) ||
        !uim_layout__finite(container->width) || !uim_layout__finite(container->height) ||
        container->width < 0.0f || container->height < 0.0f)
        return uim_ERR_ARGUMENT;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    if (axis == uim_LAYOUT_ROW) {
        free_main = container->width - gap;
        if (free_main < 0.0f) free_main = 0.0f;
        *first = *container;
        first->width = free_main * ratio;
        *second = *container;
        second->x = container->x + first->width + gap;
        second->width = free_main - first->width;
    } else {
        free_main = container->height - gap;
        if (free_main < 0.0f) free_main = 0.0f;
        *first = *container;
        first->height = free_main * ratio;
        *second = *container;
        second->y = container->y + first->height + gap;
        second->height = free_main - first->height;
    }
    return uim_OK;
}

/* Row/column solver: padding -> gaps -> grow (min/max) -> alignment. */
static inline uim_Result uim_layout_solve(const uim_Rect *container,
                                          const uim_LayoutSpec *spec,
                                          const uim_LayoutItem *items, size_t count,
                                          uim_Rect *out_rects) {
    uim_Rect content;
    float main_len, cross_len, gaps, sum_base, free_space, leftover, main_offset;
    size_t i, pass;
    bool overflow = false;

    if (!container || !spec || !uim_layout__align_ok(spec->main_align) ||
        !uim_layout__align_ok(spec->cross_align) ||
        !uim_layout__finite(spec->gap) || spec->gap < 0.0f ||
        !uim_layout__finite(spec->padding.left) || !uim_layout__finite(spec->padding.top) ||
        !uim_layout__finite(spec->padding.right) || !uim_layout__finite(spec->padding.bottom) ||
        spec->padding.left < 0.0f || spec->padding.top < 0.0f ||
        spec->padding.right < 0.0f || spec->padding.bottom < 0.0f)
        return uim_ERR_ARGUMENT;
    if (spec->axis != uim_LAYOUT_ROW && spec->axis != uim_LAYOUT_COLUMN) return uim_ERR_ARGUMENT;
    if (count > 0 && (!items || !out_rects)) return uim_ERR_ARGUMENT;
    if (!uim_layout__finite(container->width) || !uim_layout__finite(container->height) ||
        container->width < 0.0f || container->height < 0.0f)
        return uim_ERR_ARGUMENT;

    content = uim_layout_inset(container, spec->padding);
    if (spec->axis == uim_LAYOUT_ROW) {
        main_len = content.width;
        cross_len = content.height;
    } else {
        main_len = content.height;
        cross_len = content.width;
    }
    if (count == 0) return uim_OK;

    for (i = 0; i < count; i++) {
        float size;
        if (!uim_layout__finite(items[i].basis) || !uim_layout__finite(items[i].grow) ||
            !uim_layout__finite(items[i].min_main) || !uim_layout__finite(items[i].max_main) ||
            !uim_layout__finite(items[i].cross) ||
            items[i].grow < 0.0f || items[i].min_main < 0.0f || items[i].cross < 0.0f)
            return uim_ERR_ARGUMENT;
        size = items[i].basis > 0.0f ? items[i].basis : 0.0f;
        if (size < items[i].min_main) size = items[i].min_main;
        if (items[i].max_main > 0.0f && size > items[i].max_main) size = items[i].max_main;
        out_rects[i].width = size;   /* scratch: main-axis size */
    }

    gaps = spec->gap * (float)(count - 1u);
    sum_base = 0.0f;
    for (i = 0; i < count; i++) sum_base += out_rects[i].width;
    free_space = main_len - sum_base - gaps;

    if (free_space > 0.0f) {
        for (pass = 0; pass < 4u; pass++) {
            float grow_sum = 0.0f, remaining = main_len - gaps;
            for (i = 0; i < count; i++) {
                remaining -= out_rects[i].width;
                if (items[i].grow > 0.0f &&
                    (items[i].max_main <= 0.0f || out_rects[i].width < items[i].max_main - 0.0001f))
                    grow_sum += items[i].grow;
            }
            if (remaining <= 0.0001f || grow_sum <= 0.0001f) break;
            for (i = 0; i < count; i++) {
                float share;
                if (items[i].grow <= 0.0f) continue;
                if (items[i].max_main > 0.0f && out_rects[i].width >= items[i].max_main - 0.0001f)
                    continue;
                share = remaining * (items[i].grow / grow_sum);
                out_rects[i].width += share;
                if (items[i].max_main > 0.0f && out_rects[i].width > items[i].max_main)
                    out_rects[i].width = items[i].max_main;
            }
        }
    }

    sum_base = 0.0f;
    for (i = 0; i < count; i++) sum_base += out_rects[i].width;
    leftover = main_len - sum_base - gaps;
    if (leftover < 0.0f) overflow = true;
    main_offset = 0.0f;
    if (spec->main_align == uim_ALIGN_CENTER) main_offset = leftover * 0.5f;
    else if (spec->main_align == uim_ALIGN_END) main_offset = leftover;

    for (i = 0; i < count; i++) {
        float item_cross, cross_offset = 0.0f;
        float main_size = out_rects[i].width;
        if (spec->cross_align == uim_ALIGN_STRETCH || items[i].cross <= 0.0f) {
            item_cross = cross_len;
        } else {
            item_cross = items[i].cross;
            if (item_cross > cross_len) item_cross = cross_len;
            if (spec->cross_align == uim_ALIGN_CENTER) cross_offset = (cross_len - item_cross) * 0.5f;
            else if (spec->cross_align == uim_ALIGN_END) cross_offset = cross_len - item_cross;
        }
        if (spec->axis == uim_LAYOUT_ROW) {
            out_rects[i].x = content.x + main_offset;
            out_rects[i].y = content.y + cross_offset;
            out_rects[i].width = main_size;
            out_rects[i].height = item_cross;
            main_offset += main_size + spec->gap;
        } else {
            out_rects[i].x = content.x + cross_offset;
            out_rects[i].y = content.y + main_offset;
            out_rects[i].width = item_cross;
            out_rects[i].height = main_size;
            main_offset += main_size + spec->gap;
        }
    }
    return overflow ? uim_ERR_RANGE : uim_OK;
}

/* Equal-width cells, fixed row height. The last row may be partial. */
static inline uim_Result uim_layout_grid(const uim_Rect *container, uim_Insets padding,
                                         size_t columns, float gap_x, float gap_y,
                                         float row_height, size_t count,
                                         uim_Rect *out_rects) {
    uim_Rect content;
    float cell_w;
    size_t i;
    if (!container || !uim_layout__finite(gap_x) || !uim_layout__finite(gap_y) ||
        !uim_layout__finite(row_height) || gap_x < 0.0f || gap_y < 0.0f || row_height <= 0.0f ||
        !uim_layout__finite(padding.left) || !uim_layout__finite(padding.top) ||
        !uim_layout__finite(padding.right) || !uim_layout__finite(padding.bottom) ||
        padding.left < 0.0f || padding.top < 0.0f || padding.right < 0.0f || padding.bottom < 0.0f ||
        columns == 0u)
        return uim_ERR_ARGUMENT;
    if (count > 0 && !out_rects) return uim_ERR_ARGUMENT;
    if (!uim_layout__finite(container->width) || !uim_layout__finite(container->height) ||
        container->width < 0.0f || container->height < 0.0f)
        return uim_ERR_ARGUMENT;
    content = uim_layout_inset(container, padding);
    cell_w = (content.width - gap_x * (float)(columns - 1u)) / (float)columns;
    if (cell_w < 0.0f) cell_w = 0.0f;
    for (i = 0; i < count; i++) {
        size_t row = i / columns;
        size_t col = i % columns;
        out_rects[i].x = content.x + (cell_w + gap_x) * (float)col;
        out_rects[i].y = content.y + (row_height + gap_y) * (float)row;
        out_rects[i].width = cell_w;
        out_rects[i].height = row_height;
    }
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_LAYOUT_H */
