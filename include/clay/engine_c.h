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

typedef struct cl_engine_runtime cl_engine_runtime;

#define CLAY_ENGINE_ABI_VERSION 1u
#define CLAY_ENGINE_MIN_ARENA_BYTES (64u << 10)
#define CLAY_ENGINE_MAX_FRAMEBUFFER_PIXELS (64u << 20)

CLAY_API uint32_t cl_engine_runtime_abi_version(void);
CLAY_API const char *cl_engine_error_string(cl_err error);

CLAY_API cl_engine_runtime *cl_engine_runtime_create(int width, int height,
                                                      uint64_t seed);
CLAY_API cl_engine_runtime *cl_engine_runtime_create_with_arena(
    int width, int height, uint64_t seed, size_t arena_bytes);
CLAY_API void cl_engine_runtime_destroy(cl_engine_runtime *runtime);
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
CLAY_API cl_err cl_engine_runtime_load_actions(cl_engine_runtime *runtime,
                                               const char *json);
CLAY_API cl_err cl_engine_runtime_save_recording(
    const cl_engine_runtime *runtime, const char *path);
CLAY_API cl_err cl_engine_runtime_load_recording(cl_engine_runtime *runtime,
                                                 const char *path);
CLAY_API void cl_engine_runtime_set_replaying(cl_engine_runtime *runtime,
                                              bool replaying);
CLAY_API bool cl_engine_runtime_is_replaying(const cl_engine_runtime *runtime);
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
CLAY_API cl_err cl_engine_runtime_install_builtin_systems(
    cl_engine_runtime *runtime);

CLAY_API int cl_engine_runtime_width(const cl_engine_runtime *runtime);
CLAY_API int cl_engine_runtime_height(const cl_engine_runtime *runtime);
CLAY_API uint64_t cl_engine_runtime_frame(const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_sim_time(const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_sim_dt(const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_time_scale(
    const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_cursor_x(const cl_engine_runtime *runtime);
CLAY_API double cl_engine_runtime_cursor_y(const cl_engine_runtime *runtime);

/* ---------------------------------------------------------------- audio */
typedef uint32_t cl_audio_clip;
typedef uint32_t cl_audio_voice;
typedef enum cl_audio_bus {
    CLAY_AUDIO_BUS_SFX = 0,
    CLAY_AUDIO_BUS_MUSIC = 1
} cl_audio_bus;

/* Device output is opt-in and failure is non-fatal: callers can continue
 * headless and use the same deterministic runtime. */
CLAY_API cl_err cl_engine_audio_start_device(cl_engine_runtime *runtime);
CLAY_API void cl_engine_audio_stop_device(cl_engine_runtime *runtime);
CLAY_API bool cl_engine_audio_device_available(
    const cl_engine_runtime *runtime);
CLAY_API uint32_t cl_engine_audio_sample_rate(
    const cl_engine_runtime *runtime);

/* Short effects are decoded to owned float PCM. WAV and OGG/Vorbis are
 * accepted; source sample rates are resampled to the engine rate. */
CLAY_API cl_err cl_engine_audio_load_clip(cl_engine_runtime *runtime,
                                         const char *path,
                                         cl_audio_clip *out_clip);
CLAY_API cl_err cl_engine_audio_unload_clip(cl_engine_runtime *runtime,
                                           cl_audio_clip clip);
CLAY_API cl_err cl_engine_audio_play(cl_engine_runtime *runtime,
                                    cl_audio_clip clip, cl_audio_bus bus,
                                    bool loop, float gain,
                                    cl_audio_voice *out_voice);
CLAY_API cl_err cl_engine_audio_play_spatial(
    cl_engine_runtime *runtime, cl_audio_clip clip, uint32_t entity_index,
    uint32_t entity_generation, float max_distance, float gain, bool loop,
    cl_audio_voice *out_voice);
CLAY_API cl_err cl_engine_audio_stop_voice(cl_engine_runtime *runtime,
                                          cl_audio_voice voice);
CLAY_API void cl_engine_audio_stop_all(cl_engine_runtime *runtime);

CLAY_API void cl_engine_audio_set_master_gain(cl_engine_runtime *runtime,
                                              float gain);
CLAY_API void cl_engine_audio_set_bus_gain(cl_engine_runtime *runtime,
                                           cl_audio_bus bus, float gain);
CLAY_API float cl_engine_audio_master_gain(const cl_engine_runtime *runtime);
CLAY_API float cl_engine_audio_bus_gain(const cl_engine_runtime *runtime,
                                        cl_audio_bus bus);

CLAY_API cl_err cl_engine_audio_set_listener_entity(
    cl_engine_runtime *runtime, uint32_t entity_index,
    uint32_t entity_generation);
CLAY_API cl_err cl_engine_audio_set_listener_position(
    cl_engine_runtime *runtime, float x, float y);
CLAY_API void cl_engine_audio_clear_listener(cl_engine_runtime *runtime);
CLAY_API void cl_engine_audio_update_spatial(cl_engine_runtime *runtime);

/* Music stays streamed from disk and loops; a replacement track crossfades
 * over the requested duration without loading the whole file. */
CLAY_API cl_err cl_engine_audio_play_music(cl_engine_runtime *runtime,
                                          const char *path,
                                          float crossfade_seconds);
CLAY_API void cl_engine_audio_stop_music(cl_engine_runtime *runtime,
                                         float fade_seconds);
CLAY_API bool cl_engine_audio_music_playing(
    const cl_engine_runtime *runtime);

/* Packed 0x00RRGGBB pixels, row-major, top-left origin. */
CLAY_API const uint32_t *cl_engine_runtime_pixels(
    const cl_engine_runtime *runtime, size_t *count);
CLAY_API const uint8_t *cl_engine_runtime_pixels_rgba(
    const cl_engine_runtime *runtime, size_t *byte_count);
CLAY_API cl_err cl_engine_runtime_save_png(const cl_engine_runtime *runtime,
                                           const char *path);

#ifdef __cplusplus
}
#endif

#undef CLAY_API

#endif /* CLAY_ENGINE_C_H */
