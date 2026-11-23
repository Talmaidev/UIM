#include "uim_file_service.h"
#include <string.h>

uim_FileResult uim_file_service_validate(const uim_FileService *service)
{
    if (!service) return uim_FILE_ERR_ARGUMENT;
    if (service->abi_version != uim_FILE_SERVICE_ABI_VERSION) return uim_FILE_ERR_UNSUPPORTED;
    if (service->struct_size < sizeof(uim_FileService)) return uim_FILE_ERR_ARGUMENT;
    if (!service->ops.stat_path ||
        !service->ops.directory_open || !service->ops.directory_next ||
        !service->ops.directory_close ||
        !service->ops.create_directory ||
        !service->ops.remove_file || !service->ops.remove_directory ||
        !service->ops.rename_path ||
        !service->ops.open_read || !service->ops.open_write_atomic ||
        !service->ops.read || !service->ops.write || !service->ops.seek ||
        !service->ops.flush || !service->ops.commit_atomic ||
        !service->ops.abort_atomic || !service->ops.close) {
        return uim_FILE_ERR_ARGUMENT;
    }
    return uim_FILE_OK;
}
