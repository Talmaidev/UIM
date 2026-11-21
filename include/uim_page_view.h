#ifndef UIM_PAGE_VIEW_H
#define UIM_PAGE_VIEW_H
/* Shared UIM document → ZCSR_UI preview. player and editor both call this, so
 * there is exactly one component renderer for a .uireproj page. */
#include <stdbool.h>
#include "zcsr/gl_render.h"
#include "uim_component_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UIPageView UIPageView;

UIPageView* uipage_view_create(void);
void uipage_view_destroy(UIPageView* view);

/* Asset base directory for Image `asset` paths (project root). Optional. */
bool uipage_view_set_project_root(UIPageView* view, const char* root);

/* Rebuild (when page/manager changes) and render the page inside the given
 * client rect. Returns false if the page has no root node or ZCSR_UI cannot
 * be created. */
bool uipage_view_render(UIPageView* view, zcsr_gl_renderer* renderer,
                        UIM_ComponentManager* manager, uim_PageId page,
                        int x, int y, int w, int h);

/* Last computed transform (document coordinates → client pixels). */
float uipage_view_scale(const UIPageView* view);

#ifdef __cplusplus
}
#endif
#endif
