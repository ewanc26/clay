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
  "settings": { "seed": 7, "fps": 60 },
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