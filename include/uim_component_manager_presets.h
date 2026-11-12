/* uim_component_manager_presets.h — Component Manager 内置预设数据模型
 *
 * 这里定义游戏常用的页面和节点预设。
 * game_test / editor 都可以直接调用这些 API 创建 Manager 实例，
 * 不需要自己手动写 Page/Node 初始化代码。
 */

#ifndef UIM_COMPONENT_MANAGER_PRESETS_H
#define UIM_COMPONENT_MANAGER_PRESETS_H

#include "uim_component_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 创建完整的游戏 Component Manager 数据模型（MainWindow + Settings + Light2D）
 * 返回的 Manager 实例已经包含所有预设页面和节点。
 */
uim_Result uim_component_manager_create_game_preset(UIM_ComponentManager** out_manager);

#ifdef __cplusplus
}
#endif

#endif /* UIM_COMPONENT_MANAGER_PRESETS_H */
