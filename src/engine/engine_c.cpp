#include <clay/engine_c.h>

#include "audio/audio_system.hpp"
#include "imageio.hpp"
#include "runtime.hpp"
#include "systems/builtin.hpp"

#include <cmath>
#include <new>
#include <utility>

namespace {

bool finite(float value) { return std::isfinite(value); }
bool valid_color(float r, float g, float b, float a) {
    return finite(r) && finite(g) && finite(b) && finite(a);
}
bool valid_dimensions(int width, int height) {
    return width > 0 && height > 0 &&
           (size_t)width <= SIZE_MAX / (size_t)height &&
           (size_t)width * (size_t)height <= CLAY_ENGINE_MAX_FRAMEBUFFER_PIXELS;
}
bool valid_bus(cl_audio_bus bus) {
    return bus == CLAY_AUDIO_BUS_SFX || bus == CLAY_AUDIO_BUS_MUSIC;
}
clay::AudioBus to_bus(cl_audio_bus bus) {
    return bus == CLAY_AUDIO_BUS_MUSIC ? clay::AudioBus::Music
                                       : clay::AudioBus::Sfx;
}

template <typename Fn> cl_err guarded(Fn &&fn) {
    try {
        std::forward<Fn>(fn)();
        return CLAY_OK;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_INVALID_ARG;
    }
}

} // namespace

struct cl_engine_runtime {
    clay::Runtime impl;
    clay::AudioSystem audio;

    cl_engine_runtime(int width, int height, uint64_t seed, size_t arena_bytes)
        : impl(width, height, seed, arena_bytes),
          audio(impl.world(), impl.hub()) {}
};

extern "C" uint32_t cl_engine_runtime_abi_version(void) {
    return CLAY_ENGINE_ABI_VERSION;
}
extern "C" const char *cl_engine_error_string(cl_err error) {
    return cl_err_str(error);
}
extern "C" cl_engine_runtime *cl_engine_runtime_create(int width, int height,
                                                          uint64_t seed) {
    return cl_engine_runtime_create_with_arena(width, height, seed, 4u << 20);
}
extern "C" cl_engine_runtime *cl_engine_runtime_create_with_arena(
    int width, int height, uint64_t seed, size_t arena_bytes) {
    if (!valid_dimensions(width, height) ||
        arena_bytes < CLAY_ENGINE_MIN_ARENA_BYTES) return nullptr;
    try {
        return new cl_engine_runtime(width, height, seed, arena_bytes);
    } catch (...) {
        return nullptr;
    }
}
extern "C" void cl_engine_runtime_destroy(cl_engine_runtime *runtime) {
    delete runtime;
}
extern "C" cl_err cl_engine_runtime_step(cl_engine_runtime *runtime,
                                           double dt_seconds) {
    if (!runtime || !std::isfinite(dt_seconds) || dt_seconds < 0.0)
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        runtime->impl.step(dt_seconds);
        runtime->audio.update_spatial();
    });
}
extern "C" cl_err cl_engine_runtime_resize(cl_engine_runtime *runtime,
                                             int width, int height) {
    if (!runtime || !valid_dimensions(width, height)) return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.resize(width, height) ? CLAY_OK : CLAY_ERR_INVALID_ARG;
    } catch (const std::bad_alloc &) { return CLAY_ERR_OOM; }
    catch (...) { return CLAY_ERR_INVALID_ARG; }
}
extern "C" cl_err cl_engine_runtime_feed(cl_engine_runtime *runtime,
                                           const cl_input_event *event) {
    if (!runtime || !event || !cl_input_event_valid(event)) return CLAY_ERR_INVALID_ARG;
    return guarded([&] { runtime->impl.feed(*event); });
}
extern "C" cl_err cl_engine_runtime_feed_key(cl_engine_runtime *runtime,
                                               cl_key key, bool pressed) {
    if (!runtime || key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT)
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] { runtime->impl.feed(pressed ? cl_input_event_make(CLAY_IN_PRESS, key)
                                                    : cl_input_event_make(CLAY_IN_RELEASE, key)); });
}
extern "C" cl_err cl_engine_runtime_feed_key_at(
    cl_engine_runtime *runtime, cl_key key, bool pressed, double x, double y,
    int mods) {
    if (!runtime || key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT ||
        !std::isfinite(x) || !std::isfinite(y)) return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        cl_input_event event = cl_input_event_make(
            pressed ? CLAY_IN_PRESS : CLAY_IN_RELEASE, key);
        event.x = x; event.y = y; event.mods = mods;
        runtime->impl.feed(event);
    });
}
extern "C" cl_err cl_engine_runtime_feed_motion(cl_engine_runtime *runtime,
                                                  double x, double y, double dx,
                                                  double dy) {
    if (!runtime || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(dx) || !std::isfinite(dy)) return CLAY_ERR_INVALID_ARG;
    return guarded([&] { runtime->impl.feed_motion(x, y, dx, dy); });
}
extern "C" cl_err cl_engine_runtime_feed_wheel(cl_engine_runtime *runtime,
                                                 double x, double y, int wheel) {
    if (!runtime || !std::isfinite(x) || !std::isfinite(y)) return CLAY_ERR_INVALID_ARG;
    cl_input_event event = cl_input_event_make(CLAY_IN_WHEEL, CLAY_KEY_NONE);
    event.x = x; event.y = y; event.wheel = wheel;
    return guarded([&] { runtime->impl.feed(event); });
}
extern "C" cl_err cl_engine_runtime_feed_focus(cl_engine_runtime *runtime,
                                                 bool focused) {
    if (!runtime) return CLAY_ERR_INVALID_ARG;
    cl_input_event event = cl_input_event_make(CLAY_IN_FOCUS, CLAY_KEY_NONE);
    event.focus = focused;
    return guarded([&] { runtime->impl.feed(event); });
}
extern "C" bool cl_engine_runtime_is_key_down(
    const cl_engine_runtime *runtime, cl_key key) {
    return runtime && key > CLAY_KEY_NONE && key < CLAY_KEY_COUNT &&
           runtime->impl.is_key_down(key);
}
extern "C" bool cl_engine_runtime_is_focused(const cl_engine_runtime *runtime) {
    return runtime && runtime->impl.input_state().focus;
}
extern "C" cl_err cl_engine_runtime_load_reactions(cl_engine_runtime *runtime,
                                                     const char *json) {
    if (!runtime || !json) return CLAY_ERR_INVALID_ARG;
    try { return runtime->impl.reactions().load_text(json) ? CLAY_OK : CLAY_ERR_PARSE; }
    catch (const std::bad_alloc &) { return CLAY_ERR_OOM; }
    catch (...) { return CLAY_ERR_INVALID_ARG; }
}
extern "C" cl_err cl_engine_runtime_load_actions(cl_engine_runtime *runtime,
                                                   const char *json) {
    if (!runtime || !json) return CLAY_ERR_INVALID_ARG;
    try { return runtime->impl.load_actions(json) ? CLAY_OK : CLAY_ERR_PARSE; }
    catch (const std::bad_alloc &) { return CLAY_ERR_OOM; }
    catch (...) { return CLAY_ERR_INVALID_ARG; }
}
extern "C" cl_err cl_engine_runtime_save_recording(
    const cl_engine_runtime *runtime, const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try { return runtime->impl.save_recording(path); }
    catch (const std::bad_alloc &) { return CLAY_ERR_OOM; }
    catch (...) { return CLAY_ERR_IO; }
}
extern "C" cl_err cl_engine_runtime_load_recording(cl_engine_runtime *runtime,
                                                     const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try { return runtime->impl.load_recording(path); }
    catch (const std::bad_alloc &) { return CLAY_ERR_OOM; }
    catch (...) { return CLAY_ERR_PARSE; }
}
extern "C" void cl_engine_runtime_set_replaying(cl_engine_runtime *runtime,
                                                  bool replaying) {
    if (runtime) runtime->impl.set_replaying(replaying);
}
extern "C" bool cl_engine_runtime_is_replaying(const cl_engine_runtime *runtime) {
    return runtime && runtime->impl.replaying();
}
extern "C" size_t cl_engine_runtime_recording_count(const cl_engine_runtime *runtime) {
    return runtime ? cl_input_log_count(&runtime->impl.input_log()) : 0;
}
extern "C" uint64_t cl_engine_runtime_recording_fingerprint(
    const cl_engine_runtime *runtime) {
    return runtime ? cl_input_log_fingerprint(&runtime->impl.input_log()) : 0;
}
extern "C" cl_err cl_engine_runtime_spawn_species(
    cl_engine_runtime *runtime, const char *species, float x, float y, float r,
    float g, float b, float a, float life) {
    if (!runtime || !species || species[0] == '\0' || !finite(x) || !finite(y) ||
        !valid_color(r, g, b, a) || !finite(life)) return CLAY_ERR_INVALID_ARG;
    return guarded([&] { runtime->impl.spawn_species(species, x, y, {r, g, b, a}, life); });
}
extern "C" cl_err cl_engine_runtime_spawn_ripple(
    cl_engine_runtime *runtime, float x, float y, float radius, float r,
    float g, float b, float a) {
    if (!runtime || !finite(x) || !finite(y) || !finite(radius) ||
        !valid_color(r, g, b, a)) return CLAY_ERR_INVALID_ARG;
    return guarded([&] { runtime->impl.spawn_ripple(x, y, radius, {r, g, b, a}); });
}
extern "C" void cl_engine_runtime_set_time_scale(cl_engine_runtime *runtime,
                                                   double time_scale) {
    if (runtime) runtime->impl.set_time_scale(time_scale);
}
extern "C" cl_err cl_engine_runtime_install_builtin_systems(cl_engine_runtime *runtime) {
    if (!runtime || runtime->impl.systems().size() != 0) return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        runtime->impl.systems().add(std::make_unique<clay::MovementSystem>());
        runtime->impl.systems().add(std::make_unique<clay::CursorMagnetSystem>());
        runtime->impl.systems().add(std::make_unique<clay::LifespanSystem>());
        runtime->impl.systems().add(std::make_unique<clay::HueShiftSystem>());
        runtime->impl.systems().add(std::make_unique<clay::RippleSystem>());
        runtime->impl.systems().add(std::make_unique<clay::SceneGraphSystem>());
        runtime->impl.systems().add(std::make_unique<clay::PhysicsSystem>());
        runtime->impl.systems().add(std::make_unique<clay::AnimationSystem>());
        runtime->impl.systems().add(std::make_unique<clay::SceneGraphSystem>());
    });
}
extern "C" int cl_engine_runtime_width(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.width() : 0;
}
extern "C" int cl_engine_runtime_height(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.height() : 0;
}
extern "C" uint64_t cl_engine_runtime_frame(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.frame() : 0;
}
extern "C" double cl_engine_runtime_sim_time(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.sim_time() : 0.0;
}
extern "C" double cl_engine_runtime_sim_dt(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.sim_dt() : 0.0;
}
extern "C" double cl_engine_runtime_time_scale(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.time_scale() : 0.0;
}
extern "C" double cl_engine_runtime_cursor_x(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.cursor_x() : 0.0;
}
extern "C" double cl_engine_runtime_cursor_y(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.cursor_y() : 0.0;
}

/* ---------------------------------------------------------------- audio */
extern "C" cl_err cl_engine_audio_start_device(cl_engine_runtime *runtime) {
    if (!runtime) return CLAY_ERR_INVALID_ARG;
    return runtime->audio.start_device() ? CLAY_OK : CLAY_ERR_IO;
}
extern "C" void cl_engine_audio_stop_device(cl_engine_runtime *runtime) {
    if (runtime) runtime->audio.stop_device();
}
extern "C" bool cl_engine_audio_device_available(const cl_engine_runtime *runtime) {
    return runtime && runtime->audio.device_available();
}
extern "C" uint32_t cl_engine_audio_sample_rate(const cl_engine_runtime *runtime) {
    return runtime ? runtime->audio.sample_rate() : 0;
}
extern "C" cl_err cl_engine_audio_load_clip(cl_engine_runtime *runtime,
                                             const char *path,
                                             cl_audio_clip *out_clip) {
    if (out_clip) *out_clip = 0;
    if (!runtime || !path || path[0] == '\0' || !out_clip) return CLAY_ERR_INVALID_ARG;
    try {
        auto clip = runtime->audio.load_clip_file(path);
        if (!clip) return CLAY_ERR_IO;
        *out_clip = *clip;
        return CLAY_OK;
    } catch (const std::bad_alloc &) { return CLAY_ERR_OOM; }
    catch (...) { return CLAY_ERR_PARSE; }
}
extern "C" cl_err cl_engine_audio_unload_clip(cl_engine_runtime *runtime,
                                               cl_audio_clip clip) {
    if (!runtime || clip == 0) return CLAY_ERR_INVALID_ARG;
    return runtime->audio.unload_clip(clip) ? CLAY_OK : CLAY_ERR_NOT_FOUND;
}
extern "C" cl_err cl_engine_audio_play(cl_engine_runtime *runtime,
                                        cl_audio_clip clip, cl_audio_bus bus,
                                        bool loop, float gain,
                                        cl_audio_voice *out_voice) {
    if (out_voice) *out_voice = 0;
    if (!runtime || clip == 0 || !valid_bus(bus) || !finite(gain) || !out_voice)
        return CLAY_ERR_INVALID_ARG;
    auto voice = runtime->audio.play(clip, to_bus(bus), loop, gain);
    if (!voice) return CLAY_ERR_NOT_FOUND;
    *out_voice = *voice;
    return CLAY_OK;
}
extern "C" cl_err cl_engine_audio_play_spatial(
    cl_engine_runtime *runtime, cl_audio_clip clip, uint32_t entity_index,
    uint32_t entity_generation, float max_distance, float gain, bool loop,
    cl_audio_voice *out_voice) {
    if (out_voice) *out_voice = 0;
    if (!runtime || clip == 0 || !finite(max_distance) || max_distance <= 0.0F ||
        !finite(gain) || !out_voice) return CLAY_ERR_INVALID_ARG;
    auto voice = runtime->audio.play_spatial(
        clip, clay::Entity{entity_index, entity_generation}, max_distance, gain, loop);
    if (!voice) return CLAY_ERR_NOT_FOUND;
    *out_voice = *voice;
    return CLAY_OK;
}
extern "C" cl_err cl_engine_audio_stop_voice(cl_engine_runtime *runtime,
                                              cl_audio_voice voice) {
    if (!runtime || voice == 0) return CLAY_ERR_INVALID_ARG;
    return runtime->audio.stop(voice) ? CLAY_OK : CLAY_ERR_NOT_FOUND;
}
extern "C" void cl_engine_audio_stop_all(cl_engine_runtime *runtime) {
    if (runtime) runtime->audio.stop_all();
}
extern "C" void cl_engine_audio_set_master_gain(cl_engine_runtime *runtime,
                                                  float gain) {
    if (runtime) runtime->audio.set_master_gain(gain);
}
extern "C" void cl_engine_audio_set_bus_gain(cl_engine_runtime *runtime,
                                               cl_audio_bus bus, float gain) {
    if (runtime && valid_bus(bus)) runtime->audio.set_bus_gain(to_bus(bus), gain);
}
extern "C" float cl_engine_audio_master_gain(const cl_engine_runtime *runtime) {
    return runtime ? runtime->audio.master_gain() : 0.0F;
}
extern "C" float cl_engine_audio_bus_gain(const cl_engine_runtime *runtime,
                                           cl_audio_bus bus) {
    return runtime && valid_bus(bus) ? runtime->audio.bus_gain(to_bus(bus)) : 0.0F;
}
extern "C" cl_err cl_engine_audio_set_listener_entity(
    cl_engine_runtime *runtime, uint32_t entity_index, uint32_t entity_generation) {
    if (!runtime) return CLAY_ERR_INVALID_ARG;
    clay::Entity entity{entity_index, entity_generation};
    if (!runtime->impl.world().alive(entity)) return CLAY_ERR_NOT_FOUND;
    runtime->audio.set_listener(entity);
    return CLAY_OK;
}
extern "C" cl_err cl_engine_audio_set_listener_position(
    cl_engine_runtime *runtime, float x, float y) {
    if (!runtime || !finite(x) || !finite(y)) return CLAY_ERR_INVALID_ARG;
    runtime->audio.set_listener_position(x, y);
    return CLAY_OK;
}
extern "C" void cl_engine_audio_clear_listener(cl_engine_runtime *runtime) {
    if (runtime) runtime->audio.clear_listener();
}
extern "C" void cl_engine_audio_update_spatial(cl_engine_runtime *runtime) {
    if (runtime) runtime->audio.update_spatial();
}
extern "C" cl_err cl_engine_audio_play_music(cl_engine_runtime *runtime,
                                               const char *path,
                                               float crossfade_seconds) {
    if (!runtime || !path || path[0] == '\0' || !finite(crossfade_seconds) ||
        crossfade_seconds < 0.0F) return CLAY_ERR_INVALID_ARG;
    return runtime->audio.play_music_file(path, crossfade_seconds) ? CLAY_OK : CLAY_ERR_IO;
}
extern "C" void cl_engine_audio_stop_music(cl_engine_runtime *runtime,
                                             float fade_seconds) {
    if (runtime) runtime->audio.stop_music(fade_seconds);
}
extern "C" bool cl_engine_audio_music_playing(const cl_engine_runtime *runtime) {
    return runtime && runtime->audio.music_playing();
}

extern "C" const uint32_t *cl_engine_runtime_pixels(
    const cl_engine_runtime *runtime, size_t *count) {
    if (count) *count = 0;
    if (!runtime) return nullptr;
    const auto &pixels = runtime->impl.framebuffer().pixels;
    if (count) *count = pixels.size();
    return pixels.data();
}
extern "C" const uint8_t *cl_engine_runtime_pixels_rgba(
    const cl_engine_runtime *runtime, size_t *byte_count) {
    if (byte_count) *byte_count = 0;
    if (!runtime) return nullptr;
    const auto &framebuffer = runtime->impl.framebuffer();
    if (byte_count) *byte_count = framebuffer.pixels.size() * sizeof(uint32_t);
    return framebuffer.as_rgba();
}
extern "C" cl_err cl_engine_runtime_save_png(
    const cl_engine_runtime *runtime, const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try {
        const auto &framebuffer = runtime->impl.framebuffer();
        return clay::save_png(path, framebuffer.width, framebuffer.height,
                              framebuffer.pixels.data()) ? CLAY_OK : CLAY_ERR_IO;
    } catch (const std::bad_alloc &) { return CLAY_ERR_OOM; }
    catch (...) { return CLAY_ERR_IO; }
}
