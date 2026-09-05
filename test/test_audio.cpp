#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "audio/audio_decode.hpp"
#if CLAY_BUILD_AUDIO_DEVICE
#include "audio/audio_device.hpp"
#endif
#include "audio/audio_mixer.hpp"

#include <cstdlib>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <vector>

using namespace clay;

namespace {

void append_u16(std::vector<std::uint8_t> &bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t> &bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void append_fourcc(std::vector<std::uint8_t> &bytes, const char (&id)[5]) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<std::uint8_t>(id[i]));
    }
}

void patch_u32(std::vector<std::uint8_t> &bytes, std::size_t offset,
               std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    bytes[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    bytes[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    bytes[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

std::vector<std::uint8_t>
make_wav(std::uint16_t encoding, std::uint16_t channels,
         std::uint32_t sample_rate, std::uint16_t bits_per_sample,
         std::vector<std::uint8_t> data, bool add_odd_junk = false,
         std::uint16_t block_align_override = 0) {
    std::vector<std::uint8_t> bytes;
    append_fourcc(bytes, "RIFF");
    append_u32(bytes, 0);
    append_fourcc(bytes, "WAVE");

    if (add_odd_junk) {
        append_fourcc(bytes, "JUNK");
        append_u32(bytes, 3);
        bytes.insert(bytes.end(), {0xAA, 0xBB, 0xCC, 0x00});
    }

    const std::uint16_t bytes_per_sample = bits_per_sample / 8;
    const std::uint16_t block_align =
        block_align_override != 0
            ? block_align_override
            : static_cast<std::uint16_t>(channels * bytes_per_sample);

    append_fourcc(bytes, "fmt ");
    append_u32(bytes, 16);
    append_u16(bytes, encoding);
    append_u16(bytes, channels);
    append_u32(bytes, sample_rate);
    append_u32(bytes, sample_rate * block_align);
    append_u16(bytes, block_align);
    append_u16(bytes, bits_per_sample);

    append_fourcc(bytes, "data");
    append_u32(bytes, static_cast<std::uint32_t>(data.size()));
    bytes.insert(bytes.end(), data.begin(), data.end());
    if ((data.size() & 1U) != 0) {
        bytes.push_back(0);
    }

    patch_u32(bytes, 4, static_cast<std::uint32_t>(bytes.size() - 8));
    return bytes;
}

void append_float32(std::vector<std::uint8_t> &bytes, float value) {
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

} // namespace

TEST_CASE("audio mixer rejects incompatible clips") {
    AudioMixer mixer(48000);

    AudioClip empty;
    CHECK_FALSE(mixer.add_clip(std::move(empty)).has_value());

    AudioClip bad_channels{48000, 3, {0.0F, 0.0F, 0.0F}};
    CHECK_FALSE(mixer.add_clip(std::move(bad_channels)).has_value());

    AudioClip wrong_rate{44100, 1, {0.25F}};
    CHECK_FALSE(mixer.add_clip(std::move(wrong_rate)).has_value());

    AudioClip partial_stereo{48000, 2, {0.25F, -0.25F, 0.5F}};
    CHECK_FALSE(mixer.add_clip(std::move(partial_stereo)).has_value());

    AudioClip non_finite{
        48000, 1, {0.25F, std::numeric_limits<float>::quiet_NaN()}};
    CHECK_FALSE(mixer.add_clip(std::move(non_finite)).has_value());
    CHECK(mixer.clip_count() == 0);
}

TEST_CASE("audio mixer expands mono and completes a voice") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {0.25F, -0.5F}});
    REQUIRE(clip.has_value());
    auto voice = mixer.play(*clip);
    REQUIRE(voice.has_value());

    std::array<float, 4> output{9.0F, 9.0F, 9.0F, 9.0F};
    CHECK(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.25F));
    CHECK(output[1] == doctest::Approx(0.25F));
    CHECK(output[2] == doctest::Approx(-0.5F));
    CHECK(output[3] == doctest::Approx(-0.5F));
    CHECK(mixer.voice_count() == 0);
    CHECK_FALSE(mixer.stop(*voice));
}

TEST_CASE("audio mixer preserves stereo channels") {
    AudioMixer mixer;
    auto clip =
        mixer.add_clip(AudioClip{48000, 2, {0.75F, -0.25F, 0.5F, -0.5F}});
    REQUIRE(clip.has_value());
    REQUIRE(mixer.play(*clip).has_value());

    std::array<float, 4> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.75F));
    CHECK(output[1] == doctest::Approx(-0.25F));
    CHECK(output[2] == doctest::Approx(0.5F));
    CHECK(output[3] == doctest::Approx(-0.5F));
}

TEST_CASE("audio mixer applies per-voice stereo pan") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {0.75F}});
    REQUIRE(clip.has_value());
    auto voice = mixer.play(*clip);
    REQUIRE(voice.has_value());

    CHECK(mixer.set_voice_pan(*voice, 1.0F));
    CHECK(mixer.voice_pan(*voice) == doctest::Approx(1.0F));
    std::array<float, 2> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.0F));
    CHECK(output[1] == doctest::Approx(0.75F));

    auto second_voice = mixer.play(*clip);
    REQUIRE(second_voice.has_value());
    CHECK(mixer.set_voice_pan(*second_voice, -1.0F));
    CHECK(mixer.voice_pan(*second_voice) == doctest::Approx(-1.0F));
    output = {};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.75F));
    CHECK(output[1] == doctest::Approx(0.0F));
    CHECK_FALSE(mixer.set_voice_pan(*second_voice,
                                    std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("audio mixer updates active voice gain") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {0.75F}});
    REQUIRE(clip.has_value());
    auto voice = mixer.play(*clip);
    REQUIRE(voice.has_value());

    CHECK(mixer.set_voice_gain(*voice, 0.25F));
    CHECK(mixer.voice_gain(*voice) == doctest::Approx(0.25F));
    std::array<float, 2> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.1875F));
    CHECK(output[1] == doctest::Approx(0.1875F));

    auto second_voice = mixer.play(*clip);
    REQUIRE(second_voice.has_value());
    CHECK(mixer.set_voice_gain(*second_voice, 4.0F));
    CHECK(mixer.voice_gain(*second_voice) == doctest::Approx(1.0F));
    CHECK_FALSE(mixer.set_voice_gain(*second_voice,
                                     std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("audio mixer fades a voice over mixer frames") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {1.0F, 1.0F, 1.0F}});
    REQUIRE(clip.has_value());
    auto voice = mixer.play(*clip);
    REQUIRE(voice.has_value());
    REQUIRE(mixer.fade_voice(*voice, 0.0F, 2));

    std::array<float, 4> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(1.0F));
    CHECK(output[1] == doctest::Approx(1.0F));
    CHECK(output[2] == doctest::Approx(0.5F));
    CHECK(output[3] == doctest::Approx(0.5F));
    CHECK(mixer.voice_gain(*voice) == doctest::Approx(0.0F));
    CHECK(mixer.voice_count() == 1);

    output = {};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.0F));
    CHECK(mixer.voice_count() == 0);
}

TEST_CASE("audio mixer crossfades music voices") {
    AudioMixer mixer;
    auto old_clip =
        mixer.add_clip(AudioClip{48000, 1, {1.0F, 1.0F, 1.0F, 1.0F}});
    auto new_clip =
        mixer.add_clip(AudioClip{48000, 1, {0.25F, 0.25F, 0.25F, 0.25F}});
    REQUIRE(old_clip.has_value());
    REQUIRE(new_clip.has_value());
    REQUIRE(mixer.play(*old_clip, AudioBus::Music, true).has_value());
    auto voice = mixer.crossfade_music(*new_clip, 2);
    REQUIRE(voice.has_value());

    std::array<float, 6> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(1.0F));
    CHECK(output[2] == doctest::Approx(0.625F));
    CHECK(output[4] == doctest::Approx(0.25F));
    CHECK(mixer.voice_count() == 1);

    output = {};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.25F));
    CHECK(mixer.voice_count() == 0);
}

TEST_CASE("audio mixer applies master bus and voice gains") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {1.0F}});
    REQUIRE(clip.has_value());

    mixer.set_master_gain(0.5F);
    mixer.set_bus_gain(AudioBus::Sfx, 0.5F);
    mixer.set_bus_gain(AudioBus::Music, 0.25F);

    REQUIRE(mixer.play(*clip, AudioBus::Sfx, false, 0.5F).has_value());
    REQUIRE(mixer.play(*clip, AudioBus::Music).has_value());

    std::array<float, 2> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.25F));
    CHECK(output[1] == doctest::Approx(0.25F));

    mixer.set_master_gain(4.0F);
    mixer.set_bus_gain(AudioBus::Sfx, -2.0F);
    CHECK(mixer.master_gain() == doctest::Approx(1.0F));
    CHECK(mixer.bus_gain(AudioBus::Sfx) == doctest::Approx(0.0F));

    mixer.set_master_gain(std::numeric_limits<float>::quiet_NaN());
    mixer.set_bus_gain(AudioBus::Music, std::numeric_limits<float>::infinity());
    CHECK(mixer.master_gain() == doctest::Approx(0.0F));
    CHECK(mixer.bus_gain(AudioBus::Music) == doctest::Approx(0.0F));
}

TEST_CASE("audio mixer spatializes a voice against the listener") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {1.0F}});
    REQUIRE(clip.has_value());
    auto voice = mixer.play(*clip);
    REQUIRE(voice.has_value());

    mixer.set_listener_position(0.0F, 0.0F);
    REQUIRE(mixer.set_voice_position(*voice, 5.0F, 0.0F, 10.0F));
    std::array<float, 2> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.25F));
    CHECK(output[1] == doctest::Approx(0.5F));

    auto quiet_clip = mixer.add_clip(AudioClip{48000, 1, {1.0F}});
    REQUIRE(quiet_clip.has_value());
    auto quiet_voice = mixer.play(*quiet_clip);
    REQUIRE(quiet_voice.has_value());
    REQUIRE(mixer.set_voice_position(*quiet_voice, 10.0F, 0.0F, 10.0F));
    output = {};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.0F));
    CHECK(output[1] == doctest::Approx(0.0F));
}

TEST_CASE("audio mixer loops and stops voices") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {0.25F, -0.25F}});
    REQUIRE(clip.has_value());
    auto voice = mixer.play(*clip, AudioBus::Music, true);
    REQUIRE(voice.has_value());

    std::array<float, 8> output{};
    REQUIRE(mixer.mix_stereo(output));
    const std::array<float, 8> expected{
        0.25F, 0.25F, -0.25F, -0.25F, 0.25F, 0.25F, -0.25F, -0.25F,
    };
    for (std::size_t i = 0; i < output.size(); ++i) {
        CHECK(output[i] == doctest::Approx(expected[i]));
    }
    CHECK(mixer.voice_count() == 1);
    CHECK(mixer.stop(*voice));
    CHECK(mixer.voice_count() == 0);
}

TEST_CASE("audio mixer distinguishes paused voices from finished voices") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {0.25F, -0.25F}});
    REQUIRE(clip.has_value());
    auto voice = mixer.play(*clip);
    REQUIRE(voice.has_value());
    CHECK_FALSE(mixer.voice_paused(*voice));
    REQUIRE(mixer.pause(*voice));
    CHECK_FALSE(mixer.voice_active(*voice));
    CHECK(mixer.voice_paused(*voice));
    REQUIRE(mixer.resume(*voice));
    CHECK(mixer.voice_active(*voice));
    CHECK_FALSE(mixer.voice_paused(*voice));
    REQUIRE(mixer.stop(*voice));
    CHECK_FALSE(mixer.voice_paused(*voice));
}

TEST_CASE("audio mixer clamps summed output and removes clip voices") {
    AudioMixer mixer;
    auto clip = mixer.add_clip(AudioClip{48000, 1, {0.8F}});
    REQUIRE(clip.has_value());
    REQUIRE(mixer.play(*clip).has_value());
    REQUIRE(mixer.play(*clip).has_value());

    std::array<float, 2> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(1.0F));
    CHECK(output[1] == doctest::Approx(1.0F));

    auto looping = mixer.play(*clip, AudioBus::Sfx, true);
    REQUIRE(looping.has_value());
    CHECK(mixer.remove_clip(*clip));
    CHECK(mixer.clip_count() == 0);
    CHECK(mixer.voice_count() == 0);
    CHECK_FALSE(mixer.remove_clip(*clip));
}

TEST_CASE("audio mixer rejects non-stereo output spans") {
    AudioMixer mixer;
    std::array<float, 3> output{1.0F, 1.0F, 1.0F};
    CHECK_FALSE(mixer.mix_stereo(output));
    CHECK(output[0] == 0.0F);
    CHECK(output[1] == 0.0F);
    CHECK(output[2] == 0.0F);
}

#if CLAY_BUILD_AUDIO_DEVICE
TEST_CASE("audio device remains safe before opening a platform device") {
    AudioMixer mixer;
    AudioDevice device(mixer);
    CHECK_FALSE(device.is_open());
    CHECK_FALSE(device.is_started());
    CHECK_FALSE(device.start());
    CHECK_FALSE(device.stop());

    if (std::getenv("CLAY_TEST_AUDIO_DEVICE") == nullptr) return;
    REQUIRE(device.open());
    REQUIRE(device.start());
    CHECK(device.is_open());
    CHECK(device.is_started());
    REQUIRE(device.stop());
    CHECK_FALSE(device.is_started());
}
#endif

TEST_CASE("WAV decoder converts signed 16-bit mono PCM") {
    auto wav = make_wav(1, 1, 48000, 16, {0x00, 0x80, 0x00, 0x00, 0xFF, 0x7F});
    auto clip = decode_wav(wav);
    REQUIRE(clip.has_value());
    CHECK(clip->sample_rate == 48000);
    CHECK(clip->channels == 1);
    REQUIRE(clip->samples.size() == 3);
    CHECK(clip->samples[0] == doctest::Approx(-1.0F));
    CHECK(clip->samples[1] == doctest::Approx(0.0F));
    CHECK(clip->samples[2] == doctest::Approx(32767.0F / 32768.0F));
}

TEST_CASE("WAV decoder handles stereo PCM and padded unknown chunks") {
    auto wav = make_wav(1, 2, 22050, 8, {0x00, 0xFF, 0x80, 0x40}, true);
    auto clip = decode_wav(wav);
    REQUIRE(clip.has_value());
    CHECK(clip->sample_rate == 22050);
    CHECK(clip->channels == 2);
    REQUIRE(clip->samples.size() == 4);
    CHECK(clip->samples[0] == doctest::Approx(-1.0F));
    CHECK(clip->samples[1] == doctest::Approx(127.0F / 128.0F));
    CHECK(clip->samples[2] == doctest::Approx(0.0F));
    CHECK(clip->samples[3] == doctest::Approx(-0.5F));
}

TEST_CASE("WAV decoder supports 24-bit PCM and 32-bit float") {
    auto pcm24 =
        make_wav(1, 1, 48000, 24, {0x00, 0x00, 0x80, 0xFF, 0xFF, 0x7F});
    auto int_clip = decode_wav(pcm24);
    REQUIRE(int_clip.has_value());
    REQUIRE(int_clip->samples.size() == 2);
    CHECK(int_clip->samples[0] == doctest::Approx(-1.0F));
    CHECK(int_clip->samples[1] == doctest::Approx(8388607.0F / 8388608.0F));

    std::vector<std::uint8_t> float_data;
    append_float32(float_data, -0.25F);
    append_float32(float_data, 0.75F);
    auto float_wav = make_wav(3, 2, 48000, 32, std::move(float_data));
    auto float_clip = decode_wav(float_wav);
    REQUIRE(float_clip.has_value());
    REQUIRE(float_clip->samples.size() == 2);
    CHECK(float_clip->samples[0] == doctest::Approx(-0.25F));
    CHECK(float_clip->samples[1] == doctest::Approx(0.75F));
}

TEST_CASE("WAV decoder preserves native rate for mixer validation") {
    auto wav = make_wav(1, 1, 44100, 16, {0x00, 0x20});
    auto clip = decode_wav(wav);
    REQUIRE(clip.has_value());
    CHECK(clip->sample_rate == 44100);

    AudioMixer mixer(48000);
    CHECK_FALSE(mixer.add_clip(std::move(*clip)).has_value());
}

TEST_CASE("WAV decoder rejects malformed and unsupported files") {
    auto truncated = make_wav(1, 1, 48000, 16, {0x00, 0x00});
    truncated.pop_back();
    CHECK_FALSE(decode_wav(truncated).has_value());

    auto compressed = make_wav(6, 1, 48000, 16, {0x00, 0x00});
    CHECK_FALSE(decode_wav(compressed).has_value());

    auto bad_channels =
        make_wav(1, 3, 48000, 16, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    CHECK_FALSE(decode_wav(bad_channels).has_value());

    auto bad_align =
        make_wav(1, 2, 48000, 16, {0x00, 0x00, 0x00, 0x00}, false, 2);
    CHECK_FALSE(decode_wav(bad_align).has_value());
}

TEST_CASE("WAV decoder rejects non-finite float samples") {
    std::vector<std::uint8_t> data;
    append_float32(data, std::numeric_limits<float>::infinity());
    auto wav = make_wav(3, 1, 48000, 32, std::move(data));
    CHECK_FALSE(decode_wav(wav).has_value());
}
