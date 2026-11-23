#include "uim_component_manager.h"
#include <stddef.h>

int main(void) {
    UIM_ComponentManagerConfig config = {0};
    UIM_ComponentManager *manager = NULL;
    uim_PageId page = uim_PAGE_INVALID;
    uim_NodeId node = uim_NODE_INVALID;
    uim_NodeDesc desc = {0};
    uim_NodeInfo info;

    if (uim_component_manager_create(&config, &manager) != uim_OK) return 1;
    if (uim_page_create(manager, "main", &page) != uim_OK) {
        uim_component_manager_destroy(manager);
        return 2;
    }
    desc.stable_key = "root";
    desc.page_id = page;
    desc.component_id = 1u;
    desc.visible = true;
    desc.enabled = true;
    if (uim_node_create(manager, &desc, &node) != uim_OK ||
        uim_node_get(manager, node, &info) != uim_OK ||
        info.page_id != page) {
        uim_component_manager_destroy(manager);
        return 3;
    }
    if (uim_page_destroy(manager, page) != uim_OK) {
        uim_component_manager_destroy(manager);
        return 4;
    }
    uim_component_manager_destroy(manager);
    return 0;
}
