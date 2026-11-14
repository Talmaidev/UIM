#ifndef ZCSR_UI_PAGE_MANAGER_H
#define ZCSR_UI_PAGE_MANAGER_H

/* zcsr_ui_page_manager — page stack owner.
 * A page is a composition of UI primitives (settings/menu/etc.).
 * This layer owns route/back/modal policy; it does not own widgets.
 * Fixed-capacity, no heap in the frame path. */

#include <stdbool.h>
#include <stddef.h>

#define ZCSR_UI_PAGE_MANAGER_CAPACITY 16

typedef struct zcsr_ui_page_manager {
    const char* stack[ZCSR_UI_PAGE_MANAGER_CAPACITY];
    size_t depth;
} zcsr_ui_page_manager;

void zcsr_ui_page_manager_init(zcsr_ui_page_manager* pm);
bool zcsr_ui_page_manager_open(zcsr_ui_page_manager* pm, const char* page_id);
bool zcsr_ui_page_manager_back(zcsr_ui_page_manager* pm);
void zcsr_ui_page_manager_close(zcsr_ui_page_manager* pm);
const char* zcsr_ui_page_manager_current(const zcsr_ui_page_manager* pm);
size_t zcsr_ui_page_manager_depth(const zcsr_ui_page_manager* pm);

#endif /* ZCSR_UI_PAGE_MANAGER_H */
