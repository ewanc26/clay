#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#include "miniaudio.h"

#include "audio_device.hpp"

#include <span>

namespace clay {

struct AudioDevice::State {
    ma_device device{};
};

void AudioDevice::callback(ma_device *device, void *output, const void *,
                           ma_uint32 frames) {
    auto *owner = static_cast<AudioDevice *>(device->pUserData);
    auto *samples = static_cast<float *>(output);
    if (!owner->mixer_.mix_stereo(std::span<float>(samples, frames * 2u))) {
        for (ma_uint32 i = 0; i < frames * 2u; ++i) samples[i] = 0.0F;
    }
}

AudioDevice::AudioDevice(AudioMixer &mixer) noexcept : mixer_(mixer) {}

AudioDevice::~AudioDevice() {
    if (open_) ma_device_uninit(&state_->device);
    delete state_;
}

bool AudioDevice::open(std::uint32_t sample_rate) {
    if (open_ || sample_rate == 0 || sample_rate != mixer_.sample_rate())
        return false;
    State *state = nullptr;
    try {
        state = new State{};
    } catch (...) {
        return false;
    }
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = sample_rate;
    config.dataCallback = &AudioDevice::callback;
    config.pUserData = this;
    if (ma_device_init(nullptr, &config, &state->device) != MA_SUCCESS) {
        delete state;
        return false;
    }
    state_ = state;
    open_ = true;
    return true;
}

bool AudioDevice::start() {
    if (!open_ || started_ || ma_device_start(&state_->device) != MA_SUCCESS)
        return false;
    started_ = true;
    return true;
}

bool AudioDevice::stop() {
    if (!open_ || !started_ || ma_device_stop(&state_->device) != MA_SUCCESS)
        return false;
    started_ = false;
    return true;
}

} // namespace clay
