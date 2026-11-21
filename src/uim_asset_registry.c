#include "uim_asset_registry.h"

#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(uir_AssetRecord) > 0u, "asset record must not be empty");
_Static_assert(UIR_ASSET_REGISTRY_ABI_VERSION == 1u, "asset registry ABI version changed");

static const unsigned char k_magic[4] = { 'U', 'I', 'R', 'A' };

static bool asset_path_valid(const char *path)
{
    size_t len;
    if (!path) return false;
    len = strlen(path);
    if (len == 0u || len >= UIR_ASSET_PATH_CAP) return false;
    if (path[0] == '/' || path[0] == '\\' || path[0] == ':') return false;
    if (path[len - 1u] == '/') return false;

    size_t seg_start = 0u;
    for (size_t i = 0u; i <= len; ++i) {
        if (i == len || path[i] == '/') {
            size_t seg_len = i - seg_start;
            if (seg_len == 0u) return false;
            if (seg_len == 1u && path[seg_start] == '.') return false;
            if (seg_len == 2u && path[seg_start] == '.' && path[seg_start + 1u] == '.')
                return false;
            seg_start = i + 1u;
        } else if (path[i] == '\\' || path[i] == ':') {
            return false;
        }
    }
    return true;
}

static const char *asset_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void put_u32(unsigned char *out, uint32_t value)
{
    out[0] = (unsigned char)(value & 0xFFu);
    out[1] = (unsigned char)((value >> 8) & 0xFFu);
    out[2] = (unsigned char)((value >> 16) & 0xFFu);
    out[3] = (unsigned char)((value >> 24) & 0xFFu);
}

static void put_u64(unsigned char *out, uint64_t value)
{
    put_u32(out, (uint32_t)(value & 0xFFFFFFFFu));
    put_u32(out + 4, (uint32_t)((value >> 32) & 0xFFFFFFFFu));
}

static uint32_t get_u32(const unsigned char *in)
{
    return ((uint32_t)in[0]) | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

static uint64_t get_u64(const unsigned char *in)
{
    return (uint64_t)get_u32(in) | ((uint64_t)get_u32(in + 4) << 32);
}

static uim_Result write_exact(const uim_FileService *fs, uim_FileHandle handle,
                              const void *data, size_t bytes)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t remaining = bytes;
    while (remaining > 0u) {
        size_t wrote = 0u;
        uim_FileResult rc = fs->ops.write((void *)fs->user, handle, p, remaining, &wrote);
        if (rc != uim_FILE_OK || wrote == 0u || wrote > remaining) return uim_ERR_STATE;
        p += wrote;
        remaining -= wrote;
    }
    return uim_OK;
}

static uim_Result read_exact(const uim_FileService *fs, uim_FileHandle handle,
                             void *data, size_t bytes)
{
    unsigned char *p = (unsigned char *)data;
    size_t remaining = bytes;
    while (remaining > 0u) {
        size_t got = 0u;
        uim_FileResult rc = fs->ops.read((void *)fs->user, handle, p, remaining, &got);
        if (rc != uim_FILE_OK || got == 0u || got > remaining) return uim_ERR_RANGE;
        p += got;
        remaining -= got;
    }
    return uim_OK;
}

uim_Result uir_asset_registry_init(uir_AssetRegistry *registry,
                                   uir_AssetRecord *records,
                                   size_t capacity)
{
    if (!registry) return uim_ERR_ARGUMENT;
    if (capacity > 0u && !records) return uim_ERR_ARGUMENT;
    memset(registry, 0, sizeof(*registry));
    registry->abi_version = UIR_ASSET_REGISTRY_ABI_VERSION;
    registry->records = records;
    registry->capacity = capacity;
    registry->next_id = 1u;
    return uim_OK;
}

uim_Result uir_asset_registry_register(uir_AssetRegistry *registry,
                                       const char *relative_path,
                                       uint32_t kind,
                                       uint64_t *out_id)
{
    const char *base;
    size_t path_len;
    size_t base_len;

    if (!registry || registry->abi_version != UIR_ASSET_REGISTRY_ABI_VERSION)
        return uim_ERR_ARGUMENT;
    if (!asset_path_valid(relative_path)) return uim_ERR_ARGUMENT;
    if (kind > UIR_ASSET_OTHER) return uim_ERR_ARGUMENT;
    if (registry->count >= registry->capacity) return uim_ERR_BUFFER;

    for (size_t i = 0u; i < registry->count; ++i) {
        if (strcmp(registry->records[i].relative_path, relative_path) == 0)
            return uim_ERR_EXISTS;
    }

    path_len = strlen(relative_path);
    base = asset_basename(relative_path);
    base_len = strlen(base);
    if (base_len == 0u || base_len >= UIR_ASSET_NAME_CAP) return uim_ERR_ARGUMENT;

    uir_AssetRecord *record = &registry->records[registry->count];
    memset(record, 0, sizeof(*record));
    record->id = registry->next_id;
    record->kind = kind;
    record->size_bytes = 0u;
    memcpy(record->relative_path, relative_path, path_len + 1u);
    memcpy(record->display_name, base, base_len + 1u);

    registry->next_id++;
    if (registry->next_id == 0u) registry->next_id = 1u;
    registry->count++;
    if (out_id) *out_id = record->id;
    return uim_OK;
}

uim_Result uir_asset_registry_find_path(const uir_AssetRegistry *registry,
                                        const char *relative_path,
                                        uint64_t *out_id)
{
    if (!registry || registry->abi_version != UIR_ASSET_REGISTRY_ABI_VERSION)
        return uim_ERR_ARGUMENT;
    if (!relative_path) return uim_ERR_ARGUMENT;
    for (size_t i = 0u; i < registry->count; ++i) {
        if (strcmp(registry->records[i].relative_path, relative_path) == 0) {
            if (out_id) *out_id = registry->records[i].id;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

uim_Result uir_asset_registry_remove(uir_AssetRegistry *registry, uint64_t id)
{
    if (!registry || registry->abi_version != UIR_ASSET_REGISTRY_ABI_VERSION)
        return uim_ERR_ARGUMENT;
    if (id == 0u) return uim_ERR_ARGUMENT;
    for (size_t i = 0u; i < registry->count; ++i) {
        if (registry->records[i].id == id) {
            size_t tail = registry->count - i - 1u;
            if (tail > 0u)
                memmove(&registry->records[i], &registry->records[i + 1u],
                        tail * sizeof(uir_AssetRecord));
            registry->count--;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

uim_Result uir_asset_registry_save(const uir_AssetRegistry *registry,
                                   const uim_FileService *fs,
                                   const char *file_path)
{
    uim_FileHandle handle = uim_FILE_HANDLE_INVALID;
    uim_FileResult frc;
    uim_Result rc = uim_OK;
    unsigned char header[12];
    unsigned char fixed[32];

    if (!registry || registry->abi_version != UIR_ASSET_REGISTRY_ABI_VERSION)
        return uim_ERR_ARGUMENT;
    if (!fs || fs->abi_version != uim_FILE_SERVICE_ABI_VERSION || !file_path)
        return uim_ERR_ARGUMENT;
    if (!fs->ops.open_write_atomic || !fs->ops.write || !fs->ops.commit_atomic)
        return uim_ERR_UNSUPPORTED;

    frc = fs->ops.open_write_atomic((void *)fs->user, file_path, &handle);
    if (frc != uim_FILE_OK || handle == uim_FILE_HANDLE_INVALID) return uim_ERR_STATE;

    memcpy(header, k_magic, sizeof(k_magic));
    put_u32(header + 4, UIR_ASSET_REGISTRY_ABI_VERSION);
    put_u32(header + 8, (uint32_t)registry->count);
    rc = write_exact(fs, handle, header, sizeof(header));

    for (size_t i = 0u; rc == uim_OK && i < registry->count; ++i) {
        const uir_AssetRecord *r = &registry->records[i];
        size_t path_len = strlen(r->relative_path);
        size_t name_len = strlen(r->display_name);
        if (path_len >= UIR_ASSET_PATH_CAP || name_len >= UIR_ASSET_NAME_CAP) {
            rc = uim_ERR_RANGE;
            break;
        }
        put_u64(fixed, r->id);
        put_u32(fixed + 8, r->kind);
        put_u32(fixed + 12, r->flags);
        put_u64(fixed + 16, r->size_bytes);
        put_u32(fixed + 24, (uint32_t)path_len);
        put_u32(fixed + 28, (uint32_t)name_len);
        rc = write_exact(fs, handle, fixed, sizeof(fixed));
        if (rc == uim_OK) rc = write_exact(fs, handle, r->relative_path, path_len);
        if (rc == uim_OK) rc = write_exact(fs, handle, r->display_name, name_len);
    }

    if (rc == uim_OK) {
        rc = (fs->ops.commit_atomic((void *)fs->user, handle) == uim_FILE_OK)
             ? uim_OK : uim_ERR_STATE;
        /* commit_atomic owns flush + close; the handle must not be closed again. */
    } else if (fs->ops.abort_atomic) {
        (void)fs->ops.abort_atomic((void *)fs->user, handle);  /* closes + deletes temp */
    } else if (fs->ops.close) {
        (void)fs->ops.close((void *)fs->user, handle);
    }

    return rc;
}

uim_Result uir_asset_registry_load(uir_AssetRegistry *registry,
                                   const uim_FileService *fs,
                                   const char *file_path)
{
    uim_FileHandle handle = uim_FILE_HANDLE_INVALID;
    uim_FileResult frc;
    uim_Result rc;
    unsigned char header[12];
    unsigned char fixed[32];
    uint32_t count;
    uint32_t version;

    if (!registry || registry->abi_version != UIR_ASSET_REGISTRY_ABI_VERSION)
        return uim_ERR_ARGUMENT;
    if (!fs || fs->abi_version != uim_FILE_SERVICE_ABI_VERSION || !file_path)
        return uim_ERR_ARGUMENT;
    if (!fs->ops.open_read || !fs->ops.read) return uim_ERR_UNSUPPORTED;

    frc = fs->ops.open_read((void *)fs->user, file_path, &handle);
    if (frc != uim_FILE_OK || handle == uim_FILE_HANDLE_INVALID) return uim_ERR_STATE;

    registry->count = 0u;
    rc = read_exact(fs, handle, header, sizeof(header));
    if (rc != uim_OK) goto done;
    if (memcmp(header, k_magic, sizeof(k_magic)) != 0) { rc = uim_ERR_RANGE; goto done; }
    version = get_u32(header + 4);
    if (version != UIR_ASSET_REGISTRY_ABI_VERSION) { rc = uim_ERR_RANGE; goto done; }
    count = get_u32(header + 8);
    if (count > registry->capacity) { rc = uim_ERR_RANGE; goto done; }

    for (uint32_t i = 0u; i < count; ++i) {
        uint64_t id;
        uint32_t kind;
        uint32_t flags;
        uint64_t size_bytes;
        uint32_t path_len;
        uint32_t name_len;
        uir_AssetRecord *r = &registry->records[i];

        rc = read_exact(fs, handle, fixed, sizeof(fixed));
        if (rc != uim_OK) goto done;
        id = get_u64(fixed);
        kind = get_u32(fixed + 8);
        flags = get_u32(fixed + 12);
        size_bytes = get_u64(fixed + 16);
        path_len = get_u32(fixed + 24);
        name_len = get_u32(fixed + 28);
        if (id == 0u || id == UINT64_MAX || kind > UIR_ASSET_OTHER ||
            path_len == 0u || path_len >= UIR_ASSET_PATH_CAP ||
            name_len == 0u || name_len >= UIR_ASSET_NAME_CAP) {
            rc = uim_ERR_RANGE;
            goto done;
        }

        memset(r, 0, sizeof(*r));
        r->id = id;
        r->kind = kind;
        r->flags = flags;
        r->size_bytes = size_bytes;
        rc = read_exact(fs, handle, r->relative_path, path_len);
        if (rc == uim_OK) rc = read_exact(fs, handle, r->display_name, name_len);
        if (rc != uim_OK) goto done;
        r->relative_path[path_len] = '\0';
        r->display_name[name_len] = '\0';
        if (!asset_path_valid(r->relative_path)) { rc = uim_ERR_RANGE; goto done; }
        for (uint32_t j = 0u; j < i; ++j) {
            if (registry->records[j].id == id ||
                strcmp(registry->records[j].relative_path, r->relative_path) == 0) {
                rc = uim_ERR_RANGE;
                goto done;
            }
        }
        if (id >= registry->next_id) registry->next_id = id + 1u;
    }

    registry->count = count;
    rc = uim_OK;

done:
    if (rc != uim_OK) registry->count = 0u;
    if (fs->ops.close) (void)fs->ops.close((void *)fs->user, handle);
    return rc;
}

size_t uir_asset_registry_count(const uir_AssetRegistry *registry)
{
    return registry ? registry->count : 0u;
}

