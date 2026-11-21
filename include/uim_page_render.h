#ifndef UIM_PAGE_RENDER_H
#define UIM_PAGE_RENDER_H

/* 把文档页面画到指定视口矩形（左上角 view_x/view_y，尺寸 view_w/view_h）。
 * 编辑器 PLAY 模式与打包 player 共用同一份绘制实现，避免两套渲染漂移。
 * 只读文档，不修改任何节点状态。 */

#include "uim_project.h"

#include "zcsr/gl_render.h"

#include <stdbool.h>

bool zcsr_page_render(zcsr_gl_renderer *renderer,
                      const UIM_ComponentManager *manager,
                      uim_PageId page,
                      int view_x, int view_y,
                      int view_w, int view_h,
                      const char *project_root);

#endif
