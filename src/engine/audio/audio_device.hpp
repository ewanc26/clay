#ifndef CLAY_ENGINE_AUDIO_AUDIO_DEVICE_HPP
#define CLAY_ENGINE_AUDIO_AUDIO_DEVICE_HPP

#include "audio_mixer.hpp"
#include "miniaudio.h"

#include <cstdint>

namespace clay {

/* Optional real-time presenter for AudioMixer. Construction does not touch a
 * device; open() selects the platform default and start() begins callbacks. */
class AudioDevice {
  public:
    explicit AudioDevice(AudioMixer &mixer) noexcept;
    ~AudioDevice();

    AudioDevice(const AudioDevice &) = delete;
    AudioDevice &operator=(const AudioDevice &) = delete;

    bool open(std::uint32_t sample_rate = 48000);
    bool start();
    bool stop();
    bool is_open() const noexcept { return open_; }
    bool is_started() const noexcept { return started_; }

  private:
    static void callback(ma_device *device, void *output, const void *input,
                         ma_uint32 frames);
    struct State;
    AudioMixer &mixer_;
    State *state_ = nullptr;
    bool open_ = false;
    bool started_ = false;
};

} // namespace clay

#endif /* CLAY_ENGINE_AUDIO_AUDIO_DEVICE_HPP */
