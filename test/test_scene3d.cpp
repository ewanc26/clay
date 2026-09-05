#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "clay/clay.h"
#include "render/renderer_sw.hpp"
#include "render/scene3d.hpp"

using namespace clay;

namespace {

const char *kScene = R"json({
  "version": 1,
  "settings": {
    "seed": 7,
    "fps": 60,
    "render": { "width": 320, "height": 240, "clear": [12, 14, 18, 255] }
  },
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
      "transform": { "pos": [0, 0, -3], "scale": 1 } },
    { "name": "pidge", "component": "mesh_instance", "mesh": "missing",
      "mesh_uid": "ball01",
      "transform": { "pos": [1.5, 0, -3.5], "euler": [0, 0.6, 0] },
      "color": [60, 120, 220] }
  ]
})json";

uint64_t framebuffer_hash(const Framebuffer &fb) {
    uint64_t h = 1469598103934665603ULL;
    for (uint32_t v : fb.pixels) {
        for (int i = 0; i < 4; i++) {
            h ^= (uint64_t)(v & 0xffu);
            h *= 1099511628211ULL;
            v >>= 8;
        }
    }
    return h;
}

uint32_t render_center(ClayScene &sc) {
    RendererSW rs(64, 64);
    rs.begin_frame(sc.settings().clear);
    sc.render(rs, cl_m4_identity(),
              cl_m4_perspective(0.9f, 1.0f, 0.1f, 20.0f));
    rs.end_frame();
    return rs.pixel(32, 32);
}

} // namespace

TEST_CASE("scene3d: loads builtin and inline meshes with stable ids") {
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(kScene)));
    CHECK(sc.mesh_count() == 4);
    CHECK(sc.instance_count() == 2);

    bool saw_cube = false;
    bool saw_inline = false;
    for (const auto &m : sc.meshes()) {
        if (m.name == "cube") {
            saw_cube = true;
            CHECK(m.mesh.indices.size() / 3 == 12);
            CHECK(m.color.r == 180);
        }
        if (m.name == "tri") {
            saw_inline = true;
            CHECK(m.mesh.indices.size() == 3);
            CHECK(m.mesh.positions.size() == 3);
        }
    }
    CHECK(saw_cube);
    CHECK(saw_inline);
}

TEST_CASE("scene3d: uid resolves before name and supports a missing-name fallback") {
    const char *doc = R"json({
      "version": 1,
      "meshes": [
        { "name": "wrong", "uid": "red", "primitive": "cube",
          "color": [220, 30, 30] },
        { "name": "right", "uid": "blue", "primitive": "cube",
          "color": [30, 30, 220] }
      ],
      "scene": [
        { "component": "mesh_instance", "mesh": "wrong", "mesh_uid": "blue",
          "transform": { "pos": [0, 0, -3] } },
        { "component": "mesh_instance", "mesh": "does-not-exist",
          "mesh_uid": "blue", "transform": { "pos": [3, 0, -3] } }
      ]
    })json";

    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(doc)));
    uint32_t center = render_center(sc);
    CHECK((center & 0xffu) > 100u);
    CHECK(((center >> 16) & 0xffu) < 100u);
}

TEST_CASE("scene3d: version is mandatory and recognised fields are strict") {
    ClayScene sc;
    CHECK(!sc.load(cl_str_c("{\"meshes\":[],\"scene\":[]}")));
    CHECK(!sc.load(cl_str_c(
        "{\"version\":\"1\",\"meshes\":[],\"scene\":[]}")));
    CHECK(!sc.load(cl_str_c(
        "{\"version\":99,\"meshes\":[],\"scene\":[]}")));
    CHECK(!sc.load(cl_str_c(
        "{\"version\":1,\"settings\":{\"fps\":\"60\"}}")));
    CHECK(!sc.load(cl_str_c(
        "{\"version\":1,\"meshes\":[{\"name\":\"x\",\"primitive\":\"wat\"}]}")));
    CHECK(!sc.load(cl_str_c(
        "{\"version\":1,\"meshes\":[{\"name\":\"x\",\"positions\":[[0,0,0]],\"indices\":[0,1,2]}]}")));
    CHECK(!sc.load(cl_str_c(
        "{\"version\":1,\"scene\":[{\"component\":\"mesh_instance\",\"mesh\":\"missing\"}]}")));
    CHECK(!sc.load(cl_str_c("{ malformed")));
}

TEST_CASE("scene3d: duplicate stable ids and ambiguous point lights are rejected") {
    ClayScene sc;
    CHECK(!sc.load(cl_str_c(R"json({
      "version": 1,
      "meshes": [
        { "name": "a", "uid": "same", "primitive": "cube" },
        { "name": "b", "uid": "same", "primitive": "cube" }
      ]
    })json")));
    CHECK(!sc.load(cl_str_c(R"json({
      "version": 1,
      "scene": [
        { "component": "point_light" },
        { "component": "point_light" }
      ]
    })json")));
}

TEST_CASE("scene3d: scalar scale and SRT composition preserve world position") {
    const char *doc = R"json({
      "version": 1,
      "meshes": [{ "name": "c", "primitive": "cube" }],
      "scene": [{
        "component": "mesh_instance", "mesh": "c",
        "transform": { "pos": [3, 4, -5], "euler": [0.2, 0.7, -0.1], "scale": 2 }
      }]
    })json";
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(doc)));
    REQUIRE(sc.instances().size() == 1);

    cl_m4 model = sc.instances()[0].model;
    cl_v3 origin = cl_m4_mul_vec3(model, cl_v3_make(0, 0, 0));
    CHECK(origin.x == doctest::Approx(3.0f).epsilon(1e-4));
    CHECK(origin.y == doctest::Approx(4.0f).epsilon(1e-4));
    CHECK(origin.z == doctest::Approx(-5.0f).epsilon(1e-4));

    cl_v3 x = cl_m4_mul_vec3(model, cl_v3_make(1, 0, 0));
    CHECK(cl_v3_length(cl_v3_sub(x, origin)) ==
          doctest::Approx(2.0f).epsilon(1e-4));
}

TEST_CASE("scene3d: mesh colour is inherited when instance colour is omitted") {
    const char *doc = R"json({
      "version": 1,
      "meshes": [{ "name": "c", "primitive": "cube", "color": [20, 40, 220] }],
      "scene": [{ "component": "mesh_instance", "mesh": "c",
                  "transform": { "pos": [0, 0, -3] } }]
    })json";
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(doc)));
    uint32_t center = render_center(sc);
    CHECK((center & 0xffu) > ((center >> 16) & 0xffu));
}

TEST_CASE("scene3d: canonical render settings and legacy resolution alias load") {
    ClayScene canonical;
    REQUIRE(canonical.load(cl_str_c(kScene)));
    CHECK(canonical.settings().has_seed);
    CHECK(canonical.settings().seed == 7);
    CHECK(canonical.settings().fps == 60);
    CHECK(canonical.settings().resolution[0] == 320);
    CHECK(canonical.settings().resolution[1] == 240);
    CHECK(canonical.settings().has_resolution);
    CHECK(canonical.settings().clear.r == 12);

    ClayScene legacy;
    REQUIRE(legacy.load(cl_str_c(
        "{\"version\":1,\"settings\":{\"resolution\":[80,60]}}")));
    CHECK(legacy.settings().resolution[0] == 80);
    CHECK(legacy.settings().resolution[1] == 60);
    CHECK(legacy.settings().has_resolution);

    ClayScene defaults;
    REQUIRE(defaults.load(cl_str_c("{\"version\":1}")));
    CHECK_FALSE(defaults.settings().has_resolution);
}

TEST_CASE("scene3d: camera entity is parsed into view and projection matrices") {
    ClayScene sc;
    REQUIRE(sc.load(cl_str_c(kScene)));

    const ClayCamera &cam = sc.camera();
    CHECK(cam.eye.z == doctest::Approx(6.0f).epsilon(1e-3));
    CHECK(cam.fov_y_rad == doctest::Approx(0.9f).epsilon(1e-3));

    cl_v3 eye_cam = cl_m4_mul_vec3(sc.view_matrix(), cam.eye);
    CHECK(eye_cam.x == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(eye_cam.y == doctest::Approx(0.0f).epsilon(1e-3));
    CHECK(eye_cam.z == doctest::Approx(0.0f).epsilon(1e-3));

    cl_v4 projected =
        cl_m4_mul_vec4(sc.proj_matrix(1.0f), cl_v4_make(0, 0, -6, 1));
    CHECK(projected.w == doctest::Approx(6.0f).epsilon(1e-3));
}

TEST_CASE("scene3d: a loaded scene renders deterministically") {
    ClayScene a;
    ClayScene b;
    REQUIRE(a.load(cl_str_c(kScene)));
    REQUIRE(b.load(cl_str_c(kScene)));

    RendererSW ra(96, 72);
    RendererSW rb(96, 72);
    ra.begin_frame(a.settings().clear);
    rb.begin_frame(b.settings().clear);
    cl_m4 proj = cl_m4_perspective(0.9f, 96.0f / 72.0f, 0.1f, 20.0f);
    a.render(ra, cl_m4_identity(), proj);
    b.render(rb, cl_m4_identity(), proj);
    ra.end_frame();
    rb.end_frame();

    CHECK(framebuffer_hash(ra.framebuffer()) == framebuffer_hash(rb.framebuffer()));
}
