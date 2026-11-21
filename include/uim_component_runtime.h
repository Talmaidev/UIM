#ifndef UIM_COMPONENT_RUNTIME_H
#define UIM_COMPONENT_RUNTIME_H

/* 文档级运行循环：只推进 MEMORY 中的节点状态，不写盘、不置脏。
 * 进入播放时 uim_runtime_begin() 对当前页节点做位置快照，
 * 退出时 uim_runtime_end() 逐节点恢复，保证“运行不修改工程”。 */

#include "uim_component_manager.h"

#include <stdbool.h>

bool uim_runtime_begin(UIM_ComponentManager *manager, uim_PageId page);
void uim_runtime_end(void);
/* dt 秒；内部夹到 [0, 0.05]，暂停时直接返回。 */
void uim_runtime_tick(UIM_ComponentManager *manager, uim_PageId page, double dt);
void uim_runtime_set_paused(bool paused);
bool uim_runtime_paused(void);
/* 单步一帧（1/60s），不受暂停状态影响，执行后恢复原暂停状态。 */
void uim_runtime_step(UIM_ComponentManager *manager, uim_PageId page);

/* ---------- M3：命中 / 点击 / Rule ---------- */

/* 页面坐标命中：返回最上层可见且启用、矩形包含 (x,y) 的节点，未命中返回 INVALID。
 * 2D 节点使用 local_rect；3D 节点没有页面矩形，必须由宿主提供投影回调。 */
uim_NodeId uim_runtime_pick(UIM_ComponentManager *manager, uim_PageId page,
                             float x, float y);

/* 3D 节点的投影回调：把节点投影成页面坐标矩形（不是屏幕像素）。
 * 返回 false 表示该节点当前不可命中（例如在相机后方）。user 由宿主提供。 */
typedef bool (*uim_RuntimeProjectNodeFn)(void *user, const UIM_ComponentManager *manager,
                                         const uim_NodeInfo *info, uim_Rect *out_rect);
/* 带 3D 投影的命中；project 为 NULL 时等价于 uim_runtime_pick。 */
uim_NodeId uim_runtime_pick_ex(UIM_ComponentManager *manager, uim_PageId page,
                                float x, float y,
                                uim_RuntimeProjectNodeFn project, void *project_user);
/* 对一个节点执行它的 click 触发器规则；返回实际执行了几条（0 或 1）。 */
int uim_runtime_fire_click(UIM_ComponentManager *manager, uim_PageId page,
                            uim_NodeId node_id);
/* pick + fire_click 的合体：宿主（编辑器 PLAY / player）一次调用即可，
 * 返回值仍是被点中的节点，便于宿主做选中/日志。 */
uim_NodeId uim_runtime_click(UIM_ComponentManager *manager, uim_PageId page,
                              float x, float y);
uim_NodeId uim_runtime_click_ex(UIM_ComponentManager *manager, uim_PageId page,
                                 float x, float y,
                                 uim_RuntimeProjectNodeFn project, void *project_user);
/* goto 效果登记的一次性请求：1-based 页序号，取走后清零。 */
int uim_runtime_take_goto_request(void);

#endif
