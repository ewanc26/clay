#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "clay/clay.h"
#include "render/renderer_sw.hpp"
#include "render/scene3d.hpp"

#include <array>
#include <cstring>

using namespace clay;

namespace {

const char *kClayDoc = R"json({
  "version": 1,
  "settings": { "seed": 1337, "fps": 60,
                "render": { "width": 64, "height": 64 } },
  "meshes": [
    { "name": "cube", "primitive": "cube", "color": [180, 120, 80] }
  ],
  "scene": [
    { "name": "light", "component": "directional_light",
      "dir": [0.3, 0.5, 0.8], "intensity": 1.0 },
    { "name": "garden-block", "component": "mesh_instance",
      "mesh": "cube",
      "transform": { "pos": [0, 0, -3], "euler": [0, 0.5, 0], "scale": 1 } }
  ]
})json";

struct Arena {
    std::array<unsigned char, 1 << 16> storage;
    cl_arena a;
    Arena() {
        cl_arena_init(&a, storage.data(), storage.size());
    }
};

void check_schema(cl_json_node *root) {
    REQUIRE(root != nullptr);

    cl_json_node *version = cl_json_get_cstr(root, "version");
    REQUIRE(version != nullptr);
    CHECK(version->kind == CLAY_J_I64);
    CHECK(version->i == 1);

    cl_json_node *seed = cl_json_get_cstr(root, "settings.seed");
    REQUIRE(seed != nullptr);
    CHECK(seed->kind == CLAY_J_I64);
    CHECK(seed->i == 1337);

    cl_json_node *meshes = cl_json_get_cstr(root, "meshes");
    REQUIRE(meshes != nullptr);
    REQUIRE(meshes->kind == CLAY_J_ARR);
    CHECK(meshes->arr.n == 1);
    cl_json_node *mname = cl_json_get_cstr(meshes->arr.items[0], "name");
    REQUIRE(mname != nullptr);
    CHECK(mname->kind == CLAY_J_STR);
    CHECK(mname->s.len == 4);
    CHECK(std::memcmp(mname->s.data, "cube", 4) == 0);
}

} // namespace

TEST_CASE("clayfile: schema syntax loads via the C ABI JSON parser") {
    Arena arena;
    cl_json_node root;
    REQUIRE(cl_json_parse(&root, &arena.a, cl_str_c(kClayDoc)) == CLAY_OK);
    check_schema(&root);
}

TEST_CASE("clayfile: C ABI JSON round-trip remains loadable as a ClayScene") {
    Arena arena;
    cl_json_node root;
    REQUIRE(cl_json_parse(&root, &arena.a, cl_str_c(kClayDoc)) == CLAY_OK);

    cl_str out;
    REQUIRE(cl_json_write(&root, &arena.a, &out) == CLAY_OK);
    REQUIRE(out.len > 0);

    Arena arena2;
    cl_json_node root2;
    REQUIRE(cl_json_parse(&root2, &arena2.a, out) == CLAY_OK);
    check_schema(&root2);

    ClayScene scene;
    CHECK(scene.load(out));
    CHECK(scene.mesh_count() == 1);
    CHECK(scene.instance_count() == 1);
}

TEST_CASE("clayfile: the document itself reaches the renderer end to end") {
    ClayScene scene;
    REQUIRE(scene.load(cl_str_c(kClayDoc)));

    RendererSW rs(64, 64);
    rs.begin_frame(scene.settings().clear);
    scene.render(rs, cl_m4_identity(),
                 cl_m4_perspective(0.9f, 1.0f, 0.1f, 10.0f));
    rs.end_frame();

    CHECK(rs.touched() > 0);
    uint32_t center = rs.pixel(32, 32);
    CHECK(((center >> 16) & 0xffu) > 100u);
}

TEST_CASE("clayfile: malformed syntax fails cleanly at the C ABI") {
    Arena arena;
    cl_json_node root;
    cl_err err = cl_json_parse(&root, &arena.a,
                               cl_str_c("{\"version\": 1, \"scene\": ["));
    CHECK((err == CLAY_ERR_PARSE || err == CLAY_ERR_OOM));
}
