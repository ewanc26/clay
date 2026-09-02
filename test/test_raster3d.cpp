#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "clay/clay.h"
#include "render/raster3d.hpp"
#include "render/renderer_sw.hpp"

using namespace clay;

namespace {

constexpr int W = 64;
constexpr int H = 64;
constexpr Rgba kCubeColor = {180, 120, 80, 255};

cl_m4 identity_view() {
    return cl_m4_identity();
}

cl_m4 standard_proj() {
    return cl_m4_perspective(0.9f, 1.0f, 0.1f, 10.0f);
}

Mesh3D unit_cube() {
    Mesh3D m;
    build_cube(m, 0.5f);
    return m;
}

uint32_t screen_at(const uint32_t *fb, int x, int y) {
    return fb[(size_t)y * W + (size_t)x];
}

int pixel_r(uint32_t p) {
    return (p >> 16) & 0xff;
}

int pixel_b(uint32_t p) {
    return p & 0xff;
}

cl_v3 triangle_normal(const Mesh3D &m, size_t tri) {
    unsigned a = m.indices[tri * 3 + 0];
    unsigned b = m.indices[tri * 3 + 1];
    unsigned c = m.indices[tri * 3 + 2];
    return cl_v3_normalize(cl_v3_cross(cl_v3_sub(m.positions[b], m.positions[a]),
                                       cl_v3_sub(m.positions[c], m.positions[a])));
}

} // namespace

TEST_CASE("raster3d: perspective projection places a centered point in NDC") {
    cl_v4 c =
        cl_m4_mul_vec4(standard_proj(), cl_v4_make(0.0f, 0.0f, -2.0f, 1.0f));
    const float nx = c.x / c.w;
    const float ny = c.y / c.w;
    const float nz = c.z / c.w;
    CHECK(nx == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(ny == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(nz > 0.5f);
    CHECK(nz < 1.0f);
    CHECK(c.w > 0.0f);
}

TEST_CASE("raster3d: a cube renders opaque front faces, no background holes") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0xFF2020FF);

    r3.clear();
    Mesh3DStats stats = r3.draw_mesh(
        fb.data(), W, unit_cube(), cl_m4_translate(0.0f, 0.0f, -3.0f),
        identity_view(), standard_proj(), kCubeColor);

    CHECK(stats.pixels_written > 0);
    CHECK(stats.triangles_rasterized > 0);
    CHECK(pixel_r(screen_at(fb.data(), W / 2, H / 2)) > 100);
}

TEST_CASE("raster3d: depth survives multiple Renderer3D mesh draws") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0xFF000000);

    Mesh3D near_q;
    near_q.add_triangle({-1.0f, -1.0f, -2.0f}, {1.0f, -1.0f, -2.0f},
                        {1.0f, 1.0f, -2.0f});
    near_q.add_triangle({-1.0f, -1.0f, -2.0f}, {1.0f, 1.0f, -2.0f},
                        {-1.0f, 1.0f, -2.0f});
    Mesh3D far_q;
    far_q.add_triangle({-2.0f, -2.0f, -4.0f}, {2.0f, -2.0f, -4.0f},
                       {2.0f, 2.0f, -4.0f});
    far_q.add_triangle({-2.0f, -2.0f, -4.0f}, {2.0f, 2.0f, -4.0f},
                       {-2.0f, 2.0f, -4.0f});

    r3.clear();
    r3.draw_mesh(fb.data(), W, near_q, cl_m4_identity(), identity_view(),
                 standard_proj(), {0, 0, 255, 255});
    r3.draw_mesh(fb.data(), W, far_q, cl_m4_identity(), identity_view(),
                 standard_proj(), {255, 0, 0, 255});

    uint32_t center = screen_at(fb.data(), W / 2, H / 2);
    CHECK(pixel_b(center) > 100);
    CHECK(pixel_r(center) < 100);
}

TEST_CASE("raster3d: RendererSW keeps one depth buffer for the whole frame") {
    RendererSW rs(W, H);
    rs.begin_frame({0, 0, 0, 255});

    Mesh3D near_q;
    near_q.add_triangle({-1.0f, -1.0f, -2.0f}, {1.0f, -1.0f, -2.0f},
                        {1.0f, 1.0f, -2.0f});
    near_q.add_triangle({-1.0f, -1.0f, -2.0f}, {1.0f, 1.0f, -2.0f},
                        {-1.0f, 1.0f, -2.0f});
    Mesh3D far_q;
    far_q.add_triangle({-2.0f, -2.0f, -4.0f}, {2.0f, -2.0f, -4.0f},
                       {2.0f, 2.0f, -4.0f});
    far_q.add_triangle({-2.0f, -2.0f, -4.0f}, {2.0f, 2.0f, -4.0f},
                       {-2.0f, 2.0f, -4.0f});

    rs.draw_mesh(near_q, cl_m4_identity(), identity_view(), standard_proj(),
                 {0, 0, 255, 255});
    rs.draw_mesh(far_q, cl_m4_identity(), identity_view(), standard_proj(),
                 {255, 0, 0, 255});
    rs.end_frame();

    uint32_t center = rs.pixel(W / 2, H / 2);
    CHECK(pixel_b(center) > 100);
    CHECK(pixel_r(center) < 100);
}

TEST_CASE("raster3d: geometry beyond zfar is clipped before rasterization") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0);
    r3.clear();

    Mesh3D beyond;
    beyond.add_triangle({-1.0f, -1.0f, -20.0f}, {1.0f, -1.0f, -20.0f},
                        {0.0f, 1.0f, -20.0f});
    Mesh3DStats stats = r3.draw_mesh(fb.data(), W, beyond, cl_m4_identity(),
                                     identity_view(), standard_proj(),
                                     kCubeColor);
    CHECK(stats.triangles_rasterized == 0);
    CHECK(stats.pixels_written == 0);
}

TEST_CASE("raster3d: plane generator is outward CCW from +Y") {
    Mesh3D plane;
    build_plane(plane, 2.0f, 2.0f, 1, 1);
    REQUIRE(plane.indices.size() == 6);
    cl_v3 n0 = triangle_normal(plane, 0);
    cl_v3 n1 = triangle_normal(plane, 1);
    CHECK(n0.y > 0.99f);
    CHECK(n1.y > 0.99f);
}

TEST_CASE("raster3d: sphere generator normals point away from its centre") {
    Mesh3D sphere;
    build_sphere(sphere, 1.0f, 8, 12);
    REQUIRE(!sphere.empty());

    size_t nondegenerate = 0;
    for (size_t tri = 0; tri < sphere.indices.size() / 3; tri++) {
        unsigned ia = sphere.indices[tri * 3 + 0];
        unsigned ib = sphere.indices[tri * 3 + 1];
        unsigned ic = sphere.indices[tri * 3 + 2];
        cl_v3 a = sphere.positions[ia];
        cl_v3 b = sphere.positions[ib];
        cl_v3 c = sphere.positions[ic];
        cl_v3 cross = cl_v3_cross(cl_v3_sub(b, a), cl_v3_sub(c, a));
        if (cl_v3_length(cross) < 1e-5f) continue;
        nondegenerate++;
        cl_v3 normal = cl_v3_normalize(cross);
        cl_v3 centroid =
            cl_v3_scale(cl_v3_add(cl_v3_add(a, b), c), 1.0f / 3.0f);
        CHECK(cl_v3_dot(normal, centroid) > 0.0f);
    }
    CHECK(nondegenerate > 0);
}

TEST_CASE("raster3d: backfaces are culled, no pixels from a rear-facing tri") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0);
    r3.clear();

    Mesh3D back;
    back.add_triangle({-1.5f, -1.5f, -3.0f}, {-1.5f, 1.5f, -3.0f},
                      {1.5f, 1.5f, -3.0f});
    auto s = r3.draw_mesh(fb.data(), W, back, cl_m4_identity(), identity_view(),
                          standard_proj(), kCubeColor);
    CHECK(s.pixels_written == 0);
}

TEST_CASE("raster3d: vertex behind the eye is clipped without wrapping") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0xFF000000);
    r3.clear();

    Mesh3D t;
    t.add_triangle({-1.0f, -1.0f, -3.0f}, {1.0f, -1.0f, -3.0f},
                   {0.0f, 1.0f, 0.5f});
    auto stats = r3.draw_mesh(fb.data(), W, t, cl_m4_identity(),
                              identity_view(), standard_proj(), kCubeColor);
    CHECK(stats.pixels_written > 0);
}

TEST_CASE("raster3d: cube renders through the RendererSW surface") {
    RendererSW rs(W, H);
    rs.begin_frame({0x20, 0x20, 0x20, 255});
    rs.draw_mesh(unit_cube(), cl_m4_translate(0.0f, 0.0f, -3.0f),
                 identity_view(), standard_proj(), kCubeColor);
    rs.end_frame();

    CHECK(pixel_r(rs.pixel(W / 2, H / 2)) > 100);
    CHECK(rs.touched() > 0);
}

TEST_CASE("raster3d: light intensity scales the diffuse term") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0);

    cl_m4 model = cl_m4_translate(0.0f, 0.0f, -3.0f);
    r3.clear();
    r3.draw_mesh(fb.data(), W, unit_cube(), model, identity_view(),
                 standard_proj(), kCubeColor, {0.0f, 0.0f, -1.0f}, 0.0f,
                 cl_v3_make(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.35f);

    int red = pixel_r(screen_at(fb.data(), W / 2, H / 2));
    CHECK(red > 50);
    CHECK(red < 150);
}

TEST_CASE("raster3d: point light adds brightness near its position") {
    Renderer3D r3;
    r3.resize(W, H);
    cl_m4 model = cl_m4_translate(0.0f, 0.0f, -3.0f);

    std::vector<uint32_t> fb_near((size_t)W * H, 0);
    r3.clear();
    Mesh3DStats near_stats = r3.draw_mesh(
        fb_near.data(), W, unit_cube(), model, identity_view(),
        standard_proj(), kCubeColor, {0.0f, 0.0f, 1.0f}, 0.0f,
        cl_v3_make(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.35f);

    std::vector<uint32_t> fb_far((size_t)W * H, 0);
    r3.clear();
    r3.draw_mesh(fb_far.data(), W, unit_cube(), model, identity_view(),
                 standard_proj(), kCubeColor, {0.0f, 0.0f, 1.0f}, 0.0f,
                 cl_v3_make(0.0f, 0.0f, 8.0f), 1.0f, 0.3f, 0.35f);

    CHECK(near_stats.pixels_written > 0);
    CHECK(pixel_r(screen_at(fb_near.data(), W / 2, H / 2)) >
          pixel_r(screen_at(fb_far.data(), W / 2, H / 2)));
}
