#include <clay/engine_c.h>

#include "runtime.hpp"
#include "systems/builtin.hpp"

#include <new>

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
    if (!runtime || dt_seconds < 0.0) return CLAY_ERR_INVALID_ARG;
    runtime->impl.step(dt_seconds);
    return CLAY_OK;
}

extern "C" cl_err cl_engine_runtime_feed(cl_engine_runtime *runtime,
                                           const cl_input_event *event) {
    if (!runtime || !event) return CLAY_ERR_INVALID_ARG;
    runtime->impl.feed(*event);
    return CLAY_OK;
}

extern "C" cl_err cl_engine_runtime_load_reactions(cl_engine_runtime *runtime,
                                                     const char *json) {
    if (!runtime || !json) return CLAY_ERR_INVALID_ARG;
    return runtime->impl.reactions().load_text(json) ? CLAY_OK : CLAY_ERR_PARSE;
}

extern "C" cl_err cl_engine_runtime_spawn_species(
    cl_engine_runtime *runtime, const char *species, float x, float y, float r,
    float g, float b, float a, float life) {
    if (!runtime || !species) return CLAY_ERR_INVALID_ARG;
    runtime->impl.spawn_species(species, x, y, {r, g, b, a}, life);
    return CLAY_OK;
}

extern "C" cl_err cl_engine_runtime_spawn_ripple(
    cl_engine_runtime *runtime, float x, float y, float radius, float r,
    float g, float b, float a) {
    if (!runtime) return CLAY_ERR_INVALID_ARG;
    runtime->impl.spawn_ripple(x, y, radius, {r, g, b, a});
    return CLAY_OK;
}

extern "C" void cl_engine_runtime_set_time_scale(cl_engine_runtime *runtime,
                                                   double time_scale) {
    if (runtime) runtime->impl.set_time_scale(time_scale);
}

extern "C" cl_err cl_engine_runtime_install_builtin_systems(
    cl_engine_runtime *runtime) {
    if (!runtime) return CLAY_ERR_INVALID_ARG;
    if (runtime->impl.systems().size() != 0) return CLAY_ERR_INVALID_ARG;
    runtime->impl.systems().add(std::make_unique<clay::MovementSystem>());
    runtime->impl.systems().add(std::make_unique<clay::CursorMagnetSystem>());
    runtime->impl.systems().add(std::make_unique<clay::LifespanSystem>());
    runtime->impl.systems().add(std::make_unique<clay::HueShiftSystem>());
    runtime->impl.systems().add(std::make_unique<clay::RippleSystem>());
    return CLAY_OK;
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

extern "C" const uint32_t *cl_engine_runtime_pixels(
    const cl_engine_runtime *runtime, size_t *count) {
    if (count) *count = 0;
    if (!runtime) return nullptr;
    const auto &pixels = runtime->impl.framebuffer().pixels;
    if (count) *count = pixels.size();
    return pixels.data();
}
