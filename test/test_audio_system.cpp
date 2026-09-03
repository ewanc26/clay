#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "audio/audio_system.hpp"
#include "ecs/world.hpp"
#include "event.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
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

int base64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::vector<std::uint8_t> decode_base64(std::string_view text) {
    std::vector<std::uint8_t> out;
    std::uint32_t accumulator = 0;
    int bits = -8;
    for (char c : text) {
        if (c == '=') break;
        const int value = base64_value(c);
        if (value < 0) continue;
        accumulator = (accumulator << 6U) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<std::uint8_t>((accumulator >> bits) & 0xFFU));
            bits -= 8;
        }
    }
    return out;
}

std::vector<std::uint8_t> vorbis_fixture() {
    const auto path = std::filesystem::path(__FILE__).parent_path() / "data" /
                      "tiny-vorbis.ogg.b64";
    std::ifstream file(path);
    if (!file) return {};
    const std::string encoded((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    return decode_base64(encoded);
}

std::filesystem::path write_temp_wav(std::string_view name,
                                     std::int16_t sample) {
    const auto path = std::filesystem::temp_directory_path() /
                      std::filesystem::path(name);
    const auto bytes = wav16(48000, std::vector<std::int16_t>(512, sample));
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return {};
    file.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!file) return {};
    return path;
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

TEST_CASE("audio system decodes and resamples short WAV effects") {
    Fixture f;
    // Give miniaudio enough source material to move beyond resampler filter
    // startup latency; the first frame is not guaranteed to be steady state.
    const auto encoded = wav16(24000, std::vector<std::int16_t>(1024, 16384));
    auto clip = f.audio.load_clip_memory(encoded);
    REQUIRE(clip.has_value());
    REQUIRE(f.audio.play(*clip).has_value());

    std::array<float, 256> output{};
    REQUIRE(f.audio.mix_stereo(output));
    bool heard_steady_state = false;
    for (std::size_t i = 32; i + 1 < output.size(); i += 2) {
        if (output[i] == doctest::Approx(0.5F).epsilon(0.03) &&
            output[i + 1] == doctest::Approx(0.5F).epsilon(0.03)) {
            heard_steady_state = true;
            break;
        }
    }
    CHECK(heard_steady_state);
}

TEST_CASE("audio system decodes and resamples real OGG Vorbis effects") {
    Fixture f;
    const auto encoded = vorbis_fixture();
    REQUIRE(encoded.size() > 1000);
    CHECK(encoded[0] == static_cast<std::uint8_t>('O'));
    CHECK(encoded[1] == static_cast<std::uint8_t>('g'));
    CHECK(encoded[2] == static_cast<std::uint8_t>('g'));
    CHECK(encoded[3] == static_cast<std::uint8_t>('S'));

    auto clip = f.audio.load_clip_memory(encoded);
    REQUIRE(clip.has_value());
    REQUIRE(f.audio.play(*clip).has_value());
    std::array<float, 512> output{};
    REQUIRE(f.audio.mix_stereo(output));
    bool heard_signal = false;
    for (float sample : output) {
        if (std::abs(sample) > 0.01F) {
            heard_signal = true;
            break;
        }
    }
    CHECK(heard_signal);
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

TEST_CASE("streamed music crossfades between tracks by audio frames") {
    Fixture f;
    const auto first = write_temp_wav("clay_audio_crossfade_a.wav", 8192);
    const auto second = write_temp_wav("clay_audio_crossfade_b.wav", -8192);
    REQUIRE_FALSE(first.empty());
    REQUIRE_FALSE(second.empty());

    REQUIRE(f.audio.play_music_file(first.string(), 0.0F));
    std::array<float, 4> initial{};
    REQUIRE(f.audio.mix_stereo(initial));
    CHECK(initial[0] == doctest::Approx(0.25F).epsilon(0.01));

    // 1 ms at 48 kHz gives a 48-frame transition. The beginning must still
    // favour the old positive track; after more than 48 frames the new
    // negative track must be the only audible stream.
    REQUIRE(f.audio.play_music_file(second.string(), 0.001F));
    std::array<float, 16> early{};
    REQUIRE(f.audio.mix_stereo(early));
    CHECK(early[0] > 0.0F);

    std::array<float, 128> settled{};
    REQUIRE(f.audio.mix_stereo(settled));
    CHECK(settled[settled.size() - 2] == doctest::Approx(-0.25F).epsilon(0.02));
    CHECK(settled[settled.size() - 1] == doctest::Approx(-0.25F).epsilon(0.02));

    f.audio.stop_music(0.0F);
    std::error_code ignored;
    std::filesystem::remove(first, ignored);
    std::filesystem::remove(second, ignored);
}
