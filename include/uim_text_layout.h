#ifndef UIM_TEXT_LAYOUT_H
#define UIM_TEXT_LAYOUT_H
/* uim_text_layout.h — the text-metrics atom: measure, wrap, ellipsize.
 *
 * The settings page currently estimates text width as strlen * 8 * scale and
 * wraps on spaces only, capping at 6 lines and 128 bytes per line. That is
 * wrong for CJK (three UTF-8 bytes per glyph, counted three times) and for any
 * proportional font. This header replaces both with one greedy line breaker
 * driven by a caller-supplied advance function, so the engine can pass its
 * real font metrics and a test can pass a deterministic monospace stub.
 *
 * Contract:
 * - Header only, no allocation, no globals; the caller owns every buffer.
 * - UTF-8 decoding is delegated to zcsr/ui_text_edit.h (the single decoder in
 *   the workspace); UIM already includes the engine's public headers.
 * - The advance callback returns the advance in pixels for one code point;
 *   it must be finite and >= 0. A NULL callback, invalid UTF-8 or a negative
 *   advance is uim_ERR_ARGUMENT and changes no output.
 * - Wrapping is greedy: break at the last space / hyphen / slash, otherwise
 *   between CJK glyphs, otherwise mid-token. Trailing spaces are excluded from
 *   a line's byte_length and width. '\n' is a hard break; '\r' is ignored.
 * - max_lines == 0 means unlimited; a positive value cuts the result and sets
 *   truncated. A full output array returns uim_ERR_BUFFER with the valid
 *   prefix still written and truncated set.
 */
#include "uim_types.h"

#include "zcsr/ui_text_edit.h"

#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef float (*uim_TextAdvanceFn)(void *user, uint32_t codepoint);

typedef struct uim_TextLine {
    size_t byte_start;
    size_t byte_length;   /* excludes trailing spaces */
    float  width;         /* excludes trailing spaces */
    bool   hard_break;    /* produced by '\n' (or by the end of the text) */
} uim_TextLine;

/* Measure one UTF-8 string. Empty text is 0 wide. */
static inline uim_Result uim_text_measure(const char *utf8, size_t bytes,
                                          uim_TextAdvanceFn advance, void *user,
                                          float *out_width) {
    size_t pos = 0;
    float width = 0.0f;
    if (!out_width || !advance) return uim_ERR_ARGUMENT;
    if (bytes > 0 && !utf8) return uim_ERR_ARGUMENT;
    while (pos < bytes) {
        uint32_t cp = 0;
        size_t seq = 0;
        float adv;
        if (!zcsr_text_utf8_codepoint(utf8, bytes, pos, &cp, &seq)) return uim_ERR_ARGUMENT;
        adv = advance(user, cp);
        if (!isfinite(adv) || adv < 0.0f) return uim_ERR_ARGUMENT;
        width += adv;
        pos += seq;
    }
    *out_width = width;
    return uim_OK;
}

/* CJK / full-width ranges where a line may break between glyphs. */
static inline bool uim_text__is_cjk(uint32_t cp) {
    return (cp >= 0x1100u && cp <= 0x11FFu) ||   /* Hangul Jamo */
           (cp >= 0x2E80u && cp <= 0xA4CFu) ||   /* radicals, kana, CJK, Yi */
           (cp >= 0xAC00u && cp <= 0xD7AFu) ||   /* Hangul syllables */
           (cp >= 0xF900u && cp <= 0xFAFFu) ||   /* compatibility ideographs */
           (cp >= 0xFE30u && cp <= 0xFE4Fu) ||   /* CJK compatibility forms */
           (cp >= 0xFF00u && cp <= 0xFFEFu) ||   /* full-width forms */
           (cp >= 0x20000u && cp <= 0x3FFFDdu);  /* extensions B.. */
}

/* Closing punctuation that must not start a line. */
static inline bool uim_text__no_break_before(uint32_t cp) {
    switch (cp) {
    case 0x3001u: case 0x3002u: case 0x3009u: case 0x300Bu: case 0x300Du:
    case 0x300Fu: case 0x3011u: case 0x3015u: case 0x3017u: case 0x3019u:
    case 0x301Bu: case 0x301Eu: case 0x301Fu:
    case 0xFF09u: case 0xFF0Cu: case 0xFF0Eu: case 0xFF1Au: case 0xFF1Bu:
    case 0xFF1Fu: case 0xFF3Du: case 0xFF5Du:
    case 0x2019u: case 0x201Du: case 0x2026u:
        return true;
    default:
        return false;
    }
}

/* Opening punctuation that must not end a line. */
static inline bool uim_text__no_break_after(uint32_t cp) {
    switch (cp) {
    case 0x3008u: case 0x300Au: case 0x300Cu: case 0x300Eu: case 0x3010u:
    case 0x3014u: case 0x3016u: case 0x3018u: case 0x301Au:
    case 0xFF08u: case 0xFF3Bu: case 0xFF5Bu:
    case 0x2018u: case 0x201Cu:
        return true;
    default:
        return false;
    }
}

static inline uim_Result uim_text__emit_line(uim_TextLine *lines, size_t capacity,
                                             size_t *count, size_t max_lines,
                                             size_t start, size_t end, size_t content_end,
                                             float width, bool hard,
                                             bool *truncated, bool *stop) {
    uim_TextLine *line;
    if (*count >= capacity) {
        *truncated = true;
        *stop = true;
        return uim_ERR_BUFFER;
    }
    if (max_lines > 0u && *count >= max_lines) {
        *truncated = true;
        *stop = true;
        return uim_OK;
    }
    line = &lines[*count];
    line->byte_start = start;
    line->byte_length = (content_end > start && content_end <= end) ? (content_end - start) : 0u;
    line->width = line->byte_length > 0u ? width : 0.0f;
    line->hard_break = hard;
    (*count)++;
    return uim_OK;
}

/* Greedy wrap. *inout_count is capacity in, produced lines out. */
static inline uim_Result uim_text_wrap(const char *utf8, size_t bytes, float max_width,
                                       uim_TextAdvanceFn advance, void *user,
                                       size_t max_lines, uim_TextLine *out_lines,
                                       size_t *inout_count, bool *out_truncated) {
    size_t pos = 0, line_start = 0, content_end = 0, last_break = SIZE_MAX;
    size_t produced = 0, capacity;
    float line_width = 0.0f, trim_width = 0.0f;
    bool has_content = false, truncated = false, stop = false;
    uim_Result rc = uim_OK;

    if (!inout_count || !advance) return uim_ERR_ARGUMENT;
    if (bytes > 0 && !utf8) return uim_ERR_ARGUMENT;
    if (!isfinite(max_width) || max_width < 0.0f) return uim_ERR_ARGUMENT;
    capacity = *inout_count;
    if (capacity > 0 && !out_lines) return uim_ERR_ARGUMENT;

    while (pos < bytes && (utf8[pos] == ' ' || utf8[pos] == '\t' || utf8[pos] == '\r')) pos++;
    line_start = pos;

    while (pos < bytes) {
        unsigned char raw = (unsigned char)utf8[pos];
        uint32_t cp = 0;
        size_t seq = 0;
        float adv;
        if (raw == '\n') {
            rc = uim_text__emit_line(out_lines, capacity, &produced, max_lines,
                                     line_start, pos, content_end, trim_width, true,
                                     &truncated, &stop);
            if (rc != uim_OK || stop) break;
            pos++;
            while (pos < bytes && (utf8[pos] == ' ' || utf8[pos] == '\t' || utf8[pos] == '\r')) pos++;
            line_start = pos;
            line_width = trim_width = 0.0f;
            content_end = line_start;
            has_content = false;
            last_break = SIZE_MAX;
            continue;
        }
        if (raw == '\r') { pos++; continue; }
        if (!zcsr_text_utf8_codepoint(utf8, bytes, pos, &cp, &seq)) { rc = uim_ERR_ARGUMENT; break; }
        adv = advance(user, cp);
        if (!isfinite(adv) || adv < 0.0f) { rc = uim_ERR_ARGUMENT; break; }

        if (has_content && line_width + adv > max_width) {
            if (last_break != SIZE_MAX && last_break > line_start && last_break <= pos) {
                rc = uim_text__emit_line(out_lines, capacity, &produced, max_lines,
                                         line_start, last_break, content_end, trim_width, false,
                                         &truncated, &stop);
                if (rc != uim_OK || stop) break;
                pos = last_break;
                while (pos < bytes && (utf8[pos] == ' ' || utf8[pos] == '\t' || utf8[pos] == '\r')) pos++;
                line_start = pos;
                line_width = trim_width = 0.0f;
                content_end = line_start;
                has_content = false;
                last_break = SIZE_MAX;
                continue;
            }
            rc = uim_text__emit_line(out_lines, capacity, &produced, max_lines,
                                     line_start, pos, content_end, trim_width, false,
                                     &truncated, &stop);
            if (rc != uim_OK || stop) break;
            line_start = pos;
            line_width = trim_width = 0.0f;
            content_end = line_start;
            has_content = false;
            last_break = SIZE_MAX;
            continue;
        }

        line_width += adv;
        pos += seq;
        if (raw != ' ' && raw != '\t') {
            has_content = true;
            trim_width = line_width;
            content_end = pos;
        }
        if (raw == ' ' || raw == '\t' || raw == '-' || raw == '/') {
            last_break = pos;
        } else {
            uint32_t next_cp = 0;
            size_t next_seq = 0;
            if (pos < bytes && zcsr_text_utf8_codepoint(utf8, bytes, pos, &next_cp, &next_seq)) {
                bool pair_ok = !uim_text__no_break_after(cp) && !uim_text__no_break_before(next_cp);
                if (pair_ok && (uim_text__is_cjk(next_cp) || uim_text__is_cjk(cp)))
                    last_break = pos;
            }
        }
    }

    if (rc == uim_OK && !stop && line_start < bytes) {
        rc = uim_text__emit_line(out_lines, capacity, &produced, max_lines,
                                 line_start, bytes, content_end, trim_width, false,
                                 &truncated, &stop);
    }
    *inout_count = produced;
    if (out_truncated) *out_truncated = truncated;
    return rc;
}

/* Fit text into max_width, appending an ellipsis when it does not fit. NULL
 * ellipsis means U+2026 ("…"). Output is always NUL terminated on success;
 * uim_ERR_BUFFER leaves the buffer untouched. */
static inline uim_Result uim_text_ellipsize(const char *utf8, size_t bytes, float max_width,
                                            uim_TextAdvanceFn advance, void *user,
                                            const char *ellipsis,
                                            char *out, size_t out_capacity, size_t *out_bytes) {
    static const char default_ellipsis[] = "\xE2\x80\xA6";
    size_t eps_bytes, pos = 0, cut = 0, last_space = 0, i;
    float total = 0.0f, eps_width = 0.0f, prefix_width = 0.0f;
    uim_Result rc;

    if (!advance || !out || !out_bytes) return uim_ERR_ARGUMENT;
    if (bytes > 0 && !utf8) return uim_ERR_ARGUMENT;
    if (!isfinite(max_width) || max_width < 0.0f) return uim_ERR_ARGUMENT;
    if (!ellipsis) ellipsis = default_ellipsis;
    eps_bytes = strlen(ellipsis);
    if (!zcsr_text_utf8_valid(ellipsis, eps_bytes)) return uim_ERR_ARGUMENT;

    rc = uim_text_measure(utf8, bytes, advance, user, &total);
    if (rc != uim_OK) return rc;
    if (total <= max_width) {
        if (out_capacity < bytes + 1u) return uim_ERR_BUFFER;
        if (bytes > 0) memcpy(out, utf8, bytes);
        out[bytes] = '\0';
        *out_bytes = bytes;
        return uim_OK;
    }
    rc = uim_text_measure(ellipsis, eps_bytes, advance, user, &eps_width);
    if (rc != uim_OK) return rc;
    if (eps_width > max_width) {
        out[0] = '\0';
        *out_bytes = 0;
        return uim_OK;
    }

    while (pos < bytes) {
        uint32_t cp = 0;
        size_t seq = 0;
        float adv;
        if (!zcsr_text_utf8_codepoint(utf8, bytes, pos, &cp, &seq)) return uim_ERR_ARGUMENT;
        adv = advance(user, cp);
        if (!isfinite(adv) || adv < 0.0f) return uim_ERR_ARGUMENT;
        if (prefix_width + adv + eps_width > max_width) break;
        prefix_width += adv;
        pos += seq;
        cut = pos;
        if (cp == ' ' || cp == '\t') last_space = pos;
    }
    if (last_space > 0u && last_space <= cut) cut = last_space;
    while (cut > 0u && (utf8[cut - 1] == ' ' || utf8[cut - 1] == '\t')) cut--;
    if (cut + eps_bytes + 1u > out_capacity) return uim_ERR_BUFFER;
    for (i = 0; i < cut; i++) out[i] = utf8[i];
    for (i = 0; i < eps_bytes; i++) out[cut + i] = ellipsis[i];
    out[cut + eps_bytes] = '\0';
    *out_bytes = cut + eps_bytes;
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_TEXT_LAYOUT_H */
