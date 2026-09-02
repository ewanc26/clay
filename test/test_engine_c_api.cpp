#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <clay/engine_c.h>

TEST_CASE("C ABI runtime owns a deterministic rendered frame") {
    cl_engine_runtime *runtime = cl_engine_runtime_create(32, 24, 7);
    REQUIRE(runtime != nullptr);
    CHECK(cl_engine_runtime_width(runtime) == 32);
    CHECK(cl_engine_runtime_height(runtime) == 24);
    CHECK(cl_engine_runtime_frame(runtime) == 0);

    CHECK(cl_engine_runtime_step(runtime, 1.0 / 60.0) == CLAY_OK);
    CHECK(cl_engine_runtime_load_reactions(runtime, "not json") ==
          CLAY_ERR_PARSE);
    CHECK(cl_engine_runtime_load_reactions(runtime, "{\"rules\": []}") ==
          CLAY_OK);
    CHECK(cl_engine_runtime_install_builtin_systems(runtime) == CLAY_OK);
    CHECK(cl_engine_runtime_install_builtin_systems(runtime) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_spawn_species(runtime, "animal", 8, 9, 0.7f,
                                          0.8f, 0.5f, 1.0f, 10.0f) ==
          CLAY_OK);
    CHECK(cl_engine_runtime_spawn_ripple(runtime, 8, 9, 20, 1, 0.5f, 0.2f,
                                         1) == CLAY_OK);
    CHECK(cl_engine_runtime_feed_key(runtime, CLAY_KEY_SPACE, true) == CLAY_OK);
    CHECK(cl_engine_runtime_feed_motion(runtime, 8, 9, 1, 2) == CLAY_OK);
    size_t count = 0;
    const uint32_t *pixels = cl_engine_runtime_pixels(runtime, &count);
    CHECK(pixels != nullptr);
    CHECK(count == 32u * 24u);
    CHECK(cl_engine_runtime_frame(runtime) == 1);

    CHECK(cl_engine_runtime_feed(nullptr, nullptr) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(runtime, -1.0) == CLAY_ERR_INVALID_ARG);
    cl_engine_runtime_destroy(runtime);
}

TEST_CASE("C ABI rejects invalid construction") {
    CHECK(cl_engine_runtime_create(0, 10, 1) == nullptr);
    CHECK(cl_engine_runtime_create(10, 0, 1) == nullptr);
    CHECK(cl_engine_runtime_width(nullptr) == 0);
    CHECK(cl_engine_runtime_height(nullptr) == 0);
    CHECK(cl_engine_runtime_frame(nullptr) == 0);
}
