#include "uim_component_manager.h"
#include <stddef.h>

int main(void) {
    UIM_ComponentManagerConfig config = {0};
    UIM_ComponentManager *manager = NULL;
    uim_Value value = uim_value_int(7);
    if (uim_component_manager_create(&config, &manager) != uim_OK) return 1;
    if (uim_property_register(manager, 1u, value) != uim_OK) {
        uim_component_manager_destroy(manager);
        return 2;
    }
    uim_component_manager_destroy(manager);
    return 0;
}
