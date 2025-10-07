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
#include "platform/input.h"
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

/* FPS Camera */
static vec3 g_camera_pos = {0.0f, 1.0f, 5.0f};
static float g_camera_yaw = PI;
static float g_camera_pitch = 0.0f;

static void app_update_camera(renderer_t *R, int w, int h) {
    if (!g_ctx.active_rhi || !g_ctx.active_device) return;

    if (input_mouse_button_pressed(INPUT_MOUSE_RIGHT)) {
        float dx, dy;
        input_mouse_delta(&dx, &dy);
        g_camera_yaw -= dx * 0.002f;
        g_camera_pitch -= dy * 0.002f;

        if (g_camera_pitch > PI / 2.0f - 0.01f) g_camera_pitch = PI / 2.0f - 0.01f;
        if (g_camera_pitch < -PI / 2.0f + 0.01f) g_camera_pitch = -PI / 2.0f + 0.01f;
    }

    vec3_t forward = {
        sinf(g_camera_yaw) * cosf(g_camera_pitch),
        sinf(g_camera_pitch),
        cosf(g_camera_yaw) * cosf(g_camera_pitch)
    };
    vec3_t right = {
        -cosf(g_camera_yaw),
        0.0f,
        sinf(g_camera_yaw)
    };

    float move_speed = 0.1f;
    if (input_key_pressed(INPUT_KEY_W)) {
        g_camera_pos[0] += forward[0] * move_speed;
        g_camera_pos[1] += forward[1] * move_speed;
        g_camera_pos[2] += forward[2] * move_speed;
    }
    if (input_key_pressed(INPUT_KEY_S)) {
        g_camera_pos[0] -= forward[0] * move_speed;
        g_camera_pos[1] -= forward[1] * move_speed;
        g_camera_pos[2] -= forward[2] * move_speed;
    }
    if (input_key_pressed(INPUT_KEY_A)) {
        g_camera_pos[0] -= right[0] * move_speed;
        g_camera_pos[2] -= right[2] * move_speed;
    }
    if (input_key_pressed(INPUT_KEY_D)) {
        g_camera_pos[0] += right[0] * move_speed;
        g_camera_pos[2] += right[2] * move_speed;
    }
    if (input_key_pressed(INPUT_KEY_SPACE)) g_camera_pos[1] += move_speed;
    if (input_key_pressed(INPUT_KEY_SHIFT)) g_camera_pos[1] -= move_speed;

    rhi_capabilities_t caps;
    g_ctx.active_rhi->get_capabilities(g_ctx.active_device, &caps);
    if (h <= 0) h = 1;
    float aspect = (float) w / (float) h;

    mat4_t P;
    perspective_from_caps(&caps, 60.0f * (PI / 180.0f), aspect, 0.1f, 100.0f, P);

    mat4_t V;
    vec3_t target;
    glm_vec3_add(g_camera_pos, forward, target);
    vec3_t up = {0.0f, 1.0f, 0.0f};
    look_at(g_camera_pos, target, up, V);

    /* Optional: Auto-rotate triangle for demo */
    static float s_angle = 0.0f;
    s_angle += 0.5f * (PI / 180.0f);
    transform_set_euler(&g_triangle_transform, 0.0f, s_angle, 0.0f);

    /* Set camera to renderer (MVP will be auto-calculated) */
    renderer_set_camera(R, (const float*)V, (const float*)P);
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

    /* Load cube mesh using asset importer */
    mesh_handle_t *mesh_h = (mesh_handle_t*)asset_import("mesh/cube.obj", NULL);
    if (!mesh_h) {
        ERROR("failed to load cube mesh from OBJ");
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

    /* Initial camera setup */
    extern renderer_t g_renderer;
    int cw = 0, ch = 0;
    platform_window_get_size(g_ctx.window, &cw, &ch);
    app_update_camera(&g_renderer, cw, ch);
    app_update_sprite_mvp(cw, ch);
}

static void app_update(void) {
    extern renderer_t g_renderer;
    int cur_w = 0, cur_h = 0;
    platform_window_get_size(g_ctx.window, &cur_w, &cur_h);
    app_update_camera(&g_renderer, cur_w, cur_h);
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
