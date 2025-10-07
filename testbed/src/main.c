#include "engine.h"
#include "engine_context.h"
#include "renderer/renderer.h"
#include "renderer/render_object.h"
#include "material/material.h"
#include "asset/mesh.h"
#include "asset/sprite.h"
#include "asset/texture_manager.h"
#include "asset/asset.h"
#include "asset/importer.h"
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

/* Transform */
static transform_t g_triangle_transform;

/* Render objects */
static render_object_handle_t g_triangle_obj = RENDER_OBJECT_HANDLE_INVALID;

static void app_update_triangle_mvp(int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device || g_triangle_material == MAT_HANDLE_INVALID) return;

    static float s_angle = 0.0f;
    s_angle += 0.8f * (PI / 180.0f);

    rhi_capabilities_t caps;
    g_ctx.active_rhi->get_capabilities(g_ctx.active_device, &caps);

    if (h <= 0) h = 1;
    float aspect = (float) w / (float) h;

    /* Projection */
    mat4_t P;
    perspective_from_caps(&caps, 60.0f * (PI / 180.0f), aspect, 0.1f, 100.0f, P);

    /* View */
    mat4_t V;
    vec3_t eye = {0.f, 0.f, 2.5f};
    vec3_t at = {0.f, 0.f, 0.f};
    vec3_t up = {0.f, 1.f, 0.f};
    look_at(eye, at, up, V);

    /* Model - Use transform system */
    transform_set_euler(&g_triangle_transform, 0.0f, s_angle, 0.0f);
    const mat4 *M = transform_get_local_matrix(&g_triangle_transform);

    /* MVP composition */
    mat4_t PV, MVP;
    mat4_mul(P, V, PV);
    mat4_mul(PV, *M, MVP);
    mat4_to_backend_order(&caps, MVP, MVP);

    material_set_mat4(g_triangle_material, "uMVP", (const float*) MVP);
}

static void app_update_sprite_mvp(int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device || g_sprite_material == MAT_HANDLE_INVALID) return;

    rhi_capabilities_t caps;
    g_ctx.active_rhi->get_capabilities(g_ctx.active_device, &caps);

    mat4_t P, MVP;
    ortho_from_caps(&caps, 0.0f, (float) w, 0.0f, (float) h, -1.0f, 1.0f, P);

    mat4_identity(MVP);
    vec3_t position = {100, 200, 0.0f};
    glm_translate(MVP, position);

    mat4_mul(P, MVP, MVP);

    material_set_mat4(g_sprite_material, "uMVP", (const float*) MVP);
}

static void app_render(renderer_t *R, void *user) {
    /* Draw 2D sprite first (no depth write) */
    if (g_sprite != SPRITE_HANDLE_INVALID) {
        renderer_bind_material(R, g_sprite_material);
        renderer_draw_sprite(R, g_sprite, 0.0f, 0.0f);
    }

    /* Draw 3D triangle using render object */
    if (g_triangle_obj != RENDER_OBJECT_HANDLE_INVALID) {
        renderer_draw_object(R, g_triangle_obj);
    }
}

static void app_init(void) {
    /* Initialize render object system */
    render_object_system_init(128);

    /* Initialize transform */
    transform_init(&g_triangle_transform);
    vec3 pos = {0.0f, 0.0f, 0.0f};
    transform_set_position(&g_triangle_transform, pos);

    /* Load triangle mesh using asset importer */
    mesh_handle_t *mesh_h = (mesh_handle_t*)asset_import("mesh/triangle.obj", NULL);
    if (!mesh_h) {
        ERROR("failed to load triangle mesh from OBJ");
        return;
    }
    g_triangle_mesh = *mesh_h;
    MARU_FREE(mesh_h);

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
        .shader_path = "shader/sprite.hlsl",
        .vs_entry = "VSMain",
        .ps_entry = "PSMain"
    };
    g_sprite_material = material_create(&sprite_mat_desc);
    if (g_sprite_material == MAT_HANDLE_INVALID) {
        ERROR("sprite material create failed");
        return;
    }

    /* Load texture */
    g_texture = tex_create_from_file("texture/karina.jpg");
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

    /* Create render object for triangle */
    g_triangle_obj = render_object_create();
    if (g_triangle_obj != RENDER_OBJECT_HANDLE_INVALID) {
        render_object_set_mesh(g_triangle_obj, g_triangle_mesh);
        render_object_set_material(g_triangle_obj, g_triangle_material);
        render_object_set_transform(g_triangle_obj, &g_triangle_transform);
        render_object_set_visible(g_triangle_obj, 1);
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
    /* Cleanup render objects */
    if (g_triangle_obj != RENDER_OBJECT_HANDLE_INVALID) {
        render_object_destroy(g_triangle_obj);
        g_triangle_obj = RENDER_OBJECT_HANDLE_INVALID;
    }

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

    /* Shutdown render object system */
    render_object_system_shutdown();
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
