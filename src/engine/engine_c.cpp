#include <clay/engine_c.h>

#include "imageio.hpp"
#include "runtime.hpp"
#include "systems/builtin.hpp"

#include <cmath>
#include <new>
#include <utility>

namespace {

bool valid_input_event(const cl_input_event &event) {
    if (event.type < CLAY_IN_PRESS || event.type > CLAY_IN_FOCUS)
        return false;
    if (!std::isfinite(event.x) || !std::isfinite(event.y) ||
        !std::isfinite(event.dx) || !std::isfinite(event.dy))
        return false;
    if ((event.type == CLAY_IN_PRESS || event.type == CLAY_IN_RELEASE) &&
        (event.key <= CLAY_KEY_NONE || event.key >= CLAY_KEY_COUNT))
        return false;
    return true;
}

bool finite(float value) {
    return std::isfinite(value);
}

bool valid_color(float r, float g, float b, float a) {
    return finite(r) && finite(g) && finite(b) && finite(a);
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

    cl_engine_runtime(int width, int height, uint64_t seed)
        : impl(width, height, seed) {}
};

extern "C" cl_engine_runtime *cl_engine_runtime_create(int width, int height,
                                                          uint64_t seed) {
    if (width <= 0 || height <= 0) return nullptr;
    try {
        return new cl_engine_runtime(width, height, seed);
    } catch (const std::bad_alloc &) {
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
    if (!runtime || width <= 0 || height <= 0) return CLAY_ERR_INVALID_ARG;
    return guarded([&] { runtime->impl.resize(width, height); });
}

extern "C" cl_err cl_engine_runtime_feed(cl_engine_runtime *runtime,
                                           const cl_input_event *event) {
    if (!runtime || !event || !valid_input_event(*event))
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

extern "C" cl_err cl_engine_runtime_feed_key_at(
    cl_engine_runtime *runtime, cl_key key, bool pressed, double x, double y,
    int mods) {
    if (!runtime || key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT ||
        !std::isfinite(x) || !std::isfinite(y))
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        cl_input_event event = cl_input_event_make(
            pressed ? CLAY_IN_PRESS : CLAY_IN_RELEASE, key);
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
                                                 double x, double y,
                                                 int wheel) {
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

extern "C" bool cl_engine_runtime_is_key_down(
    const cl_engine_runtime *runtime, cl_key key) {
    if (!runtime || key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT)
        return false;
    return runtime->impl.is_key_down(key);
}

extern "C" bool cl_engine_runtime_is_focused(
    const cl_engine_runtime *runtime) {
    return runtime && runtime->impl.input_state().focus;
}

extern "C" cl_err cl_engine_runtime_load_reactions(cl_engine_runtime *runtime,
                                                     const char *json) {
    if (!runtime || !json) return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.reactions().load_text(json) ? CLAY_OK
                                                          : CLAY_ERR_PARSE;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_INVALID_ARG;
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

extern "C" cl_err cl_engine_runtime_save_recording(
    const cl_engine_runtime *runtime, const char *path) {
    if (!runtime || !path || path[0] == '\0') return CLAY_ERR_INVALID_ARG;
    try {
        return runtime->impl.save_recording(path);
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_IO;
    }
}

extern "C" cl_err cl_engine_runtime_load_recording(
    cl_engine_runtime *runtime, const char *path) {
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

extern "C" size_t cl_engine_runtime_recording_count(
    const cl_engine_runtime *runtime) {
    return runtime ? cl_input_log_count(&runtime->impl.input_log()) : 0;
}

extern "C" uint64_t cl_engine_runtime_recording_fingerprint(
    const cl_engine_runtime *runtime) {
    return runtime ? cl_input_log_fingerprint(&runtime->impl.input_log()) : 0;
}

extern "C" cl_err cl_engine_runtime_spawn_species(
    cl_engine_runtime *runtime, const char *species, float x, float y, float r,
    float g, float b, float a, float life) {
    if (!runtime || !species || species[0] == '\0' || !finite(x) ||
        !finite(y) || !valid_color(r, g, b, a) || !finite(life))
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        runtime->impl.spawn_species(species, x, y, {r, g, b, a}, life);
    });
}

extern "C" cl_err cl_engine_runtime_spawn_ripple(
    cl_engine_runtime *runtime, float x, float y, float radius, float r,
    float g, float b, float a) {
    if (!runtime || !finite(x) || !finite(y) || !finite(radius) ||
        !valid_color(r, g, b, a))
        return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        runtime->impl.spawn_ripple(x, y, radius, {r, g, b, a});
    });
}

extern "C" void cl_engine_runtime_set_time_scale(cl_engine_runtime *runtime,
                                                   double time_scale) {
    if (runtime) runtime->impl.set_time_scale(time_scale);
}

extern "C" cl_err cl_engine_runtime_install_builtin_systems(
    cl_engine_runtime *runtime) {
    if (!runtime) return CLAY_ERR_INVALID_ARG;
    if (runtime->impl.systems().size() != 0) return CLAY_ERR_INVALID_ARG;
    return guarded([&] {
        runtime->impl.systems().add(std::make_unique<clay::MovementSystem>());
        runtime->impl.systems().add(
            std::make_unique<clay::CursorMagnetSystem>());
        runtime->impl.systems().add(std::make_unique<clay::LifespanSystem>());
        runtime->impl.systems().add(std::make_unique<clay::HueShiftSystem>());
        runtime->impl.systems().add(std::make_unique<clay::RippleSystem>());
    });
}

extern "C" int cl_engine_runtime_width(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.width() : 0;
}

extern "C" int cl_engine_runtime_height(const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.height() : 0;
}

extern "C" uint64_t cl_engine_runtime_frame(
    const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.frame() : 0;
}

extern "C" double cl_engine_runtime_sim_time(
    const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.sim_time() : 0.0;
}

extern "C" double cl_engine_runtime_cursor_x(
    const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.cursor_x() : 0.0;
}

extern "C" double cl_engine_runtime_cursor_y(
    const cl_engine_runtime *runtime) {
    return runtime ? runtime->impl.cursor_y() : 0.0;
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
                              framebuffer.pixels.data())
                   ? CLAY_OK
                   : CLAY_ERR_IO;
    } catch (const std::bad_alloc &) {
        return CLAY_ERR_OOM;
    } catch (...) {
        return CLAY_ERR_IO;
    }
}
