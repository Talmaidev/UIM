// tests/test_cpp_consumer.cpp — C++ consumption contract for UIM.
//
// UIM is a C11 library, but its public headers are consumed from C++ (the
// editor is a C++ TU). This test is the regression guard for that boundary:
// include the headers from C++, call exported functions, and link against the
// C archive. It catches missing extern "C" declarations (link-time name
// mangling) and C-only header constructs that a C compile would not see.
//
// The target is optional: if the host has no C++ compiler, CMake skips it.
#include "uim_component_manager.h"
#include "uim_dialog.h"
#include "uim_format.h"
#include "uim_input_scale.h"
#include "uim_layout.h"
#include "uim_menu.h"
#include "uim_numeric_field.h"
#include "uim_property_row.h"
#include "uim_selection.h"
#include "uim_splitter.h"
#include "uim_text_input.h"
#include "uim_text_layout.h"
#include "uim_theme.h"
#include "uim_tooltip.h"
#include "uim_tree.h"

#include <cstdio>
#include <cstring>

static float mono_advance(void *, uint32_t) { return 10.0f; }

int main() {
    UIM_ComponentManager *manager = nullptr;
    UIM_ComponentManagerConfig config{0, 0};
    if (uim_component_manager_create(&config, &manager) != uim_OK || manager == nullptr)
        return 1;
    uim_component_manager_destroy(manager);

    uim_InputScale scale;
    if (uim_input_scale_init_default(&scale) != uim_OK) return 2;

    uim_TextInput input;
    if (uim_text_input_init(&input, 64) != uim_OK) return 3;
    if (uim_text_input_set_text(&input, "C++", 3) != uim_OK) return 4;
    if (uim_text_input_set_selection(&input, 0, 2) != uim_OK) return 11;
    size_t sel_start = 0, sel_len = 0;
    if (uim_text_input_selection(&input, &sel_start, &sel_len) != uim_OK) return 12;
    if (sel_start != 0 || sel_len != 2) return 13;
    if (uim_text_input_move_cursor_extend(&input, 1) != uim_OK) return 14;
    if (input.anchor != 0 || input.cursor != 3) return 15;

    uim_Selection selection;
    if (uim_selection_init(&selection) != uim_OK) return 5;
    uim_selection_add(&selection, 1u);

    uim_Rect container{0, 0, 200, 40};
    uim_Rect rects[2];
    uim_LayoutSpec spec{};
    spec.axis = uim_LAYOUT_ROW;
    spec.gap = 8.0f;
    uim_LayoutItem items[2] = {{40.0f, 0.0f, 0.0f, 0.0f, 20.0f},
                               {40.0f, 0.0f, 0.0f, 0.0f, 20.0f}};
    if (uim_layout_solve(&container, &spec, items, 2, rects) != uim_OK) return 6;

    char text[32];
    if (uim_format_int(1234567, true, text, sizeof(text)) != uim_OK) return 7;
    if (std::strcmp(text, "1,234,567") != 0) return 8;

    uim_TextLine lines[4];
    size_t count = 4;
    bool truncated = false;
    if (uim_text_wrap("hello world", 11, 60.0f, mono_advance, nullptr, 0, lines,
                      &count, &truncated) != uim_OK)
        return 9;
    if (count != 2) return 10;

    uim_NumericField num;
    if (uim_numeric_field_init(&num, uim_NUMERIC_FLOAT, 0.0, 100.0, 20.0, 5.0) != uim_OK)
        return 20;
    if (uim_numeric_field_step(&num, 1) != uim_OK) return 21;

    uim_DialogStack dialogs;
    if (uim_dialog_init(&dialogs) != uim_OK) return 22;
    if (uim_dialog_open(&dialogs, 1u, true, 0u, nullptr) != uim_OK) return 23;

    uim_Menu menu;
    if (uim_menu_init(&menu) != uim_OK) return 24;

    uim_Tree tree;
    if (uim_tree_init(&tree) != uim_OK) return 25;

    uim_Tooltip tip;
    if (uim_tooltip_init(&tip, 0.25f) != uim_OK) return 26;

    uim_PropertyRow row;
    if (uim_property_row_init(&row, 80.0f) != uim_OK) return 27;

    uim_Splitter splitter;
    if (uim_splitter_init(&splitter, uim_LAYOUT_ROW, 0.3f, 20.0f, 20.0f) != uim_OK)
        return 28;

    return 0;
}
