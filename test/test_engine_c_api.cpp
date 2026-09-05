#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <clay/engine_c.h>

#include <array>
#include <filesystem>
#include <cstring>
#include <limits>
#include <fstream>
#include <string>
#include <vector>

static std::vector<unsigned char> decode_base64(const std::string &text) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<unsigned char> output;
    std::array<int, 4> group{};
    size_t group_size = 0;
    for (unsigned char c : text) {
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        if (c == '=') {
            group[group_size++] = 64;
        } else {
            const char *found = std::strchr(alphabet, c);
            if (!found) continue;
            group[group_size++] = static_cast<int>(found - alphabet);
        }
        if (group_size == group.size()) {
            output.push_back(
                static_cast<unsigned char>((group[0] << 2) | (group[1] >> 4)));
            if (group[2] != 64)
                output.push_back(static_cast<unsigned char>((group[1] << 4) |
                                                            (group[2] >> 2)));
            if (group[3] != 64)
                output.push_back(
                    static_cast<unsigned char>((group[2] << 6) | group[3]));
            group_size = 0;
        }
    }
    return output;
}

TEST_CASE("C ABI audio device lifecycle is safe before platform playback") {
    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 3);
    REQUIRE(runtime != nullptr);
    CHECK_FALSE(cl_engine_runtime_audio_device_is_open(runtime));
    CHECK_FALSE(cl_engine_runtime_audio_device_is_started(runtime));
    CHECK_FALSE(cl_engine_runtime_audio_device_start(runtime));
    CHECK_FALSE(cl_engine_runtime_audio_device_stop(runtime));
    cl_engine_runtime_destroy(runtime);
}

TEST_CASE("C ABI decodes a generic FLAC audio file") {
    std::ifstream encoded(std::string(CLAY_TEST_DATA_DIR) +
                          "/tiny-flac.flac.b64");
    REQUIRE(encoded);
    const std::string text((std::istreambuf_iterator<char>(encoded)),
                           std::istreambuf_iterator<char>());
    const auto flac = decode_base64(text);
    REQUIRE(flac.size() > 100);
    const auto path =
        std::filesystem::temp_directory_path() / "clay-engine-c-api-flac.flac";
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output);
    output.write(reinterpret_cast<const char *>(flac.data()),
                 static_cast<std::streamsize>(flac.size()));
    output.close();
    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 2);
    REQUIRE(runtime != nullptr);
    uint32_t clip = 0;
    CHECK(cl_engine_runtime_audio_load_file(
              runtime,
              (path.parent_path() / "clay-no-such-file.flac").string().c_str(),
              &clip) == CLAY_ERR_IO);
    const auto malformed_path = path.parent_path() / "clay-malformed-audio.bin";
    {
        std::ofstream malformed(malformed_path, std::ios::binary);
        REQUIRE(malformed);
        malformed << "not an audio file";
    }
    CHECK(cl_engine_runtime_audio_load_file(runtime,
                                            malformed_path.string().c_str(),
                                            &clip) == CLAY_ERR_PARSE);
    std::filesystem::remove(malformed_path);
    CHECK(cl_engine_runtime_audio_load_file(runtime, path.string().c_str(),
                                            &clip) == CLAY_OK);
    REQUIRE(clip != 0);
    CHECK(cl_engine_runtime_audio_clip_frame_count(runtime, clip) > 0);
    const uint32_t voice =
        cl_engine_runtime_audio_play(runtime, clip, 0, false, 1.0F);
    REQUIRE(voice != 0);
    CHECK(cl_engine_runtime_audio_voice_active(runtime, voice));
    float samples[2048] = {};
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, samples, 2048) ==
          CLAY_OK);
    bool nonzero = false;
    for (float sample : samples) nonzero |= sample != 0.0F;
    CHECK(nonzero);
    uint32_t stream = 0;
    CHECK(cl_engine_runtime_audio_load_stream(runtime, path.string().c_str(),
                                              &stream) == CLAY_OK);
    REQUIRE(stream != 0);
    CHECK(cl_engine_runtime_audio_stream_frame_count(runtime, stream) > 0);
    CHECK(cl_engine_runtime_audio_stream_read_status(runtime, stream) ==
          CLAY_AUDIO_STREAM_READY);
    const uint32_t stream_voice =
        cl_engine_runtime_audio_play_stream(runtime, stream, 1, true, 1.0F);
    REQUIRE(stream_voice != 0);
    float stream_samples[2048] = {};
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, stream_samples, 2048) ==
          CLAY_OK);
    nonzero = false;
    for (float sample : stream_samples) nonzero |= sample != 0.0F;
    CHECK(nonzero);
    CHECK(cl_engine_runtime_audio_unload_stream(runtime, stream));
    CHECK(!cl_engine_runtime_audio_voice_active(runtime, stream_voice));
    CHECK(cl_engine_runtime_audio_unload_clip(runtime, clip));
    CHECK(cl_engine_runtime_audio_clip_frame_count(runtime, clip) == 0);
    CHECK(!cl_engine_runtime_audio_voice_active(runtime, voice));
    CHECK(!cl_engine_runtime_audio_stop(runtime, voice));
    cl_engine_runtime_destroy(runtime);
    std::filesystem::remove(path);
}

TEST_CASE("C ABI decodes a generic MP3 audio file") {
    std::ifstream encoded(std::string(CLAY_TEST_DATA_DIR) +
                          "/tiny-mp3.mp3.b64");
    REQUIRE(encoded);
    const std::string text((std::istreambuf_iterator<char>(encoded)),
                           std::istreambuf_iterator<char>());
    const auto mp3 = decode_base64(text);
    REQUIRE(mp3.size() > 100);
    const auto path =
        std::filesystem::temp_directory_path() / "clay-engine-c-api-mp3.mp3";
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output);
    output.write(reinterpret_cast<const char *>(mp3.data()),
                 static_cast<std::streamsize>(mp3.size()));
    output.close();

    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 3);
    REQUIRE(runtime != nullptr);
    uint32_t clip = 0;
    CHECK(cl_engine_runtime_audio_load_file(runtime, path.string().c_str(),
                                            &clip) == CLAY_OK);
    REQUIRE(clip != 0);
    const uint32_t voice =
        cl_engine_runtime_audio_play(runtime, clip, 1, false, 1.0F);
    REQUIRE(voice != 0);
    float samples[8192] = {};
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, samples, 8192) ==
          CLAY_OK);
    bool nonzero = false;
    for (float sample : samples) nonzero |= sample != 0.0F;
    CHECK(nonzero);
    CHECK(cl_engine_runtime_audio_unload_clip(runtime, clip));
    CHECK(!cl_engine_runtime_audio_stop(runtime, voice));
    cl_engine_runtime_destroy(runtime);
    std::filesystem::remove(path);
}

TEST_CASE("C ABI decodes a generic Ogg Vorbis audio file") {
    std::ifstream encoded(std::string(CLAY_TEST_DATA_DIR) +
                          "/tiny-ogg.ogg.b64");
    REQUIRE(encoded);
    const std::string text((std::istreambuf_iterator<char>(encoded)),
                           std::istreambuf_iterator<char>());
    const auto ogg = decode_base64(text);
    REQUIRE(ogg.size() > 100);
    const auto path =
        std::filesystem::temp_directory_path() / "clay-engine-c-api-ogg.ogg";
    std::ofstream output(path, std::ios::binary);
    REQUIRE(output);
    output.write(reinterpret_cast<const char *>(ogg.data()),
                 static_cast<std::streamsize>(ogg.size()));
    output.close();

    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 4);
    REQUIRE(runtime != nullptr);
    uint32_t clip = 0;
    CHECK(cl_engine_runtime_audio_load_file(runtime, path.string().c_str(),
                                            &clip) == CLAY_OK);
    REQUIRE(clip != 0);
    CHECK(cl_engine_runtime_audio_clip_frame_count(runtime, clip) > 0);
    const uint32_t voice =
        cl_engine_runtime_audio_play(runtime, clip, 0, false, 1.0F);
    REQUIRE(voice != 0);
    float samples[4096] = {};
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, samples, 4096) ==
          CLAY_OK);
    bool nonzero = false;
    for (float sample : samples) nonzero |= sample != 0.0F;
    CHECK(nonzero);
    CHECK(cl_engine_runtime_audio_unload_clip(runtime, clip));
    CHECK(!cl_engine_runtime_audio_voice_active(runtime, voice));
    cl_engine_runtime_destroy(runtime);
    std::filesystem::remove(path);
}

static std::vector<unsigned char> tiny_wav() {
    return {'R',  'I',  'F', 'F', 38,   0,    0, 0, 'W',  'A', 'V', 'E',
            'f',  'm',  't', ' ', 16,   0,    0, 0, 1,    0,   1,   0,
            0x80, 0xbb, 0,   0,   0x80, 0xbb, 0, 0, 1,    0,   8,   0,
            'd',  'a',  't', 'a', 1,    0,    0, 0, 0xFF, 0};
}

TEST_CASE("C ABI exposes deterministic headless audio") {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "clay-engine-c-api.wav";
    const auto wav = tiny_wav();
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(wav.data()),
                 static_cast<std::streamsize>(wav.size()));
    output.close();

    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 1);
    REQUIRE(runtime != nullptr);
    CHECK(cl_engine_runtime_audio_sample_rate(runtime) == 48000);
    CHECK(cl_engine_runtime_audio_master_gain(runtime) ==
          doctest::Approx(1.0F));
    CHECK(cl_engine_runtime_audio_bus_gain(runtime, 0) ==
          doctest::Approx(1.0F));
    cl_engine_runtime_audio_set_master_gain(runtime, 0.5F);
    cl_engine_runtime_audio_set_bus_gain(runtime, 1, 0.25F);
    CHECK(cl_engine_runtime_audio_master_gain(runtime) ==
          doctest::Approx(0.5F));
    CHECK(cl_engine_runtime_audio_bus_gain(runtime, 1) ==
          doctest::Approx(0.25F));
    CHECK(cl_engine_runtime_audio_bus_gain(runtime, 9) ==
          doctest::Approx(0.0F));
    cl_engine_runtime_audio_set_master_gain(runtime, 1.0F);
    cl_engine_runtime_audio_set_bus_gain(runtime, 1, 1.0F);
    uint32_t clip = 0;
    CHECK(cl_engine_runtime_audio_load_wav(runtime, path.string().c_str(),
                                           &clip) == CLAY_OK);
    REQUIRE(clip != 0);
    uint32_t voice = cl_engine_runtime_audio_play(runtime, clip, 0, false, 1);
    REQUIRE(voice != 0);
    CHECK(cl_engine_runtime_audio_pause(runtime, voice));
    CHECK(!cl_engine_runtime_audio_voice_active(runtime, voice));
    CHECK(cl_engine_runtime_audio_voice_paused(runtime, voice));
    float paused_samples[4] = {9, 9, 9, 9};
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, paused_samples, 4) ==
          CLAY_OK);
    CHECK(paused_samples[0] == 0.0F);
    CHECK(cl_engine_runtime_audio_resume(runtime, voice));
    CHECK(cl_engine_runtime_audio_voice_active(runtime, voice));
    CHECK(!cl_engine_runtime_audio_voice_paused(runtime, voice));
    float samples[4] = {9, 9, 9, 9};
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, samples, 4) == CLAY_OK);
    CHECK(samples[0] == doctest::Approx(127.0F / 128.0F));
    CHECK(samples[1] == doctest::Approx(127.0F / 128.0F));
    CHECK(cl_engine_runtime_audio_stop(runtime, voice) == false);
    CHECK(!cl_engine_runtime_audio_voice_paused(runtime, voice));
    const uint32_t loop_a =
        cl_engine_runtime_audio_play(runtime, clip, 0, true, 1.0F);
    const uint32_t loop_b =
        cl_engine_runtime_audio_play(runtime, clip, 1, true, 1.0F);
    REQUIRE(loop_a != 0);
    REQUIRE(loop_b != 0);
    CHECK(cl_engine_runtime_audio_voice_active(runtime, loop_a));
    CHECK(cl_engine_runtime_audio_voice_active(runtime, loop_b));
    cl_engine_runtime_audio_stop_all(runtime);
    CHECK(!cl_engine_runtime_audio_voice_active(runtime, loop_a));
    CHECK(!cl_engine_runtime_audio_voice_active(runtime, loop_b));
    CHECK(cl_engine_runtime_audio_unload_clip(runtime, clip));
    CHECK(!cl_engine_runtime_audio_unload_clip(runtime, clip));
    CHECK(cl_engine_runtime_audio_mix_stereo(runtime, samples, 3) ==
          CLAY_ERR_INVALID_ARG);
    CHECK(cl_engine_runtime_audio_load_wav(
              runtime,
              (path.parent_path() / "clay-no-such-file.wav").string().c_str(),
              &clip) == CLAY_ERR_IO);
    cl_engine_runtime_destroy(runtime);
    std::filesystem::remove(path);
}

TEST_CASE("C ABI owns an authored 3D scene render system") {
    constexpr const char *scene = R"({
      "version": 1,
      "settings": {"seed": 1234, "render": {"width": 32, "height": 24}},
      "meshes": [{"name": "cube", "primitive": "cube"}],
      "scene": [{"component": "mesh", "mesh": "cube"}]
    })";
    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 9);
    REQUIRE(runtime != nullptr);
    CHECK(cl_engine_runtime_seed(runtime) == 9);
    CHECK(!cl_engine_runtime_has_scene(runtime));
    CHECK(cl_engine_runtime_load_scene(runtime, scene) == CLAY_OK);
    CHECK(cl_engine_runtime_seed(runtime) == 1234);
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

TEST_CASE("C ABI loads an authored scene from a file") {
    const auto path =
        std::filesystem::temp_directory_path() / "clay-engine-c-api-scene.clay";
    const std::string scene =
        R"({"version":1,"settings":{"render":{"width":20,"height":12}},"scene":[]})";
    {
        std::ofstream output(path, std::ios::binary);
        REQUIRE(output);
        output << scene;
    }

    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 11);
    REQUIRE(runtime != nullptr);
    CHECK(cl_engine_runtime_load_scene_file(runtime, path.string().c_str()) ==
          CLAY_OK);
    CHECK(cl_engine_runtime_has_scene(runtime));
    CHECK(cl_engine_runtime_width(runtime) == 20);
    CHECK(cl_engine_runtime_height(runtime) == 12);
    CHECK(cl_engine_runtime_load_scene_file(
              runtime, (path.parent_path() / "clay-missing-scene.clay")
                           .string()
                           .c_str()) == CLAY_ERR_IO);
    cl_engine_runtime_destroy(runtime);
    std::filesystem::remove(path);
}

TEST_CASE("C ABI loads action and reaction data from files") {
    const auto actions_path = std::filesystem::temp_directory_path() /
                              "clay-engine-c-api-actions.json";
    const auto reactions_path = std::filesystem::temp_directory_path() /
                                "clay-engine-c-api-reactions.json";
    {
        std::ofstream actions(actions_path, std::ios::binary);
        std::ofstream reactions(reactions_path, std::ios::binary);
        REQUIRE(actions);
        REQUIRE(reactions);
        actions << R"({"actions":{"primary":{"key":"SPACE"}}})";
        reactions << R"({"rules":[]})";
    }

    cl_engine_runtime *runtime = cl_engine_runtime_create(8, 8, 12);
    REQUIRE(runtime != nullptr);
    CHECK(cl_engine_runtime_load_actions_file(
              runtime, actions_path.string().c_str()) == CLAY_OK);
    CHECK(cl_engine_runtime_load_reactions_file(
              runtime, reactions_path.string().c_str()) == CLAY_OK);
    cl_engine_runtime_destroy(runtime);
    std::filesystem::remove(actions_path);
    std::filesystem::remove(reactions_path);
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
    CHECK(cl_engine_runtime_seed(runtime) == 7);
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
