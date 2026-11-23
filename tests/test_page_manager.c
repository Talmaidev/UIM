#include "zcsr_ui_page_manager/page_manager.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s (line %d)\n", (name), __LINE__); ++failures; } \
} while (0)

int main(void) {
    zcsr_ui_page_manager pm;
    zcsr_setting_page_id video = ZCSR_SETTING_PAGE_INVALID;
    zcsr_setting_page_id audio = ZCSR_SETTING_PAGE_INVALID;
    zcsr_setting_page_id found = ZCSR_SETTING_PAGE_INVALID;

    zcsr_ui_page_manager_init(&pm);
    CHECK(zcsr_ui_page_manager_count(&pm) == 0, "initial count");
    CHECK(zcsr_ui_page_manager_key(&pm, video) == NULL, "unknown key");

    video = zcsr_ui_page_manager_create(&pm, "video");
    CHECK(video != ZCSR_SETTING_PAGE_INVALID, "create video");
    audio = zcsr_ui_page_manager_create(&pm, "audio");
    CHECK(audio != ZCSR_SETTING_PAGE_INVALID, "create audio");
    CHECK(video != audio, "distinct ids");
    CHECK(zcsr_ui_page_manager_count(&pm) == 2, "count two");
    CHECK(strcmp(zcsr_ui_page_manager_key(&pm, video), "video") == 0, "video key");
    CHECK(zcsr_ui_page_manager_create(&pm, "video") == ZCSR_SETTING_PAGE_INVALID,
          "duplicate reject");
    CHECK(zcsr_ui_page_manager_create(&pm, "") == ZCSR_SETTING_PAGE_INVALID,
          "empty key reject");

    CHECK(zcsr_ui_page_manager_find(&pm, "audio", &found) == true, "find audio");
    CHECK(found == audio, "find id");
    CHECK(zcsr_ui_page_manager_find(&pm, "missing", &found) == false, "missing key");

    CHECK(zcsr_ui_page_manager_visible(&pm, video) == true, "visible by default");
    CHECK(zcsr_ui_page_manager_set_visible(&pm, video, false) == true, "hide video");
    CHECK(zcsr_ui_page_manager_visible(&pm, video) == false, "video hidden");
    CHECK(zcsr_ui_page_manager_set_visible(&pm, video, true) == true, "show video");
    CHECK(zcsr_ui_page_manager_visible(&pm, video) == true, "video visible");

    CHECK(zcsr_ui_page_manager_destroy(&pm, audio) == true, "destroy audio");
    CHECK(zcsr_ui_page_manager_count(&pm) == 1, "count one");
    CHECK(zcsr_ui_page_manager_key(&pm, audio) == NULL, "destroyed key");
    CHECK(zcsr_ui_page_manager_destroy(&pm, audio) == false, "destroy again");

    zcsr_ui_page_manager_init(&pm);
    {
        static char keys[ZCSR_SETTING_PAGE_MANAGER_CAPACITY][24];
        for (size_t i = 0; i < ZCSR_SETTING_PAGE_MANAGER_CAPACITY; ++i) {
            snprintf(keys[i], sizeof keys[i], "page_%zu", i);
            CHECK(zcsr_ui_page_manager_create(&pm, keys[i]) != ZCSR_SETTING_PAGE_INVALID,
                  "fill capacity");
        }
    }
    CHECK(zcsr_ui_page_manager_create(&pm, "overflow") == ZCSR_SETTING_PAGE_INVALID,
          "capacity reject");

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("page manager: all checks passed\n");
    return 0;
}
