#include "render/raster3d.hpp"

#include "clay/clay.h"

#include <algorithm>
#include <cmath>

namespace clay {

namespace {

constexpr float kNearEps = 1e-6f;

/* ---------- homogeneous frustum clipping (in clip space) ----------
 * Clips a convex polygon against the six frustum planes sequentially. Each
 * plane is defined by a signed distance function d(v) where inside means
 * d >= 0. The near plane uses w > kNearEps to avoid the divide-by-zero; the
 * other five planes use the standard NDC [-1,1] inequalities (x >= -w, x <= w,
 * y >= -w, y <= w, z >= -w, z <= w). */

struct ClipVertex {
    cl_v4 clip;  /* homogeneous clip-space position */
    cl_v3 world; /* world-space for the normal basis */
};

/* Clip a convex polygon (up to 9 vertices after all planes) against one
 * plane. `dist` returns the signed distance: inside when >= 0. */
static int clip_plane(const ClipVertex *in, int n,
                      float (*dist)(const cl_v4 &),
                      ClipVertex *out) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        const ClipVertex &a = in[i];
        const ClipVertex &b = in[(i + 1) % n];
        float da = dist(a.clip);
        float db = dist(b.clip);
        bool a_in = da >= 0.0f;
        bool b_in = db >= 0.0f;
        if (a_in) {
            if (m < 9) out[m++] = a;
        }
        if (a_in != b_in) {
            float t = da / (da - db);
            ClipVertex v;
            v.clip.x = a.clip.x + (b.clip.x - a.clip.x) * t;
            v.clip.y = a.clip.y + (b.clip.y - a.clip.y) * t;
            v.clip.z = a.clip.z + (b.clip.z - a.clip.z) * t;
            v.clip.w = a.clip.w + (b.clip.w - a.clip.w) * t;
            v.world = cl_v3_make(
                a.world.x + (b.world.x - a.world.x) * t,
                a.world.y + (b.world.y - a.world.y) * t,
                a.world.z + (b.world.z - a.world.z) * t);
            if (m < 9) out[m++] = v;
        }
    }
    return m;
}

static float dist_near(const cl_v4 &v)  { return v.w - kNearEps; }
static float dist_left(const cl_v4 &v)  { return v.w + v.x; }
static float dist_right(const cl_v4 &v) { return v.w - v.x; }
static float dist_bottom(const cl_v4 &v) { return v.w + v.y; }
static float dist_top(const cl_v4 &v)   { return v.w - v.y; }
static float dist_far(const cl_v4 &v)   { return v.w + v.z; }

/* Clip a triangle against all six frustum planes. Returns the number of
 * output triangles (0 = fully clipped) and fills out[] with triangle fans. */
static int clip_frustum(const ClipVertex in[3], ClipVertex out[8][3]) {
    ClipVertex buf_a[9], buf_b[9];
    int na, nb;

    /* Start with the input triangle. */
    buf_a[0] = in[0];
    buf_a[1] = in[1];
    buf_a[2] = in[2];
    na = 3;

    /* Chain through all six planes: near, left, right, bottom, top, far. */
    using DistFn = float (*)(const cl_v4 &);
    DistFn planes[] = {dist_near, dist_left, dist_right,
                       dist_bottom, dist_top, dist_far};

    for (int p = 0; p < 6; p++) {
        na = clip_plane(buf_a, na, planes[p], buf_b);
        /* Swap buffers: next plane reads from buf_b, writes to buf_a. */
        std::swap(buf_a, buf_b);
        /* nb is unused but keeps the swap symmetric. */
        (void)nb;
        if (na < 3) return 0;
    }

    /* Fan-triangulate the convex polygon in buf_a. */
    int count = 0;
    for (int i = 1; i + 1 < na && count < 8; i++) {
        out[count][0] = buf_a[0];
        out[count][1] = buf_a[i];
        out[count][2] = buf_a[i + 1];
        count++;
    }
    return count;
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

void build_cube(Mesh3D &out, float half_extent) {
    const float s = half_extent;
    const cl_v3 corners[8] = {
        {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},
        {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s},
    };
    const unsigned faces[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {4, 7, 3, 0},
        {1, 2, 6, 5}, {0, 1, 5, 4}, {3, 7, 6, 2},
    };
    for (auto &f : faces) {
        out.add_triangle(corners[f[0]], corners[f[1]], corners[f[2]]);
        out.add_triangle(corners[f[0]], corners[f[2]], corners[f[3]]);
    }
}

void build_plane(Mesh3D &out, float w, float h, unsigned nx, unsigned ny) {
    /* Axis-aligned, +Y up. Top-left origin, +Y from -h/2 to +h/2. */
    if (nx < 1) nx = 1;
    if (ny < 1) ny = 1;
    const float x0 = -w * 0.5f;
    const float y0 = -h * 0.5f;
    const float dx = w / (float)nx;
    const float dy = h / (float)ny;
    const unsigned cols = nx + 1;
    std::vector<cl_v3> grid((size_t)cols * (size_t)(ny + 1));
    for (unsigned j = 0; j <= ny; j++)
        for (unsigned i = 0; i <= nx; i++)
            grid[(size_t)j * cols + i] =
                cl_v3_make(x0 + (float)i * dx, 0.0f, y0 + (float)j * dy);
    for (unsigned j = 0; j < ny; j++)
        for (unsigned i = 0; i < nx; i++) {
            unsigned a = j * cols + i;
            unsigned b = j * cols + i + 1;
            unsigned c = (j + 1) * cols + i + 1;
            unsigned d = (j + 1) * cols + i;
            /* CCW from +Y: a(c,d),a(d,b) is unwound here as a-b-c then a-c-d
             * with +Y up normal after our rasterizer's winding. */
            out.add_triangle(grid[a], grid[b], grid[c]);
            out.add_triangle(grid[a], grid[c], grid[d]);
        }
}

void build_sphere(Mesh3D &out, float radius, unsigned rings, unsigned slices) {
    if (rings < 2) rings = 2;
    if (slices < 3) slices = 3;
    /* UV-sphere: rings sweep latitude (excluding poles), slices longitude. */
    const unsigned lat = rings;
    const unsigned lon = slices;
    auto idx = [&](unsigned j, unsigned i) { return j * (lon + 1) + i; };
    std::vector<cl_v3> grid((size_t)(lat + 1) * (size_t)(lon + 1));
    for (unsigned j = 0; j <= lat; j++) {
        const float phi =
            -3.14159265f * 0.5f + 3.14159265f * (float)j / (float)lat;
        const float y = std::sin(phi) * radius;
        const float rr = radius * std::cos(phi);
        for (unsigned i = 0; i <= lon; i++) {
            const float th = 2.0f * 3.14159265f * (float)i / (float)lon;
            grid[idx(j, i)] =
                cl_v3_make(std::cos(th) * rr, y, std::sin(th) * rr);
        }
    }
    for (unsigned j = 0; j < lat; j++)
        for (unsigned i = 0; i < lon; i++) {
            unsigned a = idx(j, i);
            unsigned b = idx(j, i + 1);
            unsigned c = idx(j + 1, i + 1);
            unsigned d = idx(j + 1, i);
            out.add_triangle(grid[a], grid[b], grid[c]);
            out.add_triangle(grid[a], grid[c], grid[d]);
        }
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

        ClipVertex clipped[8][3];
        int clip_count = clip_frustum(cv, clipped);

        for (int piece = 0; piece < clip_count; piece++) {
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