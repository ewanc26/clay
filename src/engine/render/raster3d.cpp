#include "render/raster3d.hpp"

#include "clay/clay.h"

#include <algorithm>
#include <cmath>

namespace clay {

namespace {

constexpr float kNearEps = 1e-6f;

/* ---------- homogeneous near-plane clipping (in clip space) ----------
 * The view space convention used here (like OpenGL) is -Z forward: after
 * `view`, a point in front of the camera has w = -z > 0. We clip each triangle
 * against w > kNearEps so no vertex behind/at the eye passes to the divide
 * (which would otherwise wrap around at infinity). Triangulates into 0, 1, or
 * 2 output triangles. */

struct ClipVertex {
    cl_v4 clip;  /* homogeneous clip-space position */
    cl_v3 world; /* world-space for the normal basis */
};

static int clip_triangle(const ClipVertex in[3], ClipVertex out[2][3],
                         int *out_count_second) {
    float w[3] = {in[0].clip.w, in[1].clip.w, in[2].clip.w};
    int inside_count = 0;
    for (int i = 0; i < 3; i++) {
        if (w[i] > kNearEps) inside_count++;
    }
    if (inside_count == 0) return 0; /* fully clipped */
    if (inside_count == 3) {         /* fully inside: no clipping needed */
        for (int i = 0; i < 3; i++) out[0][i] = in[i];
        *out_count_second = 0;
        return 1;
    }

    /* One or two vertices inside; clip edges against w == kNearEps. */
    ClipVertex gathered[4];
    int g = 0;
    for (int i = 0; i < 3; i++) {
        const ClipVertex &a = in[i];
        const ClipVertex &b = in[(i + 1) % 3];
        float wa = a.clip.w;
        float wb = b.clip.w;
        bool a_in = wa > kNearEps;
        bool b_in = wb > kNearEps;
        if (a_in) gathered[g++] = a;
        if (a_in != b_in) {
            float t = (kNearEps - wa) / (wb - wa);
            ClipVertex m;
            m.clip.x = a.clip.x + (b.clip.x - a.clip.x) * t;
            m.clip.y = a.clip.y + (b.clip.y - a.clip.y) * t;
            m.clip.z = a.clip.z + (b.clip.z - a.clip.z) * t;
            m.clip.w = kNearEps;
            m.world = cl_v3_make(a.world.x + (b.world.x - a.world.x) * t,
                                 a.world.y + (b.world.y - a.world.y) * t,
                                 a.world.z + (b.world.z - a.world.z) * t);
            gathered[g++] = m;
        }
    }

    /* Fan the gathered polygon (convex, 3 or 4 vertices). */
    if (g < 3) return 0;
    out[0][0] = gathered[0];
    out[0][1] = gathered[1];
    out[0][2] = gathered[2];
    if (g == 4) {
        out[1][0] = gathered[0];
        out[1][1] = gathered[2];
        out[1][2] = gathered[3];
        *out_count_second = 1;
    } else {
        *out_count_second = 0;
    }
    return 1;
}

static inline int32_t round_to_screen(float ndc, int size) {
    /* NDC [-1,1] -> pixel center. Cast to int with a floor bias for
     * determinism; long double is overkill, float precision is fine. */
    float p = (ndc * 0.5f + 0.5f) * (float)size;
    return (int32_t)std::floor(p);
}

/* Signed area * 2. Positive when the triangle is CCW on screen (under our
 * top-left, +y-down convention this flips the usual sign; we use it only for
 * the cull decision, consistently). */
static inline float signed_area2(float ax, float ay, float bx, float by,
                                 float cx, float cy) {
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

} // namespace

void Mesh3D::add_triangle(cl_v3 a, cl_v3 b, cl_v3 c) {
    unsigned base = (unsigned)positions.size();
    positions.push_back(a);
    positions.push_back(b);
    positions.push_back(c);
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
}

void Renderer3D::resize(int width, int height) {
    width_ = width;
    height_ = height;
    depth_.assign((size_t)width * (size_t)height, 1.0f);
}

void Renderer3D::clear() {
    std::fill(depth_.begin(), depth_.end(), 1.0f);
}

Mesh3DStats Renderer3D::draw_mesh(uint32_t *dst, int dst_pitch,
                                  const Mesh3D &mesh, cl_m4 model, cl_m4 view,
                                  cl_m4 proj, Rgba color, cl_v3 light_dir,
                                  float ambient) {
    Mesh3DStats stats;
    if (width_ <= 0 || height_ <= 0 || !dst) return stats;

    cl_m4 mvp = cl_m4_mul(cl_m4_mul(model, view), proj);
    light_dir = cl_v3_normalize(light_dir);

    const size_t tri_count = mesh.indices.size() / 3;
    for (size_t t = 0; t < tri_count; t++) {
        stats.triangles_in++;
        unsigned i0 = mesh.indices[t * 3 + 0];
        unsigned i1 = mesh.indices[t * 3 + 1];
        unsigned i2 = mesh.indices[t * 3 + 2];
        if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() ||
            i2 >= mesh.positions.size()) {
            continue;
        }

        cl_v3 p0 = mesh.positions[i0];
        cl_v3 p1 = mesh.positions[i1];
        cl_v3 p2 = mesh.positions[i2];

        /* World-space positions + object-space face normal for lighting. */
        cl_v3 w0 = cl_m4_mul_vec3(model, p0);
        cl_v3 w1 = cl_m4_mul_vec3(model, p1);
        cl_v3 w2 = cl_m4_mul_vec3(model, p2);

        /* Normals: rotate the object-space normal into world space using the
         * model's linear part (uniform scale is presumed for flat shading). */
        cl_v3 normal =
            cl_v3_normalize(cl_v3_cross(cl_v3_sub(p1, p0), cl_v3_sub(p2, p0)));
        cl_v3 world_normal = {
            cl_v3_dot({model.m[0][0], model.m[1][0], model.m[2][0]}, normal),
            cl_v3_dot({model.m[0][1], model.m[1][1], model.m[2][1]}, normal),
            cl_v3_dot({model.m[0][2], model.m[1][2], model.m[2][2]}, normal)};
        world_normal = cl_v3_normalize(world_normal);

        ClipVertex cv[3];
        cv[0].clip = cl_m4_mul_vec4(mvp, cl_v4_make(p0.x, p0.y, p0.z, 1.0f));
        cv[1].clip = cl_m4_mul_vec4(mvp, cl_v4_make(p1.x, p1.y, p1.z, 1.0f));
        cv[2].clip = cl_m4_mul_vec4(mvp, cl_v4_make(p2.x, p2.y, p2.z, 1.0f));
        cv[0].world = w0;
        cv[1].world = w1;
        cv[2].world = w2;

        ClipVertex clipped[2][3];
        int second = 0;
        int primary = clip_triangle(cv, clipped, &second);

        for (int piece = 0; piece < (primary + second); piece++) {
            ClipVertex *tri = clipped[piece];
            cl_v4 c0 = tri[0].clip;
            cl_v4 c1 = tri[1].clip;
            cl_v4 c2 = tri[2].clip;

            /* Perspective divide -> NDC. */
            float w0d = 1.0f / c0.w;
            float w1d = 1.0f / c1.w;
            float w2d = 1.0f / c2.w;
            float ndc[3][2] = {{c0.x * w0d, c0.y * w0d},
                               {c1.x * w1d, c1.y * w1d},
                               {c2.x * w2d, c2.y * w2d}};
            /* Depth: NDC z in [-1,1] remapped to [0,1] for the depth buffer
             * (1.0 = far). Nearer (larger w / smaller z) wins. */
            float depth[3] = {(c0.z * w0d) * 0.5f + 0.5f,
                              (c1.z * w1d) * 0.5f + 0.5f,
                              (c2.z * w2d) * 0.5f + 0.5f};

            /* Viewport transform (top-left origin, +y down). */
            float sx[3] = {(float)round_to_screen(ndc[0][0], width_),
                           (float)round_to_screen(ndc[1][0], width_),
                           (float)round_to_screen(ndc[2][0], width_)};
            float sy[3] = {(float)round_to_screen(-ndc[0][1], height_),
                           (float)round_to_screen(-ndc[1][1], height_),
                           (float)round_to_screen(-ndc[2][1], height_)};

            /* Backface cull by screen winding. Screen y is downward, so the
             * front-facing convention flips the naive cross-product sign. */
            float area = signed_area2(sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
            if (area >= 0.0f) continue;

            /* Flat shading: clamp diffuse by the world-space normal. */
            float diff = std::max(0.0f, cl_v3_dot(world_normal, light_dir));
            float shade = ambient + (1.0f - ambient) * diff;
            shade = std::min(1.0f, std::max(0.0f, shade));

            stats.triangles_rasterized++;

            /* Bounding box for the fill, clamped to the viewport. */
            int min_x = (int)std::max(
                0.0f, std::floor(std::min({sx[0], sx[1], sx[2]})));
            int max_x =
                (int)std::min((float)width_ - 1.0f,
                              std::ceil(std::max({sx[0], sx[1], sx[2]})));
            int min_y = (int)std::max(
                0.0f, std::floor(std::min({sy[0], sy[1], sy[2]})));
            int max_y =
                (int)std::min((float)height_ - 1.0f,
                              std::ceil(std::max({sy[0], sy[1], sy[2]})));

            /* Shaded flat color (fully opaque). */
            uint8_t sr = (uint8_t)std::min(255.0f, (float)color.r * shade);
            uint8_t sg = (uint8_t)std::min(255.0f, (float)color.g * shade);
            uint8_t sb = (uint8_t)std::min(255.0f, (float)color.b * shade);
            uint32_t px = ((uint32_t)255u << 24) | ((uint32_t)sr << 16) |
                          ((uint32_t)sg << 8) | (uint32_t)sb;

            for (int y = min_y; y <= max_y; y++) {
                for (int x = min_x; x <= max_x; x++) {
                    /* Barycentric coords normalized by the (signed) triangle
                     * area. The pixel sample (x+0.5, y+0.5) is inside when all
                     * three barycentrics are >= 0. */
                    float l0 = signed_area2(sx[1], sy[1], sx[2], sy[2],
                                            (float)x + 0.5f, (float)y + 0.5f) /
                               area;
                    float l1 = signed_area2(sx[2], sy[2], sx[0], sy[0],
                                            (float)x + 0.5f, (float)y + 0.5f) /
                               area;
                    float l2 = 1.0f - l0 - l1;
                    if (l0 < 0.0f || l1 < 0.0f || l2 < 0.0f) continue;

                    float d = l0 * depth[0] + l1 * depth[1] + l2 * depth[2];
                    size_t idx = (size_t)(y * width_ + x);
                    if (d < depth_[idx] && d >= 0.0f && d <= 1.0f) {
                        depth_[idx] = d;
                        dst[y * dst_pitch + x] = px;
                        stats.pixels_written++;
                    }
                }
            }
        }
    }
    return stats;
}

} // namespace clay