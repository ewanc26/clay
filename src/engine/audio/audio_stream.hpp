#ifndef CLAY_ENGINE_AUDIO_AUDIO_STREAM_HPP
#define CLAY_ENGINE_AUDIO_AUDIO_STREAM_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace clay {

/* Incremental decoder source for long-form audio. The decoder resamples to
 * the mixer's stereo float format, so a stream never becomes a mixer-owned
 * PCM buffer. */
class AudioStream {
  public:
    AudioStream();
    ~AudioStream();

    AudioStream(const AudioStream &) = delete;
    AudioStream &operator=(const AudioStream &) = delete;
    AudioStream(AudioStream &&) noexcept;
    AudioStream &operator=(AudioStream &&) noexcept;

    bool open(const std::string &path, std::uint32_t sample_rate);
    void close() noexcept;
    [[nodiscard]] bool is_open() const noexcept {
        return state_ != nullptr;
    }
    [[nodiscard]] std::size_t frame_count() const noexcept;
    [[nodiscard]] std::size_t read(std::span<float> stereo_frames) noexcept;
    bool rewind() noexcept;

  private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace clay

#endif /* CLAY_ENGINE_AUDIO_AUDIO_STREAM_HPP */
