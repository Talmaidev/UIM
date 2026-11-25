#ifndef UIM_TYPES_H
#define UIM_TYPES_H
/* UIM base value types — the smallest shared vocabulary.
 *
 * Every atom header (layout, theme, text layout, scroll, focus, placement,
 * progress, virtual list, animation, tabs, toast, formatting) includes only
 * this file, so a consumer can pick one atom without pulling the component
 * manager, the document model or the page manager into the build.
 *
 * Plain C, no allocation, no globals, no renderer, no OS header. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Same enumerator order as uim_component_manager.h historically exposed; the
 * manager now includes this header instead of redefining the values. */
typedef enum uim_Result {
    uim_OK = 0, uim_ERR_ARGUMENT, uim_ERR_NOMEM, uim_ERR_NOT_FOUND,
    uim_ERR_EXISTS, uim_ERR_TYPE, uim_ERR_STATE, uim_ERR_RANGE,
    uim_ERR_STALE, uim_ERR_BUFFER, uim_ERR_UNSUPPORTED
} uim_Result;

typedef struct uim_Rect { float x, y, width, height; } uim_Rect;
typedef struct uim_Point { float x, y; } uim_Point;
typedef struct uim_Insets { float left, top, right, bottom; } uim_Insets;

#ifdef __cplusplus
}
#endif
#endif /* UIM_TYPES_H */
