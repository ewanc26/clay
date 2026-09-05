#include "audio_stream.hpp"

#include "miniaudio.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace clay {

struct AudioStream::State {
    ma_decoder decoder{};
    std::size_t frames = 0;
    std::vector<float> buffer;
    std::size_t buffered_frames = 0;
    std::size_t buffer_offset = 0;
    bool at_end = false;
    AudioStreamReadStatus last_status = AudioStreamReadStatus::Ready;
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
    constexpr std::size_t kBufferFrames = 4096;
    state->buffer.resize(kBufferFrames * 2);
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
    if (length > std::numeric_limits<std::size_t>::max()) {
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
    if (state_ == nullptr || stereo_frames.size() % 2 != 0) {
        if (state_ != nullptr) state_->last_status = AudioStreamReadStatus::Error;
        return 0;
    }
    state_->last_status = AudioStreamReadStatus::Ready;
    std::size_t requested = stereo_frames.size() / 2;
    std::size_t written = 0;
    while (requested > 0) {
        if (state_->buffer_offset == state_->buffered_frames) {
            if (state_->at_end) {
                state_->last_status = AudioStreamReadStatus::EndOfStream;
                break;
            }
            ma_uint64 frames_read = 0;
            const ma_result result = ma_decoder_read_pcm_frames(
                &state_->decoder, state_->buffer.data(),
                static_cast<ma_uint64>(state_->buffer.size() / 2),
                &frames_read);
            if (result != MA_SUCCESS && result != MA_AT_END) {
                state_->last_status = AudioStreamReadStatus::Error;
                return written;
            }
            state_->buffered_frames = static_cast<std::size_t>(frames_read);
            state_->buffer_offset = 0;
            state_->at_end = result == MA_AT_END;
            if (state_->buffered_frames == 0) {
                state_->last_status = AudioStreamReadStatus::EndOfStream;
                break;
            }
        }

        const std::size_t available =
            state_->buffered_frames - state_->buffer_offset;
        const std::size_t count = std::min(requested, available);
        std::copy_n(state_->buffer.data() + state_->buffer_offset * 2,
                    count * 2, stereo_frames.data() + written * 2);
        state_->buffer_offset += count;
        written += count;
        requested -= count;
    }
    return written;
}

AudioStreamReadStatus AudioStream::last_read_status() const noexcept {
    return state_ == nullptr ? AudioStreamReadStatus::Error
                             : state_->last_status;
}

bool AudioStream::rewind() noexcept {
    if (state_ == nullptr ||
        ma_decoder_seek_to_pcm_frame(&state_->decoder, 0) != MA_SUCCESS)
        return false;
    state_->buffered_frames = 0;
    state_->buffer_offset = 0;
    state_->at_end = false;
    state_->last_status = AudioStreamReadStatus::Ready;
    return true;
}

} // namespace clay
