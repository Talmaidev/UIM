#include "uim_project.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#define UIR_PROJECT_FILE_LIMIT (64u * 1024u * 1024u)
#define UIR_PROJECT_STRING_LIMIT (1024u * 1024u)
#define UIR_PROJECT_NODE_LIMIT 100000u
#define UIR_PROJECT_PAGE_LIMIT 1024u
#define UIR_PROJECT_COMPONENT_LIMIT 1024u

typedef struct ProjectWriter {
    unsigned char *data;
    size_t size;
    size_t cap;
} ProjectWriter;

typedef struct ProjectReader {
    const unsigned char *data;
    size_t size;
    size_t at;
} ProjectReader;

typedef struct LoadProperty {
    uim_PropertyId property_id;
    char *stable_key; /* only used by the global property table */
    uim_Value value;
} LoadProperty;

typedef struct LoadComponent {
    uim_ComponentInstanceInfo info;
    LoadProperty *properties;
    uint32_t property_count;
    uint32_t property_loaded;
} LoadComponent;

typedef struct LoadNode {
    char *key;
    char *parent;
    uim_ComponentId component_id;
    uim_Rect rect;
    uim_Space space;
    uim_Transform3 transform;
    uim_LayoutConstraints layout;
    bool visible;
    bool enabled;
    int32_t z_order;
    uim_ActionId action_id;
    uint32_t child_order;
    const unsigned char *config;
    uint32_t config_size;
    bool config_owned;
    LoadComponent *components;
    uint32_t component_count;
    uint32_t component_loaded;
    bool has_binding;
    uim_BindingDesc binding;
    uim_NodeId node_id;
} LoadNode;

typedef struct LoadPage {
    char *key;
    LoadNode *nodes;
    uint32_t node_count;
    uim_PageId page_id;
    uim_NodeId root_id;
} LoadPage;

static uint32_t project_hash(const unsigned char *data, size_t size) {
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < size; ++i) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

static bool writer_reserve(ProjectWriter *w, size_t extra) {
    size_t need;
    size_t cap;
    unsigned char *next;
    if (extra > UIR_PROJECT_FILE_LIMIT || w->size > UIR_PROJECT_FILE_LIMIT - extra) {
        return false;
    }
    need = w->size + extra;
    if (need <= w->cap) return true;
    cap = w->cap ? w->cap : 1024u;
    while (cap < need) {
        if (cap > UIR_PROJECT_FILE_LIMIT / 2u) {
            cap = UIR_PROJECT_FILE_LIMIT;
            break;
        }
        cap *= 2u;
    }
    next = (unsigned char *)realloc(w->data, cap);
    if (!next) return false;
    w->data = next;
    w->cap = cap;
    return true;
}

static bool put_bytes(ProjectWriter *w, const void *data, size_t size) {
    if (size && !data) return false;
    if (!writer_reserve(w, size)) return false;
    if (size) memcpy(w->data + w->size, data, size);
    w->size += size;
    return true;
}

static bool put_u8(ProjectWriter *w, uint8_t value) {
    return put_bytes(w, &value, 1u);
}

static bool put_u32(ProjectWriter *w, uint32_t value) {
    unsigned char b[4];
    b[0] = (unsigned char)(value & 0xffu);
    b[1] = (unsigned char)((value >> 8) & 0xffu);
    b[2] = (unsigned char)((value >> 16) & 0xffu);
    b[3] = (unsigned char)((value >> 24) & 0xffu);
    return put_bytes(w, b, 4u);
}

static bool put_f32(ProjectWriter *w, float value) {
    uint32_t bits;
    if (!isfinite(value)) return false;
    memcpy(&bits, &value, sizeof(bits));
    return put_u32(w, bits);
}

static bool put_u64(ProjectWriter *w, uint64_t value) {
    unsigned char b[8];
    for (int i = 0; i < 8; ++i) b[i] = (unsigned char)((value >> (i * 8)) & 0xffu);
    return put_bytes(w, b, sizeof(b));
}

static bool put_string(ProjectWriter *w, const char *s) {
    size_t n;
    if (!s) s = "";
    n = strlen(s);
    if (n > UIR_PROJECT_STRING_LIMIT || n > UINT32_MAX) return false;
    return put_u32(w, (uint32_t)n) && put_bytes(w, s, n);
}

static bool reader_bytes(ProjectReader *r, const unsigned char **out, size_t size) {
    if (!out || size > r->size - r->at) return false;
    *out = r->data + r->at;
    r->at += size;
    return true;
}

static bool get_u8(ProjectReader *r, uint8_t *out) {
    const unsigned char *p;
    if (!out || !reader_bytes(r, &p, 1u)) return false;
    *out = p[0];
    return true;
}

static bool get_u32(ProjectReader *r, uint32_t *out) {
    const unsigned char *p;
    if (!out || !reader_bytes(r, &p, 4u)) return false;
    *out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
    return true;
}

static bool get_f32(ProjectReader *r, float *out) {
    uint32_t bits;
    if (!out || !get_u32(r, &bits)) return false;
    memcpy(out, &bits, sizeof(*out));
    return isfinite(*out);
}

static bool get_u64(ProjectReader *r, uint64_t *out) {
    const unsigned char *p;
    uint64_t value = 0u;
    if (!out || !reader_bytes(r, &p, 8u)) return false;
    for (int i = 0; i < 8; ++i) value |= (uint64_t)p[i] << (i * 8);
    *out = value;
    return true;
}

static bool get_string(ProjectReader *r, char **out) {
    uint32_t n;
    const unsigned char *p;
    char *s;
    if (!out || !get_u32(r, &n)) return false;
    if (n > UIR_PROJECT_STRING_LIMIT || n > r->size - r->at) return false;
    if (!reader_bytes(r, &p, (size_t)n)) return false;
    if (memchr(p, '\0', (size_t)n) != NULL) return false;
    s = (char *)malloc((size_t)n + 1u);
    if (!s) return false;
    if (n) memcpy(s, p, (size_t)n);
    s[n] = '\0';
    *out = s;
    return true;
}

static bool put_i64(ProjectWriter *w, int64_t value) {
    return put_u64(w, (uint64_t)value);
}

static bool get_i64(ProjectReader *r, int64_t *out) {
    uint64_t value;
    if (!out || !get_u64(r, &value)) return false;
    *out = (int64_t)value;
    return true;
}

static bool put_value(ProjectWriter *w, uim_Value value) {
    if (!put_u8(w, (uint8_t)value.type)) return false;
    switch (value.type) {
        case uim_VALUE_NONE:
            return true;
        case uim_VALUE_BOOL:
            return put_u8(w, value.as.boolean ? 1u : 0u);
        case uim_VALUE_INT64:
            return put_i64(w, value.as.integer);
        case uim_VALUE_FLOAT64: {
            uint64_t bits;
            if (!isfinite(value.as.real)) return false;
            memcpy(&bits, &value.as.real, sizeof(bits));
            return put_u64(w, bits);
        }
        case uim_VALUE_STRING:
            if (value.as.string.size > UIR_PROJECT_STRING_LIMIT) return false;
            return put_u32(w, (uint32_t)value.as.string.size) &&
                   put_bytes(w, value.as.string.data, value.as.string.size);
        default:
            return false;
    }
}

static bool get_value(ProjectReader *r, uim_Value *out) {
    uint8_t type;
    uint8_t boolean;
    int64_t integer;
    uint64_t bits;
    uint32_t n;
    const unsigned char *p;
    char *s;
    if (!out || !get_u8(r, &type)) return false;
    memset(out, 0, sizeof(*out));
    out->type = (uim_ValueType)type;
    switch (out->type) {
        case uim_VALUE_NONE:
            return true;
        case uim_VALUE_BOOL:
            if (!get_u8(r, &boolean) || boolean > 1u) return false;
            out->as.boolean = boolean != 0u;
            return true;
        case uim_VALUE_INT64:
            if (!get_i64(r, &integer)) return false;
            out->as.integer = integer;
            return true;
        case uim_VALUE_FLOAT64:
            if (!get_u64(r, &bits)) return false;
            memcpy(&out->as.real, &bits, sizeof(out->as.real));
            return isfinite(out->as.real);
        case uim_VALUE_STRING:
            if (!get_u32(r, &n) || n > UIR_PROJECT_STRING_LIMIT || n > r->size - r->at ||
                !reader_bytes(r, &p, (size_t)n)) return false;
            s = (char *)malloc((size_t)n + 1u);
            if (!s) return false;
            if (n) memcpy(s, p, (size_t)n);
            s[n] = '\0';
            out->as.string.data = s;
            out->as.string.size = (size_t)n;
            return true;
        default:
            return false;
    }
}

static void value_free(uim_Value *value) {
    if (!value) return;
    if (value->type == uim_VALUE_STRING) free((void *)value->as.string.data);
    memset(value, 0, sizeof(*value));
}

static bool path_join(char *out, size_t cap, const char *a, const char *b) {
    int n;
    if (!out || cap == 0u || !a || !b) return false;
    n = snprintf(out, cap, "%s%s%s", a, (a[0] && a[strlen(a) - 1u] != '/' && a[strlen(a) - 1u] != '\\') ? "/" : "", b);
    return n > 0 && (size_t)n < cap;
}

static const char *path_basename(const char *path) {
    const char *a;
    const char *b;
    if (!path) return "";
    a = strrchr(path, '/');
    b = strrchr(path, '\\');
    if (b && (!a || b > a)) a = b;
    return a ? a + 1 : path;
}

static void path_dirname(const char *path, char *out, size_t cap) {
    const char *a;
    const char *b;
    const char *sep;
    size_t n;
    if (!out || cap == 0u) return;
    out[0] = '\0';
    if (!path || !path[0]) {
        snprintf(out, cap, ".");
        return;
    }
    a = strrchr(path, '/');
    b = strrchr(path, '\\');
    sep = (b && (!a || b > a)) ? b : a;
    if (!sep) {
        snprintf(out, cap, ".");
        return;
    }
    n = (size_t)(sep - path);
    if (n == 0u) {
        snprintf(out, cap, ".");
        return;
    }
#if defined(_WIN32)
    if (n == 2u && path[1] == ':') n = 3u; /* keep C:\ rather than C: */
#endif
    if (n >= cap) n = cap - 1u;
    memcpy(out, path, n);
    out[n] = '\0';
}

static void project_name_from_path(const char *path, char *out, size_t cap) {
    const char *base = path_basename(path);
    const char *dot = strrchr(base, '.');
    size_t n;
    if (!out || cap == 0u) return;
    if (!base) base = "project";
    n = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    if (n == 0u) n = 7u, base = "project";
    if (n >= cap) n = cap - 1u;
    memcpy(out, base, n);
    out[n] = '\0';
}

static bool file_exists(const char *path) {
#if defined(_WIN32)
    DWORD attr;
    if (!path || !path[0]) return false;
    attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    return path && path[0] && access(path, F_OK) == 0;
#endif
}

static bool ensure_directory(const char *path) {
#if defined(_WIN32)
    char buffer[UIR_PROJECT_PATH_CAP];
    size_t i;
    size_t n;
    if (!path || !path[0]) return false;
    n = strlen(path);
    if (n >= sizeof(buffer)) return false;
    memcpy(buffer, path, n + 1u);
    for (i = 1u; i < n; ++i) {
        if (buffer[i] == '/' || buffer[i] == '\\') {
            char saved = buffer[i];
            buffer[i] = '\0';
            if (i > 3u || (i == 2u && buffer[1] == ':')) {
                (void)CreateDirectoryA(buffer, NULL);
            }
            buffer[i] = saved;
        }
    }
    if (CreateDirectoryA(buffer, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) return true;
    return false;
#else
    char buffer[UIR_PROJECT_PATH_CAP];
    size_t i;
    size_t n;
    if (!path || !path[0]) return false;
    n = strlen(path);
    if (n >= sizeof(buffer)) return false;
    memcpy(buffer, path, n + 1u);
    for (i = 1u; i < n; ++i) {
        if (buffer[i] == '/') {
            char saved = buffer[i];
            buffer[i] = '\0';
            (void)mkdir(buffer, 0777);
            buffer[i] = saved;
        }
    }
    return mkdir(buffer, 0777) == 0 || errno == EEXIST;
#endif
}

static bool copy_file(const char *source, const char *dest) {
#if defined(_WIN32)
    if (!source || !dest) return false;
    return CopyFileA(source, dest, FALSE) != 0;
#else
    FILE *in;
    FILE *out;
    unsigned char buffer[8192];
    size_t got;
    bool ok = true;
    if (!source || !dest) return false;
    in = fopen(source, "rb");
    if (!in) return false;
    out = fopen(dest, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    while ((got = fread(buffer, 1u, sizeof(buffer), in)) > 0u) {
        if (fwrite(buffer, 1u, got, out) != got) {
            ok = false;
            break;
        }
    }
    if (ferror(in)) ok = false;
    if (fclose(out) != 0) ok = false;
    fclose(in);
    return ok;
#endif
}

static bool read_whole_file(const char *path, unsigned char **out, size_t *out_size) {
    FILE *f;
    long length;
    unsigned char *data;
    size_t n;
    if (!path || !out || !out_size) return false;
    *out = NULL;
    *out_size = 0u;
    f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0 || (length = ftell(f)) < 0 ||
        (size_t)length > UIR_PROJECT_FILE_LIMIT || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    n = (size_t)length;
    if (n == 0u) {
        fclose(f);
        return false;
    }
    data = (unsigned char *)malloc(n);
    if (!data) {
        fclose(f);
        return false;
    }
    if (fread(data, 1u, n, f) != n) {
        free(data);
        fclose(f);
        return false;
    }
    if (fclose(f) != 0) {
        free(data);
        return false;
    }
    *out = data;
    *out_size = n;
    return true;
}

static bool write_atomic(const char *path, const unsigned char *data, size_t size) {
    char tmp[UIR_PROJECT_PATH_CAP + 8u];
    FILE *f;
    bool ok = true;
    int n;
    if (!path || !path[0] || !data || size > UIR_PROJECT_FILE_LIMIT) return false;
    n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n <= 0 || (size_t)n >= sizeof(tmp)) return false;
    f = fopen(tmp, "wb");
    if (!f) return false;
    if (size && fwrite(data, 1u, size, f) != size) ok = false;
    if (fflush(f) != 0) ok = false;
    if (fclose(f) != 0) ok = false;
    if (ok) {
#if defined(_WIN32)
        ok = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        ok = rename(tmp, path) == 0;
#endif
    }
    if (!ok) remove(tmp);
    return ok;
}

static bool write_node(ProjectWriter *w, const UIM_ComponentManager *manager,
                       uim_NodeId node_id) {
    uim_NodeInfo info;
    const char *parent_key = "";
    uim_NodeInfo parent;
    uint32_t component_count = 0u;
    size_t instances = 0u;
    size_t i;
    if (uim_node_get(manager, node_id, &info) != uim_OK) return false;
    if (!info.stable_key || !info.stable_key[0]) return false;
    if (info.parent_id != 0u) {
        if (uim_node_get(manager, info.parent_id, &parent) != uim_OK ||
            !parent.stable_key || !parent.stable_key[0]) return false;
        parent_key = parent.stable_key;
    }
    if (uim_node_component_count(manager, node_id, &instances) != uim_OK) return false;
    if (instances > UIR_PROJECT_COMPONENT_LIMIT) return false;
    if (info.component_config_size > UINT32_MAX) return false;
    component_count = (uint32_t)instances;
    if (!put_string(w, info.stable_key) || !put_string(w, parent_key) ||
        !put_u32(w, (uint32_t)info.component_id) ||
        !put_f32(w, info.local_rect.x) || !put_f32(w, info.local_rect.y) ||
        !put_f32(w, info.local_rect.width) || !put_f32(w, info.local_rect.height) ||
        !put_u8(w, (uint8_t)info.space) ||
        !put_f32(w, info.transform.position[0]) || !put_f32(w, info.transform.position[1]) ||
        !put_f32(w, info.transform.position[2]) ||
        !put_f32(w, info.transform.rotation[0]) || !put_f32(w, info.transform.rotation[1]) ||
        !put_f32(w, info.transform.rotation[2]) ||
        !put_f32(w, info.transform.scale[0]) || !put_f32(w, info.transform.scale[1]) ||
        !put_f32(w, info.transform.scale[2]) ||
        !put_u8(w, info.layout.enabled ? 1u : 0u) ||
        !put_f32(w, info.layout.anchor_min_x) || !put_f32(w, info.layout.anchor_min_y) ||
        !put_f32(w, info.layout.anchor_max_x) || !put_f32(w, info.layout.anchor_max_y) ||
        !put_f32(w, info.layout.offset_left) || !put_f32(w, info.layout.offset_top) ||
        !put_f32(w, info.layout.offset_right) || !put_f32(w, info.layout.offset_bottom) ||
        !put_u8(w, info.visible ? 1u : 0u) || !put_u8(w, info.enabled ? 1u : 0u) ||
        !put_u32(w, (uint32_t)info.z_order) || !put_u32(w, info.action_id) ||
        !put_u32(w, info.child_order) || !put_u32(w, (uint32_t)info.component_config_size) ||
        !put_bytes(w, info.component_config, info.component_config_size) ||
        !put_u32(w, component_count)) {
        return false;
    }
    for (i = 0; i < instances; ++i) {
        uim_ComponentInstanceInfo instance;
        uim_ComponentInstanceId instance_id;
        size_t property_count = 0u;
        size_t j;
        if (uim_node_component_get_at(manager, node_id, i, &instance) != uim_OK) return false;
        instance_id = instance.instance_id;
        if (uim_component_instance_property_count(manager, instance_id, &property_count) != uim_OK)
            return false;
        if (property_count > UIR_PROJECT_COMPONENT_LIMIT) return false;
        if (!put_u32(w, (uint32_t)instance.type_id) ||
            !put_u32(w, instance.schema_version) ||
            !put_u64(w, instance.instance_id) ||
            !put_u32(w, (uint32_t)property_count)) return false;
        for (j = 0; j < property_count; ++j) {
            uim_PropertyId property_id = 0u;
            uim_Value value;
            if (uim_component_instance_property_get_at(manager, instance_id, j,
                                                       &property_id, &value) != uim_OK)
                return false;
            if (!put_u32(w, property_id) || !put_value(w, value)) return false;
        }
    }
    if (info.binding_id != 0u) {
        uim_BindingDesc binding;
        if (uim_binding_get(manager, info.binding_id, &binding) != uim_OK) return false;
        if (!put_u8(w, 1u) ||
            !put_u32(w, binding.property_id) ||
            !put_u8(w, (uint8_t)binding.value_type) ||
            !put_u8(w, (uint8_t)binding.direction) ||
            !put_u8(w, (uint8_t)binding.initial_sync) ||
            !put_u8(w, (uint8_t)binding.state)) return false;
    } else if (!put_u8(w, 0u)) {
        return false;
    }
    return true;
}

static bool write_global_properties(ProjectWriter *w,
                                    const UIM_ComponentManager *manager) {
    size_t count = 0u;
    size_t i;
    if (uim_setting_count(manager, &count) != uim_OK || count > UIR_PROJECT_NODE_LIMIT)
        return false;
    if (!put_u32(w, (uint32_t)count)) return false;
    for (i = 0; i < count; ++i) {
        uim_PropertyId property_id = 0u;
        uim_SettingState state;
        const char *stable_key = NULL;
        if (uim_setting_get_at(manager, i, &property_id, &state) != uim_OK ||
            uim_property_get_stable_key(manager, property_id, &stable_key) != uim_OK ||
            !stable_key || !stable_key[0]) return false;
        if (!put_u32(w, property_id) || !put_string(w, stable_key) ||
            !put_value(w, state.committed)) return false;
    }
    return true;
}

static bool write_page(ProjectWriter *w, const UIM_ComponentManager *manager,
                       uim_PageId page_id) {
    const char *page_key = NULL;
    size_t count = 0u;
    size_t i;
    if (uim_page_get_key(manager, page_id, &page_key) != uim_OK ||
        !page_key || !page_key[0]) return false;
    if (uim_page_get_node_count(manager, page_id, &count) != uim_OK ||
        count > UIR_PROJECT_NODE_LIMIT) return false;
    if (!put_string(w, page_key) || !put_u32(w, (uint32_t)count)) return false;
    for (i = 0; i < count; ++i) {
        uim_NodeInfo info;
        if (uim_page_get_node_at(manager, page_id, i, &info) != uim_OK) return false;
        if (!write_node(w, manager, info.id)) return false;
    }
    return true;
}

static bool text_append(ProjectWriter *w, const char *text, size_t length) {
    return put_bytes(w, (const unsigned char *)text, length);
}

static bool text_appendf(ProjectWriter *w, const char *format, ...) {
    char stack[512];
    va_list args;
    va_list copy;
    int needed;
    if (!w || !format) return false;
    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(stack, sizeof(stack), format, args);
    va_end(args);
    if (needed < 0) {
        va_end(copy);
        return false;
    }
    if ((size_t)needed < sizeof(stack)) {
        va_end(copy);
        return text_append(w, stack, (size_t)needed);
    }
    {
        char *heap = (char *)malloc((size_t)needed + 1u);
        bool ok;
        if (!heap) {
            va_end(copy);
            return false;
        }
        vsnprintf(heap, (size_t)needed + 1u, format, copy);
        va_end(copy);
        ok = text_append(w, heap, (size_t)needed);
        free(heap);
        return ok;
    }
}

static bool text_append_quoted(ProjectWriter *w, const char *value) {
    const char *p = value ? value : "";
    if (!text_append(w, "\"", 1u)) return false;
    while (*p) {
        unsigned char c = (unsigned char)*p++;
        switch (c) {
        case '"': if (!text_append(w, "\\\"", 2u)) return false; break;
        case '\\': if (!text_append(w, "\\\\", 2u)) return false; break;
        case '\n': if (!text_append(w, "\\n", 2u)) return false; break;
        case '\r': if (!text_append(w, "\\r", 2u)) return false; break;
        case '\t': if (!text_append(w, "\\t", 2u)) return false; break;
        default:
            if (c < 32u) {
                char escaped[5];
                snprintf(escaped, sizeof(escaped), "\\x%02x", (unsigned)c);
                if (!text_append(w, escaped, 4u)) return false;
            } else if (!text_append(w, (const char *)&c, 1u)) {
                return false;
            }
            break;
        }
    }
    return text_append(w, "\"", 1u);
}

static bool text_append_value(ProjectWriter *w, uim_Value value) {
    switch (value.type) {
    case uim_VALUE_NONE:
        return text_append(w, "none", 4u);
    case uim_VALUE_BOOL:
        return text_appendf(w, "bool %d", value.as.boolean ? 1 : 0);
    case uim_VALUE_INT64:
        return text_appendf(w, "int %lld", (long long)value.as.integer);
    case uim_VALUE_FLOAT64:
        return text_appendf(w, "float %.17g", value.as.real);
    case uim_VALUE_STRING:
        if (!text_append(w, "string ", 7u)) return false;
        {
            char *copy = (char *)malloc(value.as.string.size + 1u);
            bool ok;
            if (!copy) return false;
            if (value.as.string.size) memcpy(copy, value.as.string.data, value.as.string.size);
            copy[value.as.string.size] = '\0';
            ok = text_append_quoted(w, copy);
            free(copy);
            return ok;
        }
    default:
        return false;
    }
}

static bool text_append_hex(ProjectWriter *w, const unsigned char *data, uint32_t size) {
    uint32_t i;
    if (!size) return text_append(w, "-", 1u);
    for (i = 0; i < size; ++i) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", data[i]);
        if (!text_append(w, hex, 2u)) return false;
    }
    return true;
}

static bool write_text_node(ProjectWriter *w, const UIM_ComponentManager *manager,
                            uim_NodeId node_id) {
    uim_NodeInfo info;
    const char *parent_key = "";
    uim_NodeInfo parent;
    size_t instances = 0u;
    size_t i;
    if (uim_node_get(manager, node_id, &info) != uim_OK) return false;
    if (!info.stable_key || !info.stable_key[0]) return false;
    if (info.parent_id != 0u) {
        if (uim_node_get(manager, info.parent_id, &parent) != uim_OK ||
            !parent.stable_key || !parent.stable_key[0]) return false;
        parent_key = parent.stable_key;
    }
    if (uim_node_component_count(manager, node_id, &instances) != uim_OK) return false;
    if (instances > UIR_PROJECT_COMPONENT_LIMIT) return false;
    if (!text_append(w, "node ", 5u) || !text_append_quoted(w, info.stable_key) ||
        !text_append(w, " ", 1u) || !text_append_quoted(w, parent_key) ||
        !text_appendf(w, " %u %.9g %.9g %.9g %.9g %u",
                      (unsigned)info.component_id,
                      info.local_rect.x, info.local_rect.y,
                      info.local_rect.width, info.local_rect.height,
                      (unsigned)info.space) ||
        !text_appendf(w, " %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g",
                      info.transform.position[0], info.transform.position[1], info.transform.position[2],
                      info.transform.rotation[0], info.transform.rotation[1], info.transform.rotation[2],
                      info.transform.scale[0], info.transform.scale[1], info.transform.scale[2]) ||
        !text_appendf(w, " %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g",
                      info.layout.enabled ? 1u : 0u,
                      info.layout.anchor_min_x, info.layout.anchor_min_y,
                      info.layout.anchor_max_x, info.layout.anchor_max_y,
                      info.layout.offset_left, info.layout.offset_top,
                      info.layout.offset_right, info.layout.offset_bottom) ||
        !text_appendf(w, " %u %u %d %u %u ",
                      info.visible ? 1u : 0u, info.enabled ? 1u : 0u,
                      (int)info.z_order, (unsigned)info.action_id, (unsigned)info.child_order) ||
        !text_append_hex(w, info.component_config, (uint32_t)info.component_config_size) ||
        !text_appendf(w, " %u\n", (unsigned)instances)) {
        return false;
    }
    for (i = 0; i < instances; ++i) {
        uim_ComponentInstanceInfo instance;
        size_t property_count = 0u;
        size_t j;
        if (uim_node_component_get_at(manager, node_id, i, &instance) != uim_OK) return false;
        if (uim_component_instance_property_count(manager, instance.instance_id, &property_count) != uim_OK)
            return false;
        if (property_count > UIR_PROJECT_COMPONENT_LIMIT) return false;
        if (!text_appendf(w, "comp %u %u %llu %u\n",
                          (unsigned)instance.type_id, (unsigned)instance.schema_version,
                          (unsigned long long)instance.instance_id, (unsigned)property_count))
            return false;
        for (j = 0; j < property_count; ++j) {
            uim_PropertyId property_id = 0u;
            uim_Value value;
            if (uim_component_instance_property_get_at(manager, instance.instance_id, j,
                                                       &property_id, &value) != uim_OK)
                return false;
            if (!text_appendf(w, "prop %u ", (unsigned)property_id) ||
                !text_append_value(w, value) || !text_append(w, "\n", 1u))
                return false;
        }
    }
    if (info.binding_id != 0u) {
        uim_BindingDesc binding;
        if (uim_binding_get(manager, info.binding_id, &binding) != uim_OK) return false;
        if (!text_appendf(w, "bind %u %u %u %u %u\n",
                          (unsigned)binding.property_id, (unsigned)binding.value_type,
                          (unsigned)binding.direction, (unsigned)binding.initial_sync,
                          (unsigned)binding.state))
            return false;
    }
    return true;
}

static bool write_text_globals(ProjectWriter *w, const UIM_ComponentManager *manager) {
    size_t count = 0u;
    size_t i;
    if (uim_setting_count(manager, &count) != uim_OK || count > UIR_PROJECT_NODE_LIMIT)
        return false;
    for (i = 0; i < count; ++i) {
        uim_PropertyId property_id = 0u;
        uim_SettingState state;
        const char *stable_key = NULL;
        if (uim_setting_get_at(manager, i, &property_id, &state) != uim_OK ||
            uim_property_get_stable_key(manager, property_id, &stable_key) != uim_OK ||
            !stable_key || !stable_key[0]) return false;
        if (!text_appendf(w, "global %u ", (unsigned)property_id) ||
            !text_append_quoted(w, stable_key) || !text_append(w, " ", 1u) ||
            !text_append_value(w, state.committed) || !text_append(w, "\n", 1u))
            return false;
    }
    return true;
}

uim_Result uir_project_serialize(const UIM_ComponentManager *manager,
                                 const uim_PageId *pages,
                                 size_t page_count,
                                 const char *project_name,
                                 unsigned char **out_data,
                                 size_t *out_size) {
    ProjectWriter w;
    char name[UIR_PROJECT_NAME_CAP];
    const char *entry_key = NULL;
    bool ok = true;
    size_t i;
    if (!manager || !pages || page_count == 0u || page_count > UIR_PROJECT_PAGE_LIMIT ||
        !out_data || !out_size) return uim_ERR_ARGUMENT;
    *out_data = NULL;
    *out_size = 0u;
    memset(&w, 0, sizeof(w));
    if (project_name && project_name[0]) {
        size_t length = strlen(project_name);
        if (length >= sizeof(name)) return uim_ERR_ARGUMENT;
        memcpy(name, project_name, length + 1u);
    } else {
        name[0] = '\0';
    }
    if (uim_page_get_key(manager, pages[0], &entry_key) != uim_OK || !entry_key || !entry_key[0])
        return uim_ERR_ARGUMENT;
    ok = text_appendf(&w, "UIRJ %u\nname ", UIR_PROJECT_VERSION) &&
         text_append_quoted(&w, name) && text_append(&w, "\nentry ", 7u) &&
         text_append_quoted(&w, entry_key) && text_append(&w, "\n", 1u);
    if (ok) ok = write_text_globals(&w, manager);
    for (i = 0; ok && i < page_count; ++i) {
        const char *page_key = NULL;
        size_t count = 0u;
        size_t j;
        if (uim_page_get_key(manager, pages[i], &page_key) != uim_OK ||
            !page_key || !page_key[0] ||
            uim_page_get_node_count(manager, pages[i], &count) != uim_OK ||
            count > UIR_PROJECT_NODE_LIMIT) {
            ok = false;
            break;
        }
        ok = text_appendf(&w, "page %u ", (unsigned)count) &&
             text_append_quoted(&w, page_key) && text_append(&w, "\n", 1u);
        for (j = 0; ok && j < count; ++j) {
            uim_NodeInfo info;
            if (uim_page_get_node_at(manager, pages[i], j, &info) != uim_OK) {
                ok = false;
                break;
            }
            ok = write_text_node(&w, manager, info.id);
        }
    }
    if (!ok) {
        free(w.data);
        return uim_ERR_STATE;
    }
    *out_data = w.data;
    *out_size = w.size;
    return uim_OK;
}

uim_Result uir_project_save(const UIM_ComponentManager *manager,
                            const uim_PageId *pages,
                            size_t page_count,
                            const char *path) {
    char name[UIR_PROJECT_NAME_CAP];
    unsigned char *data = NULL;
    size_t size = 0u;
    uim_Result result;
    if (!manager || !pages || page_count == 0u || page_count > UIR_PROJECT_PAGE_LIMIT ||
        !path || !path[0]) return uim_ERR_ARGUMENT;
    project_name_from_path(path, name, sizeof(name));
    result = uir_project_serialize(manager, pages, page_count, name, &data, &size);
    if (result != uim_OK) return result;
    if (!write_atomic(path, data, size)) {
        free(data);
        return uim_ERR_STATE;
    }
    free(data);
    return uim_OK;
}

static void free_load_pages(LoadPage *pages, size_t page_count) {
    size_t i;
    if (!pages) return;
    for (i = 0; i < page_count; ++i) {
        uint32_t j;
        free(pages[i].key);
        if (pages[i].nodes) {
            for (j = 0; j < pages[i].node_count; ++j) {
                LoadNode *node = &pages[i].nodes[j];
                uint32_t k;
                free(node->key);
                free(node->parent);
                if (node->config_owned) free((void *)node->config);
                if (node->components) {
                    for (k = 0; k < node->component_count; ++k) {
                        LoadComponent *component = &node->components[k];
                        uint32_t p;
                        for (p = 0; p < component->property_count; ++p)
                            value_free(&component->properties[p].value);
                        free(component->properties);
                    }
                    free(node->components);
                }
            }
            free(pages[i].nodes);
        }
    }
    free(pages);
}

static int find_node_index(const LoadPage *page, const char *key) {
    uint32_t i;
    if (!page || !key) return -1;
    for (i = 0; i < page->node_count; ++i) {
        if (strcmp(page->nodes[i].key, key) == 0) return (int)i;
    }
    return -1;
}

static uim_Result apply_page(UIM_ComponentManager *manager, LoadPage *page, bool entry) {
    uint32_t i;
    if (!page || !page->key || !page->key[0] || page->node_count == 0u ||
        page->node_count > UIR_PROJECT_NODE_LIMIT) return uim_ERR_RANGE;
    if (uim_page_create(manager, page->key, &page->page_id) != uim_OK) return uim_ERR_STATE;
    for (i = 0; i < page->node_count; ++i) {
        LoadNode *node = &page->nodes[i];
        uim_NodeDesc desc;
        memset(&desc, 0, sizeof(desc));
        desc.page_id = page->page_id;
        desc.parent_id = 0u;
        desc.stable_key = node->key;
        desc.component_id = node->component_id;
        desc.local_rect = node->rect;
        desc.space = node->space;
        desc.transform = node->transform;
        desc.layout = node->layout;
        desc.visible = node->visible;
        desc.enabled = node->enabled;
        desc.z_order = node->z_order;
        desc.action_id = node->action_id;
        desc.child_order = node->child_order;
        desc.component_config = node->config;
        desc.component_config_size = node->config_size;
        if (uim_node_create(manager, &desc, &node->node_id) != uim_OK) return uim_ERR_STATE;
    }
    for (i = 0; i < page->node_count; ++i) {
        LoadNode *node = &page->nodes[i];
        if (node->parent[0]) {
            int parent_index = find_node_index(page, node->parent);
            size_t steps = 0u;
            int cursor = (int)i;
            if (parent_index < 0 || parent_index == (int)i) return uim_ERR_STATE;
            while (cursor >= 0 && page->nodes[cursor].parent[0]) {
                cursor = find_node_index(page, page->nodes[cursor].parent);
                if (cursor == (int)i || ++steps > page->node_count) return uim_ERR_STATE;
            }
        }
    }
    for (i = 0; i < page->node_count; ++i) {
        LoadNode *node = &page->nodes[i];
        if (node->parent[0]) {
            int parent_index = find_node_index(page, node->parent);
            if (parent_index < 0 || parent_index == (int)i) return uim_ERR_STATE;
            if (uim_node_set_parent(manager, node->node_id,
                                    page->nodes[parent_index].node_id) != uim_OK)
                return uim_ERR_STATE;
        }
    }
    for (i = 0; i < page->node_count; ++i) {
        LoadNode *node = &page->nodes[i];
        uint32_t j;
        for (j = 0; j < node->component_count; ++j) {
            LoadComponent *component = &node->components[j];
            uim_ComponentInstanceInfo info = component->info;
            uint32_t p;
            info.node_id = node->node_id;
            if (uim_component_instance_restore(manager, &info) != uim_OK) return uim_ERR_STATE;
            for (p = 0; p < component->property_count; ++p) {
                if (uim_component_instance_set_property(manager, info.instance_id,
                                                        component->properties[p].property_id,
                                                        component->properties[p].value) != uim_OK)
                    return uim_ERR_STATE;
            }
        }
        if (node->has_binding) {
            uim_BindingDesc binding = node->binding;
            uim_BindingId binding_id = 0u;
            binding.node_id = node->node_id;
            if (uim_binding_restore(manager, &binding, &binding_id) != uim_OK ||
                binding_id == 0u) return uim_ERR_STATE;
        }
    }
    for (i = 0; i < page->node_count; ++i) {
        if (page->nodes[i].parent[0] == '\0') {
            page->root_id = page->nodes[i].node_id;
            break;
        }
    }
    if (page->root_id == 0u) page->root_id = page->nodes[0].node_id;
    return uim_OK;
}

/* ---- 纯文本工程格式 ----
 * 一行一个语义单元：page / node / comp / prop / bind / global。
 * node 行包含全部节点字段，改一个属性只会改对应行，git diff 可读。
 * 二进制旧格式仍然保留读取路径，由 uir_project_deserialize 判断 UIRJ 后第 5 字节。*/

#define UIR_TEXT_TOKEN_LIMIT 64

typedef struct TextLine {
    char *tokens[UIR_TEXT_TOKEN_LIMIT];
    int count;
} TextLine;

static bool text_line_split(char *line, TextLine *out) {
    char *p = line;
    if (!line || !out) return false;
    out->count = 0;
    while (*p) {
        char *start;
        char *write;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        if (!*p || *p == '#') break;
        if (out->count >= UIR_TEXT_TOKEN_LIMIT) return false;
        if (*p == '"') {
            start = p;
            write = p;
            ++p;
            while (*p && *p != '"') {
                if (*p == '\\') {
                    ++p;
                    switch (*p) {
                    case 'n': *write++ = '\n'; ++p; break;
                    case 'r': *write++ = '\r'; ++p; break;
                    case 't': *write++ = '\t'; ++p; break;
                    case '"': *write++ = '"'; ++p; break;
                    case '\\': *write++ = '\\'; ++p; break;
                    case 'x':
                        if (isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
                            char hex[3] = { p[1], p[2], '\0' };
                            *write++ = (char)strtoul(hex, NULL, 16);
                            p += 3;
                        } else {
                            return false;
                        }
                        break;
                    default: return false;
                    }
                } else {
                    *write++ = *p++;
                }
            }
            if (*p != '"') return false;
            ++p;
            *write = '\0';
            out->tokens[out->count++] = start;
        } else {
            start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
            if (*p) *p++ = '\0';
            out->tokens[out->count++] = start;
        }
    }
    return out->count > 0;
}

static bool text_parse_u32(const char *s, uint32_t *out) {
    char *end = NULL;
    unsigned long long v;
    if (!s || !s[0] || !out) return false;
    errno = 0;
    v = strtoull(s, &end, 10);
    if (errno || !end || *end || v > UINT32_MAX) return false;
    *out = (uint32_t)v;
    return true;
}

static bool text_parse_u8(const char *s, uint8_t *out) {
    uint32_t value = 0u;
    if (!out || !text_parse_u32(s, &value) || value > 255u) return false;
    *out = (uint8_t)value;
    return true;
}

static bool text_parse_u64(const char *s, uint64_t *out) {    char *end = NULL;
    unsigned long long v;
    if (!s || !s[0] || !out) return false;
    errno = 0;
    v = strtoull(s, &end, 10);
    if (errno || !end || *end) return false;
    *out = (uint64_t)v;
    return true;
}

static bool text_parse_float(const char *s, float *out) {
    char *end = NULL;
    double v;
    if (!s || !s[0] || !out) return false;
    errno = 0;
    v = strtod(s, &end);
    if (errno || !end || *end || !isfinite(v)) return false;
    *out = (float)v;
    return true;
}

static bool text_parse_double(const char *s, double *out) {
    char *end = NULL;
    if (!s || !s[0] || !out) return false;
    errno = 0;
    *out = strtod(s, &end);
    return errno == 0 && end && *end == '\0' && isfinite(*out);
}

static bool text_parse_value(const TextLine *line, int type_index, uim_Value *out) {
    const char *type;
    if (!line || !out || type_index < 0 || type_index >= line->count) return false;
    type = line->tokens[type_index];
    memset(out, 0, sizeof(*out));
    if (strcmp(type, "none") == 0) {
        out->type = uim_VALUE_NONE;
        return type_index + 1 == line->count;
    }
    if (type_index + 1 >= line->count) return false;
    if (strcmp(type, "bool") == 0) {
        if (strcmp(line->tokens[type_index + 1], "0") == 0) out->as.boolean = false;
        else if (strcmp(line->tokens[type_index + 1], "1") == 0) out->as.boolean = true;
        else return false;
        out->type = uim_VALUE_BOOL;
        return true;
    }
    if (strcmp(type, "int") == 0) {
        char *end = NULL;
        long long v;
        errno = 0;
        v = strtoll(line->tokens[type_index + 1], &end, 10);
        if (errno || !end || *end) return false;
        out->type = uim_VALUE_INT64;
        out->as.integer = (int64_t)v;
        return true;
    }
    if (strcmp(type, "float") == 0) {
        out->type = uim_VALUE_FLOAT64;
        return text_parse_double(line->tokens[type_index + 1], &out->as.real);
    }
    if (strcmp(type, "string") == 0) {
        const char *s = line->tokens[type_index + 1];
        size_t n = strlen(s);
        char *copy = (char *)malloc(n + 1u);
        if (!copy) return false;
        if (n) memcpy(copy, s, n);
        copy[n] = '\0';
        out->type = uim_VALUE_STRING;
        out->as.string.data = copy;
        out->as.string.size = n;
        return true;
    }
    return false;
}

static bool text_parse_hex(const char *s, const unsigned char **out_data, uint32_t *out_size) {
    size_t length;
    size_t i;
    unsigned char *bytes;
    if (!s || !out_data || !out_size) return false;
    *out_data = NULL;
    *out_size = 0u;
    if (strcmp(s, "-") == 0) return true;
    length = strlen(s);
    if (length == 0u || length % 2u != 0u || length / 2u > UINT32_MAX) return false;
    bytes = (unsigned char *)malloc(length / 2u);
    if (!bytes) return false;
    for (i = 0; i < length / 2u; ++i) {
        char hex[3] = { s[i * 2u], s[i * 2u + 1u], '\0' };
        if (!isxdigit((unsigned char)hex[0]) || !isxdigit((unsigned char)hex[1])) {
            free(bytes);
            return false;
        }
        bytes[i] = (unsigned char)strtoul(hex, NULL, 16);
    }
    *out_data = bytes;
    *out_size = (uint32_t)(length / 2u);
    return true;
}

static uim_Result text_project_load(const unsigned char *bytes, size_t size,
                                    UIM_ComponentManager **out_manager,
                                    uim_PageId *out_page, uim_NodeId *out_root,
                                    char *out_name, size_t name_cap) {
    LoadProperty *globals = NULL;
    size_t global_count = 0u;
    LoadPage *pages = NULL;
    size_t page_count = 0u;
    size_t total_nodes = 0u;
    char *project_name = NULL;
    char *entry_page = NULL;
    LoadPage *page = NULL;
    LoadNode *node = NULL;
    LoadComponent *component = NULL;
    UIM_ComponentManager *manager = NULL;
    uim_Result result = uim_ERR_RANGE;
    char *copy;
    char *line;
    char *cursor;
    int line_number = 0;

    if (!bytes || size < 8u || memcmp(bytes, "UIRJ ", 5u) != 0 ||
        !out_manager || !out_page || !out_root || !out_name || name_cap == 0u)
        return uim_ERR_ARGUMENT;
    *out_manager = NULL;
    *out_page = uim_PAGE_INVALID;
    *out_root = uim_NODE_INVALID;
    out_name[0] = '\0';

    copy = (char *)malloc(size + 1u);
    if (!copy) return uim_ERR_NOMEM;
    memcpy(copy, bytes, size);
    copy[size] = '\0';
    cursor = copy;
    while (cursor && *cursor) {
        TextLine line_data;
        char *newline = strchr(cursor, '\n');
        if (newline) *newline = '\0';
        line = cursor;
        cursor = newline ? newline + 1 : NULL;
        ++line_number;
        if (*line == '\0' || *line == '#') continue;
        if (!text_line_split(line, &line_data)) {
            result = uim_ERR_RANGE;
            goto done;
        }
        if (line_number == 1) {
            uint32_t version = 0u;
            if (line_data.count != 2 || strcmp(line_data.tokens[0], "UIRJ") != 0 ||
                !text_parse_u32(line_data.tokens[1], &version) ||
                version < UIR_PROJECT_VERSION_MIN || version > UIR_PROJECT_VERSION) {
                result = uim_ERR_UNSUPPORTED;
                goto done;
            }
            continue;
        }
        if (strcmp(line_data.tokens[0], "name") == 0) {
            if (line_data.count != 2 || project_name) {
                result = uim_ERR_RANGE;
                goto done;
            }
            project_name = strdup(line_data.tokens[1]);
            if (!project_name) { result = uim_ERR_NOMEM; goto done; }
            continue;
        }
        if (strcmp(line_data.tokens[0], "entry") == 0) {
            if (line_data.count != 2 || entry_page) {
                result = uim_ERR_RANGE;
                goto done;
            }
            entry_page = strdup(line_data.tokens[1]);
            if (!entry_page) { result = uim_ERR_NOMEM; goto done; }
            continue;
        }
        if (strcmp(line_data.tokens[0], "global") == 0) {
            LoadProperty *grown;
            if (line_data.count != 5 || page_count != 0u) {
                result = uim_ERR_RANGE;
                goto done;
            }
            if (global_count >= UIR_PROJECT_NODE_LIMIT) { result = uim_ERR_RANGE; goto done; }
            grown = (LoadProperty *)realloc(globals, (global_count + 1u) * sizeof(*globals));
            if (!grown) { result = uim_ERR_NOMEM; goto done; }
            globals = grown;
            memset(&globals[global_count], 0, sizeof(globals[global_count]));
            if (!text_parse_u32(line_data.tokens[1], &globals[global_count].property_id) ||
                !text_parse_value(&line_data, 3, &globals[global_count].value)) {
                result = uim_ERR_RANGE;
                goto done;
            }
            globals[global_count].stable_key = strdup(line_data.tokens[2]);
            if (!globals[global_count].stable_key) { result = uim_ERR_NOMEM; goto done; }
            if (!globals[global_count].stable_key[0]) { result = uim_ERR_RANGE; goto done; }
            ++global_count;
            continue;
        }
        if (strcmp(line_data.tokens[0], "page") == 0) {
            uint32_t count = 0u;
            LoadPage *grown;
            if (line_data.count != 3 || page_count >= UIR_PROJECT_PAGE_LIMIT ||
                !text_parse_u32(line_data.tokens[1], &count) ||
                count == 0u || count > UIR_PROJECT_NODE_LIMIT ||
                total_nodes > UIR_PROJECT_NODE_LIMIT - count) {
                result = uim_ERR_RANGE;
                goto done;
            }
            grown = (LoadPage *)realloc(pages, (page_count + 1u) * sizeof(*pages));
            if (!grown) { result = uim_ERR_NOMEM; goto done; }
            pages = grown;
            page = &pages[page_count];
            memset(page, 0, sizeof(*page));
            page->key = strdup(line_data.tokens[2]);
            if (!page->key || !page->key[0]) { result = page->key ? uim_ERR_RANGE : uim_ERR_NOMEM; goto done; }
            page->node_count = count;
            page->nodes = (LoadNode *)calloc(count, sizeof(*page->nodes));
            if (!page->nodes) { result = uim_ERR_NOMEM; goto done; }
            for (size_t k = 0; k < count; ++k) {
                page->nodes[k].space = uim_SPACE_2D;
                page->nodes[k].transform.scale[0] = 1.0f;
                page->nodes[k].transform.scale[1] = 1.0f;
                page->nodes[k].transform.scale[2] = 1.0f;
            }
            total_nodes += count;
            ++page_count;
            node = NULL;
            component = NULL;
            continue;
        }
        if (!page) { result = uim_ERR_RANGE; goto done; }
        if (strcmp(line_data.tokens[0], "node") == 0) {
            uint32_t index;
            uint32_t component_count = 0u;
            uint32_t z_order = 0u;
            uint8_t layout_enabled = 0u;
            uint8_t visible = 0u;
            uint8_t enabled = 0u;
            uint8_t space = 0u;
            for (index = 0; index < page->node_count; ++index) {
                if (page->nodes[index].key == NULL) break;
            }
            if (index >= page->node_count || line_data.count != 34) {
                result = uim_ERR_RANGE;
                goto done;
            }
            node = &page->nodes[index];
            component = NULL;
            node->key = strdup(line_data.tokens[1]);
            node->parent = strdup(line_data.tokens[2]);
            if (!node->key || !node->parent || !node->key[0]) {
                result = uim_ERR_NOMEM;
                goto done;
            }
            if (!text_parse_u32(line_data.tokens[3], &node->component_id) ||
                !text_parse_float(line_data.tokens[4], &node->rect.x) ||
                !text_parse_float(line_data.tokens[5], &node->rect.y) ||
                !text_parse_float(line_data.tokens[6], &node->rect.width) ||
                !text_parse_float(line_data.tokens[7], &node->rect.height) ||
                !text_parse_u8(line_data.tokens[8], &space) ||
                !text_parse_float(line_data.tokens[9], &node->transform.position[0]) ||
                !text_parse_float(line_data.tokens[10], &node->transform.position[1]) ||
                !text_parse_float(line_data.tokens[11], &node->transform.position[2]) ||
                !text_parse_float(line_data.tokens[12], &node->transform.rotation[0]) ||
                !text_parse_float(line_data.tokens[13], &node->transform.rotation[1]) ||
                !text_parse_float(line_data.tokens[14], &node->transform.rotation[2]) ||
                !text_parse_float(line_data.tokens[15], &node->transform.scale[0]) ||
                !text_parse_float(line_data.tokens[16], &node->transform.scale[1]) ||
                !text_parse_float(line_data.tokens[17], &node->transform.scale[2]) ||
                !text_parse_u8(line_data.tokens[18], &layout_enabled) ||
                !text_parse_float(line_data.tokens[19], &node->layout.anchor_min_x) ||
                !text_parse_float(line_data.tokens[20], &node->layout.anchor_min_y) ||
                !text_parse_float(line_data.tokens[21], &node->layout.anchor_max_x) ||
                !text_parse_float(line_data.tokens[22], &node->layout.anchor_max_y) ||
                !text_parse_float(line_data.tokens[23], &node->layout.offset_left) ||
                !text_parse_float(line_data.tokens[24], &node->layout.offset_top) ||
                !text_parse_float(line_data.tokens[25], &node->layout.offset_right) ||
                !text_parse_float(line_data.tokens[26], &node->layout.offset_bottom) ||
                !text_parse_u8(line_data.tokens[27], &visible) ||
                !text_parse_u8(line_data.tokens[28], &enabled) ||
                !text_parse_u32(line_data.tokens[29], &z_order) ||
                !text_parse_u32(line_data.tokens[30], &node->action_id) ||
                !text_parse_u32(line_data.tokens[31], &node->child_order) ||
                !text_parse_u32(line_data.tokens[33], &component_count) ||
                component_count > UIR_PROJECT_COMPONENT_LIMIT ||
                node->rect.width < 0.0f || node->rect.height < 0.0f ||
                layout_enabled > 1u || visible > 1u || enabled > 1u || space > 1u) {
                result = uim_ERR_RANGE;
                goto done;
            }
            node->space = (uim_Space)space;
            node->layout.enabled = layout_enabled != 0u;
            node->visible = visible != 0u;
            node->enabled = enabled != 0u;
            node->z_order = (int32_t)z_order;
            node->component_count = component_count;
            node->component_loaded = 0u;
            if (!text_parse_hex(line_data.tokens[32], &node->config, &node->config_size)) {
                result = uim_ERR_RANGE;
                goto done;
            }
            node->config_owned = node->config != NULL;
            if (component_count) {
                node->components = (LoadComponent *)calloc(component_count, sizeof(*node->components));
                if (!node->components) { result = uim_ERR_NOMEM; goto done; }
            }
            continue;
        }
        if (strcmp(line_data.tokens[0], "comp") == 0) {
            uint32_t property_count = 0u;
            uint32_t index;
            if (!node || line_data.count != 5 || node->component_loaded >= node->component_count) {
                result = uim_ERR_RANGE;
                goto done;
            }
            index = node->component_loaded;
            component = &node->components[index];
            if (!text_parse_u32(line_data.tokens[1], &component->info.type_id) ||
                !text_parse_u32(line_data.tokens[2], &component->info.schema_version) ||
                !text_parse_u64(line_data.tokens[3], &component->info.instance_id) ||
                !text_parse_u32(line_data.tokens[4], &property_count) ||
                property_count > UIR_PROJECT_COMPONENT_LIMIT) {
                result = uim_ERR_RANGE;
                goto done;
            }
            component->property_count = property_count;
            if (property_count) {
                component->properties = (LoadProperty *)calloc(property_count, sizeof(*component->properties));
                if (!component->properties) { result = uim_ERR_NOMEM; goto done; }
            }
            node->component_loaded++;
            continue;
        }
        if (strcmp(line_data.tokens[0], "prop") == 0) {
            uint32_t property_id = 0u;
            LoadProperty *prop;
            if (!component || line_data.count < 4 ||
                component->property_loaded >= component->property_count) {
                result = uim_ERR_RANGE;
                goto done;
            }
            prop = &component->properties[component->property_loaded];
            if (!text_parse_u32(line_data.tokens[1], &property_id) ||
                !text_parse_value(&line_data, 2, &prop->value)) {
                result = uim_ERR_RANGE;
                goto done;
            }
            prop->property_id = property_id;
            component->property_loaded++;
            continue;
        }
        if (strcmp(line_data.tokens[0], "bind") == 0) {
            uint32_t value_type = 0u, direction = 0u, initial_sync = 0u, state = 0u;
            if (!node || line_data.count != 6 || node->has_binding) {
                result = uim_ERR_RANGE;
                goto done;
            }
            node->binding.node_id = 0u;
            if (!text_parse_u32(line_data.tokens[1], &node->binding.property_id) ||
                !text_parse_u32(line_data.tokens[2], &value_type) ||
                !text_parse_u32(line_data.tokens[3], &direction) ||
                !text_parse_u32(line_data.tokens[4], &initial_sync) ||
                !text_parse_u32(line_data.tokens[5], &state) ||
                value_type > (uint32_t)uim_VALUE_STRING ||
                direction > (uint32_t)uim_BIND_READ_WRITE ||
                initial_sync > (uint32_t)uim_SYNC_INITIAL_ONLY ||
                state > (uint32_t)uim_BINDING_BROKEN) {
                result = uim_ERR_RANGE;
                goto done;
            }
            node->binding.value_type = (uim_ValueType)value_type;
            node->binding.direction = (uim_BindingDirection)direction;
            node->binding.initial_sync = (uim_SyncPolicy)initial_sync;
            node->binding.state = (uim_BindingState)state;
            node->has_binding = true;
            continue;
        }
        result = uim_ERR_RANGE;
        goto done;
    }

    if (!entry_page || page_count == 0u) { result = uim_ERR_RANGE; goto done; }
    for (size_t i = 0; i < page_count; ++i) {
        for (size_t j = 0; j < pages[i].node_count; ++j) {
            LoadNode *n = &pages[i].nodes[j];
            if (!n->key || n->component_loaded != n->component_count) {
                result = uim_ERR_RANGE;
                goto done;
            }
            for (size_t k = 0; k < n->component_count; ++k) {
                if (n->components[k].property_loaded != n->components[k].property_count) {
                    result = uim_ERR_RANGE;
                    goto done;
                }
            }
        }
    }
    {
        bool entry_found = false;
        UIM_ComponentManagerConfig config;
        memset(&config, 0, sizeof(config));
        config.initial_page_capacity = page_count;
        config.initial_node_capacity = total_nodes ? total_nodes : 1u;
        if (uim_component_manager_create(&config, &manager) != uim_OK) {
            result = uim_ERR_NOMEM;
            goto done;
        }
        for (size_t i = 0; i < global_count; ++i) {
            if (uim_property_register_named(manager, globals[i].property_id,
                                            globals[i].stable_key, globals[i].value) != uim_OK) {
                result = uim_ERR_STATE;
                goto done;
            }
        }
        for (size_t i = 0; i < page_count; ++i) {
            bool entry = strcmp(pages[i].key, entry_page) == 0;
            if (entry) entry_found = true;
            if (apply_page(manager, &pages[i], entry) != uim_OK) {
                result = uim_ERR_STATE;
                goto done;
            }
        }
        if (!entry_found) { result = uim_ERR_NOT_FOUND; goto done; }
        for (size_t i = 0; i < page_count; ++i) {
            if (strcmp(pages[i].key, entry_page) == 0) {
                *out_page = pages[i].page_id;
                *out_root = pages[i].root_id;
                break;
            }
        }
        if (*out_page == uim_PAGE_INVALID || *out_root == uim_NODE_INVALID) {
            result = uim_ERR_STATE;
            goto done;
        }
        snprintf(out_name, name_cap, "%s", project_name ? project_name : "");
        *out_manager = manager;
        manager = NULL;
        result = uim_OK;
    }
done:
    if (manager) uim_component_manager_destroy(manager);
    free_load_pages(pages, page_count);
    for (size_t i = 0; i < global_count; ++i) {
        free(globals[i].stable_key);
        value_free(&globals[i].value);
    }
    free(globals);
    free(project_name);
    free(entry_page);
    free(copy);
    return result;
}


uim_Result uir_project_deserialize(const unsigned char *bytes,
                                   size_t size,
                                   UIM_ComponentManager **out_manager,
                                   uim_PageId *out_page,
                                   uim_NodeId *out_root,
                                   char *out_name, size_t name_cap) {
    /* Single-file container: project + opaque scene section. UIM only needs
     * the project prefix; the runtime may separately decode the scene. */
    if (bytes && size >= 11u && memcmp(bytes, "ZCSRPROJ 1\n", 11u) == 0) {
        const unsigned char *p = bytes + 11u;
        const unsigned char *end = bytes + size;
        const unsigned char *line_end;
        char line[128];
        size_t project_size = 0u;
        size_t n;

        line_end = (const unsigned char *)memchr(p, '\n', (size_t)(end - p));
        if (!line_end) return uim_ERR_RANGE;
        n = (size_t)(line_end - p);
        if (n >= sizeof(line)) return uim_ERR_RANGE;
        memcpy(line, p, n);
        line[n] = '\0';
        if (sscanf(line, "project %zu", &project_size) != 1 || project_size == 0u)
            return uim_ERR_RANGE;
        p = line_end + 1;
        line_end = (const unsigned char *)memchr(p, '\n', (size_t)(end - p));
        if (!line_end) return uim_ERR_RANGE;
        n = (size_t)(line_end - p);
        if (n >= sizeof(line)) return uim_ERR_RANGE;
        memcpy(line, p, n);
        line[n] = '\0';
        {
            size_t scene_size = 0u;
            if (sscanf(line, "scene %zu", &scene_size) != 1) return uim_ERR_RANGE;
            (void)scene_size;
        }
        p = line_end + 1;
        if (project_size > (size_t)(end - p)) return uim_ERR_RANGE;
        return uir_project_deserialize(p, project_size, out_manager, out_page,
                                       out_root, out_name, name_cap);
    }
    if (bytes && size >= 5u && memcmp(bytes, "UIRJ ", 5u) == 0) {
        return text_project_load(bytes, size, out_manager, out_page, out_root,
                                 out_name, name_cap);
    }
    ProjectReader reader;
    char *project_name = NULL;
    char *entry_page = NULL;
    uint32_t version = 0u;
    uint32_t page_count = 0u;
    uint32_t global_count = 0u;
    LoadProperty *global_props = NULL;
    LoadPage *pages = NULL;
    UIM_ComponentManager *manager = NULL;
    UIM_ComponentManagerConfig config;
    bool entry_found = false;
    uint32_t i;
    size_t total_nodes = 0u;
    uim_Result result = uim_ERR_STATE;

    if (!bytes || size < 16u || !out_manager || !out_page || !out_root ||
        !out_name || name_cap == 0u) return uim_ERR_ARGUMENT;
    *out_manager = NULL;
    *out_page = uim_PAGE_INVALID;
    *out_root = 0u;
    out_name[0] = '\0';
    if (memcmp(bytes, "UIRJ", 4u) != 0) return uim_ERR_RANGE;
    version = (uint32_t)bytes[4] | ((uint32_t)bytes[5] << 8) |
              ((uint32_t)bytes[6] << 16) | ((uint32_t)bytes[7] << 24);
    {
        uint32_t payload_size = (uint32_t)bytes[8] | ((uint32_t)bytes[9] << 8) |
                                ((uint32_t)bytes[10] << 16) | ((uint32_t)bytes[11] << 24);
        uint32_t stored;
        if (version < UIR_PROJECT_VERSION_MIN || version > UIR_PROJECT_VERSION ||
            payload_size > size - 16u || size != (size_t)payload_size + 16u)
            return uim_ERR_RANGE;
        stored = (uint32_t)bytes[12u + payload_size] |
                 ((uint32_t)bytes[13u + payload_size] << 8) |
                 ((uint32_t)bytes[14u + payload_size] << 16) |
                 ((uint32_t)bytes[15u + payload_size] << 24);
        if (project_hash(bytes + 12u, payload_size) != stored) return uim_ERR_STALE;
        reader.data = bytes + 12u;
        reader.size = payload_size;
        reader.at = 0u;
    }
    if (!get_string(&reader, &project_name) || !get_string(&reader, &entry_page)) {
        result = uim_ERR_RANGE;
        goto done;
    }
    if (version >= 2u) {
        if (!get_u32(&reader, &global_count) || global_count > UIR_PROJECT_NODE_LIMIT) {
            result = uim_ERR_RANGE;
            goto done;
        }
        if (global_count) {
            global_props = (LoadProperty *)calloc(global_count, sizeof(*global_props));
            if (!global_props) {
                result = uim_ERR_NOMEM;
                goto done;
            }
            for (i = 0; i < global_count; ++i) {
                if (!get_u32(&reader, &global_props[i].property_id) ||
                    !get_string(&reader, &global_props[i].stable_key) ||
                    !get_value(&reader, &global_props[i].value)) {
                    result = uim_ERR_RANGE;
                    goto done;
                }
                if (!global_props[i].stable_key[0]) {
                    result = uim_ERR_RANGE;
                    goto done;
                }
            }
        }
    }
    if (!get_u32(&reader, &page_count) || page_count == 0u ||
        page_count > UIR_PROJECT_PAGE_LIMIT) {
        result = uim_ERR_RANGE;
        goto done;
    }
    pages = (LoadPage *)calloc(page_count, sizeof(*pages));
    if (!pages) {
        result = uim_ERR_NOMEM;
        goto done;
    }
    for (i = 0; i < page_count; ++i) {
        uint32_t count;
        uint32_t j;
        if (!get_string(&reader, &pages[i].key) || !get_u32(&reader, &count) ||
            count == 0u || count > UIR_PROJECT_NODE_LIMIT ||
            total_nodes > UIR_PROJECT_NODE_LIMIT - count) {
            result = uim_ERR_RANGE;
            goto done;
        }
        pages[i].node_count = count;
        pages[i].nodes = (LoadNode *)calloc(count, sizeof(*pages[i].nodes));
        if (!pages[i].nodes) {
            result = uim_ERR_NOMEM;
            goto done;
        }
        for (j = 0; j < count; ++j) {
            LoadNode *node = &pages[i].nodes[j];
            uint8_t layout_enabled;
            uint8_t visible;
            uint8_t enabled;
            uint8_t space = uim_SPACE_2D;
            uint32_t config_size;
            uint32_t component_count;
            uint32_t z_order;
            uint32_t k;
            node->space = uim_SPACE_2D;
            node->transform.scale[0] = node->transform.scale[1] = node->transform.scale[2] = 1.0f;
            if (!get_string(&reader, &node->key) || !get_string(&reader, &node->parent) ||
                !get_u32(&reader, &node->component_id) ||
                !get_f32(&reader, &node->rect.x) || !get_f32(&reader, &node->rect.y) ||
                !get_f32(&reader, &node->rect.width) || !get_f32(&reader, &node->rect.height) ||
                (version >= 3u &&
                 (!get_u8(&reader, &space) ||
                  !get_f32(&reader, &node->transform.position[0]) ||
                  !get_f32(&reader, &node->transform.position[1]) ||
                  !get_f32(&reader, &node->transform.position[2]) ||
                  !get_f32(&reader, &node->transform.rotation[0]) ||
                  !get_f32(&reader, &node->transform.rotation[1]) ||
                  !get_f32(&reader, &node->transform.rotation[2]) ||
                  !get_f32(&reader, &node->transform.scale[0]) ||
                  !get_f32(&reader, &node->transform.scale[1]) ||
                  !get_f32(&reader, &node->transform.scale[2]))) ||
                !get_u8(&reader, &layout_enabled) ||
                !get_f32(&reader, &node->layout.anchor_min_x) ||
                !get_f32(&reader, &node->layout.anchor_min_y) ||
                !get_f32(&reader, &node->layout.anchor_max_x) ||
                !get_f32(&reader, &node->layout.anchor_max_y) ||
                !get_f32(&reader, &node->layout.offset_left) ||
                !get_f32(&reader, &node->layout.offset_top) ||
                !get_f32(&reader, &node->layout.offset_right) ||
                !get_f32(&reader, &node->layout.offset_bottom) ||
                !get_u8(&reader, &visible) || !get_u8(&reader, &enabled) ||
                !get_u32(&reader, &z_order) ||
                !get_u32(&reader, &node->action_id) ||
                !get_u32(&reader, &node->child_order) ||
                !get_u32(&reader, &config_size) ||
                !reader_bytes(&reader, &node->config, config_size) ||
                !get_u32(&reader, &component_count) ||
                component_count > UIR_PROJECT_COMPONENT_LIMIT) {
                result = uim_ERR_RANGE;
                goto done;
            }
            if (!node->key[0] || node->rect.width < 0.0f || node->rect.height < 0.0f ||
                layout_enabled > 1u || visible > 1u || enabled > 1u ||
                space > (uint8_t)uim_SPACE_3D) {
                result = uim_ERR_RANGE;
                goto done;
            }
            node->space = (uim_Space)space;
            node->layout.enabled = layout_enabled != 0u;
            node->visible = visible != 0u;
            node->enabled = enabled != 0u;
            node->z_order = (int32_t)z_order;
            node->config_size = config_size;
            node->component_count = component_count;
            if (component_count) {
                node->components = (LoadComponent *)calloc(component_count, sizeof(*node->components));
                if (!node->components) {
                    result = uim_ERR_NOMEM;
                    goto done;
                }
                for (k = 0; k < component_count; ++k) {
                    LoadComponent *component = &node->components[k];
                    uint32_t property_count = 0u;
                    uint32_t p;
                    if (!get_u32(&reader, &component->info.type_id) ||
                        !get_u32(&reader, &component->info.schema_version) ||
                        !get_u64(&reader, &component->info.instance_id)) {
                        result = uim_ERR_RANGE;
                        goto done;
                    }
                    if (version >= 2u) {
                        if (!get_u32(&reader, &property_count) ||
                            property_count > UIR_PROJECT_NODE_LIMIT) {
                            result = uim_ERR_RANGE;
                            goto done;
                        }
                    }
                    component->property_count = property_count;
                    if (property_count) {
                        component->properties = (LoadProperty *)calloc(property_count,
                                                                        sizeof(*component->properties));
                        if (!component->properties) {
                            result = uim_ERR_NOMEM;
                            goto done;
                        }
                        for (p = 0; p < property_count; ++p) {
                            if (!get_u32(&reader, &component->properties[p].property_id) ||
                                !get_value(&reader, &component->properties[p].value)) {
                                result = uim_ERR_RANGE;
                                goto done;
                            }
                        }
                    }
                }
            }
            if (version >= 2u) {
                uint8_t has_binding;
                if (!get_u8(&reader, &has_binding) || has_binding > 1u) {
                    result = uim_ERR_RANGE;
                    goto done;
                }
                node->has_binding = has_binding != 0u;
                if (node->has_binding) {
                    uint8_t value_type;
                    uint8_t direction;
                    uint8_t initial_sync;
                    uint8_t state;
                    if (!get_u32(&reader, &node->binding.property_id) ||
                        !get_u8(&reader, &value_type) || value_type > uim_VALUE_STRING ||
                        !get_u8(&reader, &direction) ||
                        !get_u8(&reader, &initial_sync) ||
                        !get_u8(&reader, &state)) {
                        result = uim_ERR_RANGE;
                        goto done;
                    }
                    node->binding.value_type = (uim_ValueType)value_type;
                    node->binding.direction = (uim_BindingDirection)direction;
                    node->binding.initial_sync = (uim_SyncPolicy)initial_sync;
                    node->binding.state = (uim_BindingState)state;
                }
            }
        }
        total_nodes += count;
    }
    if (reader.at != reader.size) {
        result = uim_ERR_RANGE;
        goto done;
    }
    memset(&config, 0, sizeof(config));
    config.initial_page_capacity = page_count;
    config.initial_node_capacity = total_nodes ? total_nodes : 1u;
    if (uim_component_manager_create(&config, &manager) != uim_OK) {
        result = uim_ERR_NOMEM;
        goto done;
    }
    for (i = 0; i < global_count; ++i) {
        if (uim_property_register_named(manager, global_props[i].property_id,
                                        global_props[i].stable_key,
                                        global_props[i].value) != uim_OK) {
            result = uim_ERR_STATE;
            goto done;
        }
    }
    for (i = 0; i < page_count; ++i) {
        bool entry = entry_page && strcmp(pages[i].key, entry_page) == 0;
        if (entry) entry_found = true;
        if (apply_page(manager, &pages[i], entry) != uim_OK) {
            result = uim_ERR_STATE;
            goto done;
        }
    }
    if (!entry_found) {
        result = uim_ERR_NOT_FOUND;
        goto done;
    }
    for (i = 0; i < page_count; ++i) {
        if (entry_page && strcmp(pages[i].key, entry_page) == 0) {
            *out_page = pages[i].page_id;
            *out_root = pages[i].root_id;
            break;
        }
    }
    if (*out_page == uim_PAGE_INVALID || *out_root == 0u) {
        result = uim_ERR_STATE;
        goto done;
    }
    snprintf(out_name, name_cap, "%s", project_name ? project_name : "");
    *out_manager = manager;
    manager = NULL;
    result = uim_OK;
done:
    if (manager) uim_component_manager_destroy(manager);
    free_load_pages(pages, page_count);
    if (global_props) {
        for (i = 0; i < global_count; ++i) {
            free(global_props[i].stable_key);
            value_free(&global_props[i].value);
        }
        free(global_props);
    }
    free(project_name);
    free(entry_page);
    return result;
}

uim_Result uir_project_load(const char *path,
                            UIM_ComponentManager **out_manager,
                            uim_PageId *out_page,
                            uim_NodeId *out_root,
                            char *out_root_path, size_t root_cap,
                            char *out_name, size_t name_cap) {
    unsigned char *bytes = NULL;
    size_t size = 0u;
    uim_Result result;
    if (!path || !path[0] || !out_manager || !out_page || !out_root ||
        !out_root_path || root_cap == 0u || !out_name || name_cap == 0u)
        return uim_ERR_ARGUMENT;
    *out_manager = NULL;
    *out_page = uim_PAGE_INVALID;
    *out_root = 0u;
    out_name[0] = '\0';
    if (!read_whole_file(path, &bytes, &size)) return uim_ERR_NOT_FOUND;
    result = uir_project_deserialize(bytes, size, out_manager, out_page, out_root,
                                     out_name, name_cap);
    free(bytes);
    if (result != uim_OK) return result;
    if (!out_name[0]) project_name_from_path(path, out_name, name_cap);
    path_dirname(path, out_root_path, root_cap);
    return uim_OK;
}

uim_Result uir_project_read_scene_bytes(const char *path,
                                        unsigned char **out_data,
                                        size_t *out_size) {
    unsigned char *bytes = NULL;
    size_t size = 0u;
    const unsigned char *p;
    const unsigned char *end;
    const unsigned char *line_end;
    char line[128];
    size_t n;
    size_t project_size = 0u;
    size_t scene_size = 0u;
    unsigned char *scene = NULL;

    if (!path || !path[0] || !out_data || !out_size) return uim_ERR_ARGUMENT;
    *out_data = NULL;
    *out_size = 0u;
    if (!read_whole_file(path, &bytes, &size)) return uim_ERR_NOT_FOUND;
    if (size < 11u || memcmp(bytes, "ZCSRPROJ 1\n", 11u) != 0) {
        free(bytes);
        return uim_ERR_NOT_FOUND;
    }
    p = bytes + 11u;
    end = bytes + size;
    line_end = (const unsigned char *)memchr(p, '\n', (size_t)(end - p));
    if (!line_end) { free(bytes); return uim_ERR_RANGE; }
    n = (size_t)(line_end - p);
    if (n >= sizeof(line)) { free(bytes); return uim_ERR_RANGE; }
    memcpy(line, p, n);
    line[n] = '\0';
    if (sscanf(line, "project %zu", &project_size) != 1 || project_size == 0u) {
        free(bytes);
        return uim_ERR_RANGE;
    }
    p = line_end + 1;
    line_end = (const unsigned char *)memchr(p, '\n', (size_t)(end - p));
    if (!line_end) { free(bytes); return uim_ERR_RANGE; }
    n = (size_t)(line_end - p);
    if (n >= sizeof(line)) { free(bytes); return uim_ERR_RANGE; }
    memcpy(line, p, n);
    line[n] = '\0';
    if (sscanf(line, "scene %zu", &scene_size) != 1) {
        free(bytes);
        return uim_ERR_RANGE;
    }
    p = line_end + 1;
    if (project_size > (size_t)(end - p) ||
        scene_size > (size_t)(end - p) - project_size) {
        free(bytes);
        return uim_ERR_RANGE;
    }
    if (scene_size) {
        scene = (unsigned char *)malloc(scene_size);
        if (!scene) { free(bytes); return uim_ERR_NOMEM; }
        memcpy(scene, p + project_size, scene_size);
    }
    free(bytes);
    *out_data = scene;
    *out_size = scene_size;
    return uim_OK;
}

uim_Result uir_project_build_package(const char *project_path,
                                     const char *editor_exe,
                                     const char *output_root,
                                     const char *project_root,
                                     const uir_AssetRegistry *assets,
                                     char *out_package, size_t package_cap,
                                     char *out_project, size_t project_cap,
                                     char *out_player, size_t player_cap) {
    char name[UIR_PROJECT_NAME_CAP];
    char dist[UIR_PROJECT_PATH_CAP];
    char package[UIR_PROJECT_PATH_CAP];
    char project_dest[UIR_PROJECT_PATH_CAP];
    char player_dest[UIR_PROJECT_PATH_CAP];
    char assets_dest[UIR_PROJECT_PATH_CAP];
    char manifest[UIR_PROJECT_PATH_CAP];
    char source[UIR_PROJECT_PATH_CAP];
    char dest[UIR_PROJECT_PATH_CAP];
    char assets_source[UIR_PROJECT_PATH_CAP];
    FILE *mf = NULL;
    uint32_t i;
    int n;
    if (!project_path || !project_path[0] || !editor_exe || !editor_exe[0] ||
        !project_root || !project_root[0] || !out_package || !out_project || !out_player)
        return uim_ERR_ARGUMENT;
    if (!file_exists(project_path) || !file_exists(editor_exe)) return uim_ERR_NOT_FOUND;
    project_name_from_path(project_path, name, sizeof(name));
    if (!output_root || !output_root[0]) output_root = project_root;
    if (!path_join(dist, sizeof(dist), output_root, "dist") ||
        !path_join(package, sizeof(package), dist, name)) return uim_ERR_BUFFER;
    if (!ensure_directory(package)) return uim_ERR_STATE;
    if (!path_join(project_dest, sizeof(project_dest), package, "project.uireproj") ||
        !path_join(player_dest, sizeof(player_dest), package, "player.exe") ||
        !path_join(assets_dest, sizeof(assets_dest), package, ".uirassets") ||
        !path_join(manifest, sizeof(manifest), package, "manifest.txt"))
        return uim_ERR_BUFFER;
    if (!copy_file(project_path, project_dest) || !copy_file(editor_exe, player_dest))
        return uim_ERR_STATE;
    if (!path_join(assets_source, sizeof(assets_source), project_root, ".uirassets"))
        return uim_ERR_BUFFER;
    if (file_exists(assets_source) && !copy_file(assets_source, assets_dest))
        return uim_ERR_STATE;
    if (assets) {
        for (i = 0; i < assets->count; ++i) {
            const uir_AssetRecord *record = &assets->records[i];
            char asset_dir[UIR_PROJECT_PATH_CAP];
            char relative_dest[UIR_PROJECT_PATH_CAP];
            if (!record->relative_path[0]) continue;
            if (!path_join(source, sizeof(source), project_root, record->relative_path)) return uim_ERR_BUFFER;
            /* Keep the registry path stable inside the package: .uirassets records
             * relative paths, so assets must land at the same relative location. */
            n = snprintf(relative_dest, sizeof(relative_dest), "%s", record->relative_path);
            if (n <= 0 || (size_t)n >= sizeof(relative_dest)) return uim_ERR_BUFFER;
            if (!path_join(dest, sizeof(dest), package, relative_dest)) return uim_ERR_BUFFER;
            path_dirname(dest, asset_dir, sizeof(asset_dir));
            if (!ensure_directory(asset_dir)) return uim_ERR_STATE;
            if (!file_exists(source)) return uim_ERR_NOT_FOUND;
            if (!copy_file(source, dest)) return uim_ERR_STATE;
        }
    }
    mf = fopen(manifest, "wb");
    if (!mf) return uim_ERR_STATE;
    fprintf(mf, "format=%u\n", UIR_PROJECT_VERSION);
    fprintf(mf, "project=%s\n", name);
    fprintf(mf, "project_file=project.uireproj\n");
    fprintf(mf, "player=player.exe\n");
    fprintf(mf, "assets=%u\n", assets ? (unsigned int)assets->count : 0u);
    if (fclose(mf) != 0) return uim_ERR_STATE;
    if (!path_join(out_package, package_cap, package, "")) return uim_ERR_BUFFER;
    n = snprintf(out_project, project_cap, "%s", project_dest);
    if (n <= 0 || (size_t)n >= project_cap) return uim_ERR_BUFFER;
    n = snprintf(out_player, player_cap, "%s", player_dest);
    if (n <= 0 || (size_t)n >= player_cap) return uim_ERR_BUFFER;
    return uim_OK;
}
