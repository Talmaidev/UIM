#ifndef UIM_TEXT_INPUT_H
#define UIM_TEXT_INPUT_H
/* UIM text input: the smallest editable text component, owned by the host.
 *
 * Contract (v1):
 * - No allocation, no globals, no window, no renderer, no IME pump, no
 *   clipboard. The host owns the instance, feeds key/text events in and reads
 *   the committed value out. This module never talks to the manager; the host
 *   pushes the value into a component instance / property through the existing
 *   UIM API (uim_value/uim_property_update), so any consumer can reuse it.
 * - Text is UTF-8. Cursor and selection are byte offsets that always sit on a
 *   UTF-8 code point boundary; editing is code point based (a CJK character is
 *   deleted as one character, not as three bytes). Combining marks and ZWJ
 *   emoji are individual code points; full grapheme clustering is a host
 *   concern. IME preedit is also host-side: insert the preedit text and remove
 *   it before committing, or wait for the committed string.
 * - uim_ERR_BUFFER is returned and the buffer is left unchanged when the
 *   operation would exceed max_bytes (maxLength behavior). Invalid UTF-8 is
 *   rejected with uim_ERR_ARGUMENT and never stored.
 * - focus() captures the text as the edit baseline; cancel() restores it.
 *   submit() keeps the text and ends the edit. Both require a focused instance.
 * - uim_text_input_value() and get_text() return borrowed views that stay valid
 *   until the next mutation of the same instance (the same rule as uim_Value).
 */
#include "uim_component_manager.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One editable field. 1024 UTF-8 bytes ~= 256 CJK characters, which is the
 * largest value a builtin text property should carry in a UI row. */
#define uim_TEXT_INPUT_MAX_BYTES 1024

typedef struct uim_TextInput {
    char text[uim_TEXT_INPUT_MAX_BYTES + 1];
    char base[uim_TEXT_INPUT_MAX_BYTES + 1];
    size_t length;      /* read-only: bytes in text */
    size_t base_length; /* read-only: bytes in the focus baseline */
    size_t cursor;      /* read-only: caret byte offset */
    size_t anchor;      /* read-only: selection anchor; == cursor when collapsed */
    size_t max_bytes;
    bool focused;
} uim_TextInput;

/* max_bytes must be 1..uim_TEXT_INPUT_MAX_BYTES; the field starts empty, unfocused. */
uim_Result uim_text_input_init(uim_TextInput *input, size_t max_bytes);

/* Replace the whole text. Fails with uim_ERR_BUFFER when bytes > max_bytes,
 * uim_ERR_ARGUMENT when the payload is not valid UTF-8. */
uim_Result uim_text_input_set_text(uim_TextInput *input, const char *utf8, size_t bytes);
uim_Result uim_text_input_get_text(const uim_TextInput *input,
                                   const char **out_utf8, size_t *out_bytes);

/* Data handoff. value() returns uim_VALUE_STRING with a borrowed pointer;
 * set_value() accepts STRING (copied), INT64, FLOAT64 and BOOL (formatted). */
uim_Value uim_text_input_value(const uim_TextInput *input);
uim_Result uim_text_input_set_value(uim_TextInput *input, uim_Value value);
/* Strict full-string parses. Leading/trailing spaces, partial numbers, NaN and
 * Inf are rejected with uim_ERR_TYPE; out-of-range integers return uim_ERR_RANGE. */
uim_Result uim_text_input_value_as_int(const uim_TextInput *input, int64_t *out_value);
uim_Result uim_text_input_value_as_double(const uim_TextInput *input, double *out_value);

/* begin/end edit. focus() copies the current text as the cancel baseline and
 * places the caret at the end; blur() only drops focus and keeps the text. */
uim_Result uim_text_input_focus(uim_TextInput *input);
uim_Result uim_text_input_blur(uim_TextInput *input);
bool uim_text_input_is_focused(const uim_TextInput *input);
/* True while focused and the text differs from the focus baseline. */
bool uim_text_input_is_dirty(const uim_TextInput *input);

/* Editing. insert() replaces the selection; the other operations delete the
 * selection first when one exists. */
uim_Result uim_text_input_insert(uim_TextInput *input, const char *utf8, size_t bytes);
uim_Result uim_text_input_backspace(uim_TextInput *input);
uim_Result uim_text_input_delete_forward(uim_TextInput *input);

/* delta is counted in code points and clamped at both ends. With a selection,
 * the caret collapses to the selection start for negative delta, else to its end. */
uim_Result uim_text_input_move_cursor(uim_TextInput *input, int delta);
uim_Result uim_text_input_cursor_home(uim_TextInput *input);
uim_Result uim_text_input_cursor_end(uim_TextInput *input);
/* byte_offset must be 0..length and on a code point boundary. */
uim_Result uim_text_input_set_cursor(uim_TextInput *input, size_t byte_offset);
/* Both offsets must be 0..length and on code point boundaries. anchor is the
 * fixed edge, cursor is the moving edge; equal offsets collapse the selection. */
uim_Result uim_text_input_set_selection(uim_TextInput *input, size_t anchor, size_t cursor);
/* Move the cursor by delta code points while keeping anchor fixed (Shift+Arrow).
 * Without an existing selection the anchor is the old cursor. */
uim_Result uim_text_input_move_cursor_extend(uim_TextInput *input, int delta);
uim_Result uim_text_input_select_all(uim_TextInput *input);
uim_Result uim_text_input_selection(const uim_TextInput *input,
                                    size_t *out_start, size_t *out_length);

/* Commit / revert the edit. out_value may be NULL when only the state change
 * matters; otherwise it receives the committed borrowed text. */
uim_Result uim_text_input_submit(uim_TextInput *input, uim_Value *out_value);
uim_Result uim_text_input_cancel(uim_TextInput *input, uim_Value *out_value);

#ifdef __cplusplus
}
#endif
#endif
