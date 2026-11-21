/* Contract test for the 13 builtin UIM component descriptors. The registry is
 * the single owner of component identity/schema in the editor stack; this test
 * fails if a builtin is lost, duplicated, or registered without a usable
 * property surface. */
#include "uim_component_registry.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    static const uim_ComponentTypeId expected[] = {
        uim_COMPONENT_PANEL, uim_COMPONENT_BUTTON, uim_COMPONENT_TOGGLE,
        uim_COMPONENT_SLIDER, uim_COMPONENT_DROPDOWN, uim_COMPONENT_LABEL,
        uim_COMPONENT_SEPARATOR, uim_COMPONENT_IMAGE, uim_COMPONENT_TEXT_INPUT,
        uim_COMPONENT_SCROLL_VIEW, uim_COMPONENT_MOTION, uim_COMPONENT_HEALTH,
        uim_COMPONENT_RULE
    };
    const size_t expected_count = sizeof(expected) / sizeof(expected[0]);
    uim_ComponentRegistryConfig config;
    uim_ComponentRegistry *registry = NULL;
    size_t i;

    memset(&config, 0, sizeof(config));
    config.initial_type_capacity = 16u;
    CHECK(uim_component_registry_create(&config, &registry) == uim_OK);
    CHECK(registry != NULL);

    CHECK(uim_component_registry_register_builtins(registry) == uim_OK);
    CHECK(uim_component_registry_count(registry) == expected_count);
    /* Idempotent: a second registration must not duplicate or corrupt types. */
    CHECK(uim_component_registry_register_builtins(registry) == uim_OK);
    CHECK(uim_component_registry_count(registry) == expected_count);

    for (i = 0; i < expected_count; ++i) {
        uim_ComponentTypeDesc desc;
        const uim_ComponentTypeDesc *builtin = uim_component_registry_builtin_desc(expected[i]);
        uim_ComponentTypeId resolved = 0;
        CHECK(builtin != NULL);
        CHECK(uim_component_registry_get(registry, expected[i], &desc) == uim_OK);
        CHECK(desc.type_id == expected[i]);
        CHECK(desc.schema_version == 1u);
        CHECK(desc.stable_key != NULL && strncmp(desc.stable_key, "builtin.", 8u) == 0);
        CHECK(desc.property_count > 0u);
        CHECK(desc.properties != NULL);
        CHECK(uim_component_registry_find(registry, desc.stable_key, &resolved) == uim_OK);
        CHECK(resolved == expected[i]);
    }

    /* The three behavior components are the Unity-equivalent "script-like"
     * builtins; each must expose at least one editable property. */
    {
        uim_ComponentTypeDesc desc;
        CHECK(uim_component_registry_get(registry, uim_COMPONENT_MOTION, &desc) == uim_OK);
        CHECK(desc.property_count >= 2u); /* VX/VY */
        CHECK(uim_component_registry_get(registry, uim_COMPONENT_HEALTH, &desc) == uim_OK);
        CHECK(desc.property_count >= 1u); /* HP */
        CHECK(uim_component_registry_get(registry, uim_COMPONENT_RULE, &desc) == uim_OK);
        CHECK(desc.property_count >= 3u); /* TRIGGER/SELECTOR/EFFECT */
    }

    if (failures == 0) printf("UIM component registry contract: ALL PASS (%zu types)\n",
                              expected_count);
    else printf("%d failures\n", failures);
    uim_component_registry_destroy(registry);
    return failures;
}
