/* uim_component_manager_lifecycle.c — Component Manager 应用库入口
 *
 * Component Manager 是一个独立的信号接收/组件分发库。
 * 这里只提供库初始化和清理函数，不负责启动窗口或注册演示模块。
 *
 * 演示代码（game_test / demo/）自己负责：
 *   - 创建窗口
 *   - 注册模块
 *   - 调用 Component Manager 的信号接收功能
 */

#include "uim_component_manager_lifecycle.h"

/* Component Manager 全局实例（单例） */
static UIM_ComponentManager* g_receiver = NULL;

int uim_component_manager_init(void) {
    if (g_receiver) return 0; /* 已经初始化 */

    UIM_ComponentManagerConfig config = {
        .initial_node_capacity = 64,
        .initial_page_capacity = 8,
    };

    uim_Result result = uim_component_manager_create(&config, &g_receiver);
    if (result != uim_OK) return -1;

    return 0;
}

void uim_component_manager_shutdown(void) {
    if (g_receiver) {
        uim_component_manager_destroy(g_receiver);
        g_receiver = NULL;
    }
}

UIM_ComponentManager* uim_component_manager_get(void) {
    return g_receiver;
}
