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
typedef struct rhi_sampler rhi_sampler_t;
typedef struct rhi_shader rhi_shader_t;
typedef struct rhi_pipeline rhi_pipeline_t;
typedef struct rhi_render_target rhi_render_target_t;
typedef struct rhi_cmd rhi_cmd_t;
typedef struct rhi_fence rhi_fence_t;

typedef enum rhi_vertex_format {
    RHI_VTX_F32x1,
    RHI_VTX_F32x2,
    RHI_VTX_F32x3,
    RHI_VTX_F32x4,
    RHI_VTX_UNORM8x4,
} rhi_vertex_format;

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

typedef enum rhi_buffer_usage_bits {
    RHI_BUF_VERTEX = 1 << 0,
    RHI_BUF_INDEX = 1 << 1,
    RHI_BUF_CONST = 1 << 2,
    RHI_BUF_DYNAMIC = 1 << 16
} rhi_buffer_usage_bits;

typedef enum rhi_tex_usage_bits {
    RHI_TEX_USAGE_SAMPLED = 1 << 0,
    RHI_TEX_USAGE_RENDER_TARGET = 1 << 1,
    RHI_TEX_USAGE_DEPTH = 1 << 2,
    RHI_TEX_USAGE_GEN_MIPS = 1 << 3,
} rhi_tex_usage_bits;

typedef enum rhi_filter {
    RHI_FILTER_POINT,
    RHI_FILTER_LINEAR,
    RHI_FILTER_ANISOTROPIC
} rhi_filter;

typedef enum rhi_wrap {
    RHI_WRAP_CLAMP, RHI_WRAP_REPEAT, RHI_WRAP_MIRROR
} rhi_wrap;

typedef enum rhi_blend_factor {
    RHI_BLEND_ZERO,
    RHI_BLEND_ONE,
    RHI_BLEND_SRC_ALPHA,
    RHI_BLEND_INV_SRC_ALPHA,
    RHI_BLEND_DST_ALPHA,
    RHI_BLEND_INV_DST_ALPHA,
    RHI_BLEND_SRC_COLOR,
    RHI_BLEND_INV_SRC_COLOR,
    RHI_BLEND_DST_COLOR,
    RHI_BLEND_INV_DST_COLOR,
} rhi_blend_factor;

typedef enum rhi_blend_op {
    RHI_BLEND_ADD,
    RHI_BLEND_SUB,
    RHI_BLEND_REV_SUB,
    RHI_BLEND_MIN,
    RHI_BLEND_MAX,
} rhi_blend_op;

typedef enum rhi_fill_mode {
    RHI_FILL_SOLID,
    RHI_FILL_WIREFRAME
} rhi_fill_mode;

typedef enum rhi_cull_mode {
    RHI_CULL_NONE,
    RHI_CULL_FRONT,
    RHI_CULL_BACK
} rhi_cull_mode;

typedef enum rhi_cmp_func {
    RHI_CMP_NEVER,
    RHI_CMP_LESS,
    RHI_CMP_LEQUAL,
    RHI_CMP_EQUAL,
    RHI_CMP_GEQUAL,
    RHI_CMP_GREATER,
    RHI_CMP_NOTEQUAL,
    RHI_CMP_ALWAYS,
} rhi_cmp_func;

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

    size_t stride; /* for vertex buffer */
} rhi_buffer_desc_t;

typedef struct rhi_texture_desc {
    int width, height, mip_levels;
    rhi_format format;
    uint32_t usage; /* render target, sampled, etc. */
} rhi_texture_desc_t;

typedef struct {
    rhi_filter min_filter;
    rhi_filter mag_filter;
    int anisotropy;
    rhi_wrap wrap_u, wrap_v, wrap_w;
    float mip_bias;
} rhi_sampler_desc_t;

typedef struct rhi_shader_desc {
    const char *entry_vs;
    const char *entry_ps;
    const void *blob_vs;
    size_t blob_vs_size; /* DXBC / GLSL text / SPIR-V */
    const void *blob_ps;
    size_t blob_ps_size;
} rhi_shader_desc_t;

typedef struct rhi_blend_state {
    bool enable;
    rhi_blend_factor src_rgb, dst_rgb;
    rhi_blend_op op_rgb;
    rhi_blend_factor src_a, dst_a;
    rhi_blend_op op_a;
    uint8_t write_mask;
} rhi_blend_state_t;

typedef struct rhi_depthstencil_state {
    int depth_test_enable;
    int depth_write_enable;
    rhi_cmp_func depth_func;
} rhi_depthstencil_state_t;

typedef struct rhi_raster_state {
    rhi_fill_mode fill;
    rhi_cull_mode cull;
    int front_ccw; /* 1 = CCW°¡ front */
    float depth_bias;
    float slope_scaled_depth_bias;
} rhi_raster_state_t;

typedef struct rhi_vertex_attr {
    const char *semantic;
    uint32_t location;
    uint32_t format;
    uint32_t buffer_slot;
    uint32_t offset;
} rhi_vertex_attr_t;

typedef struct rhi_vertex_layout {
    const rhi_vertex_attr_t *attrs;
    int attr_count;
    uint32_t stride[8];
} rhi_vertex_layout_t;

typedef struct rhi_pipeline_desc {
    rhi_shader_t *shader;
    rhi_vertex_layout_t layout;
    rhi_blend_state_t blend;
    rhi_depthstencil_state_t depthst;
    rhi_raster_state_t raster;
} rhi_pipeline_desc_t;

typedef struct rhi_rt_attachment_desc {
    rhi_texture_t *texture;
    int mip_level;
    int array_slice;
} rhi_rt_attachment_desc_t;

typedef struct rhi_render_target_desc {
    rhi_rt_attachment_desc_t color[4];
    int color_count;
    rhi_rt_attachment_desc_t depth;
    int samples; /* 0/1 = no MSAA */
} rhi_render_target_desc_t;

typedef struct rhi_binding {
    uint32_t binding; /* abstract slot */
    rhi_texture_t *texture; /* SRV */
    rhi_sampler_t *sampler;
    //TODO: buffer/sampler etc
} rhi_binding_t;

typedef struct rhi_dispatch {
    /* device */
    rhi_device_t * (*create_device)(const rhi_device_desc_t *);
    void (*destroy_device)(rhi_device_t *);

    /* swapchain */
    rhi_swapchain_t * (*get_swapchain)(rhi_device_t *);
    void (*present)(rhi_swapchain_t *);

    void (*resize)(rhi_device_t *, int new_w, int new_h);

    /* resources */
    rhi_buffer_t * (*create_buffer)(rhi_device_t *, const rhi_buffer_desc_t *, const void *initial);
    void (*destroy_buffer)(rhi_device_t *, rhi_buffer_t *);
    void (*update_buffer)(rhi_device_t *, rhi_buffer_t *, const void *data, size_t bytes);

    rhi_texture_t * (*create_texture)(rhi_device_t *, const rhi_texture_desc_t *, const void *initial);
    void (*destroy_texture)(rhi_device_t *, rhi_texture_t *);

    rhi_sampler_t *(*create_sampler)(rhi_device_t *, const rhi_sampler_desc_t *);
    void (*destroy_sampler)(rhi_device_t *, rhi_sampler_t *);

    rhi_shader_t * (*create_shader)(rhi_device_t *, const rhi_shader_desc_t *);
    void (*destroy_shader)(rhi_device_t *, rhi_shader_t *);

    rhi_pipeline_t * (*create_pipeline)(rhi_device_t *, const rhi_pipeline_desc_t *);
    void (*destroy_pipeline)(rhi_device_t *, rhi_pipeline_t *);

    rhi_render_target_t * (*create_render_target)(rhi_device_t *, const rhi_render_target_desc_t *);
    void (*destroy_render_target)(rhi_device_t *, rhi_render_target_t *);
    rhi_render_target_t * (*get_backbuffer_rt)(rhi_device_t *);
    rhi_texture_t * (*render_target_get_color_tex)(rhi_render_target_t *, int index);

    /* commands */
    rhi_cmd_t * (*begin_cmd)(rhi_device_t *);
    void (*end_cmd)(rhi_cmd_t *);

    void (*cmd_begin_render)(rhi_cmd_t *, rhi_render_target_t *rt, const float clear_rgba[4]);
    void (*cmd_end_render)(rhi_cmd_t *);

    void (*cmd_bind_pipeline)(rhi_cmd_t *, rhi_pipeline_t *);
    void (*cmd_bind_const_buffer)(rhi_cmd_t *, int slot, rhi_buffer_t *, uint32_t stages);

    void (*cmd_bind_texture)(rhi_cmd_t *, rhi_texture_t *texture, int slot, uint32_t stages);
    void (*cmd_bind_sampler)(rhi_cmd_t *, rhi_sampler_t *sampler, int slot, uint32_t stages);

    void (*cmd_set_viewport_scissor)(rhi_cmd_t *, int x, int y, int w, int h);
    void (*cmd_set_blend_color)(rhi_cmd_t *, float r, float g, float b, float a);
    void (*cmd_set_depth_bias)(rhi_cmd_t *, float constant, float slope_scaled);

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
