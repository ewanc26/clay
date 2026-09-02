#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "clay/clay.h"
#include "render/renderer_sw.hpp"
#include "render/scene3d.hpp"

#include <string>

using namespace clay;

namespace {

const char *kScene = R"json({
  "version": 1,
  "settings": { "seed": 7, "fps": 60, "resolution": [320, 240] },
  "meshes": [
    { "name": "cube", "primitive": "cube", "color": [180, 120, 80] },
    { "name": "ball", "uid": "ball01", "primitive": "sphere",
      "radius": 0.6, "color": [60, 120, 220] },
    { "name": "ground", "primitive": "plane", "color": [90, 90, 90] },
    { "name": "tri", "positions": [
        [0, 0, 0], [1, 0, 0], [0, 1, 0]
      ], "indices": [0, 1, 2], "color": [10, 200, 10] }
  ],
  "scene": [
    { "name": "sun", "component": "directional_light",
      "dir": [0.3, 0.5, 0.8], "intensity": 1.0 },
    { "name": "cam", "component": "camera",
      "eye": [0, 0, 6], "target": [0, 0, 0], "fov": 0.9 },
    { "name": "block", "component": "mesh_instance", "mesh": "cube",
      "transform": { "pos": [0, 0, -3], "scale": 1 }, "color": [180, 120, 80] },
    { "name": "pidge", "component": "mesh_instance", "mesh": "ball",
      "mesh_uid": "ball01",
      "transform": { "pos": [1.5, 0, -3.5], "euler": [0, 0.6, 0] },
      "color": [60, 120, 220] }
  ]
})json";

} // namespace

TEST_CASE("scene3d: loads builtin and inline meshes with names") {
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(kScene)));
    CHECK(sc.mesh_count() == 4);
    CHECK(sc.instance_count() == 2);

    /* Builtin cube is 6 faces x 2 triangles. */
    bool saw_cube = false;
    for (const auto &m : sc.meshes()) {
        if (m.name == "cube") {
            saw_cube = true;
            CHECK(m.mesh.indices.size() / 3 == 12);
            CHECK(m.color.r == 180);
        }
        if (m.name == "tri") {
            CHECK(m.mesh.indices.size() / 3 == 1);
            CHECK(m.mesh.positions.size() == 3);
        }
    }
    CHECK(saw_cube);
}

TEST_CASE("scene3d: instance references resolve by name and by uid fallback") {
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(kScene)));

    RendererSW rs(64, 64);
    cl_m4 proj = cl_m4_perspective(0.9f, 1.0f, 0.1f, 10.0f);
    sc.render(rs, cl_m4_identity(), proj);

    /* The center of the framebuffer is behind the camera-facing cube face. */
    uint32_t c = rs.pixel(32, 32);
    CHECK(((c >> 16) & 0xff) > 80); /* warm cube color, not flat background */
    CHECK(rs.touched() > 0);
}

TEST_CASE("scene3d: unsupported version and malformed JSON are rejected") {
    ClayScene sc;
    CHECK(
        !sc.load(cl_str_c("{\"version\": 99, \"meshes\": [], \"scene\": []}")));
    CHECK(!sc.load(cl_str_c("{ malformed")));
}

TEST_CASE("scene3d: camera entity is parsed into view/proj matrices") {
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(kScene)));

    const ClayCamera &cam = sc.camera();
    CHECK(cam.eye.z == doctest::Approx(6.0f).epsilon(1e-3));
    CHECK(cam.target.x == 0.0f);
    CHECK(cam.fov_y_rad == doctest::Approx(0.9f).epsilon(1e-3));

    /* view_matrix maps the eye to the origin in camera space. */
    cl_m4 view = sc.view_matrix();
    cl_v3 eye_cam = cl_m4_mul_vec3(view, cam.eye);
    CHECK(eye_cam.x == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(eye_cam.y == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(eye_cam.z == doctest::Approx(0.0f).epsilon(1e-3));

    /* proj_matrix produces a valid perspective with the scene's fov. */
    cl_m4 proj = sc.proj_matrix(1.0f);
    cl_v4 test = cl_m4_mul_vec4(proj, cl_v4_make(0, 0, -6, 1));
    CHECK(test.w == doctest::Approx(6.0f).epsilon(1e-3));
}

TEST_CASE("scene3d: settings block is parsed") {
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(kScene)));

    CHECK(sc.settings().seed == 7);
    CHECK(sc.settings().fps == 60);
    CHECK(sc.settings().resolution[0] == 320);
    CHECK(sc.settings().resolution[1] == 240);
}

TEST_CASE("scene3d: default camera looks down -Z from z=6") {
    const char *minimal = R"json({
  "version": 1,
  "meshes": [{ "name": "c", "primitive": "cube" }],
  "scene": [{ "component": "mesh_instance", "mesh": "c" }]
})json";
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(minimal)));

    /* No camera entity — defaults should apply. */
    CHECK(sc.camera().eye.z == doctest::Approx(6.0f).epsilon(1e-3));
    CHECK(sc.camera().target.z == 0.0f);

    /* Default view = translate(0,0,-6), so origin maps to z=-6. */
    cl_m4 view = sc.view_matrix();
    cl_v3 origin = cl_m4_mul_vec3(view, cl_v3_make(0, 0, 0));
    CHECK(origin.z == doctest::Approx(-6.0f).epsilon(1e-3));
}

TEST_CASE("scene3d: point_light entity is parsed") {
    const char *with_light = R"json({
  "version": 1,
  "meshes": [{ "name": "c", "primitive": "cube" }],
  "scene": [
    { "component": "point_light", "pos": [1, 2, 3],
      "intensity": 0.7, "attenuation": 0.05 },
    { "component": "mesh_instance", "mesh": "c" }
  ]
})json";
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(with_light)));
    CHECK(sc.point_lights().size() == 1);
    CHECK(sc.point_lights()[0].pos.x == doctest::Approx(1.0f).epsilon(1e-3));
    CHECK(sc.point_lights()[0].intensity == doctest::Approx(0.7f).epsilon(1e-3));
    CHECK(sc.point_lights()[0].attenuation ==
          doctest::Approx(0.05f).epsilon(1e-3));
}