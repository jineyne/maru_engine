#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rhi_axis {
    RHI_AXIS_UP = 0,
    RHI_AXIS_DOWN = 1,
} rhi_axis;

typedef enum rhi_matrix_order {
    RHI_MATRIX_COLUMN_MAJOR = 0,
    RHI_MATRIX_ROW_MAJOR = 1,
} rhi_matrix_order;

typedef struct rhi_conventions {
    rhi_axis uv_yaxis;
    rhi_axis ndc_yaxis;
    rhi_matrix_order matrix_order;
} rhi_conventions_t;

typedef struct rhi_capabilities {
    float min_depth;
    float max_depth;
    rhi_conventions_t conventions;
} rhi_capabilities_t;

typedef struct rhi_device rhi_device_t;
typedef struct rhi_swapchain rhi_swapchain_t;
typedef struct rhi_buffer rhi_buffer_t;
typedef struct rhi_texture rhi_texture_t;
typedef struct rhi_shader rhi_shader_t;
typedef struct rhi_pipeline rhi_pipeline_t;
typedef struct rhi_cmd rhi_cmd_t;
typedef struct rhi_fence rhi_fence_t;

typedef enum rhi_backend {
    RHI_BACKEND_GL,
    RHI_BACKEND_GLES,
    RHI_BACKEND_DX11,
} rhi_backend;

typedef enum rhi_format {
    RHI_FMT_RGBA8,
    RHI_FMT_BGRA8,
    RHI_FMT_D24S8,
} rhi_format;

typedef enum rhi_stage_bits {
    RHI_STAGE_VS = 1 << 0,
    RHI_STAGE_PS = 1 << 1,
} rhi_stage_bits;

typedef struct rhi_device_desc {
    rhi_backend backend; /* informational */
    void *native_window; /* HWND / EGLSurface / etc */
    int width, height;
    bool vsync;
} rhi_device_desc_t;

typedef struct rhi_buffer_desc {
    size_t size;
    uint32_t usage; /* user-defined flags */
    uint32_t cpu; /* CPU access flags */
} rhi_buffer_desc_t;

typedef struct rhi_texture_desc {
    int width, height, mip_levels;
    rhi_format format;
    uint32_t usage; /* render target, sampled, etc. */
} rhi_texture_desc_t;

typedef struct rhi_shader_desc {
    const char *entry_vs;
    const char *entry_ps;
    const void *blob_vs;
    size_t blob_vs_size; /* DXBC / GLSL text / SPIR-V */
    const void *blob_ps;
    size_t blob_ps_size;
} rhi_shader_desc_t;

typedef struct rhi_pipeline_desc {
    rhi_shader_t *shader;
    //TODO: blend/depth/raster/vertex-layout
} rhi_pipeline_desc_t;

typedef struct rhi_binding {
    uint32_t binding; /* abstract slot */
    rhi_texture_t *texture; /* SRV */
    //TODO: buffer/sampler etc
} rhi_binding_t;

typedef struct rhi_dispatch {
    /* device */
    rhi_device_t * (*create_device)(const rhi_device_desc_t *);
    void (*destroy_device)(rhi_device_t *);

    /* swapchain */
    rhi_swapchain_t * (*get_swapchain)(rhi_device_t *);
    void (*present)(rhi_swapchain_t *);

    /* resources */
    rhi_buffer_t * (*create_buffer)(rhi_device_t *, const rhi_buffer_desc_t *, const void *initial);
    void (*destroy_buffer)(rhi_device_t *, rhi_buffer_t *);

    rhi_texture_t * (*create_texture)(rhi_device_t *, const rhi_texture_desc_t *, const void *initial);
    void (*destroy_texture)(rhi_device_t *, rhi_texture_t *);

    rhi_shader_t * (*create_shader)(rhi_device_t *, const rhi_shader_desc_t *);
    void (*destroy_shader)(rhi_device_t *, rhi_shader_t *);

    rhi_pipeline_t * (*create_pipeline)(rhi_device_t *, const rhi_pipeline_desc_t *);
    void (*destroy_pipeline)(rhi_device_t *, rhi_pipeline_t *);

    /* commands */
    rhi_cmd_t * (*begin_cmd)(rhi_device_t *);
    void (*end_cmd)(rhi_cmd_t *);
    void (*cmd_begin_render)(rhi_cmd_t *, rhi_texture_t *color, rhi_texture_t *depth, const float clear_rgba[4]);
    void (*cmd_end_render)(rhi_cmd_t *);

    void (*cmd_bind_pipeline)(rhi_cmd_t *, rhi_pipeline_t *);
    void (*cmd_bind_set)(rhi_cmd_t *, const rhi_binding_t *binds, int num, uint32_t stages_mask);

    void (*cmd_set_viewport_scissor)(rhi_cmd_t *, int x, int y, int w, int h);
    void (*cmd_set_vertex_buffer)(rhi_cmd_t *, int slot, rhi_buffer_t *);
    void (*cmd_set_index_buffer)(rhi_cmd_t *, rhi_buffer_t *);
    void (*cmd_draw)(rhi_cmd_t *, uint32_t vtx_count, uint32_t first, uint32_t inst_count);
    void (*cmd_draw_indexed)(rhi_cmd_t *, uint32_t idx_count, uint32_t first, uint32_t base_vtx, uint32_t inst_count);

    /* sync */
    rhi_fence_t * (*fence_create)(rhi_device_t *);
    void (*fence_wait)(rhi_fence_t *);
    void (*fence_destroy)(rhi_fence_t *);

    /* caps */
    void (*get_capabilities)(rhi_device_t *, rhi_capabilities_t *out_caps);
} rhi_dispatch_t;

typedef const rhi_dispatch_t * (*maru_rhi_entry_fn)(void);

#ifdef __cplusplus
}
#endif
