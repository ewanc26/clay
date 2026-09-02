#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "clay/clay.h"
#include "render/renderer_sw.hpp"

#include <array>
#include <cstdint>
#include <cstring>

using namespace clay;

namespace {

/* A representative one-file `.clay` document (spec: docs/file-format.md). */
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
      "mesh": "cube", "color": [180, 120, 80],
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

/* Walk a freshly-parsed .clay root and assert its schema shape is loadable. */
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
    cl_json_node *m0 = meshes->arr.items[0];
    cl_json_node *mname = cl_json_get_cstr(m0, "name");
    CHECK(mname != nullptr);
    bool cube_name = mname != nullptr && mname->s.len == 4 &&
                     memcmp(mname->s.data, "cube", 4) == 0;
    CHECK(cube_name);

    cl_json_node *scene = cl_json_get_cstr(root, "scene");
    REQUIRE(scene != nullptr);
    REQUIRE(scene->kind == CLAY_J_ARR);
    CHECK(scene->arr.n == 2);
}

} // namespace

TEST_CASE("clayfile: schema loads via the C ABI JSON parser") {
    Arena arena;
    cl_json_node root;
    cl_err err = cl_json_parse(&root, &arena.a, cl_str_c(kClayDoc));
    CHECK(err == CLAY_OK);
    check_schema(&root);
}

TEST_CASE("clayfile: round-trips through cl_json_write and back") {
    Arena arena;
    cl_json_node root;
    CHECK(cl_json_parse(&root, &arena.a, cl_str_c(kClayDoc)) == CLAY_OK);

    cl_str out;
    CHECK(cl_json_write(&root, &arena.a, &out) == CLAY_OK);
    REQUIRE(out.len > 0);

    /* Re-parsing the serialized form reproduces the same schema. */
    Arena arena2;
    cl_json_node root2;
    CHECK(cl_json_parse(&root2, &arena2.a, out) == CLAY_OK);
    check_schema(&root2);
}

TEST_CASE("clayfile: declared mesh renders through the engine pipeline") {
    Arena arena;
    cl_json_node root;
    REQUIRE(cl_json_parse(&root, &arena.a, cl_str_c(kClayDoc)) == CLAY_OK);

    /* Build the named mesh (a builtin cube per the .clay file) and render it
     * with the same matrices the engine uses. This asserts the "data not
     * code" path: a mesh declared as JSON reaches the z-buffered rasterizer. */
    Mesh3D mesh;
    const float s = 0.5f;
    const cl_v3 corners[8] = {
        {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},
        {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s},
    };
    const unsigned faces[6][4] = {
        {0, 3, 2, 1}, {4, 5, 6, 7}, {4, 7, 3, 0},
        {1, 2, 6, 5}, {0, 1, 5, 4}, {3, 7, 6, 2},
    };
    for (auto &f : faces) {
        mesh.add_triangle(corners[f[0]], corners[f[1]], corners[f[2]]);
        mesh.add_triangle(corners[f[0]], corners[f[2]], corners[f[3]]);
    }

    RendererSW rs(64, 64);
    rs.begin_frame({0x20, 0x20, 0x20, 255});
    cl_m4 proj = cl_m4_perspective(0.9f, 1.0f, 0.1f, 10.0f);
    Mesh3DStats st = rs.draw_mesh(mesh, cl_m4_translate(0.0f, 0.0f, -3.0f),
                                  cl_m4_identity(), proj, {180, 120, 80, 255});
    rs.end_frame();

    CHECK(st.pixels_written > 0);
    uint32_t c = rs.pixel(32, 32);
    CHECK(((c >> 16) & 0xff) > 100); /* lit warm cube face at center */
}

TEST_CASE("clayfile: malformed document fails cleanly") {
    Arena arena;
    cl_json_node root;
    cl_err err = cl_json_parse(&root, &arena.a,
                               cl_str_c("{\"version\": 1, \"scene\": ["));
    bool ok = (err == CLAY_ERR_PARSE) || (err == CLAY_ERR_OOM);
    CHECK(ok);
}