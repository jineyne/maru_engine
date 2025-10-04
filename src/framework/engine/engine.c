#include "engine.h"

#include "engine_context.h"

#include "config.h"
#include "rhi/rhi.h"
#include "renderer/renderer.h"

#include <string.h>

#include "asset/asset.h"
#include "asset/texture_manager.h"
#include "asset/mesh.h"
#include "asset/sprite.h"
#include "material/material.h"
#include "math/math.h"
#include "math/proj.h"
#include "platform/window.h"

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

static renderer_t g_renderer;

static rhi_swapchain_t *g_swapchain = NULL;
static rhi_render_target_t *g_back_rt = NULL;

static mesh_handle_t g_triangle_mesh = MESH_HANDLE_INVALID;
static sprite_handle_t g_sprite = SPRITE_HANDLE_INVALID;
static material_handle_t g_triangle_material = MAT_HANDLE_INVALID;
static material_handle_t g_sprite_material = MAT_HANDLE_INVALID;
static texture_handle_t g_texture = TEX_HANDLE_INVALID;

static void update_triangle_mvp_from_size(int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device || g_triangle_material == MAT_HANDLE_INVALID) return;
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    static float s_angle = 0.0f;
    s_angle += 0.8f * (PI / 180.0f);

    rhi_capabilities_t caps;
    rhi->get_capabilities(g_ctx.active_device, &caps);

    if (h <= 0) h = 1;
    float aspect = (float) w / (float) h;

    mat4_t P, V, M, R, PV, MVP;
    perspective_from_caps(&caps, 60.0f * (PI / 180.0f), aspect, 0.1f, 100.0f, P);

    vec3_t eye = {0.f, 0.f, 2.5f};
    vec3_t at = {0.f, 0.f, 0.f};
    vec3_t up = {0.f, 1.f, 0.f};
    look_at(eye, at, up, V);

    mat4_identity(M);
    glm_rotate_y(M, s_angle, R);
    mat4_mul(P, V, PV);
    mat4_mul(PV, R, MVP);
    mat4_to_backend_order(&caps, MVP, MVP);

    material_set_mat4(g_triangle_material, "uMVP", (const float*)MVP);
}

static void update_sprite_mvp_from_size(int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device || g_sprite_material == MAT_HANDLE_INVALID) return;
    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    rhi_capabilities_t caps;
    rhi->get_capabilities(g_ctx.active_device, &caps);

    mat4_t P, MVP;
    ortho_from_caps(&caps, 0.0f, (float)w, 0.0f, (float)h, -1.0f, 1.0f, P);

    mat4_identity(MVP);
    vec3_t position = {100, 200, 0.0f};
    glm_translate(MVP, position);

    mat4_mul(P, MVP, MVP);

    material_set_mat4(g_sprite_material, "uMVP", (const float*)MVP);
}

static void record_scene(rhi_cmd_t *cmd, void *user) {
    /* Draw 2D sprite first (no depth write) */
    if (g_sprite != SPRITE_HANDLE_INVALID) {
        renderer_bind_material(cmd, g_sprite_material);
        sprite_draw(cmd, g_sprite, 0.0f, 0.0f);
    }

    /* Draw 3D triangle */
    renderer_bind_material(cmd, g_triangle_material);
    mesh_bind(cmd, g_triangle_mesh);
    mesh_draw(cmd, g_triangle_mesh);
}

static void create_triangle_resources(void) {
    float vtx[] = {
        //          position             color     uv
        /* top   */ 0.0f, 0.5f, 0.0f, 1, 0, 0, 0.5f, 0.0f,
        /* right */ 0.5f, -0.5f, 0.0f, 0, 0, 1, 1.0f, 1.0f,
        /* left  */ -0.5f, -0.5f, 0.0f, 0, 1, 0, 0.0f, 1.0f,
    };

    uint32_t idx[] = {0, 1, 2};

    static const rhi_vertex_attr_t attrs[] = {
        {"POSITION", 0, RHI_VTX_F32x3, 0, 0},
        {"COLOR",    0, RHI_VTX_F32x3, 0, (uint32_t)(sizeof(float) * 3)},
        {"TEXCOORD", 0, RHI_VTX_F32x2, 0, (uint32_t)(sizeof(float) * 6)},
    };

    mesh_desc_t mesh_desc = {0};
    mesh_desc.vertices = vtx;
    mesh_desc.vertex_size = sizeof(float) * 8;
    mesh_desc.vertex_count = 3;
    mesh_desc.indices = idx;
    mesh_desc.index_count = 3;
    mesh_desc.attrs = attrs;
    mesh_desc.attr_count = 3;

    g_triangle_mesh = mesh_create(&mesh_desc);
    if (g_triangle_mesh == MESH_HANDLE_INVALID) {
        FATAL("failed to create triangle mesh");
        return;
    }

    /* Create triangle material (3D) */
    material_desc_t triangle_mat_desc = {
        .shader_path = "shader/default.hlsl",
        .vs_entry = "VSMain",
        .ps_entry = "PSMain"
    };
    g_triangle_material = material_create(&triangle_mat_desc);
    if (g_triangle_material == MAT_HANDLE_INVALID) {
        FATAL("triangle material create failed");
        return;
    }

    /* Create sprite material (2D) */
    material_desc_t sprite_mat_desc = {
        .shader_path = "shader/default.hlsl",
        .vs_entry = "VSMain",
        .ps_entry = "PSMain"
    };
    g_sprite_material = material_create(&sprite_mat_desc);
    if (g_sprite_material == MAT_HANDLE_INVALID) {
        FATAL("sprite material create failed");
        return;
    }

    int cw = 0, ch = 0;
    platform_window_get_size(g_ctx.window, &cw, &ch);
    update_triangle_mvp_from_size(cw, ch);
    update_sprite_mvp_from_size(cw, ch);

    asset_texture_opts_t opts = {
        .gen_mips = 1,
        .flip_y = 0,
        .force_rgba = 1
    };
    g_texture = tex_create_from_file("texture/karina.jpg", &opts);
    if (g_texture == TEX_HANDLE_INVALID) {
        ERROR("failed to load texture");
    } else {
        material_set_texture(g_triangle_material, "gAlbedo", g_texture);
        material_set_texture(g_sprite_material, "gAlbedo", g_texture);

        /* Create sprite for testing */
        sprite_desc_t sprite_desc = {
            .texture = g_texture,
            .width = 100.0f,
            .height = 100.0f
        };
        g_sprite = sprite_create(&sprite_desc);
        if (g_sprite == SPRITE_HANDLE_INVALID) {
            ERROR("failed to create sprite");
        }
    }
}

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

    create_triangle_resources();
    boot_prof_step(&prof, "create_triangle_resources");

    int cw = 0, ch = 0;
    platform_window_get_size(g_ctx.window, &cw, &ch);
    renderer_init(&g_renderer, g_ctx.active_rhi, g_ctx.active_device, cw, ch);
    boot_prof_step(&prof, "renderer_init");

    renderer_set_scene(&g_renderer, record_scene, NULL);
    boot_prof_step(&prof, "renderer_set_scene");

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

    update_triangle_mvp_from_size(cur_w, cur_h);
    update_sprite_mvp_from_size(cur_w, cur_h);

    const rhi_dispatch_t *rhi = g_ctx.active_rhi;

    renderer_resize(&g_renderer, cur_w, cur_h);
    renderer_render(&g_renderer);

    if (!g_swapchain) g_swapchain = rhi->get_swapchain(g_ctx.active_device);
    rhi->present(g_swapchain);
    return true;
}

void maru_engine_shutdown(void) {
    if (!initialized) return;

    if (g_sprite != SPRITE_HANDLE_INVALID) {
        sprite_destroy(g_sprite);
        g_sprite = SPRITE_HANDLE_INVALID;
    }

    if (g_triangle_mesh != MESH_HANDLE_INVALID) {
        mesh_destroy(g_triangle_mesh);
        g_triangle_mesh = MESH_HANDLE_INVALID;
    }

    if (g_triangle_material != MAT_HANDLE_INVALID) {
        material_destroy(g_triangle_material);
        g_triangle_material = MAT_HANDLE_INVALID;
    }

    if (g_sprite_material != MAT_HANDLE_INVALID) {
        material_destroy(g_sprite_material);
        g_sprite_material = MAT_HANDLE_INVALID;
    }

    if (g_texture != TEX_HANDLE_INVALID) {
        tex_destroy(g_texture);
        g_texture = TEX_HANDLE_INVALID;
    }

    material_system_shutdown();

    sprite_system_shutdown();

    mesh_system_shutdown();

    texture_manager_shutdown();

    frame_arena_shutdown();

    renderer_shutdown(&g_renderer);

    engine_context_shutdown(&g_ctx);

    INFO("maru shutdown");

    initialized = 0;
}
