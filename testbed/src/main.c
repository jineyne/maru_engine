#include "engine.h"
#include "engine_context.h"
#include "renderer/renderer.h"
#include "material/material.h"
#include "asset/mesh.h"
#include "asset/sprite.h"
#include "asset/texture_manager.h"
#include "asset/asset.h"
#include "math/math.h"
#include "math/proj.h"
#include "platform/window.h"
#include "rhi/rhi.h"  /* For rhi_vertex_attr_t - TODO: hide this */
#include "mem/mem_diag.h"

#if WIN32
#include <crtdbg.h>
#endif

extern engine_context_t g_ctx;

/* App resources */
static mesh_handle_t g_triangle_mesh = MESH_HANDLE_INVALID;
static sprite_handle_t g_sprite = SPRITE_HANDLE_INVALID;
static material_handle_t g_triangle_material = MAT_HANDLE_INVALID;
static material_handle_t g_sprite_material = MAT_HANDLE_INVALID;
static texture_handle_t g_texture = TEX_HANDLE_INVALID;

static void app_update_triangle_mvp(int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device || g_triangle_material == MAT_HANDLE_INVALID) return;

    static float s_angle = 0.0f;
    s_angle += 0.8f * (PI / 180.0f);

    rhi_capabilities_t caps;
    g_ctx.active_rhi->get_capabilities(g_ctx.active_device, &caps);

    if (h <= 0) h = 1;
    float aspect = (float)w / (float)h;

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

static void app_update_sprite_mvp(int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device || g_sprite_material == MAT_HANDLE_INVALID) return;

    rhi_capabilities_t caps;
    g_ctx.active_rhi->get_capabilities(g_ctx.active_device, &caps);

    mat4_t P, MVP;
    ortho_from_caps(&caps, 0.0f, (float)w, 0.0f, (float)h, -1.0f, 1.0f, P);

    mat4_identity(MVP);
    vec3_t position = {100, 200, 0.0f};
    glm_translate(MVP, position);

    mat4_mul(P, MVP, MVP);

    material_set_mat4(g_sprite_material, "uMVP", (const float*)MVP);
}

static void app_render(renderer_t *R, void *user) {
    /* Draw 2D sprite first (no depth write) */
    if (g_sprite != SPRITE_HANDLE_INVALID) {
        renderer_bind_material(R, g_sprite_material);
        renderer_draw_sprite(R, g_sprite, 0.0f, 0.0f);
    }

    /* Draw 3D triangle */
    renderer_bind_material(R, g_triangle_material);
    renderer_draw_mesh(R, g_triangle_mesh);
}

static void app_init(void) {
    /* Create triangle mesh */
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
        ERROR("failed to create triangle mesh");
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
        ERROR("triangle material create failed");
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
        ERROR("sprite material create failed");
        return;
    }

    /* Load texture */
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

    int cw = 0, ch = 0;
    platform_window_get_size(g_ctx.window, &cw, &ch);
    app_update_triangle_mvp(cw, ch);
    app_update_sprite_mvp(cw, ch);
}

static void app_update(void) {
    int cur_w = 0, cur_h = 0;
    platform_window_get_size(g_ctx.window, &cur_w, &cur_h);
    app_update_triangle_mvp(cur_w, cur_h);
    app_update_sprite_mvp(cur_w, cur_h);
}

static void app_shutdown(void) {
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
}

int main(void) {
    if (maru_engine_init("../../config/engine.json") != 0) {
        return 1;
    }

    app_init();

    /* Set render callback */
    extern renderer_t g_renderer;
    renderer_set_scene(&g_renderer, app_render, NULL);

    while (maru_engine_tick()) {
        app_update();
    }

    app_shutdown();
    maru_engine_shutdown();

    mem_dump_leaks();

#if WIN32
    _CrtDumpMemoryLeaks();
#endif

    return 0;
}
