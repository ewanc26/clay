#include <clay/engine_c.h>

#include "imageio.hpp"
#include "render/scene3d.hpp"
#include "runtime.hpp"
#include "systems/builtin.hpp"

#include <cmath>
#include <fstream>
#include <iterator>
#include <new>
#include <span>
#include <string>
#include <utility>

namespace {

bool finite(float value) {
    return std::isfinite(value);
}

bool valid_color(float r, float g, float b, float a) {
    return finite(r) && finite(g) && finite(b) && finite(a);
}

bool valid_dimensions(int width, int height) {
    return width > 0 && height > 0 &&
           (size_t)width <= SIZE_MAX / (size_t)height &&
           (size_t)width * (size_t)height <= CLAY_ENGINE_MAX_FRAMEBUFFER_PIXELS;
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

    cl_engine_runtime(int width, int height, uint64_t seed, size_t arena_bytes)
        : impl(width, height, seed, arena_bytes) {}
};

extern "C" uint32_t cl_engine_runtime_abi_version(void) {
    return CLAY_ENGINE_ABI_VERSION;
}

extern "C" const char *cl_engine_error_string(int error) {
    if (error < static_cast<int>(CLAY_OK) ||
        error > static_cast<int>(CLAY_ERR_OVERFLOW)) {
        return "unknown";
    }
    return cl_err_str(static_cast<cl_err>(error));
}

extern "C" cl_engine_runtime *cl_engine_runtime_create(int width, int height,
                                                       uint64_t seed) {
    return cl_engine_runtime_create_with_arena(width, height, seed, 4u << 20);
}

extern "C" cl_engine_runtime *
cl_engine_runtime_create_with_arena(int width, int height, uint64_t seed,
                                    size_t arena_bytes) {
    if (!valid_dimensions(width, height) ||
        arena_bytes < CLAY_ENGINE_MIN_ARENA_BYTES)
        return nullptr;
    try {
        return new cl_engine_runtime(width, height, seed, arena_bytes);
    } catch (const std::bad_alloc &) {
        return nullptr;
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
    return guarded([&] { runtime->impl.step(dt_seconds); });
}

extern "C" cl_err cl_engine_runtime_resize(cl_engine_runtime *runtime,
                                           int width, int height) {
    if (!runtime || !valid_dimensions(width, height))
        return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.resize(width, height) ? CLAY_OK
                                                   : CLAY_ERR_INVALID_ARG;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_INVALID_ARG;
    }
}

extern "C" cl_err cl_engine_runtime_feed(cl_engine_runtime *runtime,
                                         const cl_input_event *event) {
    if (!runtime || !event || !cl_input_event_valid(event))
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] { runtime->impl.feed(*event); });
}

extern "C" cl_err cl_engine_runtime_feed_key(cl_engine_runtime *runtime,
                                             cl_key key, bool pressed) {
    if (!runtime || key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT)
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        runtime->impl.feed(pressed ? cl_input_event_make(CLAY_IN_PRESS, key)
                                   : cl_input_event_make(CLAY_IN_RELEASE, key));
    });
}

extern "C" cl_err cl_engine_runtime_feed_key_at(cl_engine_runtime *runtime,
                                                cl_key key, bool pressed,
                                                double x, double y, int mods) {
    if (!runtime || key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT ||
        !std::isfinite(x) || !std::isfinite(y))
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        cl_input_event event =
            cl_input_event_make(pressed ? CLAY_IN_PRESS : CLAY_IN_RELEASE, key);
        event.x = x;
        event.y = y;
        event.mods = mods;
        runtime->impl.feed(event);
    });
}

extern "C" cl_err cl_engine_runtime_feed_motion(cl_engine_runtime *runtime,
                                                double x, double y, double dx,
                                                double dy) {
    if (!runtime || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(dx) || !std::isfinite(dy))
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] { runtime->impl.feed_motion(x, y, dx, dy); });
}

extern "C" cl_err cl_engine_runtime_feed_wheel(cl_engine_runtime *runtime,
                                               double x, double y, int wheel) {
    if (!runtime || !std::isfinite(x) || !std::isfinite(y))
        return CLAY_ERR_INVALID_ARG;
    cl_input_event event = cl_input_event_make(CLAY_IN_WHEEL, CLAY_KEY_NONE);
    event.x = x;
    event.y = y;
    event.wheel = wheel;
    return guarded([&] { runtime->impl.feed(event); });
}

extern "C" cl_err cl_engine_runtime_feed_focus(cl_engine_runtime *runtime,
                                               bool focused) {
    if (!runtime) return CLAY_ERR_INVALID_ARG;
    cl_input_event event = cl_input_event_make(CLAY_IN_FOCUS, CLAY_KEY_NONE);
    event.focus = focused;
    return guarded([&] { runtime->impl.feed(event); });
}

extern "C" bool cl_engine_runtime_is_key_down(const cl_engine_runtime *runtime,
                                              cl_key key) {
    if (!runtime || key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT) return false;
    return runtime->impl.is_key_down(key);
}

extern "C" bool cl_engine_runtime_is_focused(const cl_engine_runtime *runtime) {
    return runtime && runtime->impl.input_state().focus;
}

extern "C" cl_err cl_engine_runtime_load_reactions(cl_engine_runtime *runtime,
                                                   const char *json) {
    if (!runtime || !json) return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.load_reactions(json) ? CLAY_OK : CLAY_ERR_PARSE;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_INVALID_ARG;
    }
}

extern "C" cl_err cl_engine_runtime_load_reactions_file(
    cl_engine_runtime *runtime, const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) return CLAY_ERR_IO;
        std::string json((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
        if (json.empty()) return CLAY_ERR_PARSE;
        return cl_engine_runtime_load_reactions(runtime, json.c_str());
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_IO;
    }
}

extern "C" cl_err cl_engine_runtime_load_actions(cl_engine_runtime *runtime,
                                                 const char *json) {
    if (!runtime || !json) return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.load_actions(json) ? CLAY_OK : CLAY_ERR_PARSE;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_INVALID_ARG;
    }
}

extern "C" cl_err cl_engine_runtime_load_actions_file(
    cl_engine_runtime *runtime, const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) return CLAY_ERR_IO;
        std::string json((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
        if (json.empty()) return CLAY_ERR_PARSE;
        return cl_engine_runtime_load_actions(runtime, json.c_str());
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_IO;
    }
}

extern "C" cl_err cl_engine_runtime_load_scene(cl_engine_runtime *runtime,
                                                const char *json) {
    if (!runtime || !json) return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.load_scene(json) ? CLAY_OK : CLAY_ERR_PARSE;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_INVALID_ARG;
    }
}

extern "C" cl_err
cl_engine_runtime_load_scene_file(cl_engine_runtime *runtime,
                                  const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) return CLAY_ERR_IO;
        std::string json((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
        if (json.empty()) return CLAY_ERR_PARSE;
        return runtime->impl.load_scene(json) ? CLAY_OK : CLAY_ERR_PARSE;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_IO;
    }
}

extern "C" void cl_engine_runtime_unload_scene(cl_engine_runtime *runtime) {
    if (!runtime) return;
    runtime->impl.unload_scene();
}

extern "C" bool cl_engine_runtime_has_scene(
    const cl_engine_runtime *runtime) {
    return runtime && runtime->impl.has_scene();
}

extern "C" cl_err
cl_engine_runtime_save_recording(const cl_engine_runtime *runtime,
                                 const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.save_recording(path);
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_IO;
    }
}

extern "C" cl_err cl_engine_runtime_load_recording(cl_engine_runtime *runtime,
                                                   const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.load_recording(path);
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_PARSE;
    }
}

extern "C" void cl_engine_runtime_set_replaying(cl_engine_runtime *runtime,
                                                bool replaying) {
    if (runtime) runtime->impl.set_replaying(replaying);
}

extern "C" bool
cl_engine_runtime_is_replaying(const cl_engine_runtime *runtime) {
    return runtime && runtime->impl.replaying();
}

extern "C" size_t
cl_engine_runtime_recording_count(const cl_engine_runtime *runtime) {
    return runtime ? cl_input_log_count(&runtime->impl.input_log()) : 0;
}

extern "C" uint64_t
cl_engine_runtime_recording_fingerprint(const cl_engine_runtime *runtime) {
    return runtime ? cl_input_log_fingerprint(&runtime->impl.input_log()) : 0;
}

extern "C" cl_err cl_engine_runtime_spawn_species(cl_engine_runtime *runtime,
                                                  const char *species, float x,
                                                  float y, float r, float g,
                                                  float b, float a,
                                                  float life) {
    if (!runtime || !species || species[0] == '\0' || !finite(x) ||
        !finite(y) || !valid_color(r, g, b, a) || !finite(life))
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        runtime->impl.spawn_species(species, x, y, {r, g, b, a}, life);
    });
}

extern "C" cl_err cl_engine_runtime_spawn_ripple(cl_engine_runtime *runtime,
                                                 float x, float y, float radius,
                                                 float r, float g, float b,
                                                 float a) {
    if (!runtime || !finite(x) || !finite(y) || !finite(radius) ||
        !valid_color(r, g, b, a))
        return CLAY_ERR_INVALID_ARG;
    return guarded(
        [&] { runtime->impl.spawn_ripple(x, y, radius, {r, g, b, a}); });
}

extern "C" void cl_engine_runtime_set_time_scale(cl_engine_runtime *runtime,
                                                 double time_scale) {
    if (runtime) runtime->impl.set_time_scale(time_scale);
}

extern "C" cl_err
cl_engine_runtime_install_builtin_systems(cl_engine_runtime *runtime) {
    if (!runtime) return CLAY_ERR_INVALID_ARG;
    if (runtime->impl.systems().size() != 0) return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        runtime->impl.systems().add(std::make_unique<clay::MovementSystem>());
        runtime->impl.systems().add(
            std::make_unique<clay::CursorMagnetSystem>());
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

extern "C" double
cl_engine_runtime_time_scale(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.time_scale() : 0.0;
}

extern "C" double cl_engine_runtime_cursor_x(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.cursor_x() : 0.0;
}

extern "C" double cl_engine_runtime_cursor_y(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.cursor_y() : 0.0;
}

extern "C" const uint32_t *
cl_engine_runtime_pixels(const cl_engine_runtime *runtime, size_t *count) {
    if (count) *count = 0;
    if (!runtime) return nullptr;
    const auto &pixels = runtime->impl.framebuffer().pixels;
    if (count) *count = pixels.size();
    return pixels.data();
}

extern "C" const uint8_t *
cl_engine_runtime_pixels_rgba(const cl_engine_runtime *runtime,
                              size_t *byte_count) {
    if (byte_count) *byte_count = 0;
    if (!runtime) return nullptr;
    const auto &framebuffer = runtime->impl.framebuffer();
    if (byte_count) *byte_count = framebuffer.pixels.size() * sizeof(uint32_t);
    return framebuffer.as_rgba();
}

extern "C" cl_err cl_engine_runtime_save_png(const cl_engine_runtime *runtime,
                                             const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try {
        const auto &framebuffer = runtime->impl.framebuffer();
        return clay::save_png(path, framebuffer.width, framebuffer.height,
                              framebuffer.pixels.data())
                   ? CLAY_OK
                   : CLAY_ERR_IO;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_IO;
    }
}

namespace {
bool valid_audio_bus(int bus) {
    return bus == 0 || bus == 1;
}

clay::AudioBus audio_bus(int bus) {
    return bus == 1 ? clay::AudioBus::Music : clay::AudioBus::Sfx;
}
} // namespace

extern "C" cl_err cl_engine_runtime_audio_load_wav(cl_engine_runtime *runtime,
                                                   const char *path,
                                                   uint32_t *clip_id) {
    if (!runtime || !path || path[0] == '\0' || !clip_id)
        return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.audio_load_wav(path, clip_id);
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_IO;
    }
}

extern "C" uint32_t cl_engine_runtime_audio_play(cl_engine_runtime *runtime,
                                                 uint32_t clip_id, int bus,
                                                 bool loop, float gain) {
    if (!runtime || !valid_audio_bus(bus) || !std::isfinite(gain)) return 0;
    try {
        return runtime->impl.audio_play(clip_id, audio_bus(bus), loop, gain);
    } catch (...) {
        return 0;
    }
}

extern "C" bool cl_engine_runtime_audio_stop(cl_engine_runtime *runtime,
                                             uint32_t voice_id) {
    return runtime && voice_id != 0 && runtime->impl.audio_stop(voice_id);
}

extern "C" cl_err cl_engine_runtime_audio_mix_stereo(cl_engine_runtime *runtime,
                                                     float *samples,
                                                     size_t sample_count) {
    if (!runtime || !samples || sample_count == 0 || sample_count % 2 != 0)
        return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.audio_mix_stereo(
                   std::span<float>(samples, sample_count))
                   ? CLAY_OK
                   : CLAY_ERR_INVALID_ARG;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_INVALID_ARG;
    }
}

extern "C" void
cl_engine_runtime_audio_set_master_gain(cl_engine_runtime *runtime,
                                        float gain) {
    if (runtime && std::isfinite(gain))
        runtime->impl.audio().set_master_gain(gain);
}

extern "C" void cl_engine_runtime_audio_set_bus_gain(cl_engine_runtime *runtime,
                                                     int bus, float gain) {
    if (runtime && valid_audio_bus(bus) && std::isfinite(gain))
        runtime->impl.audio().set_bus_gain(audio_bus(bus), gain);
}
