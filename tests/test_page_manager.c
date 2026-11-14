#include "zcsr_ui_page_manager/page_manager.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s (line %d)\n", (name), __LINE__); ++failures; } \
} while (0)

int main(void) {
    zcsr_ui_page_manager pm;
    zcsr_ui_page_manager_init(&pm);

    CHECK(zcsr_ui_page_manager_depth(&pm) == 0, "initial depth");
    CHECK(zcsr_ui_page_manager_current(&pm) == NULL, "initial current");
    CHECK(zcsr_ui_page_manager_back(&pm) == false, "back at root");

    CHECK(zcsr_ui_page_manager_open(&pm, "home") == true, "open home");
    CHECK(strcmp(zcsr_ui_page_manager_current(&pm), "home") == 0, "current home");
    CHECK(zcsr_ui_page_manager_open(&pm, "settings") == true, "open settings");
    CHECK(strcmp(zcsr_ui_page_manager_current(&pm), "settings") == 0, "current settings");
    CHECK(zcsr_ui_page_manager_depth(&pm) == 2, "depth 2");

    CHECK(zcsr_ui_page_manager_back(&pm) == true, "back to home");
    CHECK(strcmp(zcsr_ui_page_manager_current(&pm), "home") == 0, "current home again");
    CHECK(zcsr_ui_page_manager_depth(&pm) == 1, "depth 1");

    zcsr_ui_page_manager_close(&pm);
    CHECK(zcsr_ui_page_manager_depth(&pm) == 0, "closed depth");
    CHECK(zcsr_ui_page_manager_current(&pm) == NULL, "closed current");

    zcsr_ui_page_manager_init(&pm);
    for (size_t i = 0; i < ZCSR_UI_PAGE_MANAGER_CAPACITY; ++i) {
        CHECK(zcsr_ui_page_manager_open(&pm, "page") == true, "fill capacity");
    }
    CHECK(zcsr_ui_page_manager_open(&pm, "overflow") == false, "capacity reject");

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("page manager: all checks passed\n");
    return 0;
}
