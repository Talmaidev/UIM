/* UIM page render facade.
 * One shared UIM document -> ZCSR_UI preview path for editor and player.
 * It never mutates the document. */
#include "uim_page_render.h"
#include "uim_page_view.h"

#include <stdbool.h>

bool zcsr_page_render(zcsr_gl_renderer *renderer,
                      const UIM_ComponentManager *manager,
                      uim_PageId page,
                      int view_x, int view_y,
                      int view_w, int view_h,
                      const char *project_root) {
    static UIPageView *shared_view = NULL;
    if (!renderer || !manager || page == uim_PAGE_INVALID ||
        view_w <= 0 || view_h <= 0) return false;
    if (!shared_view) shared_view = uipage_view_create();
    if (!shared_view) return false;
    if (project_root != NULL) (void)uipage_view_set_project_root(shared_view, project_root);
    return uipage_view_render(shared_view, renderer, (UIM_ComponentManager *)manager,
                              page, view_x, view_y, view_w, view_h);
}
