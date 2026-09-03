#include "audio_system.hpp"

#include "ecs/world.hpp"
#include "event.hpp"

#define _CRT_SECURE_NO_WARNINGS
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#undef STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

namespace clay {
namespace {

constexpr std::size_t kDecodeChunkFrames = 4096;
constexpr std::size_t kMaxEffectFrames = 48000u * 60u * 10u;

float clamp_gain(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : 0.0F;
}

struct Position {
    float x = 0.0F;
    float y = 0.0F;
};

std::optional<Position> entity_position(World &world, Entity entity) {
    if (!world.alive(entity)) return std::nullopt;
    if (auto *world_transform = world.storage<WorldTransform2D>().find(entity)) {
        return Position{world_transform->x, world_transform->y};
    }
    if (auto *transform = world.storage<Transform2D>().find(entity)) {
        return Position{transform->x, transform->y};
    }
    return std::nullopt;
}

bool is_ogg(std::span<const std::uint8_t> bytes) noexcept {
    return bytes.size() >= 4 && bytes[0] == 'O' && bytes[1] == 'g' &&
           bytes[2] == 'g' && bytes[3] == 'S';
}

AudioClip decode_effect_decoder(ma_decoder &decoder, std::uint32_t sample_rate) {
    AudioClip clip;
    clip.sample_rate = sample_rate;
    clip.channels = 2;
    std::array<float, kDecodeChunkFrames * 2> chunk{};
    for (;;) {
        ma_uint64 frames_read = 0;
        const ma_result result = ma_decoder_read_pcm_frames(
            &decoder, chunk.data(), kDecodeChunkFrames, &frames_read);
        if (frames_read > 0) {
            if (clip.frame_count() + static_cast<std::size_t>(frames_read) >
                kMaxEffectFrames) {
                clip.samples.clear();
                return clip;
            }
            clip.samples.insert(
                clip.samples.end(), chunk.begin(),
                chunk.begin() + static_cast<std::ptrdiff_t>(frames_read * 2));
        }
        if (result == MA_AT_END || frames_read < kDecodeChunkFrames) break;
        if (result != MA_SUCCESS) {
            clip.samples.clear();
            return clip;
        }
    }
    return clip;
}

AudioClip decode_ogg_effect(std::span<const std::uint8_t> bytes,
                            std::uint32_t target_rate) {
    AudioClip clip;
    clip.sample_rate = target_rate;
    clip.channels = 2;
    if (!is_ogg(bytes) || bytes.size() > static_cast<std::size_t>(
                                       (std::numeric_limits<int>::max)())) {
        return clip;
    }

    int channels = 0;
    int source_rate = 0;
    short *decoded = nullptr;
    const int source_frames = stb_vorbis_decode_memory(
        bytes.data(), static_cast<int>(bytes.size()), &channels, &source_rate,
        &decoded);
    if (source_frames <= 0 || decoded == nullptr ||
        (channels != 1 && channels != 2) || source_rate <= 0 ||
        target_rate == 0) {
        std::free(decoded);
        return clip;
    }

    const double ratio = static_cast<double>(target_rate) /
                         static_cast<double>(source_rate);
    const std::size_t target_frames = static_cast<std::size_t>(
        std::ceil(static_cast<double>(source_frames) * ratio));
    if (target_frames == 0 || target_frames > kMaxEffectFrames) {
        std::free(decoded);
        return clip;
    }

    clip.samples.resize(target_frames * 2);
    auto source_sample = [&](std::size_t frame, int channel) {
        frame = (std::min)(frame, static_cast<std::size_t>(source_frames - 1));
        const int source_channel = channels == 1 ? 0 : channel;
        return static_cast<float>(decoded[frame * static_cast<std::size_t>(channels) +
                                          static_cast<std::size_t>(source_channel)]) /
               32768.0F;
    };

    for (std::size_t frame = 0; frame < target_frames; ++frame) {
        const double source_position =
            static_cast<double>(frame) * static_cast<double>(source_rate) /
            static_cast<double>(target_rate);
        const auto left_index = static_cast<std::size_t>(source_position);
        const auto right_index = (std::min)(
            left_index + 1, static_cast<std::size_t>(source_frames - 1));
        const float fraction = static_cast<float>(
            source_position - static_cast<double>(left_index));
        for (int channel = 0; channel < 2; ++channel) {
            const float a = source_sample(left_index, channel);
            const float b = source_sample(right_index, channel);
            clip.samples[frame * 2 + static_cast<std::size_t>(channel)] =
                a + (b - a) * fraction;
        }
    }

    std::free(decoded);
    return clip;
}

} // namespace

struct AudioSystem::Impl {
    struct SpatialVoice {
        AudioVoiceId voice = 0;
        Entity emitter{};
        float max_distance = 1.0F;
        float base_gain = 1.0F;
    };

    struct MusicStream {
        ma_decoder decoder{};
        bool initialized = false;
        float gain = 1.0F;
        float target_gain = 1.0F;
        std::uint64_t fade_frames = 0;

        ~MusicStream() {
            if (initialized) ma_decoder_uninit(&decoder);
        }
    };

    World &world;
    Hub &hub;
    mutable std::mutex mutex;
    AudioMixer mixer;
    std::vector<SpatialVoice> spatial;
    std::optional<Entity> listener_entity;
    Position listener_position{};
    std::vector<std::unique_ptr<MusicStream>> music;
    std::vector<float> stream_scratch;
    ma_device device{};
    bool device_initialized = false;
    bool device_started = false;
    std::uint64_t play_subscription = 0;
    std::uint64_t stop_subscription = 0;
    std::uint64_t music_stop_subscription = 0;

    Impl(World &world_in, Hub &hub_in, std::uint32_t rate)
        : world(world_in), hub(hub_in), mixer(rate) {
        stream_scratch.reserve(kDecodeChunkFrames * 2);
        play_subscription = hub.subscribe(
            channel("audio.play"), [this](const cl_event &event) {
                if (event.value.kind == CLAY_VAR_I64 && event.value.i > 0 &&
                    event.value.i <=
                        (std::numeric_limits<AudioClipId>::max)()) {
                    (void)play(static_cast<AudioClipId>(event.value.i),
                               AudioBus::Sfx, false, 1.0F);
                }
            });
        stop_subscription = hub.subscribe(
            channel("audio.stop_all"),
            [this](const cl_event &) { stop_all(); });
        music_stop_subscription = hub.subscribe(
            channel("audio.music.stop"),
            [this](const cl_event &) { stop_music(0.0F); });
    }

    ~Impl() {
        stop_device();
        hub.unsubscribe(play_subscription);
        hub.unsubscribe(stop_subscription);
        hub.unsubscribe(music_stop_subscription);
    }

    static void device_callback(ma_device *device_ptr, void *output,
                                const void *, ma_uint32 frame_count) {
        auto *self = static_cast<Impl *>(device_ptr->pUserData);
        if (!self || !output) return;
        auto *samples = static_cast<float *>(output);
        (void)self->mix_stereo(
            std::span<float>(samples,
                             static_cast<std::size_t>(frame_count) * 2));
    }

    bool start_device() {
        {
            std::scoped_lock lock(mutex);
            if (device_started) return true;
            if (device_initialized) return false;
            ma_device_config config =
                ma_device_config_init(ma_device_type_playback);
            config.playback.format = ma_format_f32;
            config.playback.channels = 2;
            config.sampleRate = mixer.sample_rate();
            config.dataCallback = device_callback;
            config.pUserData = this;
            if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
                return false;
            device_initialized = true;
        }

        // ma_device_start() may invoke the callback immediately. Do not hold
        // the mixer mutex while starting or the callback could self-deadlock.
        if (ma_device_start(&device) != MA_SUCCESS) {
            ma_device_uninit(&device);
            std::scoped_lock lock(mutex);
            device_initialized = false;
            return false;
        }
        {
            std::scoped_lock lock(mutex);
            device_started = true;
        }
        return true;
    }

    void stop_device() noexcept {
        bool uninit = false;
        {
            std::scoped_lock lock(mutex);
            uninit = device_initialized;
            device_started = false;
            device_initialized = false;
        }
        if (uninit) ma_device_uninit(&device);
    }

    std::optional<AudioClipId> load_clip_file(const std::string &path) {
        if (path.empty()) return std::nullopt;
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) return std::nullopt;
        const std::streamoff size = input.tellg();
        if (size <= 0 ||
            static_cast<std::uint64_t>(size) >
                static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> encoded(static_cast<std::size_t>(size));
        input.seekg(0, std::ios::beg);
        if (!input.read(reinterpret_cast<char *>(encoded.data()),
                        static_cast<std::streamsize>(size)))
            return std::nullopt;
        return load_clip_memory(encoded);
    }

    std::optional<AudioClipId>
    load_clip_memory(std::span<const std::uint8_t> bytes) {
        if (bytes.empty()) return std::nullopt;

        AudioClip clip;
        if (is_ogg(bytes)) {
            clip = decode_ogg_effect(bytes, mixer.sample_rate());
        } else {
            ma_decoder_config config = ma_decoder_config_init(
                ma_format_f32, 2, mixer.sample_rate());
            ma_decoder decoder{};
            if (ma_decoder_init_memory(bytes.data(), bytes.size(), &config,
                                       &decoder) != MA_SUCCESS) {
                return std::nullopt;
            }
            clip = decode_effect_decoder(decoder, mixer.sample_rate());
            ma_decoder_uninit(&decoder);
        }

        if (!clip.valid()) return std::nullopt;
        std::scoped_lock lock(mutex);
        return mixer.add_clip(std::move(clip));
    }

    std::optional<AudioVoiceId> play(AudioClipId id, AudioBus bus, bool loop,
                                     float gain) {
        std::scoped_lock lock(mutex);
        return mixer.play(id, bus, loop, gain);
    }

    void stop_all() noexcept {
        std::scoped_lock lock(mutex);
        mixer.stop_all();
        spatial.clear();
        music.clear();
    }

    void stop_music(float seconds) noexcept {
        std::scoped_lock lock(mutex);
        if (!std::isfinite(seconds) || seconds <= 0.0F) {
            music.clear();
            return;
        }
        const auto frames = static_cast<std::uint64_t>(
            seconds * static_cast<float>(mixer.sample_rate()));
        for (auto &stream : music) {
            stream->target_gain = 0.0F;
            stream->fade_frames = (std::max<std::uint64_t>)(1, frames);
        }
    }

    bool play_music_file(const std::string &path, float seconds) {
        if (path.empty()) return false;
        auto next = std::make_unique<MusicStream>();
        ma_decoder_config config =
            ma_decoder_config_init(ma_format_f32, 2, mixer.sample_rate());
        if (ma_decoder_init_file(path.c_str(), &config, &next->decoder) !=
            MA_SUCCESS)
            return false;
        next->initialized = true;
        const float fade_seconds =
            std::isfinite(seconds) ? (std::max)(0.0F, seconds) : 0.0F;
        const std::uint64_t frames = static_cast<std::uint64_t>(
            fade_seconds * static_cast<float>(mixer.sample_rate()));
        next->gain = frames > 0 ? 0.0F : 1.0F;
        next->target_gain = 1.0F;
        next->fade_frames = frames;

        std::scoped_lock lock(mutex);
        for (auto &stream : music) {
            stream->target_gain = 0.0F;
            stream->fade_frames = (std::max<std::uint64_t>)(1, frames);
            if (frames == 0) stream->gain = 0.0F;
        }
        music.push_back(std::move(next));
        std::erase_if(music, [](const auto &stream) {
            return stream->gain <= 0.0F && stream->target_gain <= 0.0F &&
                   stream->fade_frames == 0;
        });
        return true;
    }

    void update_spatial() {
        std::scoped_lock lock(mutex);
        Position listener = listener_position;
        if (listener_entity) {
            if (auto position = entity_position(world, *listener_entity))
                listener = *position;
        }

        std::erase_if(spatial, [&](SpatialVoice &binding) {
            if (!mixer.voice_active(binding.voice)) return true;
            auto emitter = entity_position(world, binding.emitter);
            if (!emitter) {
                (void)mixer.stop(binding.voice);
                return true;
            }
            const float dx = emitter->x - listener.x;
            const float dy = emitter->y - listener.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const float attenuation = std::clamp(
                1.0F - distance / binding.max_distance, 0.0F, 1.0F);
            const float pan =
                std::clamp(dx / binding.max_distance, -1.0F, 1.0F);
            (void)mixer.set_voice_gain(binding.voice,
                                       binding.base_gain * attenuation);
            (void)mixer.set_voice_pan(binding.voice, pan);
            return false;
        });
    }

    bool mix_stereo(std::span<float> output) {
        if (output.size() % 2 != 0) {
            std::fill(output.begin(), output.end(), 0.0F);
            return false;
        }
        std::scoped_lock lock(mutex);
        if (!mixer.mix_stereo(output)) return false;
        if (music.empty()) return true;

        const std::size_t frames = output.size() / 2;
        stream_scratch.resize(output.size());

        for (auto &stream : music) {
            std::fill(stream_scratch.begin(), stream_scratch.end(), 0.0F);
            std::size_t produced = 0;
            bool rewound = false;
            while (produced < frames) {
                ma_uint64 read = 0;
                const ma_result result = ma_decoder_read_pcm_frames(
                    &stream->decoder, stream_scratch.data() + produced * 2,
                    frames - produced, &read);
                produced += static_cast<std::size_t>(read);
                if (produced >= frames) break;
                if (result == MA_AT_END || read == 0) {
                    if (rewound ||
                        ma_decoder_seek_to_pcm_frame(&stream->decoder, 0) !=
                            MA_SUCCESS)
                        break;
                    rewound = true;
                    continue;
                }
                if (result != MA_SUCCESS) break;
            }

            for (std::size_t frame = 0; frame < frames; ++frame) {
                if (stream->fade_frames > 0) {
                    stream->gain +=
                        (stream->target_gain - stream->gain) /
                        static_cast<float>(stream->fade_frames);
                    --stream->fade_frames;
                    if (stream->fade_frames == 0)
                        stream->gain = stream->target_gain;
                }
                const float gain = mixer.master_gain() *
                                   mixer.bus_gain(AudioBus::Music) *
                                   stream->gain;
                output[frame * 2] += stream_scratch[frame * 2] * gain;
                output[frame * 2 + 1] +=
                    stream_scratch[frame * 2 + 1] * gain;
            }
        }

        std::erase_if(music, [](const auto &stream) {
            return stream->fade_frames == 0 && stream->target_gain <= 0.0F &&
                   stream->gain <= 0.0F;
        });
        for (float &sample : output)
            sample = std::clamp(sample, -1.0F, 1.0F);
        return true;
    }
};

AudioSystem::AudioSystem(World &world, Hub &hub, std::uint32_t sample_rate)
    : impl_(std::make_unique<Impl>(world, hub, sample_rate)) {}
AudioSystem::~AudioSystem() = default;
std::uint32_t AudioSystem::sample_rate() const noexcept {
    return impl_->mixer.sample_rate();
}
bool AudioSystem::start_device() { return impl_->start_device(); }
void AudioSystem::stop_device() noexcept { impl_->stop_device(); }
bool AudioSystem::device_available() const noexcept {
    std::scoped_lock lock(impl_->mutex);
    return impl_->device_started;
}
std::optional<AudioClipId> AudioSystem::load_clip_file(const std::string &path) {
    return impl_->load_clip_file(path);
}
std::optional<AudioClipId>
AudioSystem::load_clip_memory(std::span<const std::uint8_t> encoded) {
    return impl_->load_clip_memory(encoded);
}
bool AudioSystem::unload_clip(AudioClipId id) {
    std::scoped_lock lock(impl_->mutex);
    return impl_->mixer.remove_clip(id);
}
std::optional<AudioVoiceId> AudioSystem::play(AudioClipId id, AudioBus bus,
                                               bool loop, float gain) {
    return impl_->play(id, bus, loop, gain);
}
std::optional<AudioVoiceId>
AudioSystem::play_spatial(AudioClipId id, Entity emitter, float max_distance,
                          float gain, bool loop) {
    if (!std::isfinite(max_distance) || max_distance <= 0.0F ||
        !impl_->world.alive(emitter))
        return std::nullopt;
    std::scoped_lock lock(impl_->mutex);
    auto voice = impl_->mixer.play(id, AudioBus::Sfx, loop, gain);
    if (!voice) return std::nullopt;
    impl_->spatial.push_back(
        {*voice, emitter, max_distance, clamp_gain(gain)});
    return voice;
}
bool AudioSystem::stop(AudioVoiceId id) {
    std::scoped_lock lock(impl_->mutex);
    std::erase_if(impl_->spatial,
                  [id](const Impl::SpatialVoice &binding) {
                      return binding.voice == id;
                  });
    return impl_->mixer.stop(id);
}
void AudioSystem::stop_all() noexcept { impl_->stop_all(); }
void AudioSystem::set_master_gain(float gain) noexcept {
    std::scoped_lock lock(impl_->mutex);
    impl_->mixer.set_master_gain(gain);
}
void AudioSystem::set_bus_gain(AudioBus bus, float gain) noexcept {
    std::scoped_lock lock(impl_->mutex);
    impl_->mixer.set_bus_gain(bus, gain);
}
float AudioSystem::master_gain() const noexcept {
    std::scoped_lock lock(impl_->mutex);
    return impl_->mixer.master_gain();
}
float AudioSystem::bus_gain(AudioBus bus) const noexcept {
    std::scoped_lock lock(impl_->mutex);
    return impl_->mixer.bus_gain(bus);
}
void AudioSystem::set_listener(Entity listener) noexcept {
    std::scoped_lock lock(impl_->mutex);
    impl_->listener_entity = listener;
}
void AudioSystem::set_listener_position(float x, float y) noexcept {
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    std::scoped_lock lock(impl_->mutex);
    impl_->listener_entity.reset();
    impl_->listener_position = {x, y};
}
void AudioSystem::clear_listener() noexcept {
    std::scoped_lock lock(impl_->mutex);
    impl_->listener_entity.reset();
    impl_->listener_position = {};
}
void AudioSystem::update_spatial() { impl_->update_spatial(); }
bool AudioSystem::play_music_file(const std::string &path,
                                  float crossfade_seconds) {
    return impl_->play_music_file(path, crossfade_seconds);
}
void AudioSystem::stop_music(float fade_seconds) noexcept {
    impl_->stop_music(fade_seconds);
}
bool AudioSystem::music_playing() const noexcept {
    std::scoped_lock lock(impl_->mutex);
    return !impl_->music.empty();
}
bool AudioSystem::mix_stereo(std::span<float> output) {
    return impl_->mix_stereo(output);
}

} // namespace clay
