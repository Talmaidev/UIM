#ifndef UIM_COLOR_H
#define UIM_COLOR_H
/* uim_color.h — one color value and the four operations every UI needs.
 *
 * Chosen representation: float RGBA in 0..1, the same shape as the engine's
 * ZCSR_Color and the settings page's colors, so passing a value across the
 * two costs no conversion. Packed formats (ImU32, 0xAARRGGBB) stay a host
 * concern; this atom only parses, formats, blends and measures contrast.
 * No allocation, no globals, no renderer. */
#include "uim_types.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uim_Color { float r, g, b, a; } uim_Color;

static inline float uim_color__unit(float v) {
    if (!isfinite(v)) return 0.0f;
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline uim_Color uim_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uim_Color c;
    c.r = (float)r / 255.0f;
    c.g = (float)g / 255.0f;
    c.b = (float)b / 255.0f;
    c.a = (float)a / 255.0f;
    return c;
}

/* 0xRRGGBB plus a separate alpha in 0..1. */
static inline uim_Color uim_color_hex(uint32_t rgb, float alpha) {
    uim_Color c;
    c.r = (float)((rgb >> 16) & 0xFFu) / 255.0f;
    c.g = (float)((rgb >> 8) & 0xFFu) / 255.0f;
    c.b = (float)(rgb & 0xFFu) / 255.0f;
    c.a = uim_color__unit(alpha);
    return c;
}

static inline int uim_color__hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "#RRGGBB" or "#RRGGBBAA" (case-insensitive). Anything else is rejected and
 * the output is left untouched. */
static inline uim_Result uim_color_parse(const char *text, uim_Color *out) {
    size_t digits, i;
    uint32_t v = 0u;
    if (!text || !out) return uim_ERR_ARGUMENT;
    if (text[0] != '#') return uim_ERR_ARGUMENT;
    digits = strlen(text + 1);
    if (digits != 6u && digits != 8u) return uim_ERR_ARGUMENT;
    for (i = 0; i < digits; i++) {
        int d = uim_color__hex_digit(text[1 + i]);
        if (d < 0) return uim_ERR_ARGUMENT;
        v = (v << 4) | (uint32_t)d;
    }
    if (digits == 6u) {
        *out = uim_color_hex(v, 1.0f);
    } else {
        out->r = (float)((v >> 24) & 0xFFu) / 255.0f;
        out->g = (float)((v >> 16) & 0xFFu) / 255.0f;
        out->b = (float)((v >> 8) & 0xFFu) / 255.0f;
        out->a = (float)(v & 0xFFu) / 255.0f;
    }
    return uim_OK;
}

/* Uppercase "#RRGGBB" (or "#RRGGBBAA"). On uim_ERR_BUFFER the buffer is
 * untouched; the required size is 8 or 10 bytes including the terminator. */
static inline uim_Result uim_color_format(const uim_Color *c, bool with_alpha,
                                          char *out, size_t capacity) {
    static const char hex[] = "0123456789ABCDEF";
    size_t need = with_alpha ? 10u : 8u;
    size_t i;
    unsigned values[4];
    if (!c || !out) return uim_ERR_ARGUMENT;
    if (capacity < need) return uim_ERR_BUFFER;
    values[0] = (unsigned)(uim_color__unit(c->r) * 255.0f + 0.5f);
    values[1] = (unsigned)(uim_color__unit(c->g) * 255.0f + 0.5f);
    values[2] = (unsigned)(uim_color__unit(c->b) * 255.0f + 0.5f);
    values[3] = (unsigned)(uim_color__unit(c->a) * 255.0f + 0.5f);
    out[0] = '#';
    for (i = 0; i < (with_alpha ? 4u : 3u); i++) {
        out[1 + i * 2] = hex[(values[i] >> 4) & 0xFu];
        out[2 + i * 2] = hex[values[i] & 0xFu];
    }
    out[need - 1] = '\0';
    return uim_OK;
}

/* Source-over compositing: src drawn on top of dst (alpha weighted). */
static inline uim_Color uim_color_blend(uim_Color dst, uim_Color src) {
    uim_Color out;
    float sa = uim_color__unit(src.a);
    float da = uim_color__unit(dst.a);
    float oa = sa + da * (1.0f - sa);
    if (oa <= 0.000001f) {
        out.r = out.g = out.b = 0.0f;
        out.a = 0.0f;
        return out;
    }
    out.r = (src.r * sa + dst.r * da * (1.0f - sa)) / oa;
    out.g = (src.g * sa + dst.g * da * (1.0f - sa)) / oa;
    out.b = (src.b * sa + dst.b * da * (1.0f - sa)) / oa;
    out.a = oa;
    out.r = uim_color__unit(out.r);
    out.g = uim_color__unit(out.g);
    out.b = uim_color__unit(out.b);
    return out;
}

static inline uim_Color uim_color_lerp(uim_Color a, uim_Color b, float t) {
    uim_Color out;
    float k = uim_color__unit(t);
    out.r = a.r + (b.r - a.r) * k;
    out.g = a.g + (b.g - a.g) * k;
    out.b = a.b + (b.b - a.b) * k;
    out.a = a.a + (b.a - a.a) * k;
    return out;
}

static inline float uim_color__linear(float v) {
    v = uim_color__unit(v);
    if (v <= 0.03928f) return v / 12.92f;
    return powf((v + 0.055f) / 1.055f, 2.4f);
}

/* WCAG relative luminance (0 = black, 1 = white). */
static inline float uim_color_luminance(const uim_Color *c) {
    if (!c) return 0.0f;
    return 0.2126f * uim_color__linear(c->r) +
           0.7152f * uim_color__linear(c->g) +
           0.0722f * uim_color__linear(c->b);
}

static inline bool uim_color_is_dark(const uim_Color *c) {
    return uim_color_luminance(c) < 0.5f;
}

/* Black or white, whichever has the higher WCAG contrast against bg. */
static inline uim_Color uim_color_contrast_text(const uim_Color *bg) {
    uim_Color black = {0.0f, 0.0f, 0.0f, 1.0f};
    uim_Color white = {1.0f, 1.0f, 1.0f, 1.0f};
    float lb;
    if (!bg) return white;
    lb = uim_color_luminance(bg);
    /* Contrast vs white = 1.05/(lb+0.05); vs black = (lb+0.05)/0.05. */
    return (1.05f / (lb + 0.05f)) >= ((lb + 0.05f) / 0.05f) ? white : black;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_COLOR_H */
