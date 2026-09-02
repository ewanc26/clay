#ifndef CLAY_CORE_MATH_H
#define CLAY_CORE_MATH_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

/* Row-vector convention throughout: v' = v * M. Matrix memory is row-major,
 * M[row][col]. */
typedef struct cl_v2 {
    float x, y;
} cl_v2;

typedef struct cl_v3 {
    float x, y, z;
} cl_v3;

typedef struct cl_v4 {
    float x, y, z, w;
} cl_v4;

typedef struct cl_m4 {
    float m[4][4];
} cl_m4;

static inline cl_v2 cl_v2_make(float x, float y) {
    cl_v2 v = {x, y};
    return v;
}
static inline cl_v2 cl_v2_add(cl_v2 a, cl_v2 b) {
    return cl_v2_make(a.x + b.x, a.y + b.y);
}
static inline cl_v2 cl_v2_sub(cl_v2 a, cl_v2 b) {
    return cl_v2_make(a.x - b.x, a.y - b.y);
}
static inline cl_v2 cl_v2_scale(cl_v2 a, float s) {
    return cl_v2_make(a.x * s, a.y * s);
}
static inline float cl_v2_dot(cl_v2 a, cl_v2 b) {
    return a.x * b.x + a.y * b.y;
}
static inline float cl_v2_length(cl_v2 a) {
    return a.x == 0.0f && a.y == 0.0f ? 0.0f
                                      : __builtin_sqrtf(a.x * a.x + a.y * a.y);
}
static inline cl_v2 cl_v2_normalize(cl_v2 a) {
    float l = cl_v2_length(a);
    return l > 0.0f ? cl_v2_scale(a, 1.0f / l) : cl_v2_make(0.0f, 0.0f);
}
static inline cl_v2 cl_v2_perp(cl_v2 a) {
    return cl_v2_make(-a.y, a.x);
}
static inline cl_v2 cl_v2_lerp(cl_v2 a, cl_v2 b, float t) {
    return cl_v2_make(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}
static inline float cl_v2_dist(cl_v2 a, cl_v2 b) {
    return cl_v2_length(cl_v2_sub(a, b));
}

static inline cl_v3 cl_v3_make(float x, float y, float z) {
    cl_v3 v = {x, y, z};
    return v;
}
static inline cl_v3 cl_v3_add(cl_v3 a, cl_v3 b) {
    return cl_v3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}
static inline cl_v3 cl_v3_sub(cl_v3 a, cl_v3 b) {
    return cl_v3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}
static inline cl_v3 cl_v3_scale(cl_v3 a, float s) {
    return cl_v3_make(a.x * s, a.y * s, a.z * s);
}
static inline float cl_v3_dot(cl_v3 a, cl_v3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline cl_v3 cl_v3_cross(cl_v3 a, cl_v3 b) {
    return cl_v3_make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                      a.x * b.y - a.y * b.x);
}
static inline float cl_v3_length(cl_v3 a) {
    return __builtin_sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}
static inline cl_v3 cl_v3_normalize(cl_v3 a) {
    float l = cl_v3_length(a);
    return l > 0.0f ? cl_v3_scale(a, 1.0f / l) : cl_v3_make(0.0f, 0.0f, 0.0f);
}

static inline cl_v4 cl_v4_make(float x, float y, float z, float w) {
    cl_v4 v = {x, y, z, w};
    return v;
}

static inline cl_m4 cl_m4_identity(void) {
    cl_m4 m = {{{0}}};
    for (int i = 0; i < 4; i++) m.m[i][i] = 1.0f;
    return m;
}

cl_m4 cl_m4_mul(cl_m4 a, cl_m4 b);
cl_m4 cl_m4_ortho(float left, float right, float bottom, float top, float znear,
                  float zfar);
cl_m4 cl_m4_perspective(float vertical_fov_radians, float aspect, float znear,
                        float zfar);
cl_m4 cl_m4_translate(float x, float y, float z);
cl_m4 cl_m4_scale(float x, float y, float z);
cl_m4 cl_m4_rotate_x(float radians);
cl_m4 cl_m4_rotate_y(float radians);
cl_m4 cl_m4_rotate_z(float radians);
cl_m4 cl_m4_look_at(cl_v3 eye, cl_v3 target, cl_v3 up);
cl_v3 cl_m4_mul_vec3(cl_m4 m, cl_v3 v);
cl_v4 cl_m4_mul_vec4(cl_m4 m, cl_v4 v);

/* Deterministic hash helpers (splitmix64-style) shared by rng and containers.
 */
uint64_t cl_hash_u64(uint64_t x);
uint64_t cl_hash_bytes(const void *data, size_t len, uint64_t seed);
uint64_t cl_hash_str(cl_str s, uint64_t seed);

/* --- easing functions --- */

typedef enum {
    CL_EASE_LINEAR,
    CL_EASE_IN_QUAD,
    CL_EASE_OUT_QUAD,
    CL_EASE_IN_OUT_QUAD,
    CL_EASE_IN_SINE,
    CL_EASE_OUT_SINE,
    CL_EASE_IN_OUT_SINE,
    CL_EASE_IN_BACK,
    CL_EASE_OUT_BACK,
    CL_EASE_IN_OUT_BACK,
} cl_ease;

/* Map t in [0,1] to an eased factor in [0,1] for the given easing type. */
float cl_ease_apply(cl_ease e, float t);

#endif /* CLAY_CORE_MATH_H */