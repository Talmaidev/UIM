#ifndef UIM_FORMAT_H
#define UIM_FORMAT_H
/* uim_format.h — the display-format atom: numbers a UI can show.
 *
 * The workspace has 146 snprintf/sprintf call sites and each panel chooses its
 * own grouping, percent and byte rules. This header fixes those rules once:
 *   int      1234567      -> "1,234,567"
 *   fixed    12.345, 2    -> "12.35"
 *   percent  0.125, 1     -> "12.5%"
 *   bytes    1536, binary -> "1.5 KB"
 *   duration 3725         -> "1:02:05"
 *
 * Contract:
 * - Output buffers are caller-owned. On uim_ERR_BUFFER nothing is written;
 *   callers can query the needed size by passing capacity 0.
 * - Non-finite numbers, negative byte counts and decimals > 6 are rejected.
 * - Formatting is locale-independent: '.' is always the decimal separator and
 *   ',' the grouping separator.
 */
#include "uim_types.h"

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline size_t uim_format__digits_u64(uint64_t v) {
    size_t n = 1;
    while (v >= 10u) { v /= 10u; n++; }
    return n;
}

static inline size_t uim_format__grouped_len(size_t digits, bool grouping) {
    return grouping && digits > 3u ? digits + (digits - 1u) / 3u : digits;
}

static inline void uim_format__write_u64(uint64_t v, bool grouping, char *dst, size_t len) {
    size_t pos = len, count = 0;
    do {
        dst[--pos] = (char)('0' + (int)(v % 10u));
        v /= 10u;
        count++;
        if (grouping && v > 0u && count % 3u == 0u) dst[--pos] = ',';
    } while (v > 0u);
}

static inline uim_Result uim_format_int(int64_t value, bool grouping,
                                        char *out, size_t capacity) {
    uint64_t magnitude;
    size_t digits, len, total;
    bool negative = value < 0;
    if (!out) return uim_ERR_ARGUMENT;
    if (negative) magnitude = (uint64_t)(-(value + 1)) + 1u;
    else magnitude = (uint64_t)value;
    digits = uim_format__digits_u64(magnitude);
    len = uim_format__grouped_len(digits, grouping);
    total = len + (negative ? 1u : 0u);
    if (capacity < total + 1u) return uim_ERR_BUFFER;
    if (negative) out[0] = '-';
    uim_format__write_u64(magnitude, grouping, out + (negative ? 1u : 0u), len);
    out[total] = '\0';
    return uim_OK;
}

static inline uim_Result uim_format_fixed(double value, unsigned decimals, bool grouping,
                                         char *out, size_t capacity) {
    char temp[96];
    const char *dot;
    size_t int_digits, int_len, frac_len, total, i, src;
    bool negative;
    if (!out || !isfinite(value) || decimals > 6u) return uim_ERR_ARGUMENT;
    if (value > 1.0e15 || value < -1.0e15) return uim_ERR_RANGE;
    if (snprintf(temp, sizeof(temp), "%.*f", (int)decimals, value) <= 0) return uim_ERR_ARGUMENT;
    dot = strchr(temp, '.');
    negative = temp[0] == '-';
    int_digits = (size_t)((dot ? dot : temp + strlen(temp)) - (temp + (negative ? 1 : 0)));
    if (int_digits == 0u) int_digits = 1u;
    int_len = uim_format__grouped_len(int_digits, grouping);
    frac_len = dot ? strlen(dot + 1) : 0u;
    total = int_len + (negative ? 1u : 0u) + (decimals > 0u ? 1u + frac_len : 0u);
    if (capacity < total + 1u) return uim_ERR_BUFFER;

    /* Integer part is rebuilt from its digits so grouping can be inserted
     * without touching the fraction. */
    {
        uint64_t magnitude = 0;
        for (i = 0; i < int_digits; i++) magnitude = magnitude * 10u + (uint64_t)(temp[(negative ? 1u : 0u) + i] - '0');
        if (negative) out[0] = '-';
        uim_format__write_u64(magnitude, grouping, out + (negative ? 1u : 0u), int_len);
    }
    if (decimals > 0u) {
        size_t pos = int_len + (negative ? 1u : 0u);
        out[pos++] = '.';
        src = (size_t)((dot ? dot + 1 : temp + strlen(temp)) - temp);
        for (i = 0; i < frac_len; i++) out[pos + i] = temp[src + i];
        (void)frac_len;
        total = pos + frac_len;
    }
    out[total] = '\0';
    return uim_OK;
}

static inline uim_Result uim_format_percent(double fraction, unsigned decimals,
                                            char *out, size_t capacity) {
    if (!out || !isfinite(fraction) || decimals > 6u) return uim_ERR_ARGUMENT;
    if (fraction > 1.0e13 || fraction < -1.0e13) return uim_ERR_RANGE;
    {
        char temp[64];
        size_t need;
        if (snprintf(temp, sizeof(temp), "%.*f%%", (int)decimals, fraction * 100.0) <= 0)
            return uim_ERR_ARGUMENT;
        need = strlen(temp);
        if (capacity < need + 1u) return uim_ERR_BUFFER;
        memcpy(out, temp, need + 1u);
    }
    return uim_OK;
}

/* 1024-based when binary is true, 1000-based otherwise. */
static inline uim_Result uim_format_bytes(double bytes, bool binary, char *out, size_t capacity) {
    static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    char temp[64];
    double base = binary ? 1024.0 : 1000.0;
    double value;
    unsigned unit = 0;
    if (!out || !isfinite(bytes) || bytes < 0.0) return uim_ERR_ARGUMENT;
    value = bytes;
    while (value >= base && unit < 5u) { value /= base; unit++; }
    if (unit == 0u) {
        if (snprintf(temp, sizeof(temp), "%.0f B", value) <= 0) return uim_ERR_ARGUMENT;
    } else {
        unsigned decimals = value < 10.0 ? 1u : 0u;
        if (snprintf(temp, sizeof(temp), "%.*f %s", (int)decimals, value, units[unit]) <= 0)
            return uim_ERR_ARGUMENT;
    }
    {
        size_t need = strlen(temp);
        if (capacity < need + 1u) return uim_ERR_BUFFER;
        memcpy(out, temp, need + 1u);
    }
    return uim_OK;
}

/* < 60 s -> "3.4s"; < 1 h -> "1:02"; otherwise "1:02:05". */
static inline uim_Result uim_format_duration(double seconds, char *out, size_t capacity) {
    char temp[64];
    if (!out || !isfinite(seconds) || seconds < 0.0) return uim_ERR_ARGUMENT;
    if (seconds < 60.0) {
        if (snprintf(temp, sizeof(temp), "%.1fs", seconds) <= 0) return uim_ERR_ARGUMENT;
    } else if (seconds < 3600.0) {
        unsigned long total = (unsigned long)seconds;
        if (snprintf(temp, sizeof(temp), "%lu:%02lu", total / 60u, total % 60u) <= 0)
            return uim_ERR_ARGUMENT;
    } else {
        unsigned long total = (unsigned long)seconds;
        if (snprintf(temp, sizeof(temp), "%lu:%02lu:%02lu", total / 3600u,
                     (total % 3600u) / 60u, total % 60u) <= 0)
            return uim_ERR_ARGUMENT;
    }
    {
        size_t need = strlen(temp);
        if (capacity < need + 1u) return uim_ERR_BUFFER;
        memcpy(out, temp, need + 1u);
    }
    return uim_OK;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_FORMAT_H */
