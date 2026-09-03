#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "audio/audio_system.hpp"
#include "ecs/world.hpp"
#include "event.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace clay;

namespace {

void u16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
}
void u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
    out.push_back(static_cast<std::uint8_t>(value >> 16U));
    out.push_back(static_cast<std::uint8_t>(value >> 24U));
}
void fourcc(std::vector<std::uint8_t> &out, const char (&id)[5]) {
    out.insert(out.end(), id, id + 4);
}
std::vector<std::uint8_t> wav16(std::uint32_t rate,
                                const std::vector<std::int16_t> &samples) {
    std::vector<std::uint8_t> out;
    fourcc(out, "RIFF");
    u32(out, static_cast<std::uint32_t>(36 + samples.size() * 2));
    fourcc(out, "WAVE");
    fourcc(out, "fmt ");
    u32(out, 16); u16(out, 1); u16(out, 1); u32(out, rate);
    u32(out, rate * 2); u16(out, 2); u16(out, 16);
    fourcc(out, "data"); u32(out, static_cast<std::uint32_t>(samples.size() * 2));
    for (std::int16_t sample : samples) u16(out, static_cast<std::uint16_t>(sample));
    return out;
}

struct Fixture {
    std::vector<std::uint8_t> arena_storage = std::vector<std::uint8_t>(64u << 10);
    cl_arena arena{};
    Hub hub;
    World world;
    AudioSystem audio;

    Fixture() : hub(init_arena()), audio(world, hub) {}

    cl_arena *init_arena() {
        cl_arena_init(&arena, arena_storage.data(), arena_storage.size());
        return &arena;
    }
};

} // namespace

TEST_CASE("audio system stays headless until device start is requested") {
    Fixture f;
    CHECK(f.audio.sample_rate() == 48000);
    CHECK_FALSE(f.audio.device_available());
    std::array<float, 4> silence{1, 1, 1, 1};
    CHECK(f.audio.mix_stereo(silence));
    CHECK(silence[0] == 0.0F);
    CHECK(silence[3] == 0.0F);
}

TEST_CASE("audio system decodes and resamples short effects") {
    Fixture f;
    const auto encoded = wav16(24000, {16384, 16384, 16384, 16384});
    auto clip = f.audio.load_clip_memory(encoded);
    REQUIRE(clip.has_value());
    REQUIRE(f.audio.play(*clip).has_value());

    std::array<float, 4> output{};
    REQUIRE(f.audio.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.5F).epsilon(0.02));
    CHECK(output[1] == doctest::Approx(0.5F).epsilon(0.02));
}

TEST_CASE("audio event channel triggers loaded effects") {
    Fixture f;
    auto clip = f.audio.load_clip_memory(wav16(48000, {8192, 8192}));
    REQUIRE(clip.has_value());
    f.hub.publish(channel("audio.play"), cl_variant_i64(*clip));
    std::array<float, 2> output{};
    REQUIRE(f.audio.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.25F).epsilon(0.01));
}

TEST_CASE("spatial voices follow Transform2D attenuation and pan") {
    Fixture f;
    auto clip = f.audio.load_clip_memory(wav16(48000, {32767, 32767}));
    REQUIRE(clip.has_value());
    Entity emitter = f.world.create();
    f.world.storage<Transform2D>().set(emitter, {5.0F, 0.0F, 0.0F, 1.0F});
    f.audio.set_listener_position(0.0F, 0.0F);
    REQUIRE(f.audio.play_spatial(*clip, emitter, 10.0F).has_value());
    f.audio.update_spatial();

    std::array<float, 2> output{};
    REQUIRE(f.audio.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.25F).epsilon(0.02));
    CHECK(output[1] == doctest::Approx(0.5F).epsilon(0.02));
}

TEST_CASE("music is streamed and can fade out without a device") {
    Fixture f;
    const auto bytes = wav16(48000, std::vector<std::int16_t>(128, 4096));
    const auto path = std::filesystem::temp_directory_path() /
                      "clay_audio_stream_test.wav";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        REQUIRE(file.good());
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }

    REQUIRE(f.audio.play_music_file(path.string(), 0.0F));
    CHECK(f.audio.music_playing());
    std::array<float, 16> output{};
    REQUIRE(f.audio.mix_stereo(output));
    CHECK(output[0] > 0.0F);
    f.audio.stop_music(0.0F);
    CHECK_FALSE(f.audio.music_playing());
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
