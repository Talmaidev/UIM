#ifndef ZCSR_UI_PAGE_MANAGER_H
#define ZCSR_UI_PAGE_MANAGER_H

/* zcsr_ui_page_manager — independent setting page container.
 *
 * This container manages setting page instances only. A setting page is one
 * named group of setting rows. The container does not own UIM component types,
 * a UI document, a scene, the editor workspace, the OS window, the main loop,
 * or any global route/stack. Opening a game menu is not a setting page
 * operation and must not call this container.
 *
 * Keys are borrowed pointers. The caller must keep each key alive until the
 * page is destroyed. Fixed capacity, no heap. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZCSR_SETTING_PAGE_MANAGER_CAPACITY 16
#define ZCSR_SETTING_PAGE_VISIBLE 1
#define ZCSR_SETTING_PAGE_HIDDEN 0

typedef uint32_t zcsr_setting_page_id;
#define ZCSR_SETTING_PAGE_INVALID ((zcsr_setting_page_id)0)

typedef struct zcsr_setting_page {
    zcsr_setting_page_id id;
    const char* key;      /* borrowed; unique inside this container */
    bool visible;
} zcsr_setting_page;

typedef struct zcsr_ui_page_manager {
    zcsr_setting_page pages[ZCSR_SETTING_PAGE_MANAGER_CAPACITY];
    size_t count;
    uint32_t next_id;
} zcsr_ui_page_manager;

void zcsr_ui_page_manager_init(zcsr_ui_page_manager* pm);

/* Create one setting page. Returns the stable id, or ZCSR_SETTING_PAGE_INVALID
 * on NULL input, duplicate key, empty key, full capacity, or id overflow. */
zcsr_setting_page_id zcsr_ui_page_manager_create(zcsr_ui_page_manager* pm, const char* key);

/* Destroy one setting page. Returns false when the id is unknown. */
bool zcsr_ui_page_manager_destroy(zcsr_ui_page_manager* pm, zcsr_setting_page_id id);

/* Find a setting page by key. Returns false when missing. */
bool zcsr_ui_page_manager_find(const zcsr_ui_page_manager* pm, const char* key,
                               zcsr_setting_page_id* out_id);

size_t zcsr_ui_page_manager_count(const zcsr_ui_page_manager* pm);

/* Borrowed key, or NULL for an unknown id. */
const char* zcsr_ui_page_manager_key(const zcsr_ui_page_manager* pm,
                                     zcsr_setting_page_id id);

/* Visibility is page state only. It is not focus, navigation, route or modal
 * ownership. */
bool zcsr_ui_page_manager_set_visible(zcsr_ui_page_manager* pm,
                                      zcsr_setting_page_id id, bool visible);
bool zcsr_ui_page_manager_visible(const zcsr_ui_page_manager* pm,
                                  zcsr_setting_page_id id);

#endif /* ZCSR_UI_PAGE_MANAGER_H */
