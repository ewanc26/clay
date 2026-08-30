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

cl_v3 cl_m4_mul_vec3(cl_m4 m, cl_v3 v) {
    cl_v4 p = {v.x, v.y, v.z, 1.0f};
    float c[4] = {0, 0, 0, 0};
    for (int col = 0; col < 4; col++) {
        c[col] = p.x * m.m[0][col] + p.y * m.m[1][col] +
                 p.z * m.m[2][col] + p.w * m.m[3][col];
    }
    return cl_v3_make(c[0], c[1], c[2]);
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