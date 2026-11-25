#ifndef UIM_MENU_H
#define UIM_MENU_H
/* uim_menu.h — the menu atom: one flat item model for menu bars, dropdown
 * menus and right-click context menus.
 *
 * What it owns:
 * - item descriptors (command id, parent header, submenu link, borrowed label,
 *   enabled/checkable/checked/separator flags), open header, hover and the
 *   keyboard-active item.
 * - sibling navigation (Up/Down over the open menu, skipping separators and
 *   disabled items) and item hit testing over caller-owned rects.
 *
 * What it does not own: drawing, the popup rect (uim_placement), keyboard
 * shortcut labels, command execution or the menu bar layout. A context menu is
 * the same model opened at root (header id 0) at the cursor.
 *
 * Contract: fixed capacity (UIM_MENU_CAPACITY), no allocation, no globals.
 * Ids must be unique; a duplicate is uim_ERR_EXISTS and the model is left
 * unchanged. Label pointers are borrowed and must outlive the menu.
 */
#include "uim_types.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UIM_MENU_CAPACITY 64u

typedef enum uim_MenuFlags {
    uim_MENU_ITEM_SEPARATOR  = 1u << 0,
    uim_MENU_ITEM_CHECKABLE  = 1u << 1,
    uim_MENU_ITEM_ENABLED    = 1u << 2,
    uim_MENU_ITEM_CHECKED    = 1u << 3,
    uim_MENU_ITEM_SUBMENU    = 1u << 4
} uim_MenuFlags;

typedef struct uim_MenuItem {
    uint32_t id;
    uint32_t parent;      /* header id; 0 = root (bar header or context menu) */
    uint32_t submenu;     /* child header id for SUBMENU items; 0 = leaf */
    const char *label;    /* borrowed; NULL for separators */
    uint32_t flags;
} uim_MenuItem;

typedef struct uim_Menu {
    uim_MenuItem items[UIM_MENU_CAPACITY];
    size_t count;
    uint32_t open_header;
    uint32_t hover_header;
    uint32_t hover_item;
    size_t active_index;
    bool open;
    bool wrap;
} uim_Menu;

static inline uim_Result uim_menu_init(uim_Menu *m) {
    if (!m) return uim_ERR_ARGUMENT;
    m->count = 0;
    m->open_header = 0u;
    m->hover_header = 0u;
    m->hover_item = 0u;
    m->active_index = 0u;
    m->open = false;
    m->wrap = true;
    return uim_OK;
}

/* Replace the whole model. Ids must be unique and nonzero. */
static inline uim_Result uim_menu_set_items(uim_Menu *m, const uim_MenuItem *items, size_t count) {
    size_t i, j;
    if (!m || (count > 0u && !items)) return uim_ERR_ARGUMENT;
    if (count > UIM_MENU_CAPACITY) return uim_ERR_BUFFER;
    for (i = 0; i < count; i++) {
        if (items[i].id == 0u) return uim_ERR_ARGUMENT;
        for (j = 0; j < i; j++) {
            if (items[j].id == items[i].id) return uim_ERR_EXISTS;
        }
    }
    for (i = 0; i < count; i++) m->items[i] = items[i];
    m->count = count;
    m->open = false;
    m->open_header = 0u;
    m->hover_header = 0u;
    m->hover_item = 0u;
    m->active_index = 0u;
    return uim_OK;
}

/* Flat index of the child at the given sibling ordinal, or m->count if none. */
static inline size_t uim_menu__child_at(const uim_Menu *m, uint32_t header_id, size_t ordinal) {
    size_t seen = 0u, k;
    for (k = 0; k < m->count; k++) {
        if (m->items[k].parent != header_id) continue;
        if (seen == ordinal) return k;
        seen++;
    }
    return m->count;
}

static inline size_t uim_menu_count(const uim_Menu *m) { return m ? m->count : 0u; }

static inline uim_Result uim_menu_get_at(const uim_Menu *m, size_t index,
                                        const uim_MenuItem **out_item) {
    if (!m || !out_item || index >= m->count) return uim_ERR_RANGE;
    *out_item = &m->items[index];
    return uim_OK;
}

static inline bool uim_menu_item_enabled(const uim_MenuItem *item) {
    return item && (item->flags & uim_MENU_ITEM_ENABLED) != 0u;
}

static inline bool uim_menu_item_is_separator(const uim_MenuItem *item) {
    return item && (item->flags & uim_MENU_ITEM_SEPARATOR) != 0u;
}

/* Count items whose parent equals header_id (0 = root). */
static inline size_t uim_menu_children_of(const uim_Menu *m, uint32_t header_id) {
    size_t i, n = 0u;
    if (!m) return 0u;
    for (i = 0; i < m->count; i++) {
        if (m->items[i].parent == header_id) n++;
    }
    return n;
}

/* Open a header popup. header_id 0 opens the root (context menu). A nonzero
 * id must exist and carry the SUBMENU flag. */
static inline uim_Result uim_menu_open(uim_Menu *m, uint32_t header_id) {
    size_t i;
    if (!m) return uim_ERR_ARGUMENT;
    if (header_id != 0u) {
        bool found = false;
        for (i = 0; i < m->count; i++) {
            if (m->items[i].id == header_id) {
                found = (m->items[i].flags & uim_MENU_ITEM_SUBMENU) != 0u;
                break;
            }
        }
        if (!found) return uim_ERR_NOT_FOUND;
    }
    m->open = true;
    m->open_header = header_id;
    m->hover_header = header_id;
    m->active_index = uim_menu__child_at(m, header_id, 0u);
    return uim_OK;
}

static inline void uim_menu_close(uim_Menu *m) {
    if (!m) return;
    m->open = false;
    m->open_header = 0u;
    m->hover_header = 0u;
    m->hover_item = 0u;
}

static inline bool uim_menu_is_open(const uim_Menu *m) { return m && m->open; }

static inline uim_Result uim_menu_hover_header(uim_Menu *m, uint32_t header_id) {
    if (!m) return uim_ERR_ARGUMENT;
    m->hover_header = header_id;
    return uim_OK;
}

static inline uim_Result uim_menu_hover_item(uim_Menu *m, uint32_t item_id) {
    size_t i;
    if (!m) return uim_ERR_ARGUMENT;
    for (i = 0; i < m->count; i++) {
        if (m->items[i].id == item_id) {
            m->hover_item = item_id;
            m->active_index = i;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

static inline bool uim_menu__selectable(const uim_MenuItem *it) {
    return !uim_menu_item_is_separator(it) && uim_menu_item_enabled(it) &&
           (it->flags & uim_MENU_ITEM_SUBMENU) == 0u;
}

/* Move the active item within the open header's children, skipping separators
 * and disabled entries. direction +1 = down, -1 = up. */
static inline uim_Result uim_menu_step(uim_Menu *m, int direction) {
    size_t child_count = 0u, cur_ordinal = 0u, i, matched = 0u, k;
    if (!m || (direction != 1 && direction != -1)) return uim_ERR_ARGUMENT;
    if (!m->open) return uim_ERR_STATE;
    for (i = 0; i < m->count; i++) {
        if (m->items[i].parent != m->open_header) continue;
        if (i == m->active_index) { cur_ordinal = child_count; matched = 1u; }
        child_count++;
    }
    if (child_count == 0u) return uim_ERR_NOT_FOUND;
    if (!matched) cur_ordinal = direction > 0 ? 0u : child_count - 1u;
    for (k = 1u; k <= child_count; k++) {
        long long ord = (long long)cur_ordinal + (long long)direction * (long long)k;
        size_t flat;
        if (m->wrap) {
            ord %= (long long)child_count;
            if (ord < 0) ord += (long long)child_count;
        } else if (ord < 0 || ord >= (long long)child_count) {
            break;
        }
        flat = uim_menu__child_at(m, m->open_header, (size_t)ord);
        if (flat >= m->count) break;
        if (uim_menu__selectable(&m->items[flat])) {
            m->active_index = flat;
            m->hover_item = m->items[flat].id;
            return uim_OK;
        }
    }
    return uim_ERR_STATE;
}

/* Take the active item. On success the output id is the command to run and
 * the menu closes; a submenu/separator/disabled entry yields uim_ERR_STATE. */
static inline uim_Result uim_menu_activate(uim_Menu *m, uint32_t *out_id) {
    const uim_MenuItem *it;
    if (!m || !out_id) return uim_ERR_ARGUMENT;
    if (!m->open) return uim_ERR_STATE;
    if (m->active_index >= m->count) return uim_ERR_NOT_FOUND;
    it = &m->items[m->active_index];
    if (it->parent != m->open_header || !uim_menu__selectable(it)) return uim_ERR_STATE;
    *out_id = it->id;
    m->open = false;
    m->open_header = 0u;
    return uim_OK;
}

/* Hit test over caller-owned rects aligned to item order. On hit the active
 * item and hover id follow the pointer; outside every rect returns
 * uim_ERR_NOT_FOUND and leaves the active item untouched. */
static inline uim_Result uim_menu_hit(uim_Menu *m, const uim_Rect *rects,
                                     size_t rects_count, float x, float y,
                                     size_t *out_index) {
    size_t i;
    if (!m || !rects || !out_index || !isfinite(x) || !isfinite(y))
        return uim_ERR_ARGUMENT;
    for (i = 0; i < m->count && i < rects_count; i++) {
        const uim_Rect *r = &rects[i];
        if (!isfinite(r->x) || !isfinite(r->y) || !isfinite(r->width) || !isfinite(r->height))
            return uim_ERR_ARGUMENT;
        if (x >= r->x && x <= r->x + r->width && y >= r->y && y <= r->y + r->height) {
            *out_index = i;
            m->active_index = i;
            m->hover_item = m->items[i].id;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_MENU_H */
