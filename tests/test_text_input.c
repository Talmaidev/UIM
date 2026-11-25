/* Contract test for the UIM text input: UTF-8 editing, maxLength, caret and
 * selection, focus baseline, commit/revert, and typed value handoff. */
#include "uim_text_input.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_TEXT(input, expected) do { \
    const char *text_ = NULL; size_t bytes_ = 0; \
    CHECK(uim_text_input_get_text((input), &text_, &bytes_) == uim_OK); \
    if (bytes_ != strlen(expected) || memcmp(text_, expected, bytes_) != 0) { \
        printf("FAIL %d: text '%.*s', expected '%s'\n", __LINE__, (int)bytes_, text_, expected); \
        failures++; \
    } \
} while (0)

int main(void) {
    uim_TextInput input;
    uim_TextInput small;
    uim_TextInput utf8;
    const char *text = NULL;
    size_t bytes = 0;
    size_t start = 0, length = 0;
    uim_Value value;
    int64_t as_int = 0;
    double as_double = 0.0;

    /* --- init / basic editing --- */
    CHECK(uim_text_input_init(&input, 64u) == uim_OK);
    CHECK(uim_text_input_init(&input, 0u) == uim_ERR_ARGUMENT);
    CHECK(uim_text_input_init(&input, uim_TEXT_INPUT_MAX_BYTES + 1u) == uim_ERR_ARGUMENT);
    CHECK(uim_text_input_init(&input, 64u) == uim_OK);
    CHECK(input.length == 0u && input.cursor == 0u);
    CHECK(!uim_text_input_is_focused(&input));

    CHECK(uim_text_input_insert(&input, "hello", 5u) == uim_OK);
    CHECK_TEXT(&input, "hello");
    CHECK(input.cursor == 5u);

    /* CJK: one character is three bytes; caret and deletion stay on boundaries. */
    CHECK(uim_text_input_insert(&input, "\xE4\xB8\x96\xE7\x95\x8C", 6u) == uim_OK);
    CHECK(input.length == 11u);
    CHECK(uim_text_input_backspace(&input) == uim_OK); /* removes U+754C */
    CHECK_TEXT(&input, "hello\xE4\xB8\x96");
    CHECK(input.cursor == 8u);
    CHECK(uim_text_input_move_cursor(&input, -1) == uim_OK); /* before U+4E16 */
    CHECK(input.cursor == 5u);
    CHECK(uim_text_input_delete_forward(&input) == uim_OK);
    CHECK_TEXT(&input, "hello");

    CHECK(uim_text_input_select_all(&input) == uim_OK);
    CHECK(uim_text_input_selection(&input, &start, &length) == uim_OK);
    CHECK(start == 0u && length == 5u);
    CHECK(uim_text_input_insert(&input, "ab", 2u) == uim_OK); /* replaces selection */
    CHECK_TEXT(&input, "ab");
    CHECK(input.cursor == 2u && input.anchor == 2u);

    /* --- maxLength: refuse, never truncate silently --- */
    CHECK(uim_text_input_init(&small, 4u) == uim_OK);
    CHECK(uim_text_input_set_text(&small, "abcdef", 6u) == uim_ERR_BUFFER);
    CHECK(small.length == 0u);
    CHECK(uim_text_input_insert(&small, "abcd", 4u) == uim_OK);
    CHECK(uim_text_input_insert(&small, "e", 1u) == uim_ERR_BUFFER);
    CHECK_TEXT(&small, "abcd");
    /* Selecting everything frees room for a same-length replacement. */
    CHECK(uim_text_input_select_all(&small) == uim_OK);
    CHECK(uim_text_input_insert(&small, "wxyz", 4u) == uim_OK);
    CHECK_TEXT(&small, "wxyz");

    /* --- invalid UTF-8 is rejected and nothing is stored --- */
    CHECK(uim_text_input_insert(&small, "\xFF", 1u) == uim_ERR_ARGUMENT);
    CHECK(uim_text_input_set_text(&small, "\xE4\xB8", 2u) == uim_ERR_ARGUMENT);
    CHECK_TEXT(&small, "wxyz");

    /* --- caret must sit on a code point boundary --- */
    CHECK(uim_text_input_init(&utf8, 32u) == uim_OK);
    CHECK(uim_text_input_set_text(&utf8, "\xE4\xB8\x96\xE7\x95\x8C", 6u) == uim_OK);
    CHECK(uim_text_input_set_cursor(&utf8, 1u) == uim_ERR_ARGUMENT);
    CHECK(uim_text_input_set_cursor(&utf8, 3u) == uim_OK);
    CHECK(utf8.cursor == 3u);
    CHECK(uim_text_input_set_cursor(&utf8, 7u) == uim_ERR_RANGE);
    CHECK(uim_text_input_cursor_home(&utf8) == uim_OK);
    CHECK(utf8.cursor == 0u);
    CHECK(uim_text_input_cursor_end(&utf8) == uim_OK);
    CHECK(utf8.cursor == 6u);
    CHECK(uim_text_input_move_cursor(&utf8, -1) == uim_OK);
    CHECK(utf8.cursor == 3u);
    CHECK(uim_text_input_move_cursor(&utf8, -100) == uim_OK);
    CHECK(utf8.cursor == 0u);
    CHECK(uim_text_input_move_cursor(&utf8, 100) == uim_OK);
    CHECK(utf8.cursor == 6u);

    /* Explicit selection and Shift+Arrow extension: moving the cursor must not
     * collapse the fixed anchor. */
    CHECK(uim_text_input_set_selection(&utf8, 3u, 0u) == uim_OK);
    CHECK(uim_text_input_selection(&utf8, &start, &length) == uim_OK);
    CHECK(start == 0u && length == 3u);
    CHECK(utf8.anchor == 3u && utf8.cursor == 0u);
    CHECK(uim_text_input_move_cursor_extend(&utf8, 1) == uim_OK);
    CHECK(utf8.anchor == 3u && utf8.cursor == 3u);
    CHECK(uim_text_input_selection(&utf8, &start, &length) == uim_OK);
    CHECK(start == 3u && length == 0u);
    CHECK(uim_text_input_set_selection(&utf8, 1u, 2u) == uim_ERR_ARGUMENT);
    CHECK(uim_text_input_set_selection(&utf8, 0u, 7u) == uim_ERR_RANGE);
    CHECK(uim_text_input_set_selection(&utf8, 0u, 3u) == uim_OK);
    CHECK(uim_text_input_move_cursor_extend(&utf8, -100) == uim_OK);
    CHECK(utf8.anchor == 0u && utf8.cursor == 0u);

    /* --- data handoff --- */
    CHECK(uim_text_input_set_text(&input, "42", 2u) == uim_OK);
    CHECK(uim_text_input_value_as_int(&input, &as_int) == uim_OK && as_int == 42);
    CHECK(uim_text_input_set_text(&input, " 42", 3u) == uim_OK);
    CHECK(uim_text_input_value_as_int(&input, &as_int) == uim_ERR_TYPE);
    CHECK(uim_text_input_set_text(&input, "4.2", 3u) == uim_OK);
    CHECK(uim_text_input_value_as_int(&input, &as_int) == uim_ERR_TYPE);
    CHECK(uim_text_input_set_text(&input, "999999999999999999999999", 24u) == uim_OK);
    CHECK(uim_text_input_value_as_int(&input, &as_int) == uim_ERR_RANGE);

    CHECK(uim_text_input_set_text(&input, "3.5", 3u) == uim_OK);
    CHECK(uim_text_input_value_as_double(&input, &as_double) == uim_OK);
    CHECK(fabs(as_double - 3.5) < 1e-12);
    CHECK(uim_text_input_set_text(&input, "nan", 3u) == uim_OK);
    CHECK(uim_text_input_value_as_double(&input, &as_double) == uim_ERR_TYPE);
    CHECK(uim_text_input_set_text(&input, "0x10", 4u) == uim_OK);
    CHECK(uim_text_input_value_as_double(&input, &as_double) == uim_ERR_TYPE);

    CHECK(uim_text_input_set_value(&input, uim_value_int(7)) == uim_OK);
    CHECK_TEXT(&input, "7");
    CHECK(uim_text_input_set_value(&input, uim_value_float(1.5)) == uim_OK);
    CHECK_TEXT(&input, "1.5");
    CHECK(uim_text_input_set_value(&input, uim_value_bool(true)) == uim_OK);
    CHECK_TEXT(&input, "true");
    CHECK(uim_text_input_set_value(&input, uim_value_string("xyz", 3u)) == uim_OK);
    CHECK_TEXT(&input, "xyz");
    {
        uim_Value none;
        none.type = uim_VALUE_NONE;
        none.as.integer = 0;
        CHECK(uim_text_input_set_value(&input, none) == uim_ERR_TYPE);
    }
    value = uim_text_input_value(&input);
    CHECK(value.type == uim_VALUE_STRING);
    CHECK(value.as.string.size == 3u);
    CHECK(memcmp(value.as.string.data, "xyz", 3u) == 0);

    /* --- focus baseline / cancel / submit --- */
    CHECK(uim_text_input_set_text(&input, "orig", 4u) == uim_OK);
    CHECK(uim_text_input_submit(&input, &value) == uim_ERR_STATE); /* not focused */
    CHECK(uim_text_input_cancel(&input, &value) == uim_ERR_STATE);

    CHECK(uim_text_input_focus(&input) == uim_OK);
    CHECK(uim_text_input_is_focused(&input));
    CHECK(!uim_text_input_is_dirty(&input));
    CHECK(uim_text_input_insert(&input, "!", 1u) == uim_OK);
    CHECK_TEXT(&input, "orig!");
    CHECK(uim_text_input_is_dirty(&input));
    CHECK(uim_text_input_cancel(&input, &value) == uim_OK);
    CHECK_TEXT(&input, "orig");
    CHECK(!uim_text_input_is_focused(&input));
    CHECK(!uim_text_input_is_dirty(&input));

    CHECK(uim_text_input_focus(&input) == uim_OK);
    CHECK(uim_text_input_insert(&input, "!", 1u) == uim_OK);
    CHECK(uim_text_input_submit(&input, NULL) == uim_OK);
    CHECK_TEXT(&input, "orig!");
    CHECK(!uim_text_input_is_focused(&input));
    CHECK(uim_text_input_set_text(&input, "orig", 4u) == uim_OK);
    CHECK(uim_text_input_focus(&input) == uim_OK);
    CHECK(uim_text_input_insert(&input, "!", 1u) == uim_OK);
    CHECK(uim_text_input_submit(&input, &value) == uim_OK);
    CHECK(value.type == uim_VALUE_STRING && value.as.string.size == 5u);
    CHECK(memcmp(value.as.string.data, "orig!", 5u) == 0);
    CHECK(!uim_text_input_is_focused(&input));
    CHECK(!uim_text_input_is_dirty(&input));
    CHECK_TEXT(&input, "orig!");

    /* blur keeps the text and clears focus without a baseline commit. */
    CHECK(uim_text_input_focus(&input) == uim_OK);
    CHECK(uim_text_input_insert(&input, "?", 1u) == uim_OK);
    CHECK(uim_text_input_blur(&input) == uim_OK);
    CHECK_TEXT(&input, "orig!?");
    CHECK(!uim_text_input_is_dirty(&input));

    /* Boundary no-ops are not errors. */
    CHECK(uim_text_input_init(&small, 8u) == uim_OK);
    CHECK(uim_text_input_backspace(&small) == uim_OK);
    CHECK(uim_text_input_delete_forward(&small) == uim_OK);
    CHECK(small.length == 0u);
    CHECK(uim_text_input_get_text(&small, &text, &bytes) == uim_OK);
    CHECK(bytes == 0u);

    /* Argument guards. */
    CHECK(uim_text_input_init(NULL, 8u) == uim_ERR_ARGUMENT);
    CHECK(uim_text_input_insert(NULL, "a", 1u) == uim_ERR_ARGUMENT);
    CHECK(uim_text_input_get_text(&small, NULL, &bytes) == uim_ERR_ARGUMENT);
    CHECK(uim_text_input_selection(&small, NULL, &length) == uim_ERR_ARGUMENT);

    if (failures == 0) printf("UIM text input contract: ALL PASS\n");
    else printf("%d failures\n", failures);
    return failures;
}
