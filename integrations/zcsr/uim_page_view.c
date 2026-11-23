#include "uim_page_view.h"
#include "uim_zcsr_runtime_bridge.h"
#include "zcsr/ui.h"
#include "zcsr/ui_draw_v2.h"
#include "zcsr/ui_gl_render_v2.h"
#include "zcsr/assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UIPAGE_RESOURCE_CAP 32u
#define UIPAGE_ASSET_PATH_CAP 192u

typedef struct UIPageResource {
    char path[UIPAGE_ASSET_PATH_CAP];
    uint32_t key;
    zcsr_gl_texture texture;
} UIPageResource;

struct UIPageView {
    ZCSR_UI* ui;
    UIM_ZcsrRuntimeBridge bridge;
    UIM_ComponentManager* manager;
    uim_PageId page;
    float scale;
    int width;
    int height;
    size_t node_count;
    uint64_t structure_hash;
    bool ready;
    zcsr_gl_renderer* renderer;
    char project_root[512];
    UIPageResource resources[UIPAGE_RESOURCE_CAP];
    size_t resource_count;
};

static uint32_t resource_hash(const char* path) {
    uint32_t h = 2166136261u;
    while (path && *path) {
        h ^= (uint32_t)(unsigned char)*path++;
        h *= 16777619u;
    }
    return h ? h : 1u;
}

/* Host resolver: path -> stable key, loading the texture once. Failure is
 * cached as key with texture=0 so a bad asset cannot retry every frame; ZCSR
 * renders a missing-image placeholder for that key. */
static uint32_t page_resource_resolve(void* user, const char* asset_path) {
    UIPageView* view = (UIPageView*)user;
    size_t i;
    uint32_t key;
    if (view == NULL || asset_path == NULL || asset_path[0] == '\0') return 0u;
    if (strlen(asset_path) >= UIPAGE_ASSET_PATH_CAP) return 0u;
    key = resource_hash(asset_path);
    for (i = 0; i < view->resource_count; ++i)
        if (strcmp(view->resources[i].path, asset_path) == 0) return view->resources[i].key;
    if (view->resource_count >= UIPAGE_RESOURCE_CAP) return 0u;
    if (view->renderer == NULL) return 0u;
    (void)zcsr_assets_set_base_dir(view->project_root[0] ? view->project_root : NULL);
    {
        UIPageResource* res = &view->resources[view->resource_count++];
        zcsr_gl_texture texture = 0;
        char resolved[ZCSR_ASSET_PATH_CAP];
        FILE* file = NULL;
        long file_size = 0;
        unsigned char* encoded = NULL;
        void* arena = NULL;
        size_t encoded_cap = 0;
        size_t arena_cap = 0;

        /* The engine exposes only the caller-buffer decoder. Load the encoded
         * bytes once, size a decode arena from the file size with headroom,
         * upload, then release both scratch buffers. This is load-time work,
         * not a frame-path allocation. */
        if (zcsr_assets_resolve(asset_path, resolved, (int)sizeof(resolved)) &&
            (file = fopen(resolved, "rb")) != NULL) {
            if (fseek(file, 0, SEEK_END) == 0) file_size = ftell(file);
            if (file_size > 0) {
                encoded_cap = (size_t)file_size;
                arena_cap = encoded_cap * 16u + (4u * 1024u * 1024u);
                encoded = (unsigned char*)malloc(encoded_cap);
                arena = malloc(arena_cap);
                if (encoded && arena) {
                    (void)zcsr_assets_texture_with_buffers(view->renderer, asset_path,
                                                           encoded, encoded_cap,
                                                           arena, arena_cap,
                                                           &texture, NULL, NULL);
                }
            }
            fclose(file);
        }
        free(encoded);
        free(arena);
        snprintf(res->path, sizeof(res->path), "%s", asset_path);
        res->key = key;
        res->texture = texture; /* 0 = missing, still a stable key */
    }
    return key;
}

static uim_NodeId page_root(UIM_ComponentManager* manager, uim_PageId page) {
    size_t count = 0u;
    size_t i;
    if (uim_page_get_node_count(manager, page, &count) != uim_OK) return uim_NODE_INVALID;
    for (i = 0; i < count; ++i) {
        uim_NodeInfo info;
        if (uim_page_get_node_at(manager, page, i, &info) == uim_OK &&
            (info.parent_id == uim_NODE_INVALID || info.parent_id == 0u)) return info.id;
    }
    return uim_NODE_INVALID;
}

static uint64_t page_structure_hash(UIM_ComponentManager* manager, uim_PageId page) {
    size_t count = 0u;
    size_t i;
    uint64_t hash = 1469598103934665603ull;
    if (uim_page_get_node_count(manager, page, &count) != uim_OK) return 0u;
    for (i = 0; i < count; ++i) {
        uim_NodeInfo info;
        size_t components = 0u;
        if (uim_page_get_node_at(manager, page, i, &info) != uim_OK) return 0u;
        (void)uim_node_component_count(manager, info.id, &components);
        hash ^= (uint64_t)info.id; hash *= 1099511628211ull;
        hash ^= (uint64_t)info.parent_id; hash *= 1099511628211ull;
        hash ^= (uint64_t)info.component_id; hash *= 1099511628211ull;
        hash ^= (uint64_t)info.space; hash *= 1099511628211ull;
        hash ^= (uint64_t)info.child_order; hash *= 1099511628211ull;
        hash ^= (uint64_t)components; hash *= 1099511628211ull;
    }
    return hash;
}

UIPageView* uipage_view_create(void) {
    UIPageView* view = (UIPageView*)calloc(1, sizeof(*view));
    if (!view) return NULL;
    view->page = uim_PAGE_INVALID;
    view->scale = 1.0f;
    return view;
}

bool uipage_view_set_project_root(UIPageView* view, const char* root) {
    if (!view) return false;
    if (root == NULL) root = "";
    if (strlen(root) >= sizeof(view->project_root)) return false;
    snprintf(view->project_root, sizeof(view->project_root), "%s", root);
    return true;
}

static bool ui_create(UIPageView* view, UIM_ComponentManager* manager, int w, int h) {
    ZCSR_UIConfig cfg;
    if (view->ui) {
        zcsr_ui_destroy(view->ui);
        view->ui = NULL;
    }
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_nodes = 256u;
    cfg.event_queue_capacity = 64u;
    cfg.draw_command_capacity = 512u;
    cfg.viewport_width = (float)(w > 0 ? w : 800);
    cfg.viewport_height = (float)(h > 0 ? h : 600);
    if (zcsr_ui_create(&cfg, &view->ui) != ZCSR_OK) {
        view->ui = NULL;
        return false;
    }
    view->manager = manager;
    uim_zcsr_runtime_bridge_create(&view->bridge, view->ui, view->manager);
    uim_zcsr_runtime_bridge_set_resource_resolver(&view->bridge,
                                                   page_resource_resolve, view);
    view->ready = false;
    return true;
}

void uipage_view_destroy(UIPageView* view) {
    size_t i;
    if (!view) return;
    if (view->ui) zcsr_ui_destroy(view->ui);
    if (view->renderer) {
        for (i = 0; i < view->resource_count; ++i)
            if (view->resources[i].texture)
                zcsr_gl_texture_destroy(view->renderer, view->resources[i].texture);
    }
    free(view);
}

float uipage_view_scale(const UIPageView* view) {
    return view ? view->scale : 1.0f;
}

bool uipage_view_render(UIPageView* view, zcsr_gl_renderer* renderer,
                        UIM_ComponentManager* manager, uim_PageId page,
                        int x, int y, int w, int h) {
    uim_NodeId root;
    uim_NodeInfo root_info;
    size_t node_count = 0u;
    uint64_t structure_hash;
    float base_w = 1280.0f;
    float base_h = 800.0f;
    float scale;
    float ox;
    float oy;
    if (!view || !renderer || !manager || page == uim_PAGE_INVALID || w <= 0 || h <= 0)
        return false;
    view->renderer = renderer;
    if (view->project_root[0])
        (void)zcsr_assets_set_base_dir(view->project_root);

    root = page_root(manager, page);
    if (root == uim_NODE_INVALID) return false;
    if (uim_node_get(manager, root, &root_info) != uim_OK) return false;
    (void)uim_page_get_node_count(manager, page, &node_count);
    structure_hash = page_structure_hash(manager, page);
    if (root_info.local_rect.width > 0.0f && root_info.local_rect.height > 0.0f) {
        base_w = root_info.local_rect.width;
        base_h = root_info.local_rect.height;
    }
    scale = (float)w / base_w;
    if ((float)h / base_h < scale) scale = (float)h / base_h;
    if (scale <= 0.0f) scale = 1.0f;
    ox = (float)x + ((float)w - base_w * scale) * 0.5f;
    oy = (float)y + ((float)h - base_h * scale) * 0.5f;

    /* Node identity in ZCSR is assigned by the bridge, so a structural edit
     * (add/remove/reparent) needs a fresh mapping. Property edits reuse it. */
    if (!view->ui || view->manager != manager || view->page != page ||
        view->width != w || view->height != h || view->node_count != node_count ||
        view->structure_hash != structure_hash) {
        if (!ui_create(view, manager, w, h)) return false;
        view->page = page;
        view->width = w;
        view->height = h;
        view->node_count = node_count;
        view->structure_hash = structure_hash;
        uim_zcsr_runtime_bridge_set_view(&view->bridge, scale, ox, oy);
        (void)uim_zcsr_runtime_bridge_sync_page(&view->bridge, page);
        view->ready = true;
    } else {
        /* Document edits (x/y/w/h/color/text) must land in the next frame. */
        uim_zcsr_runtime_bridge_set_view(&view->bridge, scale, ox, oy);
        (void)uim_zcsr_runtime_bridge_sync_page(&view->bridge, page);
    }
    if (!view->ready) return false;

    (void)zcsr_ui_set_viewport(view->ui, (float)w, (float)h);
    (void)zcsr_ui_build_draw_list(view->ui);
    {
        zcsr_ui_gl_resource resources[UIPAGE_RESOURCE_CAP];
        size_t i;
        for (i = 0; i < view->resource_count; ++i) {
            resources[i].resource_key = view->resources[i].key;
            resources[i].texture = view->resources[i].texture;
        }
        zcsr_ui_gl_render_v2(renderer, view->ui, resources, view->resource_count);
    }
    view->scale = scale;
    return true;
}
