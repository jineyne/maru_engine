#ifndef MARU_MATH_TRANSFORM_H
#define MARU_MATH_TRANSFORM_H

#include "math.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct transform {
    vec3 position;
    versor rotation;        /* quaternion */
    vec3 scale;

    mat4 local_matrix;      /* cached local transform */
    mat4 world_matrix;      /* cached world transform */

    uint8_t dirty_local : 1;
    uint8_t dirty_world : 1;

    struct transform *parent;
} transform_t;

/* Lifecycle */
void transform_init(transform_t *t);
void transform_identity(transform_t *t);

/* Position */
void transform_set_position(transform_t *t, const vec3 pos);
void transform_get_position(const transform_t *t, vec3 out);
void transform_translate(transform_t *t, const vec3 delta);

/* Rotation */
void transform_set_rotation(transform_t *t, const versor quat);
void transform_get_rotation(const transform_t *t, versor out);
void transform_set_euler(transform_t *t, float pitch, float yaw, float roll);
void transform_rotate(transform_t *t, const versor delta);

/* Scale */
void transform_set_scale(transform_t *t, const vec3 scale);
void transform_get_scale(const transform_t *t, vec3 out);
void transform_set_uniform_scale(transform_t *t, float scale);

/* Hierarchy */
void transform_set_parent(transform_t *t, transform_t *parent);
transform_t* transform_get_parent(const transform_t *t);

/* Matrix (lazy evaluation) */
const mat4* transform_get_local_matrix(transform_t *t);
const mat4* transform_get_world_matrix(transform_t *t);

/* Utility */
void transform_look_at(transform_t *t, const vec3 target, const vec3 up);
void transform_get_forward(const transform_t *t, vec3 out);
void transform_get_right(const transform_t *t, vec3 out);
void transform_get_up(const transform_t *t, vec3 out);

#ifdef __cplusplus
}
#endif

#endif /* MARU_MATH_TRANSFORM_H */
