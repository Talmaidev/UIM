#ifndef UIM_THEME_H
#define UIM_THEME_H
/* uim_theme.h — the theme atom: semantic tokens, not literals.
 *
 * The settings page hard-codes its palette and its 1.4 text scale; the editor
 * hard-codes its own; every new page would otherwise pick colors again. This
 * header is the single semantic vocabulary — surface/border/text/accent/danger,
 * spacing steps, radii, font sizes — with DPI applied at init so no consumer
 * multiplies by a factor of its own.
 *
 * Contract:
 * - Pure data + pure functions; no files, no registry, no globals.
 * - DPI is applied once, at uim_theme_init; consumers read final pixels.
 * - Out-of-range dpi/base font are clamped and reported as uim_ERR_RANGE so a
 *   bad host setting is visible in tests instead of silently looking wrong.
 * - Colors are returned by value; an unknown role returns transparent black.
 */
#include "uim_color.h"
#include "uim_types.h"

#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uim_ThemeMode { uim_THEME_DARK = 0, uim_THEME_LIGHT = 1 } uim_ThemeMode;

typedef enum uim_ThemeColor {
    uim_THEME_SURFACE = 0,
    uim_THEME_SURFACE_ELEVATED,
    uim_THEME_SURFACE_SUNKEN,
    uim_THEME_BORDER,
    uim_THEME_BORDER_STRONG,
    uim_THEME_TEXT,
    uim_THEME_TEXT_MUTED,
    uim_THEME_TEXT_INVERSE,
    uim_THEME_ACCENT,
    uim_THEME_ACCENT_HOVER,
    uim_THEME_ACCENT_PRESSED,
    uim_THEME_ON_ACCENT,
    uim_THEME_DANGER,
    uim_THEME_SUCCESS,
    uim_THEME_WARNING,
    uim_THEME_FOCUS_RING,
    uim_THEME_OVERLAY,
    uim_THEME_COLOR_COUNT
} uim_ThemeColor;

typedef enum uim_ThemeFont {
    uim_THEME_FONT_BODY = 0,
    uim_THEME_FONT_CAPTION,
    uim_THEME_FONT_TITLE,
    uim_THEME_FONT_DISPLAY,
    uim_THEME_FONT_COUNT
} uim_ThemeFont;

typedef enum uim_ThemeRadius {
    uim_THEME_RADIUS_SM = 0,
    uim_THEME_RADIUS_MD,
    uim_THEME_RADIUS_LG,
    uim_THEME_RADIUS_COUNT
} uim_ThemeRadius;

#define UIM_THEME_SPACE_STEPS 7u

typedef struct uim_Theme {
    uim_ThemeMode mode;
    float dpi_scale;
    float base_font_px;                 /* the host's logical size, e.g. 16 */
    float fonts[uim_THEME_FONT_COUNT];  /* final pixels, dpi applied */
    float space[UIM_THEME_SPACE_STEPS]; /* 4,8,12,16,24,32,48 * dpi */
    float radius[uim_THEME_RADIUS_COUNT];
    uim_Color colors[uim_THEME_COLOR_COUNT];
} uim_Theme;

static inline void uim_theme__palette_dark(uim_Theme *t) {
    t->colors[uim_THEME_SURFACE]          = uim_color_hex(0x1E1F22u, 1.0f);
    t->colors[uim_THEME_SURFACE_ELEVATED] = uim_color_hex(0x2B2D31u, 1.0f);
    t->colors[uim_THEME_SURFACE_SUNKEN]   = uim_color_hex(0x16171Au, 1.0f);
    t->colors[uim_THEME_BORDER]           = uim_color_hex(0x3A3D44u, 1.0f);
    t->colors[uim_THEME_BORDER_STRONG]    = uim_color_hex(0x555A64u, 1.0f);
    t->colors[uim_THEME_TEXT]             = uim_color_hex(0xE6E8ECu, 1.0f);
    t->colors[uim_THEME_TEXT_MUTED]       = uim_color_hex(0x9AA0AAu, 1.0f);
    t->colors[uim_THEME_TEXT_INVERSE]     = uim_color_hex(0x121316u, 1.0f);
    t->colors[uim_THEME_ACCENT]           = uim_color_hex(0x3B82F6u, 1.0f);
    t->colors[uim_THEME_ACCENT_HOVER]     = uim_color_hex(0x60A5FAu, 1.0f);
    t->colors[uim_THEME_ACCENT_PRESSED]   = uim_color_hex(0x2563EBu, 1.0f);
    t->colors[uim_THEME_ON_ACCENT]        = uim_color_hex(0xFFFFFFu, 1.0f);
    t->colors[uim_THEME_DANGER]           = uim_color_hex(0xEF4444u, 1.0f);
    t->colors[uim_THEME_SUCCESS]          = uim_color_hex(0x22C55Eu, 1.0f);
    t->colors[uim_THEME_WARNING]          = uim_color_hex(0xF59E0Bu, 1.0f);
    t->colors[uim_THEME_FOCUS_RING]       = uim_color_hex(0x8AB4FFu, 1.0f);
    t->colors[uim_THEME_OVERLAY]          = uim_color_rgba(0u, 0u, 0u, 140u);
}

static inline void uim_theme__palette_light(uim_Theme *t) {
    t->colors[uim_THEME_SURFACE]          = uim_color_hex(0xFFFFFFu, 1.0f);
    t->colors[uim_THEME_SURFACE_ELEVATED] = uim_color_hex(0xF6F7F9u, 1.0f);
    t->colors[uim_THEME_SURFACE_SUNKEN]   = uim_color_hex(0xECEEF2u, 1.0f);
    t->colors[uim_THEME_BORDER]           = uim_color_hex(0xD6DAE0u, 1.0f);
    t->colors[uim_THEME_BORDER_STRONG]    = uim_color_hex(0xB4BAC4u, 1.0f);
    t->colors[uim_THEME_TEXT]             = uim_color_hex(0x1B1D21u, 1.0f);
    t->colors[uim_THEME_TEXT_MUTED]       = uim_color_hex(0x5F6673u, 1.0f);
    t->colors[uim_THEME_TEXT_INVERSE]     = uim_color_hex(0xFFFFFFu, 1.0f);
    t->colors[uim_THEME_ACCENT]           = uim_color_hex(0x2563EBu, 1.0f);
    t->colors[uim_THEME_ACCENT_HOVER]     = uim_color_hex(0x3B82F6u, 1.0f);
    t->colors[uim_THEME_ACCENT_PRESSED]   = uim_color_hex(0x1D4ED8u, 1.0f);
    t->colors[uim_THEME_ON_ACCENT]        = uim_color_hex(0xFFFFFFu, 1.0f);
    t->colors[uim_THEME_DANGER]           = uim_color_hex(0xDC2626u, 1.0f);
    t->colors[uim_THEME_SUCCESS]          = uim_color_hex(0x16A34Au, 1.0f);
    t->colors[uim_THEME_WARNING]          = uim_color_hex(0xD97706u, 1.0f);
    t->colors[uim_THEME_FOCUS_RING]       = uim_color_hex(0x1D4ED8u, 1.0f);
    t->colors[uim_THEME_OVERLAY]          = uim_color_rgba(0u, 0u, 0u, 115u);
}

/* dpi outside 0.5..4.0 or base font outside 8..48 is clamped (uim_ERR_RANGE). */
static inline uim_Result uim_theme_init(uim_Theme *t, uim_ThemeMode mode,
                                        float dpi_scale, float base_font_px) {
    bool clamped = false;
    if (!t || !isfinite(dpi_scale) || !isfinite(base_font_px)) return uim_ERR_ARGUMENT;
    if (mode != uim_THEME_DARK && mode != uim_THEME_LIGHT) return uim_ERR_ARGUMENT;
    if (dpi_scale < 0.5f) { dpi_scale = 0.5f; clamped = true; }
    if (dpi_scale > 4.0f) { dpi_scale = 4.0f; clamped = true; }
    if (base_font_px < 8.0f) { base_font_px = 8.0f; clamped = true; }
    if (base_font_px > 48.0f) { base_font_px = 48.0f; clamped = true; }

    memset(t, 0, sizeof(*t));
    t->mode = mode;
    t->dpi_scale = dpi_scale;
    t->base_font_px = base_font_px;

    t->fonts[uim_THEME_FONT_BODY] = base_font_px * dpi_scale;
    t->fonts[uim_THEME_FONT_CAPTION] = t->fonts[uim_THEME_FONT_BODY] * 0.86f;
    if (t->fonts[uim_THEME_FONT_CAPTION] < 10.0f * dpi_scale)
        t->fonts[uim_THEME_FONT_CAPTION] = 10.0f * dpi_scale;
    t->fonts[uim_THEME_FONT_TITLE] = t->fonts[uim_THEME_FONT_BODY] * 1.28f;
    t->fonts[uim_THEME_FONT_DISPLAY] = t->fonts[uim_THEME_FONT_BODY] * 1.70f;

    t->space[0] = 4.0f * dpi_scale;
    t->space[1] = 8.0f * dpi_scale;
    t->space[2] = 12.0f * dpi_scale;
    t->space[3] = 16.0f * dpi_scale;
    t->space[4] = 24.0f * dpi_scale;
    t->space[5] = 32.0f * dpi_scale;
    t->space[6] = 48.0f * dpi_scale;

    t->radius[uim_THEME_RADIUS_SM] = 4.0f * dpi_scale;
    t->radius[uim_THEME_RADIUS_MD] = 8.0f * dpi_scale;
    t->radius[uim_THEME_RADIUS_LG] = 12.0f * dpi_scale;

    if (mode == uim_THEME_DARK) uim_theme__palette_dark(t);
    else uim_theme__palette_light(t);
    return clamped ? uim_ERR_RANGE : uim_OK;
}

static inline uim_Color uim_theme_color(const uim_Theme *t, uim_ThemeColor role) {
    uim_Color none = {0.0f, 0.0f, 0.0f, 0.0f};
    if (!t || (unsigned)role >= (unsigned)uim_THEME_COLOR_COUNT) return none;
    return t->colors[role];
}

static inline float uim_theme_space(const uim_Theme *t, size_t step) {
    if (!t || step >= UIM_THEME_SPACE_STEPS) return 0.0f;
    return t->space[step];
}

static inline float uim_theme_radius(const uim_Theme *t, uim_ThemeRadius r) {
    if (!t || (unsigned)r >= (unsigned)uim_THEME_RADIUS_COUNT) return 0.0f;
    return t->radius[r];
}

static inline float uim_theme_font(const uim_Theme *t, uim_ThemeFont f) {
    if (!t || (unsigned)f >= (unsigned)uim_THEME_FONT_COUNT) return 0.0f;
    return t->fonts[f];
}

/* A hairline that stays 1 physical pixel at any dpi (never below 1). */
static inline float uim_theme_hairline(const uim_Theme *t) {
    float d = (t && t->dpi_scale > 0.0f) ? t->dpi_scale : 1.0f;
    return d < 1.0f ? 1.0f : d;
}

/* Text color that stays readable on an arbitrary surface. */
static inline uim_Color uim_theme_on(const uim_Theme *t, uim_Color background) {
    if (!t) return uim_color_contrast_text(&background);
    return uim_color_contrast_text(&background);
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_THEME_H */
