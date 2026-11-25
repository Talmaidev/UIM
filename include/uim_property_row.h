#ifndef UIM_PROPERTY_ROW_H
#define UIM_PROPERTY_ROW_H
/* uim_property_row.h — the property-row atom: label + editor pair layout and
 * the enabled / read-only interaction state shared by every inspector row.
 *
 * What it owns:
 * - the label-column / editor-column split for one row, with a fixed label
 *   width and a gap; the label is vertically centered in the row.
 * - enabled / read-only flags; interactive() reports whether the host should
 *   accept pointer and keyboard input for the editor half.
 *
 * What it does not own: the editor widget itself (button/toggle/dropdown/
 * numeric_field own their state), row text, separators or the column stack.
 * Multi-row grids belong to uim_layout_solve; this header fixes the one-row
 * label/editor convention used by inspectors and settings panels.
 *
 * Contract: pure geometry + flags. No allocation, no globals, no renderer.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_PropertyRow {
    float label_width;
    bool enabled;
    bool read_only;
    bool hovered;
    bool active;
} uim_PropertyRow;

static inline uim_Result uim_property_row_init(uim_PropertyRow *row, float label_width) {
    if (!row || !isfinite(label_width) || label_width < 0.0f) return uim_ERR_ARGUMENT;
    row->label_width = label_width;
    row->enabled = true;
    row->read_only = false;
    row->hovered = false;
    row->active = false;
    return uim_OK;
}

static inline uim_Result uim_property_row_set_enabled(uim_PropertyRow *row, bool enabled) {
    if (!row) return uim_ERR_ARGUMENT;
    row->enabled = enabled;
    return uim_OK;
}

static inline uim_Result uim_property_row_set_read_only(uim_PropertyRow *row, bool read_only) {
    if (!row) return uim_ERR_ARGUMENT;
    row->read_only = read_only;
    return uim_OK;
}

static inline bool uim_property_row_interactive(const uim_PropertyRow *row) {
    return row && row->enabled && !row->read_only;
}

/* Split one row into label and editor rects. label_gap >= 0. When the label
 * width leaves no room for the editor, the editor rect has zero width. */
static inline uim_Result uim_property_row_layout(const uim_PropertyRow *row,
                                                const uim_Rect *container, float label_gap,
                                                uim_Rect *out_label, uim_Rect *out_editor) {
    float label_w, editor_x, editor_w;
    if (!row || !container || !out_label || !out_editor || !isfinite(label_gap) ||
        label_gap < 0.0f || !isfinite(container->x) || !isfinite(container->y) ||
        !isfinite(container->width) || !isfinite(container->height) ||
        container->width < 0.0f || container->height < 0.0f)
        return uim_ERR_ARGUMENT;
    label_w = row->label_width;
    if (label_w > container->width) label_w = container->width;
    out_label->x = container->x;
    out_label->y = container->y;
    out_label->width = label_w;
    out_label->height = container->height;
    editor_x = container->x + label_w + label_gap;
    editor_w = container->x + container->width - editor_x;
    if (editor_w < 0.0f) editor_w = 0.0f;
    out_editor->x = editor_x;
    out_editor->y = container->y;
    out_editor->width = editor_w;
    out_editor->height = container->height;
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_PROPERTY_ROW_H */
