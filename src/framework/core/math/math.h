#ifndef MARU_MATH_H
#define MARU_MATH_H

#include <cglm/cglm.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PI 3.14159265f

typedef vec3 vec3_t;
typedef vec4 vec4_t;
typedef mat4 mat4_t;
typedef versor quat_t;

static inline void mat4_identity(mat4_t m) { glm_mat4_identity(m); }
static inline void mat4_mul(mat4_t a, mat4_t b, mat4_t d) { glm_mat4_mul(a, b, d); }
static inline void mat4_copy(mat4_t s, mat4_t d) { glm_mat4_copy(s, d); }
static inline void mat4_transpose_to(mat4_t m, mat4_t out) { glm_mat4_transpose_to(m, out); }
static inline void vec3_copy(vec3_t s, vec3_t d) { glm_vec3_copy(s, d); }

#ifdef __cplusplus
}
#endif

#include "transform.h"

#endif /* MARU_MATH_H */
