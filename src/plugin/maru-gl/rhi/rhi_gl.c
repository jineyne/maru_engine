#include "rhi_gl.h"

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
    GLenum target; /* GL_TEXTURE_2D... */
    GLenum internal_fmt; /* GL_RGBA8 / GL_DEPTH24_STENCIL8 */
    int w, h, mips;
    uint32_t usage; /* rhi_tex_usage_bits */
};

struct rhi_sampler {
    int _dummy;
};

struct rhi_shader {
    int linked;
};

struct rhi_pipeline {
    rhi_shader_t *sh;
    rhi_blend_state_t blend;
    rhi_depthstencil_state_t depthst;
    rhi_raster_state_t raster;
};

struct rhi_render_target {
    int is_backbuffer; /* 1=default framebuffer(0) */
    GLuint fbo;
    GLuint color_ids[4];
    int color_count;
    GLuint depth_id;
};

struct rhi_cmd {
    int dummy;
};

struct rhi_fence {
    int dummy;
};

#pragma region helpers

static int gl_srv_conflicts_with_rt(GLuint tex, const rhi_render_target_t *rt) {
    if (!rt) return 0;
    for (int i = 0; i < rt->color_count; ++i) {
        if (rt->color_ids[i] && rt->color_ids[i] == tex) {
            return 1;
        }
    }

    if (rt->depth_id && rt->depth_id == tex) {
        return 1;
    }

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

#pragma endregion

static rhi_device_t *gl_create_device(const rhi_device_desc_t *d) {
    INFO("creating openGL device");

    UNUSED(d);
    return (rhi_device_t*)calloc(1, sizeof(rhi_device_t));
}

static void gl_update_buffer(rhi_device_t *d, rhi_buffer_t *b, const void *data, size_t bytes) {
    UNUSED(d);
    UNUSED(b);
    UNUSED(data);
    UNUSED(bytes);
}

static void gl_destroy_device(rhi_device_t *d) {
    free(d);
}

static rhi_swapchain_t *gl_get_swapchain(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_swapchain_t*)calloc(1, sizeof(rhi_swapchain_t));
}

static void gl_present(rhi_swapchain_t *s) {
    UNUSED(s); /* swap buffers here */
}

static void gl_resize(rhi_device_t *d, int w, int h) {
    UNUSED(d);
    UNUSED(w);
    UNUSED(h);
}

static rhi_buffer_t *gl_create_buffer(rhi_device_t *d, const rhi_buffer_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(initial);
    return (rhi_buffer_t*)calloc(1, sizeof(rhi_buffer_t));
}

static void gl_destroy_buffer(rhi_device_t *d, rhi_buffer_t *b) {
    UNUSED(d);
    free(b);
}

static rhi_texture_t *gl_create_texture(rhi_device_t *d, const rhi_texture_desc_t *desc, const void *initial) {
    UNUSED(d);
    UNUSED(initial);

    rhi_texture_t *t = (rhi_texture_t*)calloc(1, sizeof(*t));
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

static void gl_destroy_texture(rhi_device_t *d, rhi_texture_t *t) {
    UNUSED(d);

    if (!t) return;
    if (t->id) {
        glDeleteTextures(1, &t->id);
    }

    free(t);
}

static rhi_sampler_t *gl_create_sampler(rhi_device_t *d, const rhi_sampler_desc_t *desc) {
    UNUSED(d);
    UNUSED(desc);
    rhi_sampler_t *s = (rhi_sampler_t*)calloc(1, sizeof(rhi_sampler_t));
    if (!s) return NULL;
    s->_dummy = 1;
    return s;
}

static void gl_destroy_sampler(rhi_device_t *d, rhi_sampler_t *s) {
    UNUSED(d);
    if (!s) return;
    free(s);
}

static rhi_shader_t *gl_create_shader(rhi_device_t *d, const rhi_shader_desc_t *sd) {
    UNUSED(d);
    UNUSED(sd);
    return (rhi_shader_t*)calloc(1, sizeof(rhi_shader_t));
}

static void gl_destroy_shader(rhi_device_t *d, rhi_shader_t *s) {
    UNUSED(d);
    free(s);
}

static rhi_pipeline_t *gl_create_pipeline(rhi_device_t *d, const rhi_pipeline_desc_t *pd) {
    UNUSED(d);
    rhi_pipeline_t *p = (rhi_pipeline_t*)calloc(1, sizeof(rhi_pipeline_t));
    p->sh = pd->shader;
    p->blend = pd->blend;
    p->depthst = pd->depthst;
    p->raster = pd->raster;
    return p;
}

static void gl_destroy_pipeline(rhi_device_t *d, rhi_pipeline_t *p) {
    UNUSED(d);
    free(p);
}

static rhi_render_target_t *gl_create_render_target(rhi_device_t *d, const rhi_render_target_desc_t *desc) {
    UNUSED(d);
    rhi_render_target_t *rt = (rhi_render_target_t*)calloc(1, sizeof(*rt));
    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

    rt->color_count = desc->color_count;
    for (int i = 0; i < desc->color_count; ++i) {
        rhi_texture_t *tx = desc->color[i].texture;
        rt->color_ids[i] = tx ? tx->id : 0;
        if (tx && !(tx->usage & RHI_TEX_USAGE_RENDER_TARGET)) {
            ERROR("texture for render target should have RHI_TEX_USAGE_RENDER_TARGET usage bit");
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

static void gl_destroy_render_target(rhi_device_t *d, rhi_render_target_t *rt) {
    UNUSED(d);
    if (!rt) return;

    if (!rt->is_backbuffer && rt->fbo) {
        glDeleteFramebuffers(1, &rt->fbo);
    }

    free(rt);
}

static rhi_render_target_t *gl_get_backbuffer_rt(rhi_device_t *d) {
    UNUSED(d);
    static rhi_render_target_t *s = NULL;
    if (!s) {
        s = (rhi_render_target_t*)calloc(1, sizeof(*s));
        s->is_backbuffer = 1;
    }
    return s;
}

static rhi_texture_t *gl_render_target_get_color_tex(rhi_render_target_t *rt, int index) {
    UNUSED(rt);
    UNUSED(index);
    return NULL;
}

static rhi_cmd_t *gl_begin_cmd(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_cmd_t*)calloc(1, sizeof(rhi_cmd_t));
}

static void gl_end_cmd(rhi_cmd_t *c) {
    free(c);
}

static void gl_cmd_begin_render(rhi_cmd_t *c, rhi_render_target_t *rt, const float clear_rgba[4]) {
    UNUSED(c);
    rhi_render_target_t *use = rt ? rt : gl_get_backbuffer_rt(NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, use->fbo); /* backbuffer = 0 */

    if (clear_rgba) {
        glViewport(0, 0, /* TODO: real size */ 4096, 4096);
        glClearColor(clear_rgba[0], clear_rgba[1], clear_rgba[2], clear_rgba[3]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
}

static void gl_cmd_end_render(rhi_cmd_t *c) {
    UNUSED(c);
}

static void gl_cmd_bind_pipeline(rhi_cmd_t *c, rhi_pipeline_t *p) {
    rhi_pipeline_desc_t tmp = {0};
    tmp.blend = p->blend;
    tmp.depthst = p->depthst;
    tmp.raster = p->raster;
    gl_apply_states(&tmp);
}

static void gl_cmd_bind_const_buffer(rhi_cmd_t *c, int slot, rhi_buffer_t *b, uint32_t stages_mask) {
    UNUSED(c);
    UNUSED(slot);
    UNUSED(b);
    UNUSED(stages_mask);
}

static void gl_cmd_bind_texture(rhi_cmd_t *c, rhi_texture_t *t, int slot, uint32_t stages) {
    UNUSED(stages);

    static rhi_render_target_t *s_last_rt = NULL;

    if (!t || !t->id) return;
    if (gl_srv_conflicts_with_rt(t->id, s_last_rt)) {
        DEBUG_LOG("GL: SRV-RT conflict on unit %u -> skip bind", slot);
        return;
    }

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(t->target, t->id);
}

static void gl_cmd_bind_sampler(rhi_cmd_t *c, rhi_sampler_t *s, int slot, uint32_t stages) {
    UNUSED(c);
    UNUSED(slot);
    UNUSED(s);
    UNUSED(stages);
}

static void gl_cmd_set_viewport_scissor(rhi_cmd_t *c, int x, int y, int w, int h) {
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

static void gl_cmd_set_vertex_buffer(rhi_cmd_t *c, int slot, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(slot);
    UNUSED(b);
}

static void gl_cmd_set_index_buffer(rhi_cmd_t *c, rhi_buffer_t *b) {
    UNUSED(c);
    UNUSED(b);
}

static void gl_cmd_draw(rhi_cmd_t *c, uint32_t vtx_count, uint32_t first, uint32_t inst) {
    UNUSED(c);
    UNUSED(vtx_count);
    UNUSED(first);
    UNUSED(inst);
}

static void gl_cmd_draw_indexed(rhi_cmd_t *c, uint32_t idx_count, uint32_t first, uint32_t base_vtx, uint32_t inst) {
    UNUSED(c);
    UNUSED(idx_count);
    UNUSED(first);
    UNUSED(base_vtx);
    UNUSED(inst);
}

static rhi_fence_t *gl_fence_create(rhi_device_t *d) {
    UNUSED(d);
    return (rhi_fence_t*)calloc(1, sizeof(rhi_fence_t));
}

static void gl_fence_wait(rhi_fence_t *f) {
    (void)f;
}

static void gl_fence_destroy(rhi_fence_t *f) {
    free(f);
}

PLUGIN_API const rhi_dispatch_t *maru_rhi_entry(void) {
    static const rhi_dispatch_t vtbl = {
        /* device */
        gl_create_device, gl_destroy_device,
        /* swapchain */
        gl_get_swapchain, gl_present,
        gl_resize,
        /* resources */
        gl_create_buffer, gl_destroy_buffer, gl_update_buffer,
        gl_create_texture, gl_destroy_texture,
        gl_create_sampler, gl_destroy_sampler,
        gl_create_shader, gl_destroy_shader,
        gl_create_pipeline, gl_destroy_pipeline,
        gl_create_render_target, gl_destroy_render_target,
        gl_get_backbuffer_rt, gl_render_target_get_color_tex,
        /* commands */
        gl_begin_cmd, gl_end_cmd, gl_cmd_begin_render, gl_cmd_end_render,
        gl_cmd_bind_pipeline, gl_cmd_bind_const_buffer,
        gl_cmd_bind_texture, gl_cmd_bind_sampler,
        gl_cmd_set_viewport_scissor, gl_cmd_set_blend_color, gl_cmd_set_depth_bias,
        gl_cmd_set_vertex_buffer, gl_cmd_set_index_buffer,
        gl_cmd_draw, gl_cmd_draw_indexed,
        /* sync */
        gl_fence_create, gl_fence_wait, gl_fence_destroy,
    };
    return &vtbl; /* immutable vtable */
}
