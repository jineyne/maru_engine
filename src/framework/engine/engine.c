#include "engine.h"

#include "engine_context.h"

#include "config.h"
#include "rhi/rhi.h"
#include "renderer/renderer.h"

#include <string.h>

#include "asset/asset.h"
#include "asset/importer.h"
#include "asset/texture_manager.h"
#include "asset/mesh.h"
#include "asset/sprite.h"
#include "material/material.h"
#include "platform/window.h"
#include "platform/input.h"

/* External importer vtables */
extern const asset_importer_vtable_t g_texture_importer;
extern const asset_importer_vtable_t g_mesh_obj_importer;

#include "time.h"
#include "mem/mem_diag.h"
#include "mem/mem_frame.h"

typedef struct boot_prof_s {
    uint64_t t0;
    uint64_t last;
} boot_prof_t;

static inline boot_prof_t boot_prof_begin(void) {
    uint64_t now = time_now_ms();
    return (boot_prof_t){.t0 = now, .last = now};
}

static inline void boot_prof_step(boot_prof_t *p, const char *tag) {
    uint64_t now = time_now_ms();
    INFO("[boot] %-22s : %4llu ms", tag, (unsigned long long)(now - p->last));
    p->last = now;
}

static inline void boot_prof_total(boot_prof_t *p) {
    uint64_t now = time_now_ms();
    INFO("[boot] %-22s : %4llu ms", "TOTAL INIT", (unsigned long long)(now - p->t0));
}

engine_context_t g_ctx;

static int initialized = 0;

renderer_t g_renderer;

static rhi_swapchain_t *g_swapchain = NULL;
static rhi_render_target_t *g_back_rt = NULL;

static const char *map_backend_to_regname(const char *backend) {
    if (!backend) return "gl";
    if (strcmp(backend, "dx") == 0 || strcmp(backend, "dx11") == 0) return "dx11";
    if (strcmp(backend, "gles") == 0) return "gles";
    return "gl";
}

static const rhi_backend map_backend_to_backend_key(const char *backend) {
    if (!backend) return RHI_BACKEND_GL;
    if (strcmp(backend, "dx") == 0 || strcmp(backend, "dx11") == 0) return RHI_BACKEND_DX11;
    if (strcmp(backend, "gles") == 0) return RHI_BACKEND_GLES;
    return RHI_BACKEND_GL;
}

int maru_engine_init(const char *config_path) {
    boot_prof_t prof = boot_prof_begin();

    if (initialized) {
        return MARU_OK;
    }

    INFO("maru init");

    asset_init(NULL);
    boot_prof_step(&prof, "asset_init");

    /* Initialize asset importer system */
    asset_importer_init();
    asset_importer_register(&g_texture_importer);
    asset_importer_register(&g_mesh_obj_importer);
    boot_prof_step(&prof, "importer_init");

    maru_config_t cfg = {0};
    if (config_load(config_path, &cfg) != MARU_OK) {
        ERROR("Failed to load config: %s", config_path);
        return MARU_ERR_PARSE;
    }

    boot_prof_step(&prof, "config_load");

    engine_context_init(&g_ctx);
    boot_prof_step(&prof, "engine_context_init");

    engine_context_load_rhi(&g_ctx, "maru-gl", "gl");
#if defined(_WIN32)
    engine_context_load_rhi(&g_ctx, "maru-dx11", "dx11");
#endif
    boot_prof_step(&prof, "register_rhi_backends");

    const char *want = map_backend_to_regname(cfg.graphics_backend);

    int win_w = (cfg.gfx_width > 0) ? cfg.gfx_width : 1280;
    int win_h = (cfg.gfx_height > 0) ? cfg.gfx_height : 720;
    int win_vsync = (cfg.gfx_vsync != 0);

    g_ctx.window = platform_window_create(NULL, win_w, win_h, win_vsync);
    if (!g_ctx.window) {
        ERROR("Failed to create platform window");
        engine_context_shutdown(&g_ctx);
        return MARU_ERR_INVALID;
    }
    boot_prof_step(&prof, "window_create");

    rhi_device_desc_t device_desc = (rhi_device_desc_t){0};
    device_desc.backend = map_backend_to_backend_key(want);
    device_desc.native_window = g_ctx.window->handle;
    device_desc.width = win_w;
    device_desc.height = win_h;
    device_desc.vsync = win_vsync;

    if (engine_context_select_rhi(&g_ctx, want, &device_desc) != MARU_OK) {
        if (strcmp(want, "gl") != 0 && engine_context_select_rhi(&g_ctx, "gl", &device_desc) == MARU_OK) {
            INFO("RHI fallback to gl");
        } else if (strcmp(want, "gles") != 0 && engine_context_select_rhi(&g_ctx, "gles", &device_desc) == MARU_OK) {
            INFO("RHI fallback to gles");
        } else {
            ERROR("no usable RHI backend (wanted: %s)", want);
            engine_context_shutdown(&g_ctx);
            return MARU_ERR_INVALID;
        }
    }
    boot_prof_step(&prof, "rhi_select+device_create");

    g_swapchain = g_ctx.active_rhi->get_swapchain(g_ctx.active_device);
    g_back_rt = g_ctx.active_rhi->get_backbuffer_rt(g_ctx.active_device);
    boot_prof_step(&prof, "swapchain+backbuffer");

    frame_arena_init(8 * 1024 * 1024, 2);

    if (texture_manager_init(512) != 0) {
        FATAL("texture manager initialize failed");
        return MARU_ERR_INVALID;
    }

    if (mesh_system_init(256) != 0) {
        FATAL("mesh system initialize failed");
        return MARU_ERR_INVALID;
    }

    if (sprite_system_init(256) != 0) {
        FATAL("sprite system initialize failed");
        return MARU_ERR_INVALID;
    }

    if (material_system_init(128) != 0) {
        FATAL("material system initialize failed");
        return MARU_ERR_INVALID;
    }
    boot_prof_step(&prof, "asset_systems_init");

    int cw = 0, ch = 0;
    platform_window_get_size(g_ctx.window, &cw, &ch);
    renderer_init(&g_renderer, g_ctx.active_rhi, g_ctx.active_device, cw, ch);
    boot_prof_step(&prof, "renderer_init");

    config_free(&cfg);
    boot_prof_total(&prof);


    initialized = 1;
    return MARU_OK;
}

static size_t frame_count = 0;
bool maru_engine_tick(void) {
    if (!initialized) return false;

    frame_arena_begin(frame_count++);

    if (platform_should_close(g_ctx.window)) return false;
    platform_poll_events();
    input_update(g_ctx.window);

    int cur_w = 0, cur_h = 0;
    platform_window_get_size(g_ctx.window, &cur_w, &cur_h);
    if (g_ctx.active_rhi && g_ctx.active_rhi->resize) {
        g_ctx.active_rhi->resize(g_ctx.active_device, cur_w, cur_h);

        if (g_ctx.active_rhi->get_backbuffer_rt) {
            rhi_render_target_t *new_rt = g_ctx.active_rhi->get_backbuffer_rt(g_ctx.active_device);
            if (new_rt) {
                g_back_rt = new_rt;
            }
        }
    }

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    renderer_resize(&g_renderer, cur_w, cur_h);
    renderer_render(&g_renderer);

    if (!g_swapchain) g_swapchain = rhi->get_swapchain(g_ctx.active_device);
    rhi->present(g_swapchain);
    return true;
}

void maru_engine_shutdown(void) {
    if (!initialized) return;

    material_system_shutdown();
    sprite_system_shutdown();
    mesh_system_shutdown();
    texture_manager_shutdown();
    frame_arena_shutdown();

    renderer_shutdown(&g_renderer);
    engine_context_shutdown(&g_ctx);

    /* Shutdown asset importer system */
    asset_importer_shutdown();

    INFO("maru shutdown");

    initialized = 0;
}
