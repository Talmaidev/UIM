/* uim_component_manager_lifecycle.h — Component Manager 全局单例入口
 *
 * UIM Component Manager 是独立的组件实例管理库。
 * 这里提供库级初始化和清理（全局单例），不负责启动窗口或注册演示模块。
 *
 * 演示代码（game_test / demo/）自己负责：
 *   - 创建窗口
 *   - 注册模块
 *   - 调用 Component Manager 的数据管理功能
 */

#ifndef UIM_COMPONENT_MANAGER_LIFECYCLE_H
#define UIM_COMPONENT_MANAGER_LIFECYCLE_H

#include "uim_component_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化组件管理器库（创建全局单例） */
int uim_component_manager_init(void);

/* 清理组件管理器库（销毁全局单例） */
void uim_component_manager_shutdown(void);

/* 获取组件管理器全局实例 */
UIM_ComponentManager* uim_component_manager_get(void);

#ifdef __cplusplus
}
#endif

#endif /* UIM_COMPONENT_MANAGER_LIFECYCLE_H */
