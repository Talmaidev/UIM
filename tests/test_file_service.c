#include "uim_file_service.h"
#include <stdio.h>
#include <string.h>

static uim_FileResult dummy_stat(void *u, const char *p, uim_FileStat *s)
{ (void)u; (void)p; (void)s; return uim_FILE_OK; }
static uim_FileResult dummy_dir_open(void *u, const char *p, uim_DirectoryHandle *h)
{ (void)u; (void)p; *h = 1; return uim_FILE_OK; }
static uim_FileResult dummy_dir_next(void *u, uim_DirectoryHandle h, uim_DirectoryEntry *e, char *b, size_t c)
{ (void)u; (void)h; (void)e; (void)b; (void)c; return uim_FILE_END; }
static uim_FileResult dummy_dir_close(void *u, uim_DirectoryHandle h)
{ (void)u; (void)h; return uim_FILE_OK; }
static uim_FileResult dummy_ok(void *u, const char *p)
{ (void)u; (void)p; return uim_FILE_OK; }
static uim_FileResult dummy_ok2(void *u, const char *a, const char *b)
{ (void)u; (void)a; (void)b; return uim_FILE_OK; }
static uim_FileResult dummy_open(void *u, const char *p, uim_FileHandle *h)
{ (void)u; (void)p; *h = 1; return uim_FILE_OK; }
static uim_FileResult dummy_read(void *u, uim_FileHandle h, void *b, size_t c, size_t *n)
{ (void)u; (void)h; (void)b; (void)c; *n = 0; return uim_FILE_END; }
static uim_FileResult dummy_write(void *u, uim_FileHandle h, const void *b, size_t c, size_t *n)
{ (void)u; (void)h; (void)b; (void)c; *n = c; return uim_FILE_OK; }
static uim_FileResult dummy_seek(void *u, uim_FileHandle h, int64_t o, uim_FileSeekOrigin s, uint64_t *p)
{ (void)u; (void)h; (void)o; (void)s; *p = 0; return uim_FILE_OK; }
static uim_FileResult dummy_flush(void *u, uim_FileHandle h)
{ (void)u; (void)h; return uim_FILE_OK; }
static uim_FileResult dummy_close(void *u, uim_FileHandle h)
{ (void)u; (void)h; return uim_FILE_OK; }

int main(void)
{
    uim_FileService fs;
    memset(&fs, 0, sizeof(fs));
    if (uim_file_service_validate(NULL) != uim_FILE_ERR_ARGUMENT) return 1;
    if (uim_file_service_validate(&fs) != uim_FILE_ERR_UNSUPPORTED) return 2;
    fs.abi_version = uim_FILE_SERVICE_ABI_VERSION;
    fs.struct_size = sizeof(fs);
    if (uim_file_service_validate(&fs) != uim_FILE_ERR_ARGUMENT) return 3;
    fs.ops.stat_path = dummy_stat;
    fs.ops.directory_open = dummy_dir_open;
    fs.ops.directory_next = dummy_dir_next;
    fs.ops.directory_close = dummy_dir_close;
    fs.ops.create_directory = dummy_ok;
    fs.ops.remove_file = dummy_ok;
    fs.ops.remove_directory = dummy_ok;
    fs.ops.rename_path = dummy_ok2;
    fs.ops.open_read = dummy_open;
    fs.ops.open_write_atomic = dummy_open;
    fs.ops.read = dummy_read;
    fs.ops.write = dummy_write;
    fs.ops.seek = dummy_seek;
    fs.ops.flush = dummy_flush;
    fs.ops.commit_atomic = dummy_close;
    fs.ops.abort_atomic = dummy_close;
    fs.ops.close = dummy_close;
    if (uim_file_service_validate(&fs) != uim_FILE_OK) return 4;
    fs.struct_size = sizeof(fs) - 1;
    if (uim_file_service_validate(&fs) != uim_FILE_ERR_ARGUMENT) return 5;
    printf("file service ABI validation PASS\n");
    return 0;
}
