#include "math.h"

#include <string.h>

cl_m4 cl_m4_mul(cl_m4 a, cl_m4 b) {
    cl_m4 r = {{{0}}};
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) sum += a.m[row][k] * b.m[k][col];
            r.m[row][col] = sum;
        }
    }
    return r;
}

cl_m4 cl_m4_ortho(float left, float right, float bottom, float top, float znear,
                  float zfar) {
    cl_m4 m = cl_m4_identity();
    m.m[0][0] = 2.0f / (right - left);
    m.m[1][1] = 2.0f / (top - bottom);
    m.m[2][2] = -2.0f / (zfar - znear);
    m.m[3][0] = -(right + left) / (right - left);
    m.m[3][1] = -(top + bottom) / (top - bottom);
    m.m[3][2] = -(zfar + znear) / (zfar - znear);
    return m;
}

cl_m4 cl_m4_perspective(float vertical_fov_radians, float aspect, float znear,
                        float zfar) {
    /* Row-vector convention (v' = v * M), so the projection sits in column 2
     * (the z/z-homogeneous column) and w' = -z. NDC z maps to [-1, 1]. */
    cl_m4 m = {{0}};
    float f = 1.0f / __builtin_tanf(vertical_fov_radians * 0.5f);
    float zrange = znear - zfar;
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = (zfar + znear) / zrange;
    m.m[2][3] = -1.0f;
    m.m[3][2] = (2.0f * zfar * znear) / zrange;
    return m;
}

cl_m4 cl_m4_translate(float x, float y, float z) {
    cl_m4 m = cl_m4_identity();
    m.m[3][0] = x;
    m.m[3][1] = y;
    m.m[3][2] = z;
    return m;
}

cl_m4 cl_m4_scale(float x, float y, float z) {
    cl_m4 m = cl_m4_identity();
    m.m[0][0] = x;
    m.m[1][1] = y;
    m.m[2][2] = z;
    return m;
}

cl_m4 cl_m4_rotate_z(float radians) {
    cl_m4 m = cl_m4_identity();
    float c = __builtin_cosf(radians);
    float s = __builtin_sinf(radians);
    m.m[0][0] = c;
    m.m[0][1] = s;
    m.m[1][0] = -s;
    m.m[1][1] = c;
    return m;
}

cl_m4 cl_m4_rotate_x(float radians) {
    cl_m4 m = cl_m4_identity();
    float c = __builtin_cosf(radians);
    float s = __builtin_sinf(radians);
    m.m[1][1] = c;
    m.m[1][2] = -s;
    m.m[2][1] = s;
    m.m[2][2] = c;
    return m;
}

cl_m4 cl_m4_rotate_y(float radians) {
    cl_m4 m = cl_m4_identity();
    float c = __builtin_cosf(radians);
    float s = __builtin_sinf(radians);
    m.m[0][0] = c;
    m.m[0][2] = s;
    m.m[2][0] = -s;
    m.m[2][2] = c;
    return m;
}

cl_m4 cl_m4_look_at(cl_v3 eye, cl_v3 target, cl_v3 up) {
    /* Row-vector convention: v' = v * M. The view matrix maps world space to
     * camera space. Camera looks down -Z (OpenGL convention), so forward is
     * normalize(target - eye), right is cross(forward, up), and the camera
     * basis vectors are the rows of the matrix. Translation is -eye in camera
     * coordinates (row 3). */
    cl_v3 f = cl_v3_normalize(cl_v3_sub(target, eye));
    cl_v3 r = cl_v3_normalize(cl_v3_cross(f, up));
    cl_v3 u = cl_v3_cross(r, f);

    cl_m4 m = cl_m4_identity();
    m.m[0][0] = r.x;  m.m[0][1] = u.x;  m.m[0][2] = -f.x;
    m.m[1][0] = r.y;  m.m[1][1] = u.y;  m.m[1][2] = -f.y;
    m.m[2][0] = r.z;  m.m[2][1] = u.z;  m.m[2][2] = -f.z;
    m.m[3][0] = -cl_v3_dot(r, eye);
    m.m[3][1] = -cl_v3_dot(u, eye);
    m.m[3][2] = cl_v3_dot(f, eye);
    return m;
}

cl_v3 cl_m4_mul_vec3(cl_m4 m, cl_v3 v) {
    cl_v4 p = {v.x, v.y, v.z, 1.0f};
    cl_v4 r = cl_m4_mul_vec4(m, p);
    return cl_v3_make(r.x, r.y, r.z);
}

cl_v4 cl_m4_mul_vec4(cl_m4 m, cl_v4 v) {
    const float p[4] = {v.x, v.y, v.z, v.w};
    float out[4] = {0, 0, 0, 0};
    for (int col = 0; col < 4; col++) {
        out[col] = p[0] * m.m[0][col] + p[1] * m.m[1][col] +
                   p[2] * m.m[2][col] + p[3] * m.m[3][col];
    }
    cl_v4 r = {out[0], out[1], out[2], out[3]};
    return r;
}

/* splitmix64 finalizer + stream bits; deterministic everywhere we use it. */
uint64_t cl_hash_u64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

uint64_t cl_hash_bytes(const void *data, size_t len, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed + 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h = cl_hash_u64(h);
    }
    return h;
}

uint64_t cl_hash_str(cl_str s, uint64_t seed) {
    return cl_hash_bytes(s.data, s.len, seed);
}