#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "clay/clay.h"
#include "render/raster3d.hpp"
#include "render/renderer_sw.hpp"

#include <cstring>

using namespace clay;

namespace {

constexpr int W = 64;
constexpr int H = 64;
constexpr Rgba kCubeColor = {180, 120, 80, 255};

/* A camera looking down -Z from +Z. Keeps the math independent of the view
 * matrix so tests focus on projection + rasterization. */
cl_m4 identity_view() {
    return cl_m4_identity();
}

cl_m4 standard_proj() {
    return cl_m4_perspective(0.9f /* ~51 degree fov */, 1.0f, 0.1f, 10.0f);
}

/* Unit cube centered at the origin. */
Mesh3D unit_cube() {
    Mesh3D m;
    const float s = 0.5f;
    const cl_v3 corners[8] = {
        {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},
        {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s},
    };
    /* Outward-facing CCW-mesh windings (generated: each fan's triangle normals
     * point along the face's outward axis). The camera looks down -Z, so the
     * +Z face {4,5,6,7} is the one facing the viewer. */
    const unsigned faces[6][4] = {
        {0, 3, 2, 1}, /* -z */
        {4, 5, 6, 7}, /* +z (toward camera) */
        {4, 7, 3, 0}, /* -x */
        {1, 2, 6, 5}, /* +x */
        {0, 1, 5, 4}, /* -y */
        {3, 7, 6, 2}, /* +y */
    };
    for (auto &f : faces) {
        m.add_triangle(corners[f[0]], corners[f[1]], corners[f[2]]);
        m.add_triangle(corners[f[0]], corners[f[2]], corners[f[3]]);
    }
    return m;
}

uint32_t screen_at(const uint32_t *fb, int x, int y) {
    return fb[(size_t)y * W + (size_t)x];
}

int pixel_r(uint32_t p) {
    return (p >> 16) & 0xff;
}

} // namespace

TEST_CASE("raster3d: perspective projection places a centered point in NDC") {
    /* A point on the view axis, in front of the camera. Compute the NDC
     * position explicitly (mul_vec4 + perspective divide). */
    cl_v4 c =
        cl_m4_mul_vec4(standard_proj(), cl_v4_make(0.0f, 0.0f, -2.0f, 1.0f));
    const float nx = c.x / c.w;
    const float ny = c.y / c.w;
    const float nz = c.z / c.w;
    CHECK(nx == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(ny == doctest::Approx(0.0f).epsilon(1e-4f));
    /* -Z forward; NDC z positive and magnitude < 1 (near=0.1, far=10). */
    CHECK(nz > 0.5f);
    CHECK(nz < 1.0f);
    CHECK(c.w > 0.0f); /* point in front of the eye */
}

TEST_CASE("raster3d: a cube renders opaque front faces, no background holes") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0xFF2020FF); /* distinct bg */

    r3.clear();
    cl_m4 model = cl_m4_translate(0.0f, 0.0f, -3.0f);
    Mesh3DStats stats =
        r3.draw_mesh(fb.data(), W, unit_cube(), model, identity_view(),
                     standard_proj(), kCubeColor);

    /* There is at least one cube face facing the +z view direction at center.
     */
    CHECK(stats.pixels_written > 0);
    CHECK(stats.triangles_rasterized > 0);

    /* The exact center pixel must be cube-colored (a front face covers it). */
    uint32_t c = screen_at(fb.data(), W / 2, H / 2);
    CHECK(pixel_r(c) > 100); /* lit warm cube color, not the blue bg */
}

TEST_CASE("raster3d: near and far quads compose by depth (near wins)") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0xFF000000);

    /* A big far quad + a small near quad overlapping the center. */
    Mesh3D far_q;
    far_q.add_triangle({-2.0f, -2.0f, -4.0f}, {2.0f, -2.0f, -4.0f},
                       {2.0f, 2.0f, -4.0f});
    far_q.add_triangle({-2.0f, -2.0f, -4.0f}, {2.0f, 2.0f, -4.0f},
                       {-2.0f, 2.0f, -4.0f});
    Mesh3D near_q;
    near_q.add_triangle({-1.0f, -1.0f, -2.0f}, {1.0f, -1.0f, -2.0f},
                        {1.0f, 1.0f, -2.0f});
    near_q.add_triangle({-1.0f, -1.0f, -2.0f}, {1.0f, 1.0f, -2.0f},
                        {-1.0f, 1.0f, -2.0f});

    r3.clear();
    r3.draw_mesh(fb.data(), W, far_q, cl_m4_identity(), identity_view(),
                 standard_proj(), {255, 0, 0, 255});
    r3.draw_mesh(fb.data(), W, near_q, cl_m4_identity(), identity_view(),
                 standard_proj(), {0, 0, 255, 255});

    /* The overlap point should be the near (blue) quad, not the far red. */
    CHECK(pixel_r(screen_at(fb.data(), W / 2, H / 2)) < 100);
}

TEST_CASE("raster3d: backfaces are culled, no pixels from a rear-facing tri") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0);
    r3.clear();

    /* A triangle wound clockwise from the view (back-facing under the
     * CCW-front convention) covering the whole screen. */
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

    /* A triangle straddling the camera: two verts on-screen in front (-Z),
     * one vertex clearly behind the eye (positive Z -> w < 0). Clipping must
     * keep the front sliver and must not let a divide-by-~0 wrap around. */
    Mesh3D t;
    t.add_triangle({-1.0f, -1.0f, -3.0f}, {1.0f, -1.0f, -3.0f},
                   {0.0f, 1.0f, 0.5f});
    auto stats = r3.draw_mesh(fb.data(), W, t, cl_m4_identity(),
                              identity_view(), standard_proj(), kCubeColor);

    /* A clipped sliver should still rasterize some valid pixels. */
    CHECK(stats.pixels_written > 0);
}

TEST_CASE("raster3d: cube renders through the RendererSW surface") {
    /* Exercise the IRenderer::draw_mesh path end to end. */
    RendererSW rs(W, H);
    rs.begin_frame({0x20, 0x20, 0x20, 255});
    rs.draw_mesh(unit_cube(), cl_m4_translate(0.0f, 0.0f, -3.0f),
                 identity_view(), standard_proj(), kCubeColor);
    rs.end_frame();

    /* Center pixel is covered by the lit camera-facing cube face. */
    uint32_t c = rs.pixel(W / 2, H / 2);
    CHECK(pixel_r(c) > 100);
    CHECK(rs.touched() > 0);
}

TEST_CASE("raster3d: light intensity scales the diffuse term") {
    Renderer3D r3;
    r3.resize(W, H);
    std::vector<uint32_t> fb((size_t)W * H, 0);

    cl_m4 model = cl_m4_translate(0.0f, 0.0f, -3.0f);
    cl_v3 dark_dir = {0.0f, 0.0f, -1.0f};

    r3.clear();
    r3.draw_mesh(fb.data(), W, unit_cube(), model, identity_view(),
                 standard_proj(), kCubeColor, dark_dir, 0.0f,
                 cl_v3_make(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 0.35f);

    uint32_t c = screen_at(fb.data(), W / 2, H / 2);
    CHECK(pixel_r(c) > 50);
    CHECK(pixel_r(c) < 150);
}

TEST_CASE("raster3d: point light adds brightness near its position") {
    Renderer3D r3;
    r3.resize(W, H);

    cl_m4 model = cl_m4_translate(0.0f, 0.0f, -3.0f);
    cl_v3 no_dir = {0.0f, 0.0f, 1.0f};

    /* Near point light in front of the +Z face (centroid at z=-2.5, normal
     * +Z): 2.5 units away, no attenuation → bright. */
    std::vector<uint32_t> fb_near((size_t)W * H, 0);
    r3.clear();
    Mesh3DStats near_stats = r3.draw_mesh(
        fb_near.data(), W, unit_cube(), model, identity_view(),
        standard_proj(), kCubeColor, no_dir, 0.0f,
        cl_v3_make(0.0f, 0.0f, 0.0f), 1.0f, 0.0f, 0.35f);

    /* Far point light also in front of the face, 10.5 units away with
     * attenuation → dimmer. */
    std::vector<uint32_t> fb_far((size_t)W * H, 0);
    r3.clear();
    Mesh3DStats far_stats = r3.draw_mesh(
        fb_far.data(), W, unit_cube(), model, identity_view(),
        standard_proj(), kCubeColor, no_dir, 0.0f,
        cl_v3_make(0.0f, 0.0f, 8.0f), 1.0f, 0.3f, 0.35f);

    uint32_t near_c = screen_at(fb_near.data(), W / 2, H / 2);
    uint32_t far_c = screen_at(fb_far.data(), W / 2, H / 2);

    int max_x = 0, max_y = 0;
    uint32_t max_val = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint32_t v = screen_at(fb_near.data(), x, y);
            if (pixel_r(v) > (int)pixel_r(max_val)) {
                max_val = v;
                max_x = x;
                max_y = y;
            }
        }
    }

    CHECK(near_stats.pixels_written > 0);
    CHECK(pixel_r(max_val) > 0);
    CHECK(pixel_r(near_c) > pixel_r(far_c));
}