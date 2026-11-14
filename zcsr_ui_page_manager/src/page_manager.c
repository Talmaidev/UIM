#include "zcsr_ui_page_manager/page_manager.h"

void zcsr_ui_page_manager_init(zcsr_ui_page_manager* pm) {
    if (!pm) return;
    for (size_t i = 0; i < ZCSR_UI_PAGE_MANAGER_CAPACITY; ++i) pm->stack[i] = NULL;
    pm->depth = 0;
}

bool zcsr_ui_page_manager_open(zcsr_ui_page_manager* pm, const char* page_id) {
    if (!pm || !page_id) return false;
    if (pm->depth >= ZCSR_UI_PAGE_MANAGER_CAPACITY) return false;
    pm->stack[pm->depth++] = page_id;
    return true;
}

bool zcsr_ui_page_manager_back(zcsr_ui_page_manager* pm) {
    if (!pm || pm->depth == 0) return false;
    pm->stack[--pm->depth] = NULL;
    return true;
}

void zcsr_ui_page_manager_close(zcsr_ui_page_manager* pm) {
    if (!pm) return;
    while (pm->depth > 0) {
        pm->stack[--pm->depth] = NULL;
    }
}

const char* zcsr_ui_page_manager_current(const zcsr_ui_page_manager* pm) {
    if (!pm || pm->depth == 0) return NULL;
    return pm->stack[pm->depth - 1];
}

size_t zcsr_ui_page_manager_depth(const zcsr_ui_page_manager* pm) {
    return pm ? pm->depth : 0;
}
