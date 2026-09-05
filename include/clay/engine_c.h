#ifndef CLAY_ENGINE_C_H
#define CLAY_ENGINE_C_H

#include <clay/clay.h>

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(CLAY_BUILDING_SHARED)
#define CLAY_API __declspec(dllexport)
#elif defined(_WIN32) && defined(CLAY_USING_SHARED)
#define CLAY_API __declspec(dllimport)
#else
#define CLAY_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque host-facing handle. This is the supported ABI boundary for managed
 * integrations such as Godot Mono; callers must not inspect its contents. */
typedef struct cl_engine_runtime cl_engine_runtime;

/* Increment when the host-facing ABI changes incompatibly. */
#define CLAY_ENGINE_ABI_VERSION 1u
/* Minimum arena needed for mandatory runtime initialization. */
#define CLAY_ENGINE_MIN_ARENA_BYTES (64u << 10)
/* Prevent accidental multi-gigabyte allocations through the C ABI. */
#define CLAY_ENGINE_MAX_FRAMEBUFFER_PIXELS (64u << 20)

CLAY_API uint32_t cl_engine_runtime_abi_version(void);
/* Returns a stable human-readable name for a C ABI error code. */
/* Accepts an integer so diagnostic callers can safely ask about unknown
 * future/error codes without constructing an invalid cl_err enum value. */
CLAY_API const char *cl_engine_error_string(int error);

CLAY_API cl_engine_runtime *cl_engine_runtime_create(int width, int height,
                                                      uint64_t seed);
CLAY_API cl_engine_runtime *cl_engine_runtime_create_with_arena(
    int width, int height, uint64_t seed, size_t arena_bytes);
CLAY_API void cl_engine_runtime_destroy(cl_engine_runtime *runtime);

/* Advances one deterministic frame and renders the authoritative framebuffer. */
CLAY_API cl_err cl_engine_runtime_step(cl_engine_runtime *runtime,
                                       double dt_seconds);
CLAY_API cl_err cl_engine_runtime_resize(cl_engine_runtime *runtime, int width,
                                         int height);
CLAY_API cl_err cl_engine_runtime_feed(cl_engine_runtime *runtime,
                                       const cl_input_event *event);
CLAY_API cl_err cl_engine_runtime_feed_key(cl_engine_runtime *runtime,
                                           cl_key key, bool pressed);
CLAY_API cl_err cl_engine_runtime_feed_key_at(cl_engine_runtime *runtime,
                                              cl_key key, bool pressed,
                                              double x, double y, int mods);
CLAY_API cl_err cl_engine_runtime_feed_motion(cl_engine_runtime *runtime,
                                              double x, double y, double dx,
                                              double dy);
CLAY_API cl_err cl_engine_runtime_feed_wheel(cl_engine_runtime *runtime,
                                             double x, double y, int wheel);
CLAY_API cl_err cl_engine_runtime_feed_focus(cl_engine_runtime *runtime,
                                             bool focused);
CLAY_API bool cl_engine_runtime_is_key_down(
    const cl_engine_runtime *runtime, cl_key key);
CLAY_API bool cl_engine_runtime_is_focused(const cl_engine_runtime *runtime);
CLAY_API cl_err cl_engine_runtime_load_reactions(cl_engine_runtime *runtime,
                                                 const char *json);
CLAY_API cl_err cl_engine_runtime_load_reactions_file(cl_engine_runtime *runtime,
                                                      const char *path);
CLAY_API cl_err cl_engine_runtime_load_actions(cl_engine_runtime *runtime,
                                               const char *json);
CLAY_API cl_err cl_engine_runtime_load_actions_file(cl_engine_runtime *runtime,
                                                    const char *path);
CLAY_API cl_err cl_engine_runtime_load_scene(cl_engine_runtime *runtime,
                                             const char *json);
CLAY_API cl_err cl_engine_runtime_load_scene_file(cl_engine_runtime *runtime,
                                                  const char *path);
CLAY_API void cl_engine_runtime_unload_scene(cl_engine_runtime *runtime);
CLAY_API bool cl_engine_runtime_has_scene(const cl_engine_runtime *runtime);
CLAY_API cl_err cl_engine_runtime_save_recording(
    const cl_engine_runtime *runtime, const char *path);
CLAY_API cl_err cl_engine_runtime_load_recording(cl_engine_runtime *runtime,
                                                const char *path);
CLAY_API void cl_engine_runtime_set_replaying(cl_engine_runtime *runtime,
                                              bool replaying);
CLAY_API bool cl_engine_runtime_is_replaying(
    const cl_engine_runtime *runtime);
CLAY_API size_t cl_engine_runtime_recording_count(
    const cl_engine_runtime *runtime);
CLAY_API uint64_t cl_engine_runtime_recording_fingerprint(
    const cl_engine_runtime *runtime);
CLAY_API cl_err cl_engine_runtime_spawn_species(
    cl_engine_runtime *runtime, const char *species, float x, float y, float r,
    float g, float b, float a, float life);
CLAY_API cl_err cl_engine_runtime_spawn_ripple(cl_engine_runtime *runtime,
                                               float x, float y, float radius,
                                               float r, float g, float b,
                                               float a);
CLAY_API void cl_engine_runtime_set_time_scale(cl_engine_runtime *runtime,
                                               double time_scale);
/* Installs the deterministic built-in simulation systems. Safe to call once;
 * repeated calls are rejected so systems cannot accidentally run twice. */
CLAY_API cl_err cl_engine_runtime_install_builtin_systems(
    cl_engine_runtime *runtime);

CLAY_API int cl_engine_runtime_width(const cl_engine_runtime *runtime);
CLAY_API int cl_engine_runtime_height(const cl_engine_runtime *runtime);
CLAY_API uint64_t cl_engine_runtime_frame(const cl_engine_runtime *runtime);
CLAY_API uint64_t cl_engine_runtime_seed(const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_sim_time(const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_sim_dt(const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_time_scale(
    const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_cursor_x(const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_cursor_y(const cl_engine_runtime *runtime);

/* Packed 0x00RRGGBB pixels, row-major, top-left origin. The pointer remains
 * valid until the next step or destruction of the runtime. */
CLAY_API const uint32_t *cl_engine_runtime_pixels(
    const cl_engine_runtime *runtime, size_t *count);
/* RGBA8 bytes, row-major, top-left origin. The pointer remains valid until
 * the next step or destruction of the runtime. */
CLAY_API const uint8_t *cl_engine_runtime_pixels_rgba(
    const cl_engine_runtime *runtime, size_t *byte_count);
/* Writes the most recently rendered frame as an RGB PNG. */
CLAY_API cl_err cl_engine_runtime_save_png(const cl_engine_runtime *runtime,
                                           const char *path);

/* Headless-safe software audio output. Samples are interleaved stereo float32
 * frames at 48 kHz. The caller owns the output buffer. */
CLAY_API uint32_t cl_engine_runtime_audio_sample_rate(
    const cl_engine_runtime *runtime);
CLAY_API cl_err cl_engine_runtime_audio_load_wav(cl_engine_runtime *runtime,
                                                const char *path,
                                                uint32_t *clip_id);
/* Uses miniaudio's enabled built-in decoders and resamples to stereo output. */
CLAY_API cl_err cl_engine_runtime_audio_load_file(cl_engine_runtime *runtime,
                                                 const char *path,
                                                 uint32_t *clip_id);
CLAY_API bool cl_engine_runtime_audio_unload_clip(cl_engine_runtime *runtime,
                                                  uint32_t clip_id);
CLAY_API uint32_t cl_engine_runtime_audio_play(cl_engine_runtime *runtime,
                                               uint32_t clip_id, int bus,
                                               bool loop, float gain);
CLAY_API bool cl_engine_runtime_audio_stop(cl_engine_runtime *runtime,
                                           uint32_t voice_id);
CLAY_API void cl_engine_runtime_audio_stop_all(cl_engine_runtime *runtime);
CLAY_API bool cl_engine_runtime_audio_pause(cl_engine_runtime *runtime,
                                            uint32_t voice_id);
CLAY_API bool cl_engine_runtime_audio_resume(cl_engine_runtime *runtime,
                                             uint32_t voice_id);
/* Sets stereo pan from -1 (left) through 0 (center) to 1 (right). */
CLAY_API bool cl_engine_runtime_audio_set_voice_pan(
    cl_engine_runtime *runtime, uint32_t voice_id, float pan);
CLAY_API float cl_engine_runtime_audio_voice_pan(
    const cl_engine_runtime *runtime, uint32_t voice_id);
CLAY_API bool cl_engine_runtime_audio_set_voice_gain(
    cl_engine_runtime *runtime, uint32_t voice_id, float gain);
CLAY_API float cl_engine_runtime_audio_voice_gain(
    const cl_engine_runtime *runtime, uint32_t voice_id);
CLAY_API bool cl_engine_runtime_audio_voice_active(
    const cl_engine_runtime *runtime, uint32_t voice_id);
CLAY_API bool cl_engine_runtime_audio_voice_paused(
    const cl_engine_runtime *runtime, uint32_t voice_id);
/* Returns decoded frames, or zero when clip_id is not loaded. */
CLAY_API size_t cl_engine_runtime_audio_clip_frame_count(
    const cl_engine_runtime *runtime, uint32_t clip_id);
CLAY_API cl_err cl_engine_runtime_audio_mix_stereo(cl_engine_runtime *runtime,
                                                  float *samples,
                                                  size_t sample_count);
CLAY_API void cl_engine_runtime_audio_set_master_gain(
    cl_engine_runtime *runtime, float gain);
CLAY_API void cl_engine_runtime_audio_set_bus_gain(cl_engine_runtime *runtime,
                                                   int bus, float gain);
CLAY_API float cl_engine_runtime_audio_master_gain(
    const cl_engine_runtime *runtime);
CLAY_API float cl_engine_runtime_audio_bus_gain(const cl_engine_runtime *runtime,
                                               int bus);

#ifdef __cplusplus
}
#endif

#undef CLAY_API

#endif /* CLAY_ENGINE_C_H */
