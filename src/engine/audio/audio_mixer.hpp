#ifndef CLAY_ENGINE_AUDIO_AUDIO_MIXER_HPP
#define CLAY_ENGINE_AUDIO_AUDIO_MIXER_HPP

#include "audio_stream.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

namespace clay {

enum class AudioBus : std::uint8_t {
    Sfx,
    Music,
};

using AudioClipId = std::uint32_t;
using AudioStreamId = std::uint32_t;
using AudioVoiceId = std::uint32_t;

struct AudioClip {
    std::uint32_t sample_rate = 48000;
    std::uint8_t channels = 1;
    std::vector<float> samples;

    [[nodiscard]] std::size_t frame_count() const noexcept {
        if (channels == 0) {
            return 0;
        }
        return samples.size() / channels;
    }

    [[nodiscard]] bool valid() const noexcept {
        return sample_rate > 0 && (channels == 1 || channels == 2) &&
               !samples.empty() && samples.size() % channels == 0 &&
               std::all_of(samples.begin(), samples.end(),
                           [](float sample) { return std::isfinite(sample); });
    }
};

class AudioMixer {
  public:
    explicit AudioMixer(std::uint32_t sample_rate = 48000) noexcept
        : sample_rate_(sample_rate) {}

    [[nodiscard]] std::uint32_t sample_rate() const noexcept {
        return sample_rate_;
    }

    [[nodiscard]] std::optional<AudioClipId> add_clip(AudioClip clip) {
        std::lock_guard lock(mutex_);
        if (!clip.valid() || clip.sample_rate != sample_rate_) {
            return std::nullopt;
        }

        const AudioClipId id = next_clip_id_++;
        clips_.push_back(ClipEntry{id, std::move(clip)});
        return id;
    }

    [[nodiscard]] std::optional<AudioStreamId>
    add_stream(std::unique_ptr<AudioStream> stream) {
        std::lock_guard lock(mutex_);
        if (!stream || !stream->is_open()) return std::nullopt;
        const AudioStreamId id = next_stream_id_++;
        streams_.push_back(StreamEntry{id, std::move(stream)});
        return id;
    }

    bool remove_stream(AudioStreamId id) {
        std::lock_guard lock(mutex_);
        const auto before = streams_.size();
        std::erase_if(streams_, [id](const StreamEntry &entry) {
            return entry.id == id;
        });
        if (streams_.size() == before) return false;
        std::erase_if(voices_, [id](const Voice &voice) {
            return voice.stream_id == id;
        });
        return true;
    }

    bool remove_clip(AudioClipId id) {
        std::lock_guard lock(mutex_);
        const auto before = clips_.size();
        std::erase_if(clips_,
                      [id](const ClipEntry &entry) { return entry.id == id; });
        if (clips_.size() == before) {
            return false;
        }

        std::erase_if(voices_,
                      [id](const Voice &voice) { return voice.clip_id == id; });
        return true;
    }

    [[nodiscard]] std::optional<AudioVoiceId> play(AudioClipId clip_id,
                                                   AudioBus bus = AudioBus::Sfx,
                                                   bool loop = false,
                                                   float gain = 1.0F) {
        std::lock_guard lock(mutex_);
        if (find_clip(clip_id) == nullptr) {
            return std::nullopt;
        }

        const AudioVoiceId id = next_voice_id_++;
        voices_.push_back(Voice{id, clip_id, 0, 0, bus, clamp_gain(gain), loop,
                                0.0F, clamp_gain(gain), 0.0F, 0, false, false,
                                true, false, 0.0F, 0.0F, 1.0F});
        return id;
    }

    [[nodiscard]] std::optional<AudioVoiceId>
    play_stream(AudioStreamId stream_id, AudioBus bus = AudioBus::Music,
                bool loop = true, float gain = 1.0F) {
        std::lock_guard lock(mutex_);
        if (find_stream(stream_id) == nullptr) return std::nullopt;
        const AudioVoiceId id = next_voice_id_++;
        voices_.push_back(Voice{id, 0, stream_id, 0, bus, clamp_gain(gain),
                                loop, 0.0F, clamp_gain(gain), 0.0F, 0, false,
                                false, true, false, 0.0F, 0.0F, 1.0F});
        return id;
    }

    [[nodiscard]] std::optional<AudioVoiceId>
    crossfade_music(AudioClipId clip_id, std::size_t duration_frames) {
        std::lock_guard lock(mutex_);
        if (find_clip(clip_id) == nullptr) return std::nullopt;

        for (Voice &voice : voices_) {
            if (!voice.active || voice.bus != AudioBus::Music) continue;
            voice.fade_target = 0.0F;
            voice.fade_remaining = duration_frames;
            voice.fade_stop_when_done = true;
            voice.fade_step =
                duration_frames == 0
                    ? 0.0F
                    : -voice.gain / static_cast<float>(duration_frames);
            if (duration_frames == 0) {
                voice.gain = 0.0F;
                voice.active = false;
            }
        }

        const AudioVoiceId id = next_voice_id_++;
        const float initial_gain = duration_frames == 0 ? 1.0F : 0.0F;
        const float fade_step =
            duration_frames == 0 ? 0.0F
                                 : 1.0F / static_cast<float>(duration_frames);
        voices_.push_back(Voice{id, clip_id, 0, 0, AudioBus::Music,
                                initial_gain, false, 0.0F, 1.0F, fade_step,
                                duration_frames, false, false, true, false,
                                0.0F, 0.0F, 1.0F});
        return id;
    }

    [[nodiscard]] std::optional<AudioVoiceId>
    crossfade_music_stream(AudioStreamId stream_id,
                           std::size_t duration_frames) {
        std::lock_guard lock(mutex_);
        if (find_stream(stream_id) == nullptr) return std::nullopt;
        for (Voice &voice : voices_) {
            if (!voice.active || voice.bus != AudioBus::Music) continue;
            voice.fade_target = 0.0F;
            voice.fade_remaining = duration_frames;
            voice.fade_stop_when_done = true;
            voice.fade_step =
                duration_frames == 0
                    ? 0.0F
                    : -voice.gain / static_cast<float>(duration_frames);
            if (duration_frames == 0) {
                voice.gain = 0.0F;
                voice.active = false;
            }
        }
        const AudioVoiceId id = next_voice_id_++;
        const float initial_gain = duration_frames == 0 ? 1.0F : 0.0F;
        const float fade_step =
            duration_frames == 0 ? 0.0F
                                 : 1.0F / static_cast<float>(duration_frames);
        voices_.push_back(Voice{id, 0, stream_id, 0, AudioBus::Music,
                                initial_gain, true, 0.0F, 1.0F, fade_step,
                                duration_frames, false, false, true, false,
                                0.0F, 0.0F, 1.0F});
        return id;
    }

    void set_listener_position(float x, float y) noexcept {
        std::lock_guard lock(mutex_);
        if (std::isfinite(x) && std::isfinite(y)) {
            listener_x_ = x;
            listener_y_ = y;
        }
    }

    bool set_voice_position(AudioVoiceId id, float x, float y,
                            float max_distance) noexcept {
        std::lock_guard lock(mutex_);
        if (!std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(max_distance) || max_distance <= 0.0F)
            return false;
        for (Voice &voice : voices_) {
            if (voice.id == id && voice.active) {
                voice.spatial = true;
                voice.x = x;
                voice.y = y;
                voice.max_distance = max_distance;
                return true;
            }
        }
        return false;
    }

    bool clear_voice_position(AudioVoiceId id) noexcept {
        std::lock_guard lock(mutex_);
        for (Voice &voice : voices_) {
            if (voice.id == id && voice.active) {
                voice.spatial = false;
                return true;
            }
        }
        return false;
    }

    bool stop(AudioVoiceId id) {
        std::lock_guard lock(mutex_);
        const auto before = voices_.size();
        std::erase_if(voices_,
                      [id](const Voice &voice) { return voice.id == id; });
        return voices_.size() != before;
    }

    bool pause(AudioVoiceId id) {
        std::lock_guard lock(mutex_);
        for (Voice &voice : voices_) {
            if (voice.id == id && voice.active) {
                voice.paused = true;
                return true;
            }
        }
        return false;
    }

    bool resume(AudioVoiceId id) {
        std::lock_guard lock(mutex_);
        for (Voice &voice : voices_) {
            if (voice.id == id && voice.active) {
                voice.paused = false;
                return true;
            }
        }
        return false;
    }

    bool set_voice_pan(AudioVoiceId id, float pan) noexcept {
        std::lock_guard lock(mutex_);
        if (!std::isfinite(pan)) return false;
        for (Voice &voice : voices_) {
            if (voice.id == id && voice.active) {
                voice.pan = std::clamp(pan, -1.0F, 1.0F);
                return true;
            }
        }
        return false;
    }

    bool set_voice_gain(AudioVoiceId id, float gain) noexcept {
        std::lock_guard lock(mutex_);
        if (!std::isfinite(gain)) return false;
        for (Voice &voice : voices_) {
            if (voice.id == id && voice.active) {
                voice.gain = clamp_gain(gain);
                voice.fade_target = voice.gain;
                voice.fade_step = 0.0F;
                voice.fade_remaining = 0;
                voice.fade_stop_when_done = false;
                return true;
            }
        }
        return false;
    }

    bool fade_voice(AudioVoiceId id, float target_gain,
                    std::size_t duration_frames) noexcept {
        std::lock_guard lock(mutex_);
        if (!std::isfinite(target_gain)) return false;
        for (Voice &voice : voices_) {
            if (voice.id == id && voice.active) {
                voice.fade_target = clamp_gain(target_gain);
                voice.fade_remaining = duration_frames;
                voice.fade_stop_when_done = false;
                voice.fade_step = duration_frames == 0
                                      ? 0.0F
                                      : (voice.fade_target - voice.gain) /
                                            static_cast<float>(duration_frames);
                if (duration_frames == 0) voice.gain = voice.fade_target;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] float voice_gain(AudioVoiceId id) const noexcept {
        std::lock_guard lock(mutex_);
        for (const Voice &voice : voices_) {
            if (voice.id == id && voice.active) return voice.gain;
        }
        return 0.0F;
    }

    [[nodiscard]] float voice_pan(AudioVoiceId id) const noexcept {
        std::lock_guard lock(mutex_);
        for (const Voice &voice : voices_) {
            if (voice.id == id && voice.active) return voice.pan;
        }
        return 0.0F;
    }

    [[nodiscard]] bool voice_active(AudioVoiceId id) const noexcept {
        std::lock_guard lock(mutex_);
        for (const Voice &voice : voices_) {
            if (voice.id == id) return voice.active && !voice.paused;
        }
        return false;
    }

    [[nodiscard]] bool voice_paused(AudioVoiceId id) const noexcept {
        std::lock_guard lock(mutex_);
        for (const Voice &voice : voices_) {
            if (voice.id == id) return voice.active && voice.paused;
        }
        return false;
    }

    [[nodiscard]] std::size_t clip_frame_count(AudioClipId id) const noexcept {
        std::lock_guard lock(mutex_);
        for (const ClipEntry &entry : clips_) {
            if (entry.id == id) return entry.clip.frame_count();
        }
        return 0;
    }

    [[nodiscard]] std::size_t
    stream_frame_count(AudioStreamId id) const noexcept {
        std::lock_guard lock(mutex_);
        const StreamEntry *entry = find_stream(id);
        return entry == nullptr ? 0 : entry->stream->frame_count();
    }

    [[nodiscard]] AudioStreamReadStatus
    stream_read_status(AudioStreamId id) const noexcept {
        std::lock_guard lock(mutex_);
        const StreamEntry *entry = find_stream(id);
        return entry == nullptr ? AudioStreamReadStatus::Error
                                : entry->stream->last_read_status();
    }

    void stop_all() noexcept {
        std::lock_guard lock(mutex_);
        voices_.clear();
    }

    void stop_bus(AudioBus bus) noexcept {
        std::lock_guard lock(mutex_);
        std::erase_if(voices_, [bus](const Voice &voice) {
            return voice.bus == bus;
        });
    }

    void set_master_gain(float gain) noexcept {
        std::lock_guard lock(mutex_);
        master_gain_ = clamp_gain(gain);
    }

    [[nodiscard]] float master_gain() const noexcept {
        std::lock_guard lock(mutex_);
        return master_gain_;
    }

    void set_bus_gain(AudioBus bus, float gain) noexcept {
        std::lock_guard lock(mutex_);
        const float clamped = clamp_gain(gain);
        switch (bus) {
            case AudioBus::Sfx:
                sfx_gain_ = clamped;
                break;
            case AudioBus::Music:
                music_gain_ = clamped;
                break;
        }
    }

    [[nodiscard]] float bus_gain(AudioBus bus) const noexcept {
        std::lock_guard lock(mutex_);
        switch (bus) {
            case AudioBus::Sfx:
                return sfx_gain_;
            case AudioBus::Music:
                return music_gain_;
        }
        return 1.0F;
    }

    [[nodiscard]] std::size_t clip_count() const noexcept {
        std::lock_guard lock(mutex_);
        return clips_.size();
    }

    [[nodiscard]] std::size_t stream_count() const noexcept {
        std::lock_guard lock(mutex_);
        return streams_.size();
    }

    [[nodiscard]] std::size_t voice_count() const noexcept {
        std::lock_guard lock(mutex_);
        return voices_.size();
    }

    // Mixes interleaved stereo frames into output. The mixer owns no audio
    // device; platform backends can call this from their callback, while
    // headless tests can exercise the exact same deterministic path.
    bool mix_stereo(std::span<float> output) {
        std::lock_guard lock(mutex_);
        std::fill(output.begin(), output.end(), 0.0F);
        if (output.size() % 2 != 0) {
            return false;
        }

        const std::size_t output_frames = output.size() / 2;
        for (Voice &voice : voices_) {
            const ClipEntry *clip_entry =
                voice.clip_id == 0 ? nullptr : find_clip(voice.clip_id);
            StreamEntry *stream_entry =
                voice.stream_id == 0 ? nullptr : find_stream(voice.stream_id);
            if (clip_entry == nullptr && stream_entry == nullptr) {
                voice.active = false;
                continue;
            }

            const AudioClip *clip =
                clip_entry == nullptr ? nullptr : &clip_entry->clip;
            if (voice.paused) continue;
            const std::size_t clip_frames =
                clip == nullptr ? 0 : clip->frame_count();
            const float bus_gain_value =
                voice.bus == AudioBus::Sfx ? sfx_gain_ : music_gain_;
            const float left_pan_gain =
                voice.pan > 0.0F ? 1.0F - voice.pan : 1.0F;
            const float right_pan_gain =
                voice.pan < 0.0F ? 1.0F + voice.pan : 1.0F;
            float spatial_gain = 1.0F;
            float spatial_pan = 0.0F;
            if (voice.spatial) {
                const float dx = voice.x - listener_x_;
                const float dy = voice.y - listener_y_;
                const float distance = std::sqrt(dx * dx + dy * dy);
                spatial_gain = std::clamp(1.0F - distance / voice.max_distance,
                                          0.0F, 1.0F);
                spatial_pan = std::clamp(dx / voice.max_distance, -1.0F, 1.0F);
            }
            const float spatial_left =
                spatial_pan > 0.0F ? 1.0F - spatial_pan : 1.0F;
            const float spatial_right =
                spatial_pan < 0.0F ? 1.0F + spatial_pan : 1.0F;

            for (std::size_t frame = 0; frame < output_frames; ++frame) {
                float left = 0.0F;
                float right = 0.0F;
                if (clip != nullptr) {
                    if (voice.cursor >= clip_frames) {
                        if (voice.loop) {
                            voice.cursor = 0;
                        } else {
                            voice.active = false;
                            break;
                        }
                    }
                    const std::size_t index = voice.cursor * clip->channels;
                    left = clip->samples[index];
                    right =
                        clip->channels == 1 ? left : clip->samples[index + 1];
                } else {
                    float stream_sample[2]{};
                    std::span<float> frame_samples(stream_sample, 2);
                    std::size_t frames_read =
                        stream_entry->stream->read(frame_samples);
                    if (frames_read == 0 && voice.loop &&
                        stream_entry->stream->rewind())
                        frames_read = stream_entry->stream->read(frame_samples);
                    if (frames_read == 0) {
                        voice.active = false;
                        break;
                    }
                    left = stream_sample[0];
                    right = stream_sample[1];
                }
                const float gain =
                    master_gain_ * bus_gain_value * voice.gain * spatial_gain;
                output[frame * 2] += left * gain * left_pan_gain * spatial_left;
                output[frame * 2 + 1] +=
                    right * gain * right_pan_gain * spatial_right;

                if (clip != nullptr) ++voice.cursor;
                if (voice.fade_remaining > 0) {
                    --voice.fade_remaining;
                    if (voice.fade_remaining == 0) {
                        voice.gain = voice.fade_target;
                        if (voice.fade_stop_when_done) voice.active = false;
                    } else {
                        voice.gain += voice.fade_step;
                    }
                }
                if (!voice.active) break;
                if (clip != nullptr && !voice.loop &&
                    voice.cursor >= clip_frames) {
                    voice.active = false;
                    break;
                }
            }
        }

        for (float &sample : output) {
            sample = std::clamp(sample, -1.0F, 1.0F);
        }
        std::erase_if(voices_,
                      [](const Voice &voice) { return !voice.active; });
        return true;
    }

  private:
    struct ClipEntry {
        AudioClipId id;
        AudioClip clip;
    };

    struct StreamEntry {
        AudioStreamId id;
        std::unique_ptr<AudioStream> stream;
    };

    struct Voice {
        AudioVoiceId id;
        AudioClipId clip_id;
        AudioStreamId stream_id;
        std::size_t cursor;
        AudioBus bus;
        float gain;
        bool loop;
        float pan;
        float fade_target;
        float fade_step;
        std::size_t fade_remaining;
        bool paused;
        bool fade_stop_when_done;
        bool active;
        bool spatial;
        float x;
        float y;
        float max_distance;
    };

    [[nodiscard]] static float clamp_gain(float gain) noexcept {
        if (!std::isfinite(gain)) return 0.0F;
        return std::clamp(gain, 0.0F, 1.0F);
    }

    [[nodiscard]] const ClipEntry *find_clip(AudioClipId id) const noexcept {
        const auto it = std::find_if(
            clips_.begin(), clips_.end(),
            [id](const ClipEntry &entry) { return entry.id == id; });
        return it == clips_.end() ? nullptr : &*it;
    }

    [[nodiscard]] StreamEntry *find_stream(AudioStreamId id) noexcept {
        const auto it = std::find_if(
            streams_.begin(), streams_.end(),
            [id](const StreamEntry &entry) { return entry.id == id; });
        return it == streams_.end() ? nullptr : &*it;
    }

    [[nodiscard]] const StreamEntry *
    find_stream(AudioStreamId id) const noexcept {
        const auto it = std::find_if(
            streams_.begin(), streams_.end(),
            [id](const StreamEntry &entry) { return entry.id == id; });
        return it == streams_.end() ? nullptr : &*it;
    }

    std::uint32_t sample_rate_ = 48000;
    float master_gain_ = 1.0F;
    float sfx_gain_ = 1.0F;
    float music_gain_ = 1.0F;
    float listener_x_ = 0.0F;
    float listener_y_ = 0.0F;
    AudioClipId next_clip_id_ = 1;
    AudioStreamId next_stream_id_ = 1;
    AudioVoiceId next_voice_id_ = 1;
    std::vector<ClipEntry> clips_;
    std::vector<StreamEntry> streams_;
    std::vector<Voice> voices_;
    mutable std::mutex mutex_;
};

} // namespace clay

#endif /* CLAY_ENGINE_AUDIO_AUDIO_MIXER_HPP */
