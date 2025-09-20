#include "rhi_gles.h"

#include <stdlib.h>

struct rhi_device {
    int dummy;
};

struct rhi_swapchain {
    int dummy;
};

struct rhi_buffer {
    size_t size;
};

struct rhi_texture {
    GLuint id;
    GLenum target;
    GLenum internal_fmt;
    int w, h, mips;
    uint32_t usage;
};

struct rhi_sampler {
    int _dummy;
};

struct rhi_shader {
    int linked;
};

struct rhi_pipeline {
    rhi_shader_t *sh;
};

struct rhi_render_target {
    int is_backbuffer; /* 1=default framebuffer(0) */
    GLuint fbo;
    GLuint color_ids[4];
    int color_count;
    GLuint depth_id;
};

struct rhi_cmd {
    rhi_render_target_t *current_rt;
};

struct rhi_fence {
    int dummy;
};

#pragma region Helpers

static int gl_srv_conflicts_with_rt(GLuint tex, const rhi_render_target_t *rt) {
    if (!rt) return 0;
    for (int i = 0; i < rt->color_count; ++i) {
        if (rt->color_ids[i] && rt->color_ids[i] == tex) {
            return 1;
        }
    }

    if (rt->depth_id && rt->depth_id == tex) return 1;
    return 0;
}

static void gl_apply_states(const rhi_pipeline_desc_t *s) {
    // raster
    if (s->raster.cull == RHI_CULL_NONE)
        glDisable(GL_CULL_FACE);
    else {
        glEnable(GL_CULL_FACE);
        glCullFace(s->raster.cull == RHI_CULL_FRONT ? GL_FRONT : GL_BACK);
    }
    glFrontFace(s->raster.front_ccw ? GL_CCW : GL_CW);

    // depth
    if (s->depthst.depth_test_enable) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    } else
        glDisable(GL_DEPTH_TEST);
    glDepthMask(s->depthst.depth_write_enable ? GL_TRUE : GL_FALSE);

    // blend
    if (s->blend.enable) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);
        glColorMask((s->blend.write_mask & 1) != 0, (s->blend.write_mask & 2) != 0,
                    (s->blend.write_mask & 4) != 0, (s->blend.write_mask & 8) != 0);
    } else {
        glDisable(GL_BLEND);
    }
}

#pragma endregion Helpers

static rhi_device_t *gles_create_device(const rhi_device_desc_t *d) {
    INFO("creating OpenGL ES device");
    UNUSED(d);
    return (rhi_device_t*) calloc(1, sizeof(rhi_device_t));
}

static void gles_destroy_device(rhi_device_t *d) {
    free(d);
}

static rhi_swapchain_t *gles_get_swapchain(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_swapchain_t*) calloc(1, sizeof(rhi_swapchain_t));
}

static void gles_present(rhi_swapchain_t *s) {
    UNUSED(s);
}

static void gles_resize(rhi_device_t *d, int w, int h) {
    UNUSED(d);
    UNUSED(w);
    UNUSED(h);
}

static rhi_buffer_t *gles_create_buffer(rhi_device_t *d, const rhi_buffer_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(desc);
    UNUSED(initial);
    return (rhi_buffer_t*) calloc(1, sizeof(rhi_buffer_t));
}

static void gles_destroy_buffer(rhi_device_t *d, rhi_buffer_t *b) {
    UNUSED(d);
    free(b);
}

static void gles_update_buffer(rhi_device_t *d, rhi_buffer_t *b, const void *data, size_t bytes) {
    UNUSED(d);
    UNUSED(b);
    UNUSED(data);
    UNUSED(bytes);
}

static rhi_texture_t *gles_create_texture(rhi_device_t *d, const rhi_texture_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(initial);
    rhi_texture_t *t = (rhi_texture_t*) calloc(1, sizeof(*t));
    glGenTextures(1, &t->id);
    t->target = GL_TEXTURE_2D;
    t->w = desc->width;
    t->h = desc->height;
    t->mips = desc->mip_levels > 0 ? desc->mip_levels : 1;
    t->usage = desc->usage;

    if (desc->usage & RHI_TEX_USAGE_DEPTH) {
        t->internal_fmt = GL_DEPTH24_STENCIL8;
    } else {
        t->internal_fmt = (desc->format == RHI_FMT_BGRA8) ? GL_RGBA8 : GL_RGBA8;
    }

    glBindTexture(t->target, t->id);
    if (desc->usage & RHI_TEX_USAGE_DEPTH) {
        glTexImage2D(t->target, 0, t->internal_fmt, t->w, t->h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    } else {
        glTexImage2D(t->target, 0, t->internal_fmt, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
    if (t->mips > 1 || (desc->usage & RHI_TEX_USAGE_GEN_MIPS)) {
        glGenerateMipmap(t->target);
    }

    glTexParameteri(t->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(t->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(t->target, 0);
    return t;
}

static void gles_destroy_texture(rhi_device_t *d, rhi_texture_t *t) {
    UNUSED(d);
    if (!t) return;
    if (t->id) {
        glDeleteTextures(1, &t->id);
    }

    free(t);
}

static rhi_sampler_t *gles_create_sampler(rhi_device_t *d, const rhi_sampler_desc_t *desc) {
    UNUSED(d);
    UNUSED(desc);
    rhi_sampler_t *s = (rhi_sampler_t*) calloc(1, sizeof(rhi_sampler_t));
    if (!s) return NULL;
    s->_dummy = 1;
    return s;
}

static void gles_destroy_sampler(rhi_device_t *d, rhi_sampler_t *s) {
    UNUSED(d);
    if (!s) return;
    free(s);
}


static rhi_shader_t *gles_create_shader(rhi_device_t *d, const rhi_shader_desc_t *sd) {
    UNUSED(d);
    UNUSED(sd);
    return (rhi_shader_t*) calloc(1, sizeof(rhi_shader_t));
}

static void gles_destroy_shader(rhi_device_t *d, rhi_shader_t *s) {
    UNUSED(d);
    free(s);
}

static rhi_pipeline_t *gles_create_pipeline(rhi_device_t *d, const rhi_pipeline_desc_t *pd) {
    UNUSED(d);
    rhi_pipeline_t *p = (rhi_pipeline_t*) calloc(1, sizeof(rhi_pipeline_t));
    p->sh = pd->shader;
    return p;
}

static void gles_destroy_pipeline(rhi_device_t *d, rhi_pipeline_t *p) {
    UNUSED(d);
    free(p);
}

static rhi_render_target_t *gles_create_render_target(rhi_device_t *d, const rhi_render_target_desc_t *desc) {
    UNUSED(d);
    rhi_render_target_t *rt = (rhi_render_target_t*) calloc(1, sizeof(*rt));
    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    rt->color_count = desc->color_count;
    for (int i = 0; i < desc->color_count; ++i) {
        rhi_texture_t *tx = desc->color[i].texture;
        rt->color_ids[i] = tx ? tx->id : 0;
        if (tx && !(tx->usage & RHI_TEX_USAGE_RENDER_TARGET)) {
            ERROR("texture %d is not created with RHI_TEX_USAGE_RENDER_TARGET flag, but used as render target");
        }

        if (tx) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, tx->target, tx->id,
                                   desc->color[i].mip_level);
        }
    }

    if (desc->depth.texture) {
        rhi_texture_t *dz = desc->depth.texture;
        rt->depth_id = dz->id;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, dz->target, dz->id, desc->depth.mip_level);
    }

    GLenum bufs[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    glDrawBuffers(rt->color_count > 0 ? rt->color_count : 1, bufs);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &rt->fbo);
        free(rt);
        return NULL;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return rt;
}

static void gles_destroy_render_target(rhi_device_t *d, rhi_render_target_t *rt) {
    UNUSED(d);
    if (!rt) return;
    if (!rt->is_backbuffer && rt->fbo) {
        glDeleteFramebuffers(1, &rt->fbo);
    }

    free(rt);
}

static rhi_render_target_t *gles_get_backbuffer_rt(rhi_device_t *d) {
    UNUSED(d);
    static rhi_render_target_t *s = NULL;
    if (!s) {
        s = (rhi_render_target_t*) calloc(1, sizeof(*s));
        s->is_backbuffer = 1;
    }
    return s;
}

static rhi_texture_t *gles_render_target_get_color_tex(rhi_render_target_t *rt, int index) {
    UNUSED(rt);
    UNUSED(index);
    return NULL;
}

static rhi_cmd_t *gles_begin_cmd(rhi_device_t *d) {
    UNUSED(d);
    rhi_cmd_t *c = (rhi_cmd_t*) calloc(1, sizeof(rhi_cmd_t));
    if (c) c->current_rt = NULL;
    return c;
}

static void gles_end_cmd(rhi_cmd_t *c) { free(c); }

static void gles_cmd_begin_render(rhi_cmd_t *c, rhi_render_target_t *rt, const float clear_rgba[4]) {
    rhi_render_target_t *use = rt ? rt : gles_get_backbuffer_rt(NULL);
    c->current_rt = use;
    glBindFramebuffer(GL_FRAMEBUFFER, use->fbo);

    GLint vpw = 0, vph = 0;
    if (use->is_backbuffer) {
        GLint vp[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, vp);
        vpw = vp[2];
        vph = vp[3];
    } else if (use->color_count > 0 && use->color_ids[0]) {
        GLint w = 0, h = 0;
        glBindTexture(GL_TEXTURE_2D, use->color_ids[0]);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
        glBindTexture(GL_TEXTURE_2D, 0);
        vpw = w;
        vph = h;
    }

    if (vpw <= 0 || vph <= 0) {
        vpw = 1280;
        vph = 720;
    }
    glViewport(0, 0, vpw, vph);

    if (clear_rgba) {
        glClearColor(clear_rgba[0], clear_rgba[1], clear_rgba[2], clear_rgba[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
}

static void gles_cmd_end_render(rhi_cmd_t *c) {
    UNUSED(c);
}

static void gles_cmd_bind_pipeline(rhi_cmd_t *c, rhi_pipeline_t *p) {
    UNUSED(c);
    UNUSED(p);
}

static void gles_cmd_bind_const_buffer(rhi_cmd_t *c, int slot, rhi_buffer_t *b, uint32_t stages) {
    UNUSED(c);
    UNUSED(slot);
    UNUSED(b);
    UNUSED(stages);
}

static void gles_cmd_bind_texture(rhi_cmd_t *c, rhi_texture_t *t, int slot, uint32_t stages) {
    UNUSED(stages);
    if (!t || !t->id) return;
    if (gl_srv_conflicts_with_rt(t->id, c ? c->current_rt : NULL)) {
        DEBUG_LOG("GL: SRV-RT conflict on unit %u -> skip bind", slot);
        return;
    }
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(t->target, t->id);
}

static void gles_cmd_bind_sampler(rhi_cmd_t *c, rhi_sampler_t *s, int slot, uint32_t stages) {
    UNUSED(c);
    UNUSED(slot);
    UNUSED(s);
    UNUSED(stages);
}

static void gles_cmd_set_viewport_scissor(rhi_cmd_t *c, int x, int y, int w, int h) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
}

static void gl_cmd_set_blend_color(rhi_cmd_t *c, float r, float g, float b, float a) {
    UNUSED(c);
    glBlendColor(r, g, b, a);
}

static void gl_cmd_set_depth_bias(rhi_cmd_t *c, float constant, float slope_scaled) {
    UNUSED(c);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(slope_scaled, constant);
}

static void gles_cmd_set_vertex_buffer(rhi_cmd_t *c, int slot, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(slot);
    UNUSED(b);
}

static void gles_cmd_set_index_buffer(rhi_cmd_t *c, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(b);
}

static void gles_cmd_draw(rhi_cmd_t *c, uint32_t vtx_count, uint32_t first, uint32_t inst) {
    UNUSED(c);
    UNUSED(vtx_count);
    UNUSED(first);
    UNUSED(inst);
}

static void gles_cmd_draw_indexed(rhi_cmd_t *c, uint32_t idx_count, uint32_t first, uint32_t base_vtx, uint32_t inst) {
    UNUSED(c);
    UNUSED(idx_count);
    UNUSED(first);
    UNUSED(base_vtx);
    UNUSED(inst);
}

static rhi_fence_t *gles_fence_create(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_fence_t*) calloc(1, sizeof(rhi_fence_t));
}

static void gles_fence_wait(rhi_fence_t *f) { UNUSED(f); }
static void gles_fence_destroy(rhi_fence_t *f) { free(f); }

PLUGIN_API const rhi_dispatch_t *maru_rhi_entry(void) {
    static const rhi_dispatch_t vtbl = {
        /* device */
        gles_create_device, gles_destroy_device,
        /* swapchain */
        gles_get_swapchain, gles_present,
        gles_resize,
        /* resources */
        gles_create_buffer, gles_destroy_buffer, gles_update_buffer,
        gles_create_texture, gles_destroy_texture,
        gles_create_sampler, gles_destroy_sampler,
        gles_create_shader, gles_destroy_shader,
        gles_create_pipeline, gles_destroy_pipeline,
        gles_create_render_target, gles_destroy_render_target,
        gles_get_backbuffer_rt, gles_render_target_get_color_tex,
        /* commands */
        gles_begin_cmd, gles_end_cmd, gles_cmd_begin_render, gles_cmd_end_render,
        gles_cmd_bind_pipeline, gles_cmd_bind_const_buffer,
        gles_cmd_bind_texture, gles_cmd_bind_sampler,
        gles_cmd_set_viewport_scissor, gl_cmd_set_blend_color, gl_cmd_set_depth_bias, gles_cmd_set_vertex_buffer,
        gles_cmd_set_index_buffer,
        gles_cmd_draw, gles_cmd_draw_indexed,
        /* sync */
        gles_fence_create, gles_fence_wait, gles_fence_destroy,
    };
    return &vtbl;
}
