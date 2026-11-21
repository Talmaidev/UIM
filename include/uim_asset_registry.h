#ifndef UIM_ASSET_REGISTRY_H
#define UIM_ASSET_REGISTRY_H

/* 资产登记 + .uirassets 持久化 —— P2 最小闭环。
 * 固定容量、调用方提供 records、稳定递增 ID、相对路径校验；不 malloc。 */

#include "uim_file_service.h"
#include "uim_component_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UIR_ASSET_REGISTRY_ABI_VERSION 1u
#define UIR_ASSET_PATH_CAP 256u
#define UIR_ASSET_NAME_CAP 128u

typedef enum uir_AssetKind {
    UIR_ASSET_UNKNOWN = 0,
    UIR_ASSET_IMAGE = 1,
    UIR_ASSET_FONT = 2,
    UIR_ASSET_AUDIO = 3,
    UIR_ASSET_SCENE = 4,
    UIR_ASSET_OTHER = 5
} uir_AssetKind;

typedef struct uir_AssetRecord {
    uint64_t id;
    uint32_t kind;          /* uir_AssetKind */
    uint32_t flags;
    uint64_t size_bytes;
    char relative_path[UIR_ASSET_PATH_CAP];
    char display_name[UIR_ASSET_NAME_CAP];
} uir_AssetRecord;

typedef struct uir_AssetRegistry {
    uint32_t abi_version;
    uint32_t count;
    size_t capacity;
    uint64_t next_id;
    uir_AssetRecord *records;
} uir_AssetRegistry;

uim_Result uir_asset_registry_init(uir_AssetRegistry *registry,
                                   uir_AssetRecord *records,
                                   size_t capacity);

/* 注册项目相对路径。路径已存在返回 uim_ERR_EXISTS；容量满返回 uim_ERR_BUFFER；
 * 非法路径返回 uim_ERR_ARGUMENT。out_id 可为 NULL。 */
uim_Result uir_asset_registry_register(uir_AssetRegistry *registry,
                                       const char *relative_path,
                                       uint32_t kind,
                                       uint64_t *out_id);

uim_Result uir_asset_registry_find_path(const uir_AssetRegistry *registry,
                                        const char *relative_path,
                                        uint64_t *out_id);

uim_Result uir_asset_registry_remove(uir_AssetRegistry *registry, uint64_t id);

/* 保存/载入 .uirassets。文件格式版本化，坏 magic/越界长度返回 uim_ERR_RANGE。 */
uim_Result uir_asset_registry_save(const uir_AssetRegistry *registry,
                                   const uim_FileService *fs,
                                   const char *file_path);
uim_Result uir_asset_registry_load(uir_AssetRegistry *registry,
                                   const uim_FileService *fs,
                                   const char *file_path);

size_t uir_asset_registry_count(const uir_AssetRegistry *registry);

#ifdef __cplusplus
}
#endif

#endif /* UIM_ASSET_REGISTRY_H */
