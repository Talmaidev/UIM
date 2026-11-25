#include "uim_text_input.h"
#include "zcsr/ui_text_edit.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This module owns policy (maxLength, focus baseline, value handoff) and no
 * editing algorithm: UTF-8 boundaries, caret, selection and length limits all
 * come from zcsr/ui_text_edit.h, the same header the engine widget uses. */

/* ---------- internal helpers ---------- */

static bool input_ok(const uim_TextInput *input) {
    return input && input->max_bytes >= 1u &&
           input->max_bytes <= (size_t)uim_TEXT_INPUT_MAX_BYTES &&
           input->length <= input->max_bytes &&
           input->cursor <= input->length &&
           input->anchor <= input->length;
}

/* Bind the UIM field to the shared view over its own buffer. */
static zcsr_text_edit view_of(uim_TextInput *input) {
    zcsr_text_edit e;
    e.data = input->text;
    e.capacity = input->max_bytes;
    e.length = input->length;
    e.cursor = input->cursor;
    e.anchor = input->anchor;
    e.read_only = false;
    return e;
}

static void view_store(uim_TextInput *input, const zcsr_text_edit *e) {
    input->length = e->length;
    input->cursor = e->cursor;
    input->anchor = e->anchor;
}

static uim_Result map_status(zcsr_text_status st) {
    switch (st) {
    case ZCSR_TEXT_OK:            return uim_OK;
    case ZCSR_TEXT_ERR_BUFFER:    return uim_ERR_BUFFER;
    case ZCSR_TEXT_ERR_RANGE:     return uim_ERR_RANGE;
    case ZCSR_TEXT_ERR_READ_ONLY: return uim_ERR_STATE;
    default:                      return uim_ERR_ARGUMENT;
    }
}

static uim_Result store_text(uim_TextInput *input, const char *utf8, size_t bytes) {
    zcsr_text_edit e = view_of(input);
    zcsr_text_status st = zcsr_text_set(&e, utf8, bytes);
    if (st != ZCSR_TEXT_OK) return map_status(st);
    view_store(input, &e);
    return uim_OK;
}

/* ---------- lifecycle ---------- */

uim_Result uim_text_input_init(uim_TextInput *input, size_t max_bytes) {
    if (!input || max_bytes == 0u || max_bytes > (size_t)uim_TEXT_INPUT_MAX_BYTES) {
        return uim_ERR_ARGUMENT;
    }
    memset(input, 0, sizeof(*input));
    input->max_bytes = max_bytes;
    input->text[0] = '\0';
    input->base[0] = '\0';
    return uim_OK;
}

uim_Result uim_text_input_set_text(uim_TextInput *input, const char *utf8, size_t bytes) {
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    return store_text(input, utf8, bytes);
}

uim_Result uim_text_input_get_text(const uim_TextInput *input,
                                   const char **out_utf8, size_t *out_bytes) {
    if (!input_ok(input) || !out_utf8 || !out_bytes) return uim_ERR_ARGUMENT;
    *out_utf8 = input->text;
    *out_bytes = input->length;
    return uim_OK;
}

/* ---------- value handoff ---------- */

static uim_Value text_input_value(const uim_TextInput *input) {
    uim_Value value;
    memset(&value, 0, sizeof value);
    if (!input) return value;
    value.type = uim_VALUE_STRING;
    value.as.string.data = input->text;
    value.as.string.size = input->length;
    return value;
}

uim_Value uim_text_input_value(const uim_TextInput *input) {
    return text_input_value(input);
}

uim_Result uim_text_input_set_value(uim_TextInput *input, uim_Value value) {
    char buffer[64];
    int written;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    switch (value.type) {
    case uim_VALUE_STRING:
        return store_text(input, value.as.string.data, value.as.string.size);
    case uim_VALUE_INT64:
        written = snprintf(buffer, sizeof(buffer), "%" PRId64, value.as.integer);
        break;
    case uim_VALUE_FLOAT64:
        if (!isfinite(value.as.real)) return uim_ERR_ARGUMENT;
        written = snprintf(buffer, sizeof(buffer), "%.15g", value.as.real);
        break;
    case uim_VALUE_BOOL:
        written = snprintf(buffer, sizeof(buffer), "%s", value.as.boolean ? "true" : "false");
        break;
    default:
        return uim_ERR_TYPE;
    }
    if (written < 0 || (size_t)written >= sizeof(buffer)) return uim_ERR_BUFFER;
    return store_text(input, buffer, (size_t)written);
}

uim_Result uim_text_input_value_as_int(const uim_TextInput *input, int64_t *out_value) {
    size_t i = 0;
    long long parsed;
    if (!input_ok(input) || !out_value) return uim_ERR_ARGUMENT;
    if (input->length == 0u) return uim_ERR_TYPE;
    if (input->text[0] == '-' || input->text[0] == '+') i = 1u;
    if (i >= input->length) return uim_ERR_TYPE;
    for (; i < input->length; ++i) {
        if (input->text[i] < '0' || input->text[i] > '9') return uim_ERR_TYPE;
    }
    errno = 0;
    parsed = strtoll(input->text, NULL, 10);
    if (errno == ERANGE) return uim_ERR_RANGE;
    *out_value = (int64_t)parsed;
    return uim_OK;
}

uim_Result uim_text_input_value_as_double(const uim_TextInput *input, double *out_value) {
    size_t i;
    char *end = NULL;
    double parsed;
    if (!input_ok(input) || !out_value) return uim_ERR_ARGUMENT;
    if (input->length == 0u) return uim_ERR_TYPE;
    /* Decimal only: no hex, no inf/nan, no spaces. */
    for (i = 0; i < input->length; ++i) {
        char c = input->text[i];
        if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' || c == 'e' || c == 'E') {
            continue;
        }
        return uim_ERR_TYPE;
    }
    errno = 0;
    parsed = strtod(input->text, &end);
    if (end != input->text + input->length || errno == ERANGE || !isfinite(parsed)) {
        return uim_ERR_TYPE;
    }
    *out_value = parsed;
    return uim_OK;
}

/* ---------- focus / baseline ---------- */

uim_Result uim_text_input_focus(uim_TextInput *input) {
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    if (input->focused) return uim_OK;
    memcpy(input->base, input->text, input->length + 1u);
    input->base_length = input->length;
    input->focused = true;
    input->cursor = input->length;
    input->anchor = input->length;
    return uim_OK;
}

uim_Result uim_text_input_blur(uim_TextInput *input) {
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    input->focused = false;
    return uim_OK;
}

bool uim_text_input_is_focused(const uim_TextInput *input) {
    return input ? input->focused : false;
}

bool uim_text_input_is_dirty(const uim_TextInput *input) {
    if (!input_ok(input) || !input->focused) return false;
    if (input->length != input->base_length) return true;
    return memcmp(input->text, input->base, input->length) != 0;
}

/* ---------- editing (shared algorithm) ---------- */

uim_Result uim_text_input_insert(uim_TextInput *input, const char *utf8, size_t bytes) {
    zcsr_text_edit e;
    zcsr_text_status st;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    st = zcsr_text_insert(&e, utf8, bytes);
    if (st != ZCSR_TEXT_OK) return map_status(st);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_backspace(uim_TextInput *input) {
    zcsr_text_edit e;
    zcsr_text_status st;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    st = zcsr_text_backspace(&e, NULL);
    if (st != ZCSR_TEXT_OK) return map_status(st);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_delete_forward(uim_TextInput *input) {
    zcsr_text_edit e;
    zcsr_text_status st;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    st = zcsr_text_delete_forward(&e, NULL);
    if (st != ZCSR_TEXT_OK) return map_status(st);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_move_cursor(uim_TextInput *input, int delta) {
    zcsr_text_edit e;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    zcsr_text_move_cursor(&e, delta);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_cursor_home(uim_TextInput *input) {
    zcsr_text_edit e;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    zcsr_text_cursor_home(&e);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_cursor_end(uim_TextInput *input) {
    zcsr_text_edit e;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    zcsr_text_cursor_end(&e);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_set_cursor(uim_TextInput *input, size_t byte_offset) {
    zcsr_text_edit e;
    zcsr_text_status st;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    st = zcsr_text_set_cursor(&e, byte_offset);
    if (st != ZCSR_TEXT_OK) return map_status(st);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_set_selection(uim_TextInput *input, size_t anchor, size_t cursor) {
    zcsr_text_edit e;
    zcsr_text_status st;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    st = zcsr_text_set_selection(&e, anchor, cursor);
    if (st != ZCSR_TEXT_OK) return map_status(st);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_move_cursor_extend(uim_TextInput *input, int delta) {
    zcsr_text_edit e;
    size_t anchor, pos;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    if (delta == 0) return uim_OK;
    e = view_of(input);
    anchor = e.anchor;
    pos = e.cursor;
    while (delta < 0 && pos > 0u) {
        pos = zcsr_text_utf8_prev(e.data, pos);
        delta++;
    }
    while (delta > 0 && pos < e.length) {
        pos = zcsr_text_utf8_next(e.data, e.length, pos);
        delta--;
    }
    e.anchor = anchor;
    e.cursor = pos;
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_select_all(uim_TextInput *input) {
    zcsr_text_edit e;
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    e = view_of(input);
    zcsr_text_select_all(&e);
    view_store(input, &e);
    return uim_OK;
}

uim_Result uim_text_input_selection(const uim_TextInput *input,
                                    size_t *out_start, size_t *out_length) {
    size_t start = 0u, end = 0u;
    if (!input_ok(input) || !out_start || !out_length) return uim_ERR_ARGUMENT;
    if (input->anchor <= input->cursor) {
        start = input->anchor;
        end = input->cursor;
    } else {
        start = input->cursor;
        end = input->anchor;
    }
    *out_start = start;
    *out_length = end - start;
    return uim_OK;
}

/* ---------- commit / revert ---------- */

uim_Result uim_text_input_submit(uim_TextInput *input, uim_Value *out_value) {
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    if (!input->focused) return uim_ERR_STATE;
    memcpy(input->base, input->text, input->length + 1u);
    input->base_length = input->length;
    input->focused = false;
    if (out_value) *out_value = text_input_value(input);
    return uim_OK;
}

uim_Result uim_text_input_cancel(uim_TextInput *input, uim_Value *out_value) {
    if (!input_ok(input)) return uim_ERR_ARGUMENT;
    if (!input->focused) return uim_ERR_STATE;
    memcpy(input->text, input->base, input->base_length + 1u);
    input->length = input->base_length;
    input->cursor = input->length;
    input->anchor = input->length;
    input->focused = false;
    if (out_value) *out_value = text_input_value(input);
    return uim_OK;
}
