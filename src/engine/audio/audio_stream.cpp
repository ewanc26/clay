#include "audio_stream.hpp"

#include "miniaudio.h"

#include <algorithm>
#include <utility>

namespace clay {

struct AudioStream::State {
    ma_decoder decoder{};
    std::size_t frames = 0;
};

AudioStream::AudioStream() = default;
AudioStream::~AudioStream() {
    close();
}

AudioStream::AudioStream(AudioStream &&other) noexcept
    : state_(std::move(other.state_)) {}

AudioStream &AudioStream::operator=(AudioStream &&other) noexcept {
    if (this == &other) return *this;
    close();
    state_ = std::move(other.state_);
    return *this;
}

bool AudioStream::open(const std::string &path, std::uint32_t sample_rate) {
    close();
    if (path.empty() || sample_rate == 0) return false;

    auto state = std::make_unique<State>();
    const ma_decoder_config config =
        ma_decoder_config_init(ma_format_f32, 2, sample_rate);
    if (ma_decoder_init_file(path.c_str(), &config, &state->decoder) !=
        MA_SUCCESS) {
        return false;
    }
    ma_uint64 length = 0;
    if (ma_decoder_get_length_in_pcm_frames(&state->decoder, &length) !=
        MA_SUCCESS) {
        ma_decoder_uninit(&state->decoder);
        return false;
    }
    state->frames = static_cast<std::size_t>(length);
    state_ = std::move(state);
    return true;
}

void AudioStream::close() noexcept {
    if (state_ != nullptr) ma_decoder_uninit(&state_->decoder);
    state_.reset();
}

std::size_t AudioStream::frame_count() const noexcept {
    return state_ == nullptr ? 0 : state_->frames;
}

std::size_t AudioStream::read(std::span<float> stereo_frames) noexcept {
    if (state_ == nullptr || stereo_frames.size() % 2 != 0) return 0;
    ma_uint64 frames_read = 0;
    const ma_result result = ma_decoder_read_pcm_frames(
        &state_->decoder, stereo_frames.data(),
        static_cast<ma_uint64>(stereo_frames.size() / 2), &frames_read);
    if (result != MA_SUCCESS && result != MA_AT_END) return 0;
    return std::min<std::size_t>(stereo_frames.size() / 2,
                                 static_cast<std::size_t>(frames_read));
}

bool AudioStream::rewind() noexcept {
    return state_ != nullptr &&
           ma_decoder_seek_to_pcm_frame(&state_->decoder, 0) == MA_SUCCESS;
}

} // namespace clay
