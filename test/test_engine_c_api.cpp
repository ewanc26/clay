#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <clay/engine_c.h>

#include <limits>

TEST_CASE("C ABI runtime owns a deterministic rendered frame") {
    CHECK(cl_engine_runtime_abi_version() == CLAY_ENGINE_ABI_VERSION);
    cl_engine_runtime *runtime = cl_engine_runtime_create(32, 24, 7);
    REQUIRE(runtime != nullptr);
    CHECK(cl_engine_runtime_width(runtime) == 32);
    CHECK(cl_engine_runtime_height(runtime) == 24);
    CHECK(cl_engine_runtime_resize(runtime, 48, 20) == CLAY_OK);
    CHECK(cl_engine_runtime_width(runtime) == 48);
    CHECK(cl_engine_runtime_height(runtime) == 20);
    size_t resized_pixels = 0;
    CHECK(cl_engine_runtime_pixels(runtime, &resized_pixels) != nullptr);
    CHECK(resized_pixels == 48u * 20u);
    CHECK(cl_engine_runtime_resize(runtime, 0, 20) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_resize(runtime, 32, 24) == CLAY_OK);
    CHECK(cl_engine_runtime_frame(runtime) == 0);
    CHECK(cl_engine_runtime_sim_time(runtime) == doctest::Approx(0.0));
    CHECK(cl_engine_runtime_sim_dt(runtime) == doctest::Approx(1.0 / 60.0));
    CHECK(cl_engine_runtime_time_scale(runtime) == doctest::Approx(1.0));
    CHECK(cl_engine_runtime_cursor_x(runtime) == doctest::Approx(0.0));
    CHECK(cl_engine_runtime_cursor_y(runtime) == doctest::Approx(0.0));

    CHECK(cl_engine_runtime_step(runtime, 1.0 / 60.0) == CLAY_OK);
    CHECK(cl_engine_runtime_load_reactions(runtime, "not json") ==
          CLAY_ERR_PARSE);
    CHECK(cl_engine_runtime_load_reactions(runtime, "{\"rules\": []}") ==
          CLAY_OK);
    CHECK(cl_engine_runtime_load_actions(
              runtime, "{\"actions\": {\"primary\": {\"key\": \"SPACE\"}}}") ==
          CLAY_OK);
    CHECK(cl_engine_runtime_load_actions(runtime, "not json") == CLAY_ERR_PARSE);
    CHECK(cl_engine_runtime_load_actions(runtime, nullptr) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_save_recording(runtime,
                                           "/tmp/clay-engine-c-api.clayrec") ==
          CLAY_OK);
    CHECK(cl_engine_runtime_load_recording(
              runtime, "/tmp/clay-engine-c-api.clayrec") == CLAY_OK);
    CHECK(cl_engine_runtime_load_recording(runtime, "/no/such/file.clayrec") ==
          CLAY_ERR_IO);
    cl_engine_runtime_set_replaying(runtime, true);
    CHECK(cl_engine_runtime_is_replaying(runtime));
    cl_engine_runtime_set_replaying(runtime, false);
    CHECK(!cl_engine_runtime_is_replaying(runtime));

    cl_engine_runtime *recorded = cl_engine_runtime_create(16, 16, 42);
    REQUIRE(recorded != nullptr);
    CHECK(cl_engine_runtime_step(recorded, 1.0 / 60.0) == CLAY_OK);
    CHECK(cl_engine_runtime_feed_key(recorded, CLAY_KEY_A, true) == CLAY_OK);
    CHECK(cl_engine_runtime_save_recording(
              recorded, "/tmp/clay-engine-c-api-replay.clayrec") == CLAY_OK);
    CHECK(cl_engine_runtime_recording_count(recorded) == 1);
    CHECK(cl_engine_runtime_recording_fingerprint(recorded) != 0);
    cl_engine_runtime *replayed = cl_engine_runtime_create(16, 16, 42);
    REQUIRE(replayed != nullptr);
    CHECK(cl_engine_runtime_load_recording(
              replayed, "/tmp/clay-engine-c-api-replay.clayrec") == CLAY_OK);
    CHECK(cl_engine_runtime_recording_count(replayed) == 1);
    CHECK(cl_engine_runtime_recording_fingerprint(replayed) ==
          cl_engine_runtime_recording_fingerprint(recorded));
    cl_engine_runtime_set_replaying(replayed, true);
    CHECK(cl_engine_runtime_step(replayed, 1.0 / 60.0) == CLAY_OK);
    CHECK(cl_engine_runtime_is_key_down(replayed, CLAY_KEY_A));
    cl_engine_runtime_destroy(replayed);
    cl_engine_runtime_destroy(recorded);
    CHECK(cl_engine_runtime_install_builtin_systems(runtime) == CLAY_OK);
    CHECK(cl_engine_runtime_install_builtin_systems(runtime) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_spawn_species(runtime, "animal", 8, 9, 0.7f,
                                          0.8f, 0.5f, 1.0f, 10.0f) ==
          CLAY_OK);
    CHECK(cl_engine_runtime_spawn_ripple(runtime, 8, 9, 20, 1, 0.5f, 0.2f,
                                         1) == CLAY_OK);
    CHECK(cl_engine_runtime_spawn_species(
              runtime, "animal", std::numeric_limits<float>::quiet_NaN(), 9,
              0.7f, 0.8f, 0.5f, 1.0f, 10.0f) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_spawn_species(
              runtime, "animal", 8, 9, 0.7f, 0.8f, 0.5f, 1.0f,
              std::numeric_limits<float>::infinity()) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_spawn_ripple(
              runtime, 8, 9, std::numeric_limits<float>::infinity(), 1,
              0.5f, 0.2f, 1) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_spawn_ripple(
              runtime, 8, 9, 20, 1, std::numeric_limits<float>::quiet_NaN(),
              0.2f, 1) == CLAY_ERR_INVALID_ARG);
    CHECK(!cl_engine_runtime_is_key_down(runtime, CLAY_KEY_SPACE));
    CHECK(!cl_engine_runtime_is_focused(runtime));
    CHECK(cl_engine_runtime_feed_key(runtime, CLAY_KEY_SPACE, true) == CLAY_OK);
    CHECK(cl_engine_runtime_feed_key_at(runtime, CLAY_KEY_A, true, 3, 4,
                                        CLAY_MOD_SHIFT) == CLAY_OK);
    CHECK(cl_engine_runtime_cursor_x(runtime) == doctest::Approx(3.0));
    CHECK(cl_engine_runtime_cursor_y(runtime) == doctest::Approx(4.0));
    CHECK(cl_engine_runtime_feed_key_at(
              runtime, CLAY_KEY_A, false,
              std::numeric_limits<double>::quiet_NaN(), 4, 0) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_feed_motion(runtime, 8, 9, 1, 2) == CLAY_OK);
    CHECK(cl_engine_runtime_feed_wheel(runtime, 8, 9, 1) == CLAY_OK);
    CHECK(cl_engine_runtime_cursor_x(runtime) == doctest::Approx(8.0));
    CHECK(cl_engine_runtime_cursor_y(runtime) == doctest::Approx(9.0));
    CHECK(cl_engine_runtime_feed_focus(runtime, true) == CLAY_OK);
    CHECK(cl_engine_runtime_is_focused(runtime));
    CHECK(cl_engine_runtime_feed_key(runtime, CLAY_KEY_SPACE, true) == CLAY_OK);
    CHECK(cl_engine_runtime_is_key_down(runtime, CLAY_KEY_SPACE));
    CHECK(cl_engine_runtime_feed_key(runtime, CLAY_KEY_SPACE, false) == CLAY_OK);
    CHECK(!cl_engine_runtime_is_key_down(runtime, CLAY_KEY_SPACE));
    CHECK(cl_engine_runtime_feed_focus(runtime, false) == CLAY_OK);
    CHECK(!cl_engine_runtime_is_focused(runtime));
    CHECK(cl_engine_runtime_feed_wheel(nullptr, 0, 0, 1) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_feed_key(runtime, CLAY_KEY_NONE, true) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_feed_motion(
              runtime, std::numeric_limits<double>::quiet_NaN(), 0, 0, 0) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_feed_wheel(
              runtime, 0, std::numeric_limits<double>::infinity(), 1) ==
          CLAY_ERR_INVALID_ARG);
    size_t count = 0;
    const uint32_t *pixels = cl_engine_runtime_pixels(runtime, &count);
    CHECK(pixels != nullptr);
    CHECK(count == 32u * 24u);
    size_t rgba_bytes = 0;
    const uint8_t *rgba = cl_engine_runtime_pixels_rgba(runtime, &rgba_bytes);
    CHECK(rgba != nullptr);
    CHECK(rgba_bytes == 32u * 24u * 4u);
    CHECK(cl_engine_runtime_save_png(runtime, "/tmp/clay-engine-c-api.png") ==
          CLAY_OK);
    CHECK(cl_engine_runtime_save_png(runtime, nullptr) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_save_png(runtime, "") == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_save_png(runtime, "/no/such/directory/frame.png") ==
          CLAY_ERR_IO);
    CHECK(cl_engine_runtime_frame(runtime) == 1);
    CHECK(cl_engine_runtime_sim_time(runtime) == doctest::Approx(1.0 / 60.0));
    CHECK(cl_engine_runtime_cursor_x(runtime) == doctest::Approx(8.0));
    CHECK(cl_engine_runtime_cursor_y(runtime) == doctest::Approx(9.0));
    cl_engine_runtime_set_time_scale(
        runtime, std::numeric_limits<double>::infinity());
    CHECK(cl_engine_runtime_time_scale(runtime) == doctest::Approx(1.0));
    CHECK(cl_engine_runtime_step(runtime, 1.0 / 60.0) == CLAY_OK);
    CHECK(cl_engine_runtime_sim_time(runtime) == doctest::Approx(2.0 / 60.0));

    CHECK(cl_engine_runtime_feed(nullptr, nullptr) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(runtime, -1.0) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(runtime, std::numeric_limits<double>::quiet_NaN()) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(runtime,
                                 std::numeric_limits<double>::infinity()) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(nullptr, 1.0 / 60.0) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_load_reactions(runtime, nullptr) ==
          CLAY_ERR_INVALID_ARG);
    cl_engine_runtime_destroy(runtime);
}

TEST_CASE("C ABI rejects invalid construction") {
    CHECK(cl_engine_runtime_create(0, 10, 1) == nullptr);
    CHECK(cl_engine_runtime_create(10, 0, 1) == nullptr);
    CHECK(cl_engine_runtime_width(nullptr) == 0);
    CHECK(cl_engine_runtime_height(nullptr) == 0);
    CHECK(cl_engine_runtime_frame(nullptr) == 0);
    CHECK(cl_engine_runtime_sim_time(nullptr) == doctest::Approx(0.0));
    CHECK(cl_engine_runtime_sim_dt(nullptr) == doctest::Approx(0.0));
    CHECK(cl_engine_runtime_time_scale(nullptr) == doctest::Approx(0.0));
    CHECK(cl_engine_runtime_cursor_x(nullptr) == doctest::Approx(0.0));
    CHECK(cl_engine_runtime_cursor_y(nullptr) == doctest::Approx(0.0));
    CHECK(!cl_engine_runtime_is_key_down(nullptr, CLAY_KEY_SPACE));
    CHECK(!cl_engine_runtime_is_focused(nullptr));
    CHECK(!cl_engine_runtime_is_replaying(nullptr));
    CHECK(cl_engine_runtime_recording_count(nullptr) == 0);
    CHECK(cl_engine_runtime_recording_fingerprint(nullptr) == 0);
}
