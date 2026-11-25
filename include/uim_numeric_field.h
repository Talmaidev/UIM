#ifndef UIM_NUMERIC_FIELD_H
#define UIM_NUMERIC_FIELD_H
/* uim_numeric_field.h — the numeric editor atom: clamped int/float value with
 * step, drag-delta, typed-text validation and a commit/revert baseline.
 *
 * What it owns:
 * - range, step, current value and the committed baseline (dirty tracking).
 * - strict full-string parse for typed entry; invalid text is rejected with
 *   uim_ERR_TYPE and the stored value is left untouched.
 * - edge clamping with uim_ERR_RANGE, read-only rejection with uim_ERR_STATE.
 *
 * What it does not own: the text buffer/caret/selection (uim_text_input owns
 * that), drawing, spin-button geometry, wheel speed or the numeric display
 * format (uim_format owns that). The host feeds typed text in, applies step
 * or drag deltas, and commits/cancels on Enter / focus loss / Escape.
 *
 * Contract: pure state + math. No allocation, no globals, no renderer, no OS
 * header. All inputs are finite; NaN/Inf are rejected with uim_ERR_ARGUMENT.
 */
#include "uim_types.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uim_NumericKind {
    uim_NUMERIC_FLOAT = 0,
    uim_NUMERIC_INT = 1
} uim_NumericKind;

typedef struct uim_NumericField {
    uim_NumericKind kind;
    double min_value;
    double max_value;
    double step;
    double value;
    double baseline;
    bool dirty;
    bool read_only;
} uim_NumericField;

static inline bool uim_numeric__finite(double v) { return isfinite(v) != 0; }

/* step may be 0 when the field has no +/- buttons; it must not be negative.
 * An initial value outside [min, max] is clamped and uim_ERR_RANGE is returned. */
static inline uim_Result uim_numeric_field_init(uim_NumericField *f, uim_NumericKind kind,
                                                double min_value, double max_value,
                                                double value, double step) {
    if (!f || !uim_numeric__finite(min_value) || !uim_numeric__finite(max_value) ||
        !uim_numeric__finite(value) || !uim_numeric__finite(step) ||
        step < 0.0 || min_value > max_value ||
        (kind != uim_NUMERIC_FLOAT && kind != uim_NUMERIC_INT))
        return uim_ERR_ARGUMENT;
    f->kind = kind;
    f->min_value = min_value;
    f->max_value = max_value;
    f->step = step;
    f->read_only = false;
    {
        bool clamped = value < min_value || value > max_value;
        if (value < min_value) value = min_value;
        else if (value > max_value) value = max_value;
        f->value = value;
        f->baseline = value;
        f->dirty = false;
        return clamped ? uim_ERR_RANGE : uim_OK;
    }
}

static inline uim_Result uim_numeric_field_set_read_only(uim_NumericField *f, bool read_only) {
    if (!f) return uim_ERR_ARGUMENT;
    f->read_only = read_only;
    return uim_OK;
}

static inline bool uim_numeric_field_is_read_only(const uim_NumericField *f) {
    return f ? f->read_only : false;
}

static inline bool uim_numeric_field_is_dirty(const uim_NumericField *f) {
    return f ? f->dirty : false;
}

static inline uim_Result uim_numeric_field_clamp(uim_NumericField *f, double value,
                                                  double *out_clamped) {
    bool clamped = false;
    if (!f || !out_clamped || !uim_numeric__finite(value)) return uim_ERR_ARGUMENT;
    if (value < f->min_value) { value = f->min_value; clamped = true; }
    else if (value > f->max_value) { value = f->max_value; clamped = true; }
    *out_clamped = value;
    return clamped ? uim_ERR_RANGE : uim_OK;
}

static inline uim_Result uim_numeric_field_set_value(uim_NumericField *f, double value) {
    double clamped;
    uim_Result rc;
    if (!f) return uim_ERR_ARGUMENT;
    if (f->read_only) return uim_ERR_STATE;
    rc = uim_numeric_field_clamp(f, value, &clamped);
    if (rc == uim_ERR_ARGUMENT) return rc;
    f->value = clamped;
    f->dirty = clamped != f->baseline;
    return rc;
}

/* Step by delta units (negative = down). A zero step disables stepping and
 * leaves the value untouched (uim_OK). Clamping at an edge returns ERR_RANGE. */
static inline uim_Result uim_numeric_field_step(uim_NumericField *f, int delta) {
    double target, clamped;
    uim_Result rc;
    if (!f) return uim_ERR_ARGUMENT;
    if (f->read_only) return uim_ERR_STATE;
    if (delta == 0 || f->step <= 0.0) return uim_OK;
    target = f->value + (double)delta * f->step;
    rc = uim_numeric_field_clamp(f, target, &clamped);
    if (rc == uim_ERR_ARGUMENT) return rc;
    f->value = clamped;
    f->dirty = clamped != f->baseline;
    return rc;
}

/* Drag delta already converted to value units by the host (pixels * speed). */
static inline uim_Result uim_numeric_field_drag_by(uim_NumericField *f, double delta_units) {
    double target, clamped;
    uim_Result rc;
    if (!f || !uim_numeric__finite(delta_units)) return uim_ERR_ARGUMENT;
    if (f->read_only) return uim_ERR_STATE;
    target = f->value + delta_units;
    rc = uim_numeric_field_clamp(f, target, &clamped);
    if (rc == uim_ERR_ARGUMENT) return rc;
    f->value = clamped;
    f->dirty = clamped != f->baseline;
    return rc;
}

/* Strict full-string number parse. Spaces anywhere, NaN, Inf, partial numbers
 * and trailing junk are uim_ERR_TYPE; out-of-range values are clamped and
 * reported uim_ERR_RANGE. The stored value is unchanged on rejection. */
static inline uim_Result uim_numeric_field_parse(uim_NumericField *f,
                                                 const char *text, size_t bytes) {
    char buf[128];
    size_t i, copy;
    char *end = NULL;
    double parsed;
    long long as_int;
    bool looks_int;
    if (!f || !text) return uim_ERR_ARGUMENT;
    if (f->read_only) return uim_ERR_STATE;
    if (bytes == 0 || bytes >= sizeof(buf)) return uim_ERR_TYPE;
    for (i = 0; i < bytes; i++) {
        if (text[i] == ' ' || text[i] == '\t' || text[i] == '\n' || text[i] == '\r')
            return uim_ERR_TYPE;
    }
    copy = bytes;
    memcpy(buf, text, copy);
    buf[copy] = '\0';
    if (buf[0] == 'n' || buf[0] == 'N' || buf[0] == 'i' || buf[0] == 'I')
        return uim_ERR_TYPE;
    parsed = strtod(buf, &end);
    if (end != buf + copy || !uim_numeric__finite(parsed)) return uim_ERR_TYPE;
    if (f->kind == uim_NUMERIC_INT) {
        looks_int = true;
        for (i = 0; i < copy; i++) {
            if (buf[i] == '.' || buf[i] == 'e' || buf[i] == 'E') { looks_int = false; break; }
        }
        if (!looks_int) return uim_ERR_TYPE;
        as_int = (long long)parsed;
        if ((double)as_int != parsed) return uim_ERR_RANGE;
        parsed = (double)as_int;
    }
    if (parsed < f->min_value || parsed > f->max_value) return uim_ERR_RANGE;
    return uim_numeric_field_set_value(f, parsed);
}

/* Commit keeps the current value as the new baseline and clears dirty. */
static inline uim_Result uim_numeric_field_commit(uim_NumericField *f) {
    if (!f) return uim_ERR_ARGUMENT;
    f->baseline = f->value;
    f->dirty = false;
    return uim_OK;
}

/* Revert restores the baseline and clears dirty. */
static inline uim_Result uim_numeric_field_cancel(uim_NumericField *f) {
    if (!f) return uim_ERR_ARGUMENT;
    f->value = f->baseline;
    f->dirty = false;
    return uim_OK;
}

static inline uim_Result uim_numeric_field_get_double(const uim_NumericField *f, double *out) {
    if (!f || !out) return uim_ERR_ARGUMENT;
    *out = f->value;
    return uim_OK;
}

static inline uim_Result uim_numeric_field_get_int64(const uim_NumericField *f, int64_t *out) {
    if (!f || !out) return uim_ERR_ARGUMENT;
    *out = (int64_t)f->value;
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_NUMERIC_FIELD_H */
