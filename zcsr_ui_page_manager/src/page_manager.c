#include "zcsr_ui_page_manager/page_manager.h"

#include <string.h>

static zcsr_setting_page* page_at_id(zcsr_ui_page_manager* pm, zcsr_setting_page_id id) {
    size_t i;
    if (!pm || id == ZCSR_SETTING_PAGE_INVALID) return NULL;
    for (i = 0; i < pm->count; ++i)
        if (pm->pages[i].id == id) return &pm->pages[i];
    return NULL;
}

static const zcsr_setting_page* page_at_id_const(const zcsr_ui_page_manager* pm,
                                                 zcsr_setting_page_id id) {
    size_t i;
    if (!pm || id == ZCSR_SETTING_PAGE_INVALID) return NULL;
    for (i = 0; i < pm->count; ++i)
        if (pm->pages[i].id == id) return &pm->pages[i];
    return NULL;
}

void zcsr_ui_page_manager_init(zcsr_ui_page_manager* pm) {
    if (!pm) return;
    memset(pm, 0, sizeof(*pm));
    pm->next_id = 1;
}

zcsr_setting_page_id zcsr_ui_page_manager_create(zcsr_ui_page_manager* pm, const char* key) {
    size_t i;
    zcsr_setting_page* page;
    if (!pm || !key || !key[0]) return ZCSR_SETTING_PAGE_INVALID;
    if (pm->count >= ZCSR_SETTING_PAGE_MANAGER_CAPACITY) return ZCSR_SETTING_PAGE_INVALID;
    if (pm->next_id == 0 || pm->next_id == UINT32_MAX) return ZCSR_SETTING_PAGE_INVALID;
    for (i = 0; i < pm->count; ++i)
        if (strcmp(pm->pages[i].key, key) == 0) return ZCSR_SETTING_PAGE_INVALID;
    page = &pm->pages[pm->count++];
    page->id = pm->next_id++;
    page->key = key;
    page->visible = true;
    return page->id;
}

bool zcsr_ui_page_manager_destroy(zcsr_ui_page_manager* pm, zcsr_setting_page_id id) {
    size_t i;
    if (!pm || id == ZCSR_SETTING_PAGE_INVALID) return false;
    for (i = 0; i < pm->count; ++i) {
        if (pm->pages[i].id == id) {
            if (i + 1 < pm->count)
                memmove(&pm->pages[i], &pm->pages[i + 1],
                        (pm->count - i - 1) * sizeof(pm->pages[0]));
            --pm->count;
            memset(&pm->pages[pm->count], 0, sizeof(pm->pages[0]));
            return true;
        }
    }
    return false;
}

bool zcsr_ui_page_manager_find(const zcsr_ui_page_manager* pm, const char* key,
                               zcsr_setting_page_id* out_id) {
    size_t i;
    if (!pm || !key || !key[0] || !out_id) return false;
    for (i = 0; i < pm->count; ++i) {
        if (strcmp(pm->pages[i].key, key) == 0) {
            *out_id = pm->pages[i].id;
            return true;
        }
    }
    return false;
}

size_t zcsr_ui_page_manager_count(const zcsr_ui_page_manager* pm) {
    return pm ? pm->count : 0;
}

const char* zcsr_ui_page_manager_key(const zcsr_ui_page_manager* pm,
                                     zcsr_setting_page_id id) {
    const zcsr_setting_page* page = page_at_id_const(pm, id);
    return page ? page->key : NULL;
}

bool zcsr_ui_page_manager_set_visible(zcsr_ui_page_manager* pm,
                                      zcsr_setting_page_id id, bool visible) {
    zcsr_setting_page* page = page_at_id(pm, id);
    if (!page) return false;
    page->visible = visible;
    return true;
}

bool zcsr_ui_page_manager_visible(const zcsr_ui_page_manager* pm,
                                  zcsr_setting_page_id id) {
    const zcsr_setting_page* page = page_at_id_const(pm, id);
    return page ? page->visible : false;
}
