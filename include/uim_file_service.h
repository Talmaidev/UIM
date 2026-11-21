#ifndef UIM_FILE_SERVICE_H
#define UIM_FILE_SERVICE_H

/*
 * UIM File Service ABI v1.
 * Platform-neutral synchronous file contract for Editor Domain.
 * Implementations belong to Platform Host adapters (Win32, Android, etc.).
 * This header intentionally includes no OS, engine, or graphics SDK.
 *
 * Path contract:
 * - UTF-8, NUL-terminated, no embedded NUL.
 * - '/' is the canonical separator accepted by Core.
 * - Adapter may translate to native path syntax.
 * - Paths are interpreted by the adapter; Project Domain enforces project roots.
 * - create_directory creates one directory level (not recursive).
 * - remove_directory removes an empty directory only; recursive deletion is
 *   a Project Domain operation built from explicit traversal.
 * - rename_path must fail with ALREADY_EXISTS if the destination already exists;
 *   replacement is available only through open_write_atomic/commit_atomic.
 *
 * Lifetime/threading contract:
 * - File and directory handles are opaque, nonzero, adapter-owned tokens.
 * - All callbacks are synchronous and called by the Editor/UI thread in ABI v1.
 * - Returned handles are closed exactly once.
 * - No callback retains caller buffers or path pointers after it returns.
 * - read returns uim_FILE_OK with out_bytes_read=0 at EOF; callers loop until 0.
 * - write may be short; callers loop until all bytes are accepted or an error.
 * - Adapters must report errors; they must never report success for a no-op.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define uim_FILE_SERVICE_ABI_VERSION 1u
#define uim_FILE_SERVICE_MAX_NAME_UTF8 1024u

typedef uint64_t uim_FileHandle;
typedef uint64_t uim_DirectoryHandle;
#define uim_FILE_HANDLE_INVALID ((uim_FileHandle)0)
#define uim_DIRECTORY_HANDLE_INVALID ((uim_DirectoryHandle)0)

typedef enum uim_FileResult {
    uim_FILE_OK = 0,
    uim_FILE_END,
    uim_FILE_ERR_ARGUMENT,
    uim_FILE_ERR_NOT_FOUND,
    uim_FILE_ERR_ALREADY_EXISTS,
    uim_FILE_ERR_ACCESS,
    uim_FILE_ERR_IO,
    uim_FILE_ERR_NO_SPACE,
    uim_FILE_ERR_TOO_MANY_OPEN,
    uim_FILE_ERR_NOT_DIRECTORY,
    uim_FILE_ERR_IS_DIRECTORY,
    uim_FILE_ERR_BUFFER_TOO_SMALL,
    uim_FILE_ERR_UNSUPPORTED,
    uim_FILE_ERR_STALE_HANDLE,
    uim_FILE_ERR_INTERNAL
} uim_FileResult;

typedef enum uim_FileSeekOrigin {
    uim_FILE_SEEK_BEGIN = 0,
    uim_FILE_SEEK_CURRENT = 1,
    uim_FILE_SEEK_END = 2
} uim_FileSeekOrigin;

typedef enum uim_FileKind {
    uim_FILE_KIND_UNKNOWN = 0,
    uim_FILE_KIND_REGULAR = 1,
    uim_FILE_KIND_DIRECTORY = 2,
    uim_FILE_KIND_SYMLINK = 3
} uim_FileKind;

enum {
    uim_FILE_ATTR_READ_ONLY = 1u << 0,
    uim_FILE_ATTR_HIDDEN    = 1u << 1,
    uim_FILE_ATTR_SYSTEM    = 1u << 2
};

typedef struct uim_FileStat {
    uint32_t struct_size;
    uint32_t attributes;
    uim_FileKind kind;
    uint32_t reserved;
    uint64_t size_bytes;
    int64_t modified_time_unix_ns;
} uim_FileStat;

typedef struct uim_DirectoryEntry {
    uint32_t struct_size;
    uim_FileKind kind;
    uint32_t attributes;
    uint32_t name_bytes; /* UTF-8 bytes excluding NUL */
    uint64_t size_bytes;
    int64_t modified_time_unix_ns;
} uim_DirectoryEntry;

/* Callback table. Every callback receives the same adapter-owned user pointer. */
typedef struct uim_FileServiceOps {
    uim_FileResult (*stat_path)(void *user, const char *utf8_path,
                                uim_FileStat *out_stat);
    uim_FileResult (*directory_open)(void *user, const char *utf8_path,
                                     uim_DirectoryHandle *out_handle);
    /* Returns uim_FILE_OK with one entry, uim_FILE_END at normal end.
     * name_buffer is caller-owned, capacity includes room for trailing NUL.
     * On BUFFER_TOO_SMALL, name_bytes reports required bytes excluding NUL. */
    uim_FileResult (*directory_next)(void *user, uim_DirectoryHandle handle,
                                     uim_DirectoryEntry *out_entry,
                                     char *name_buffer, size_t name_capacity);
    uim_FileResult (*directory_close)(void *user, uim_DirectoryHandle handle);
    uim_FileResult (*create_directory)(void *user, const char *utf8_path);
    uim_FileResult (*remove_file)(void *user, const char *utf8_path);
    uim_FileResult (*remove_directory)(void *user, const char *utf8_path);
    uim_FileResult (*rename_path)(void *user, const char *utf8_from,
                                  const char *utf8_to);
    uim_FileResult (*open_read)(void *user, const char *utf8_path,
                                uim_FileHandle *out_handle);
    /* Opens a temporary sibling/transaction file for the requested target.
     * Existing target remains intact until commit_atomic succeeds. */
    uim_FileResult (*open_write_atomic)(void *user, const char *utf8_target,
                                       uim_FileHandle *out_handle);
    uim_FileResult (*read)(void *user, uim_FileHandle handle, void *buffer,
                           size_t capacity, size_t *out_bytes_read);
    uim_FileResult (*write)(void *user, uim_FileHandle handle,
                            const void *data, size_t size,
                            size_t *out_bytes_written);
    uim_FileResult (*seek)(void *user, uim_FileHandle handle, int64_t offset,
                           uim_FileSeekOrigin origin, uint64_t *out_position);
    uim_FileResult (*flush)(void *user, uim_FileHandle handle);
    /* Flush + close + replace target. Success means target now contains the
     * complete new file. Failure must preserve old target whenever the
     * platform supports atomic replace; adapter reports UNSUPPORTED otherwise. */
    uim_FileResult (*commit_atomic)(void *user, uim_FileHandle handle);
    /* Closes and deletes the temporary transaction file. */
    uim_FileResult (*abort_atomic)(void *user, uim_FileHandle handle);
    uim_FileResult (*close)(void *user, uim_FileHandle handle);
} uim_FileServiceOps;

typedef struct uim_FileService {
    uint32_t abi_version;
    uint32_t struct_size;
    void *user;
    uim_FileServiceOps ops;
} uim_FileService;

/* Validate ABI version, struct size and mandatory callbacks before use. */
uim_FileResult uim_file_service_validate(const uim_FileService *service);

#ifdef __cplusplus
}
#endif
#endif
