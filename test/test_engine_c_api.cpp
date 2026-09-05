#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <clay/engine_c.h>

#include <filesystem>
#include <limits>
#include <fstream>
#include <string>
#include <vector>

static std::vector<unsigned char> tiny_wav() {
    return {'R',  'I',  'F', 'F', 38,   0,    0, 0, 'W',  'A', 'V', 'E',
            'f',  'm',  't', ' ', 16,   0,    0, 0, 1,    0,   1,   0,
            0x80, 0xbb, 0,   0,   0x80, 0xbb, 0, 0, 1,    0,   8,   0,
            'd',  'a',  't', 'a', 1,    0,    0, 0, 0xFF, 0};
}

TEST_CASE("C ABI exposes deterministic headless audio") {
    const char *path = "/tmp/clay-engine-c-api.wav";
    const auto wav = tiny_wav();
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(wav.data()),
                 static_cast<std::streamsize>(wav.size()));
    output.close();

    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 1);
    REQUIRE(runtime != nullptr);
    uint32_t clip = 0;
    CHECK(cl_engine_runtime_audio_load_wav(runtime, path, &clip) == CLAY_OK);
    REQUIRE(clip != 0);
    uint32_t voice = cl_engine_runtime_audio_play(runtime, clip, 0, false, 1);
    REQUIRE(voice != 0);
    float samples[4] = {9, 9, 9, 9};
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, samples, 4) == CLAY_OK);
    CHECK(samples[0] == doctest::Approx(127.0F / 128.0F));
    CHECK(samples[1] == doctest::Approx(127.0F / 128.0F));
    CHECK(cl_engine_runtime_audio_stop(runtime, voice) == false);
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, samples, 3) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_audio_load_wav(runtime, "/no/such.wav", &clip) ==
          CLAY_ERR_IO);
    cl_engine_runtime_destroy(runtime);
}

TEST_CASE("C ABI owns an authored 3D scene render system") {
    constexpr const char *scene = R"({
      "version": 1,
      "settings": {"render": {"width": 32, "height": 24}},
      "meshes": [{"name": "cube", "primitive": "cube"}],
      "scene": [{"component": "mesh", "mesh": "cube"}]
    })";
    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 9);
    REQUIRE(runtime != nullptr);
    CHECK(!cl_engine_runtime_has_scene(runtime));
    CHECK(cl_engine_runtime_load_scene(runtime, scene) == CLAY_OK);
    CHECK(cl_engine_runtime_has_scene(runtime));
    CHECK(cl_engine_runtime_width(runtime) == 32);
    CHECK(cl_engine_runtime_height(runtime) == 24);
    CHECK(cl_engine_runtime_step(runtime, 1.0 / 60.0) == CLAY_OK);
    size_t count = 0;
    REQUIRE(cl_engine_runtime_pixels(runtime, &count) != nullptr);
    CHECK(count == 32u * 24u);
    cl_engine_runtime_unload_scene(runtime);
    CHECK(!cl_engine_runtime_has_scene(runtime));
    CHECK(cl_engine_runtime_step(runtime, 1.0 / 60.0) == CLAY_OK);
    cl_engine_runtime_destroy(runtime);
}

TEST_CASE("C ABI runtime owns a deterministic rendered frame") {
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path();
    const std::string recording_path =
        (temp_dir / "clay-engine-c-api.clayrec").string();
    const std::string replay_path =
        (temp_dir / "clay-engine-c-api-replay.clayrec").string();
    const std::string png_path = (temp_dir / "clay-engine-c-api.png").string();
    const std::filesystem::path missing_dir =
        temp_dir / "clay-engine-c-api-missing";
    const std::string missing_recording_path =
        (missing_dir / "missing.clayrec").string();
    const std::string missing_png_path = (missing_dir / "frame.png").string();
    std::filesystem::remove(recording_path);
    std::filesystem::remove(replay_path);
    std::filesystem::remove(png_path);
    std::filesystem::remove_all(missing_dir);

    CHECK(cl_engine_runtime_abi_version() == CLAY_ENGINE_ABI_VERSION);
    CHECK(std::string(cl_engine_error_string(CLAY_OK)) == "ok");
    CHECK(std::string(cl_engine_error_string(CLAY_ERR_PARSE)) == "parse error");
    CHECK(std::string(cl_engine_error_string((cl_err)999)) == "unknown");
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
    CHECK(cl_engine_runtime_resize(runtime, 32, 0) == CLAY_ERR_INVALID_ARG);
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
    CHECK(cl_engine_runtime_load_actions(runtime, "not json") ==
          CLAY_ERR_PARSE);
    CHECK(cl_engine_runtime_load_actions(runtime, nullptr) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_save_recording(runtime, recording_path.c_str()) ==
          CLAY_OK);
    CHECK(cl_engine_runtime_load_recording(runtime, recording_path.c_str()) ==
          CLAY_OK);
    CHECK(cl_engine_runtime_load_recording(
              runtime, missing_recording_path.c_str()) == CLAY_ERR_IO);
    cl_engine_runtime_set_replaying(runtime, true);
    CHECK(cl_engine_runtime_is_replaying(runtime));
    cl_engine_runtime_set_replaying(runtime, false);
    CHECK(!cl_engine_runtime_is_replaying(runtime));

    cl_engine_runtime *recorded = cl_engine_runtime_create(16, 16, 42);
    REQUIRE(recorded != nullptr);
    CHECK(cl_engine_runtime_step(recorded, 1.0 / 60.0) == CLAY_OK);
    CHECK(cl_engine_runtime_feed_key(recorded, CLAY_KEY_A, true) == CLAY_OK);
    CHECK(cl_engine_runtime_save_recording(recorded, replay_path.c_str()) ==
          CLAY_OK);
    CHECK(cl_engine_runtime_recording_count(recorded) == 1);
    CHECK(cl_engine_runtime_recording_fingerprint(recorded) != 0);
    cl_engine_runtime *replayed = cl_engine_runtime_create(16, 16, 42);
    REQUIRE(replayed != nullptr);
    CHECK(cl_engine_runtime_load_recording(replayed, replay_path.c_str()) ==
          CLAY_OK);
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
    CHECK(cl_engine_runtime_spawn_species(runtime, "animal", 8, 9, 0.7f, 0.8f,
                                          0.5f, 1.0f, 10.0f) == CLAY_OK);
    CHECK(cl_engine_runtime_spawn_ripple(runtime, 8, 9, 20, 1, 0.5f, 0.2f, 1) ==
          CLAY_OK);
    CHECK(cl_engine_runtime_spawn_species(
              runtime, "animal", std::numeric_limits<float>::quiet_NaN(), 9,
              0.7f, 0.8f, 0.5f, 1.0f, 10.0f) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_spawn_species(
              runtime, "animal", 8, 9, 0.7f, 0.8f, 0.5f, 1.0f,
              std::numeric_limits<float>::infinity()) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_spawn_ripple(
              runtime, 8, 9, std::numeric_limits<float>::infinity(), 1, 0.5f,
              0.2f, 1) == CLAY_ERR_INVALID_ARG);
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
    CHECK(
        cl_engine_runtime_feed_key_at(runtime, CLAY_KEY_A, false,
                                      std::numeric_limits<double>::quiet_NaN(),
                                      4, 0) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_feed_motion(runtime, 8, 9, 1, 2) == CLAY_OK);
    CHECK(cl_engine_runtime_feed_wheel(runtime, 8, 9, 1) == CLAY_OK);
    CHECK(cl_engine_runtime_cursor_x(runtime) == doctest::Approx(8.0));
    CHECK(cl_engine_runtime_cursor_y(runtime) == doctest::Approx(9.0));
    CHECK(cl_engine_runtime_feed_focus(runtime, true) == CLAY_OK);
    CHECK(cl_engine_runtime_is_focused(runtime));
    CHECK(cl_engine_runtime_feed_key(runtime, CLAY_KEY_SPACE, true) == CLAY_OK);
    CHECK(cl_engine_runtime_is_key_down(runtime, CLAY_KEY_SPACE));
    CHECK(cl_engine_runtime_feed_key(runtime, CLAY_KEY_SPACE, false) ==
          CLAY_OK);
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
    CHECK(cl_engine_runtime_feed_wheel(runtime, 0,
                                       std::numeric_limits<double>::infinity(),
                                       1) == CLAY_ERR_INVALID_ARG);
    size_t count = 0;
    const uint32_t *pixels = cl_engine_runtime_pixels(runtime, &count);
    CHECK(pixels != nullptr);
    CHECK(count == 32u * 24u);
    size_t rgba_bytes = 0;
    const uint8_t *rgba = cl_engine_runtime_pixels_rgba(runtime, &rgba_bytes);
    CHECK(rgba != nullptr);
    CHECK(rgba_bytes == 32u * 24u * 4u);
    CHECK(rgba[0] == (uint8_t)(pixels[0] >> 16));
    CHECK(rgba[1] == (uint8_t)(pixels[0] >> 8));
    CHECK(rgba[2] == (uint8_t)pixels[0]);
    CHECK(rgba[3] == 255);
    CHECK(cl_engine_runtime_pixels(runtime, nullptr) == pixels);
    CHECK(cl_engine_runtime_pixels_rgba(runtime, nullptr) == rgba);
    CHECK(cl_engine_runtime_pixels(nullptr, nullptr) == nullptr);
    CHECK(cl_engine_runtime_pixels_rgba(nullptr, nullptr) == nullptr);
    CHECK(cl_engine_runtime_save_png(runtime, png_path.c_str()) == CLAY_OK);
    CHECK(cl_engine_runtime_save_png(runtime, nullptr) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_save_png(runtime, "") == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_save_png(runtime, missing_png_path.c_str()) ==
          CLAY_ERR_IO);
    CHECK(cl_engine_runtime_frame(runtime) == 1);
    CHECK(cl_engine_runtime_sim_time(runtime) == doctest::Approx(1.0 / 60.0));
    CHECK(cl_engine_runtime_cursor_x(runtime) == doctest::Approx(8.0));
    CHECK(cl_engine_runtime_cursor_y(runtime) == doctest::Approx(9.0));
    cl_engine_runtime_set_time_scale(runtime,
                                     std::numeric_limits<double>::infinity());
    CHECK(cl_engine_runtime_time_scale(runtime) == doctest::Approx(1.0));
    CHECK(cl_engine_runtime_step(runtime, 1.0 / 60.0) == CLAY_OK);
    CHECK(cl_engine_runtime_sim_time(runtime) == doctest::Approx(2.0 / 60.0));

    CHECK(cl_engine_runtime_feed(nullptr, nullptr) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(runtime, -1.0) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(runtime,
                                 std::numeric_limits<double>::quiet_NaN()) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(runtime,
                                 std::numeric_limits<double>::infinity()) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_step(nullptr, 1.0 / 60.0) == CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_load_reactions(runtime, nullptr) ==
          CLAY_ERR_INVALID_ARG);
    cl_engine_runtime_destroy(runtime);

    std::filesystem::remove(recording_path);
    std::filesystem::remove(replay_path);
    std::filesystem::remove(png_path);
}

TEST_CASE("C ABI rejects invalid construction") {
    CHECK(cl_engine_runtime_create(0, 10, 1) == nullptr);
    CHECK(cl_engine_runtime_create(10, 0, 1) == nullptr);
    CHECK(cl_engine_runtime_create_with_arena(10, 10, 1, 0) == nullptr);
    CHECK(cl_engine_runtime_create_with_arena(
              10, 10, 1, CLAY_ENGINE_MIN_ARENA_BYTES - 1) == nullptr);
    cl_engine_runtime *sized = cl_engine_runtime_create_with_arena(
        10, 10, 1, CLAY_ENGINE_MIN_ARENA_BYTES);
    CHECK(sized != nullptr);
    cl_engine_runtime_destroy(sized);
    CHECK(cl_engine_runtime_create(std::numeric_limits<int>::max(),
                                   std::numeric_limits<int>::max(),
                                   1) == nullptr);
    CHECK(cl_engine_runtime_create(8193, 8193, 1) == nullptr);
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
