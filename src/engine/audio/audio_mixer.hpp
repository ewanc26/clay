#ifndef CLAY_ENGINE_AUDIO_AUDIO_MIXER_HPP
#define CLAY_ENGINE_AUDIO_AUDIO_MIXER_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace clay {

enum class AudioBus : std::uint8_t {
    Sfx,
    Music,
};

using AudioClipId = std::uint32_t;
using AudioVoiceId = std::uint32_t;

struct AudioClip {
    std::uint32_t sample_rate = 48000;
    std::uint8_t channels = 1;
    std::vector<float> samples;

    [[nodiscard]] std::size_t frame_count() const noexcept {
        if (channels == 0) return 0;
        return samples.size() / channels;
    }

    [[nodiscard]] bool valid() const noexcept {
        if (sample_rate == 0 || (channels != 1 && channels != 2) ||
            samples.empty() || samples.size() % channels != 0) {
            return false;
        }
        return std::all_of(samples.begin(), samples.end(), [](float sample) {
            return std::isfinite(sample);
        });
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
        if (!clip.valid() || clip.sample_rate != sample_rate_) return std::nullopt;
        const AudioClipId id = next_clip_id_++;
        clips_.push_back(ClipEntry{id, std::move(clip)});
        return id;
    }

    bool remove_clip(AudioClipId id) {
        const auto before = clips_.size();
        std::erase_if(clips_, [id](const ClipEntry &entry) {
            return entry.id == id;
        });
        if (clips_.size() == before) return false;
        std::erase_if(voices_, [id](const Voice &voice) {
            return voice.clip_id == id;
        });
        return true;
    }

    [[nodiscard]] std::optional<AudioVoiceId>
    play(AudioClipId clip_id, AudioBus bus = AudioBus::Sfx,
         bool loop = false, float gain = 1.0F, float pan = 0.0F) {
        if (find_clip(clip_id) == nullptr) return std::nullopt;
        const AudioVoiceId id = next_voice_id_++;
        voices_.push_back(Voice{id, clip_id, 0, bus, clamp_gain(gain),
                                clamp_pan(pan), loop, true});
        return id;
    }

    bool stop(AudioVoiceId id) {
        const auto before = voices_.size();
        std::erase_if(voices_, [id](const Voice &voice) {
            return voice.id == id;
        });
        return voices_.size() != before;
    }

    void stop_all() noexcept { voices_.clear(); }

    bool set_voice_gain(AudioVoiceId id, float gain) noexcept {
        Voice *voice = find_voice(id);
        if (!voice) return false;
        voice->gain = clamp_gain(gain);
        return true;
    }

    bool set_voice_pan(AudioVoiceId id, float pan) noexcept {
        Voice *voice = find_voice(id);
        if (!voice) return false;
        voice->pan = clamp_pan(pan);
        return true;
    }

    [[nodiscard]] bool voice_active(AudioVoiceId id) const noexcept {
        return find_voice(id) != nullptr;
    }

    void set_master_gain(float gain) noexcept { master_gain_ = clamp_gain(gain); }
    [[nodiscard]] float master_gain() const noexcept { return master_gain_; }

    void set_bus_gain(AudioBus bus, float gain) noexcept {
        const float clamped = clamp_gain(gain);
        switch (bus) {
        case AudioBus::Sfx: sfx_gain_ = clamped; break;
        case AudioBus::Music: music_gain_ = clamped; break;
        }
    }

    [[nodiscard]] float bus_gain(AudioBus bus) const noexcept {
        switch (bus) {
        case AudioBus::Sfx: return sfx_gain_;
        case AudioBus::Music: return music_gain_;
        }
        return 1.0F;
    }

    [[nodiscard]] std::size_t clip_count() const noexcept { return clips_.size(); }
    [[nodiscard]] std::size_t voice_count() const noexcept { return voices_.size(); }

    // Mixes interleaved stereo frames into output. Pan uses a deterministic
    // linear balance law: centre is unity on both channels, while either
    // extreme mutes the opposite channel. Clipping happens after all voices.
    bool mix_stereo(std::span<float> output) {
        std::fill(output.begin(), output.end(), 0.0F);
        if (output.size() % 2 != 0) return false;

        const std::size_t output_frames = output.size() / 2;
        for (Voice &voice : voices_) {
            const ClipEntry *entry = find_clip(voice.clip_id);
            if (!entry) {
                voice.active = false;
                continue;
            }

            const AudioClip &clip = entry->clip;
            const std::size_t clip_frames = clip.frame_count();
            const float gain = master_gain_ * bus_gain(voice.bus) * voice.gain;
            const float left_gain = gain * (voice.pan > 0.0F ? 1.0F - voice.pan : 1.0F);
            const float right_gain = gain * (voice.pan < 0.0F ? 1.0F + voice.pan : 1.0F);

            for (std::size_t frame = 0; frame < output_frames; ++frame) {
                if (voice.cursor >= clip_frames) {
                    if (voice.loop) {
                        voice.cursor = 0;
                    } else {
                        voice.active = false;
                        break;
                    }
                }

                const std::size_t index = voice.cursor * clip.channels;
                const float left = clip.samples[index];
                const float right = clip.channels == 1 ? left : clip.samples[index + 1];
                output[frame * 2] += left * left_gain;
                output[frame * 2 + 1] += right * right_gain;

                ++voice.cursor;
                if (!voice.loop && voice.cursor >= clip_frames) {
                    voice.active = false;
                    break;
                }
            }
        }

        for (float &sample : output) sample = std::clamp(sample, -1.0F, 1.0F);
        std::erase_if(voices_, [](const Voice &voice) { return !voice.active; });
        return true;
    }

  private:
    struct ClipEntry {
        AudioClipId id;
        AudioClip clip;
    };

    struct Voice {
        AudioVoiceId id;
        AudioClipId clip_id;
        std::size_t cursor;
        AudioBus bus;
        float gain;
        float pan;
        bool loop;
        bool active;
    };

    [[nodiscard]] static float clamp_gain(float gain) noexcept {
        return std::isfinite(gain) ? std::clamp(gain, 0.0F, 1.0F) : 0.0F;
    }

    [[nodiscard]] static float clamp_pan(float pan) noexcept {
        return std::isfinite(pan) ? std::clamp(pan, -1.0F, 1.0F) : 0.0F;
    }

    [[nodiscard]] const ClipEntry *find_clip(AudioClipId id) const noexcept {
        const auto it = std::find_if(clips_.begin(), clips_.end(),
                                     [id](const ClipEntry &entry) {
                                         return entry.id == id;
                                     });
        return it == clips_.end() ? nullptr : &*it;
    }

    [[nodiscard]] Voice *find_voice(AudioVoiceId id) noexcept {
        const auto it = std::find_if(voices_.begin(), voices_.end(),
                                     [id](const Voice &voice) {
                                         return voice.id == id;
                                     });
        return it == voices_.end() ? nullptr : &*it;
    }

    [[nodiscard]] const Voice *find_voice(AudioVoiceId id) const noexcept {
        const auto it = std::find_if(voices_.begin(), voices_.end(),
                                     [id](const Voice &voice) {
                                         return voice.id == id;
                                     });
        return it == voices_.end() ? nullptr : &*it;
    }

    std::uint32_t sample_rate_ = 48000;
    float master_gain_ = 1.0F;
    float sfx_gain_ = 1.0F;
    float music_gain_ = 1.0F;
    AudioClipId next_clip_id_ = 1;
    AudioVoiceId next_voice_id_ = 1;
    std::vector<ClipEntry> clips_;
    std::vector<Voice> voices_;
};

} // namespace clay

#endif /* CLAY_ENGINE_AUDIO_AUDIO_MIXER_HPP */
