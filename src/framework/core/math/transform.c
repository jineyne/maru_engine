#include "transform.h"
#include <string.h>

void transform_init(transform_t *t) {
    if (!t) return;

    memset(t, 0, sizeof(*t));

    glm_vec3_zero(t->position);
    glm_quat_identity(t->rotation);
    glm_vec3_one(t->scale);

    glm_mat4_identity(t->local_matrix);
    glm_mat4_identity(t->world_matrix);

    t->dirty_local = 1;
    t->dirty_world = 1;
    t->parent = NULL;
}

void transform_identity(transform_t *t) {
    if (!t) return;

    glm_vec3_zero(t->position);
    glm_quat_identity(t->rotation);
    glm_vec3_one(t->scale);

    t->dirty_local = 1;
    t->dirty_world = 1;
}

/* Position */
void transform_set_position(transform_t *t, const vec3 pos) {
    if (!t) return;
    glm_vec3_copy(pos, t->position);
    t->dirty_local = 1;
    t->dirty_world = 1;
}

void transform_get_position(const transform_t *t, vec3 out) {
    if (!t) {
        glm_vec3_zero(out);
        return;
    }
    glm_vec3_copy(t->position, out);
}

void transform_translate(transform_t *t, const vec3 delta) {
    if (!t) return;
    glm_vec3_add(t->position, delta, t->position);
    t->dirty_local = 1;
    t->dirty_world = 1;
}

/* Rotation */
void transform_set_rotation(transform_t *t, const versor quat) {
    if (!t) return;
    glm_quat_copy(quat, t->rotation);
    t->dirty_local = 1;
    t->dirty_world = 1;
}

void transform_get_rotation(const transform_t *t, versor out) {
    if (!t) {
        glm_quat_identity(out);
        return;
    }
    glm_quat_copy(t->rotation, out);
}

void transform_set_euler(transform_t *t, float pitch, float yaw, float roll) {
    if (!t) return;

    /* Convert Euler angles to rotation matrix, then to quaternion */
    vec3 euler = {pitch, yaw, roll};
    mat4 rot_mat;
    glm_euler_xyz(euler, rot_mat);
    glm_mat4_quat(rot_mat, t->rotation);

    t->dirty_local = 1;
    t->dirty_world = 1;
}

void transform_rotate(transform_t *t, const versor delta) {
    if (!t) return;

    versor result;
    glm_quat_mul(t->rotation, delta, result);
    glm_quat_copy(result, t->rotation);

    t->dirty_local = 1;
    t->dirty_world = 1;
}

/* Scale */
void transform_set_scale(transform_t *t, const vec3 scale) {
    if (!t) return;
    glm_vec3_copy(scale, t->scale);
    t->dirty_local = 1;
    t->dirty_world = 1;
}

void transform_get_scale(const transform_t *t, vec3 out) {
    if (!t) {
        glm_vec3_one(out);
        return;
    }
    glm_vec3_copy(t->scale, out);
}

void transform_set_uniform_scale(transform_t *t, float scale) {
    if (!t) return;
    t->scale[0] = scale;
    t->scale[1] = scale;
    t->scale[2] = scale;
    t->dirty_local = 1;
    t->dirty_world = 1;
}

/* Hierarchy */
void transform_set_parent(transform_t *t, transform_t *parent) {
    if (!t) return;
    t->parent = parent;
    t->dirty_world = 1;
}

transform_t* transform_get_parent(const transform_t *t) {
    return t ? t->parent : NULL;
}

/* Matrix (lazy evaluation) */
const mat4* transform_get_local_matrix(transform_t *t) {
    if (!t) return NULL;

    if (t->dirty_local) {
        /* Compose TRS: Translation × Rotation × Scale */
        mat4 scale_mat, rot_mat, trans_mat, temp;

        /* Scale matrix */
        glm_mat4_identity(scale_mat);
        glm_scale(scale_mat, t->scale);

        /* Rotation matrix from quaternion */
        glm_quat_mat4(t->rotation, rot_mat);

        /* Translation matrix */
        glm_mat4_identity(trans_mat);
        glm_translate(trans_mat, t->position);

        /* Combine: T × R × S */
        glm_mat4_mul(rot_mat, scale_mat, temp);
        glm_mat4_mul(trans_mat, temp, t->local_matrix);

        t->dirty_local = 0;
        t->dirty_world = 1; /* World matrix also needs update */
    }

    return &t->local_matrix;
}

const mat4* transform_get_world_matrix(transform_t *t) {
    if (!t) return NULL;

    if (t->dirty_world) {
        const mat4 *local = transform_get_local_matrix(t);

        if (t->parent) {
            /* World = Parent.World × Local */
            const mat4 *parent_world = transform_get_world_matrix(t->parent);
            glm_mat4_mul(*parent_world, *local, t->world_matrix);
        } else {
            /* No parent, world = local */
            glm_mat4_copy(*local, t->world_matrix);
        }

        t->dirty_world = 0;
    }

    return &t->world_matrix;
}

/* Utility */
void transform_look_at(transform_t *t, const vec3 target, const vec3 up) {
    if (!t) return;

    vec3 forward, right, new_up;

    /* Calculate forward vector */
    glm_vec3_sub(target, t->position, forward);
    glm_vec3_normalize(forward);

    /* Calculate right vector */
    glm_vec3_cross(forward, up, right);
    glm_vec3_normalize(right);

    /* Calculate up vector */
    glm_vec3_cross(right, forward, new_up);

    /* Build rotation matrix */
    mat4 rot_mat;
    glm_mat4_identity(rot_mat);

    glm_vec3_copy(right, rot_mat[0]);
    glm_vec3_copy(new_up, rot_mat[1]);
    glm_vec3_negate_to(forward, rot_mat[2]);

    /* Convert to quaternion */
    glm_mat4_quat(rot_mat, t->rotation);

    t->dirty_local = 1;
    t->dirty_world = 1;
}

void transform_get_forward(const transform_t *t, vec3 out) {
    if (!t) {
        glm_vec3_zero(out);
        out[2] = -1.0f; /* Default forward is -Z */
        return;
    }

    /* Forward is -Z axis rotated by quaternion */
    vec3 forward = {0.0f, 0.0f, -1.0f};
    glm_quat_rotatev(t->rotation, forward, out);
}

void transform_get_right(const transform_t *t, vec3 out) {
    if (!t) {
        glm_vec3_zero(out);
        out[0] = 1.0f; /* Default right is +X */
        return;
    }

    /* Right is +X axis rotated by quaternion */
    vec3 right = {1.0f, 0.0f, 0.0f};
    glm_quat_rotatev(t->rotation, right, out);
}

void transform_get_up(const transform_t *t, vec3 out) {
    if (!t) {
        glm_vec3_zero(out);
        out[1] = 1.0f; /* Default up is +Y */
        return;
    }

    /* Up is +Y axis rotated by quaternion */
    vec3 up = {0.0f, 1.0f, 0.0f};
    glm_quat_rotatev(t->rotation, up, out);
}
