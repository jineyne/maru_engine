#ifndef MARU_PROJ_H
#define MARU_PROJ_H

#include "rhi/rhi.h"
#include "math.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void zremap_gl_to_dx(mat4_t r) {
    glm_mat4_identity(r);

    r[2][2] = 0.5f;
    r[3][2] = 0.5f;
}

static inline void mat4_to_backend_order(const rhi_capabilities_t *caps, mat4_t in, mat4_t out) {
    if (caps && caps->conventions.matrix_order == RHI_MATRIX_ROW_MAJOR) {
        mat4_transpose_to(in, out);
    } else {
        mat4_copy(in, out);
    }
}

static inline void perspective_from_caps(const rhi_capabilities_t *caps, float fovy_rad, float aspect, float zn, float zf, mat4_t out) {
    mat4_t P;
    glm_perspective_rh_no(fovy_rad, aspect, zn, zf, P);

    if (caps && caps->min_depth == 0.0f && caps->max_depth == 1.0f) {
        mat4_t R;
        zremap_gl_to_dx(R);
        mat4_mul(R, P, P);
    }

    mat4_to_backend_order(caps, P, out);
}

static inline void ortho_from_caps(const rhi_capabilities_t *caps, float left, float right, float bottom, float top, float zn, float zf, mat4_t out) {
    mat4_t O;
    glm_ortho_rh_no(left, right, bottom, top, zn, zf, O);

    if (caps && caps->min_depth == 0.0f && caps->max_depth == 1.0f) {
        mat4_t R;
        zremap_gl_to_dx(R);
        mat4_mul(R, O, O);
    }

    mat4_to_backend_order(caps, O, out);
}

static inline void look_at(vec3_t eye, vec3_t center, vec3_t up, mat4_t out) {
    glm_lookat_rh(eye, center, up, out);
}

#ifdef __cplusplus
}
#endif
#endif /* MARU_PROJ_H */
