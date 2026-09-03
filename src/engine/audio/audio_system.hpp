#ifndef CLAY_ENGINE_AUDIO_AUDIO_SYSTEM_HPP
#define CLAY_ENGINE_AUDIO_AUDIO_SYSTEM_HPP

#include "audio_mixer.hpp"
#include "ecs/components.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace clay {

class Hub;
class World;

class AudioSystem {
  public:
    explicit AudioSystem(World &world, Hub &hub,
                         std::uint32_t sample_rate = 48000);
    ~AudioSystem();

    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;

    [[nodiscard]] std::uint32_t sample_rate() const noexcept;

    // Device output is deliberately opt-in. A failed/no device remains a
    // valid headless AudioSystem and manual mix_stereo() continues to work.
    [[nodiscard]] bool start_device();
    void stop_device() noexcept;
    [[nodiscard]] bool device_available() const noexcept;

    [[nodiscard]] std::optional<AudioClipId>
    load_clip_file(const std::string &path);
    [[nodiscard]] std::optional<AudioClipId>
    load_clip_memory(std::span<const std::uint8_t> encoded);
    bool unload_clip(AudioClipId id);

    [[nodiscard]] std::optional<AudioVoiceId>
    play(AudioClipId id, AudioBus bus = AudioBus::Sfx, bool loop = false,
         float gain = 1.0F);
    [[nodiscard]] std::optional<AudioVoiceId>
    play_spatial(AudioClipId id, Entity emitter, float max_distance,
                 float gain = 1.0F, bool loop = false);
    bool stop(AudioVoiceId id);
    void stop_all() noexcept;

    void set_master_gain(float gain) noexcept;
    void set_bus_gain(AudioBus bus, float gain) noexcept;
    [[nodiscard]] float master_gain() const noexcept;
    [[nodiscard]] float bus_gain(AudioBus bus) const noexcept;

    void set_listener(Entity listener) noexcept;
    void set_listener_position(float x, float y) noexcept;
    void clear_listener() noexcept;
    void update_spatial();

    // Background music is decoded incrementally rather than stored as an
    // AudioClip. Starting a new track crossfades from the current stream.
    [[nodiscard]] bool play_music_file(const std::string &path,
                                       float crossfade_seconds = 1.0F);
    void stop_music(float fade_seconds = 0.0F) noexcept;
    [[nodiscard]] bool music_playing() const noexcept;

    // Headless/test entry point; interleaved stereo float PCM.
    bool mix_stereo(std::span<float> output);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace clay

#endif /* CLAY_ENGINE_AUDIO_AUDIO_SYSTEM_HPP */
