#ifndef UIM_PROJECT_H
#define UIM_PROJECT_H

/* Editor project (.uirproj) serialization and package build.
 *
 * The file is a complete document snapshot: every page, every node, node
 * layout/geometry/flags/action, component config bytes, and attached
 * component instances. Stable keys are the identity used to rebuild parent
 * links; node ids are deliberately not persisted.
 *
 * Asset files are kept outside the project file:
 *   <project-root>/.uirassets      asset registry
 *   <project-root>/<relative>      asset payloads
 * This keeps the format readable and lets the package builder copy the same
 * relative paths into dist/<project>/ so registry entries stay valid.
 */

#include "uim_asset_registry.h"
#include "uim_component_manager.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UIR_PROJECT_VERSION 3u
#define UIR_PROJECT_VERSION_MIN 1u
#define UIR_PROJECT_PATH_CAP 600u
#define UIR_PROJECT_NAME_CAP 128u

/* Serialize/deserialize a complete document without touching the filesystem.
 * These back both Save and the editor's structural undo snapshots.
 * `project_name` may be empty for an in-memory snapshot. */
uim_Result uir_project_serialize(const UIM_ComponentManager *manager,
                                 const uim_PageId *pages,
                                 size_t page_count,
                                 const char *project_name,
                                 unsigned char **out_data,
                                 size_t *out_size);

uim_Result uir_project_deserialize(const unsigned char *data,
                                   size_t size,
                                   UIM_ComponentManager **out_manager,
                                   uim_PageId *out_page,
                                   uim_NodeId *out_root,
                                   char *out_name, size_t name_cap);

/* Save the selected pages. `pages[0]` is the entry page written into the
 * manifest. Every node in each page must have a stable key. Version 2 stores
 * component-instance property values, registered properties and node bindings;
 * version 1 files remain loadable. */
uim_Result uir_project_save(const UIM_ComponentManager *manager,
                            const uim_PageId *pages,
                            size_t page_count,
                            const char *path);

/* Load a .uirproj into a newly created manager. On success the caller owns
 * *out_manager and must destroy it. *out_page and *out_root address the entry
 * page and its first root node. out_root_path receives the project directory. */
uim_Result uir_project_load(const char *path,
                            UIM_ComponentManager **out_manager,
                            uim_PageId *out_page,
                            uim_NodeId *out_root,
                            char *out_root_path, size_t root_cap,
                            char *out_name, size_t name_cap);

/* Build dist/<project>/ with:
 *   player.exe
 *   project.uireproj
 *   .uirassets (when present)
 *   assets/<relative-path> for every registered asset
 *   manifest.txt
 * output_root may be NULL, in which case project_root is used. */
uim_Result uir_project_build_package(const char *project_path,
                                     const char *editor_exe,
                                     const char *output_root,
                                     const char *project_root,
                                     const uir_AssetRegistry *assets,
                                     char *out_package, size_t package_cap,
                                     char *out_project, size_t project_cap,
                                     char *out_player, size_t player_cap);

#ifdef __cplusplus
}
#endif

#endif /* UIM_PROJECT_H */
