#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "audio/audio_mixer.hpp"

#include <array>
#include <vector>

using namespace clay;

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
    auto clip = mixer.add_clip(
        AudioClip{48000, 2, {0.75F, -0.25F, 0.5F, -0.5F}});
    REQUIRE(clip.has_value());
    REQUIRE(mixer.play(*clip).has_value());

    std::array<float, 4> output{};
    REQUIRE(mixer.mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.75F));
    CHECK(output[1] == doctest::Approx(-0.25F));
    CHECK(output[2] == doctest::Approx(0.5F));
    CHECK(output[3] == doctest::Approx(-0.5F));
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
        0.25F, 0.25F, -0.25F, -0.25F,
        0.25F, 0.25F, -0.25F, -0.25F,
    };
    for (std::size_t i = 0; i < output.size(); ++i) {
        CHECK(output[i] == doctest::Approx(expected[i]));
    }
    CHECK(mixer.voice_count() == 1);
    CHECK(mixer.stop(*voice));
    CHECK(mixer.voice_count() == 0);
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
