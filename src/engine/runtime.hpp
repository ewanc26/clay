#ifndef CLAY_ENGINE_RUNTIME_HPP
#define CLAY_ENGINE_RUNTIME_HPP

#include "action.hpp"
#include "audio/audio_decode.hpp"
#include "audio/audio_mixer.hpp"
#include "command.hpp"
#include "ecs/components.hpp"
#include "ecs/world.hpp"
#include "event.hpp"
#include "input_system.hpp"
#include "replay.hpp"
#include "render/renderer.hpp"
#include "render/render_system.hpp"
#include "render/renderer_sw.hpp"
#include "systems/reaction.hpp"
#include "systems/system_graph.hpp"

#include <clay/clay.h>

#include <cstdint>
#include <cmath>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace clay {

/* The Runtime is the whole reactive machine on one desk: it owns the arena,
 * the bus, the input state + transcript, the action/command pipeline, the
 * ECS world, the system graph, the reaction rules, and the renderer, and it
 * is the ONLY place — across live input, replay, and automated tests — that
 * events enter the engine. Two Runtimes given the same seed and transcript
 * produce identical worlds. */
class Runtime {
  public:
    Runtime(int width, int height, uint64_t seed,
            size_t arena_bytes = 4u << 20);
    ~Runtime();

    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;

    /* -------------------------------------------------------- the loop */
    void begin_frame(double dt_seconds);
    void update(double sim_dt_seconds);
    void render();
    void end_frame() {}
    void step(double dt_seconds) {
        begin_frame(dt_seconds);
        update(sim_dt_);
        render();
    }

    /* The single input path: state edge detection -> transcript -> bus ->
     * actions -> commands. Everyone downstream sees exactly this. */
    void feed(const cl_input_event &e);
    void feed_press(cl_key key) {
        feed(cl_input_event_make(CLAY_IN_PRESS, key));
    }
    void feed_release(cl_key key) {
        feed(cl_input_event_make(CLAY_IN_RELEASE, key));
    }
    void feed_motion(double x, double y, double dx, double dy);

    /* ---------------------------------------------- world mutation (reactive)
     */
    Entity spawn_species(const std::string &species, float x, float y,
                         Color color, float life);
    Entity spawn_ripple(float x, float y, float radius, Color color);
    bool load_actions(const std::string &text);
    bool load_actions_file(const std::string &path);
    bool load_reactions(const std::string &text);
    bool load_reactions_file(const std::string &path);
    bool load_scene(const std::string &text);
    bool load_scene_file(const std::string &path);
    void unload_scene() noexcept;
    bool has_scene() const noexcept;
    cl_err save_recording(const std::string &path) const;
    cl_err load_recording(const std::string &path);
    cl_err audio_load_wav(const std::string &path, AudioClipId *clip_id);
    cl_err audio_load_file(const std::string &path, AudioClipId *clip_id);
    bool audio_unload_clip(AudioClipId clip_id);
    AudioVoiceId audio_play(AudioClipId clip_id, AudioBus bus, bool loop,
                            float gain);
    bool audio_stop(AudioVoiceId voice_id);
    bool audio_pause(AudioVoiceId voice_id);
    bool audio_resume(AudioVoiceId voice_id);
    bool audio_set_voice_pan(AudioVoiceId voice_id, float pan) noexcept {
        return audio_.set_voice_pan(voice_id, pan);
    }
    float audio_voice_pan(AudioVoiceId voice_id) const noexcept {
        return audio_.voice_pan(voice_id);
    }
    bool audio_set_voice_gain(AudioVoiceId voice_id, float gain) noexcept {
        return audio_.set_voice_gain(voice_id, gain);
    }
    float audio_voice_gain(AudioVoiceId voice_id) const noexcept {
        return audio_.voice_gain(voice_id);
    }
    bool audio_fade_voice(AudioVoiceId voice_id, float target_gain,
                          std::size_t duration_frames) noexcept {
        return audio_.fade_voice(voice_id, target_gain, duration_frames);
    }
    bool audio_voice_active(AudioVoiceId voice_id) const noexcept {
        return audio_.voice_active(voice_id);
    }
    bool audio_voice_paused(AudioVoiceId voice_id) const noexcept {
        return audio_.voice_paused(voice_id);
    }
    std::size_t audio_clip_frame_count(AudioClipId clip_id) const noexcept {
        return audio_.clip_frame_count(clip_id);
    }
    bool audio_mix_stereo(std::span<float> output);
    std::uint32_t audio_sample_rate() const noexcept {
        return audio_.sample_rate();
    }
    bool resize(int width, int height);
    void destroy_entity(Entity e); /* publishes world.destroy */
    void flash(Color color, double duration);
    void kill_within(float x, float y, float radius);

    /* --------------------------------------------------------------- probes */
    uint64_t frame() const {
        return frame_;
    }
    double sim_time() const {
        return sim_time_;
    }
    double sim_dt() const {
        return sim_dt_;
    }
    int width() const {
        return width_;
    }
    int height() const {
        return height_;
    }
    uint64_t seed() const {
        return seed_;
    }

    double cursor_x() const {
        return input_state_.cursor_x;
    }
    double cursor_y() const {
        return input_state_.cursor_y;
    }
    bool is_key_down(cl_key key) const {
        return cl_input_down(&input_state_, key);
    }
    const cl_input_state &input_state() const {
        return input_state_;
    }

    World &world() {
        return world_;
    }
    Hub &hub() {
        return hub_;
    }
    ActionMap &actions() {
        return actions_;
    }
    const ActionMap &actions() const {
        return actions_;
    }
    CommandLog &commands() {
        return commands_;
    }
    const CommandLog &commands() const {
        return commands_;
    }

    /* Undo/redo: revert or re-apply the most recent reversible command.
     * Returns the command that was affected (nullptr if nothing to undo/redo).
     * The undone/redone command is republished on CLAY_CH_COMMAND so systems
     * can observe and revert/re-apply world state. The command log fingerprint
     * incorporates undo/redo, keeping replays byte-stable. */
    const Command *undo();
    const Command *redo();
    SystemGraph &systems() {
        return systems_;
    }
    ReactionEngine &reactions() {
        return reactions_;
    }
    RendererSW &renderer() {
        return renderer_;
    }
    const Framebuffer &framebuffer() const {
        return renderer_.framebuffer();
    }

    /* Install the draw pass. If none is set, the default GardenRenderSystem
     * (the inherited vignette) is used, so existing behavior is unchanged. */
    void set_render_system(RenderSystem *system) {
        render_system_ = system;
    }
    RenderSystem *render_system() const {
        return render_system_;
    }
    cl_rng &rng() {
        return rng_;
    }
    cl_arena &arena() {
        return arena_.a;
    }
    cl_input_log &input_log() {
        return input_log_;
    }
    const cl_input_log &input_log() const {
        return input_log_;
    }

    AudioMixer &audio() {
        return audio_;
    }
    const AudioMixer &audio() const {
        return audio_;
    }

    double time_scale() const {
        return time_scale_;
    }
    void set_time_scale(double s) {
        time_scale_ = std::isfinite(s) && s > 0.0 ? s : 1.0;
    }

    /* Render-time state a custom RenderSystem needs beyond the world/cursor. */
    const Color &flash_color() const {
        return flash_color_;
    }
    double flash_remaining() const {
        return flash_remaining_;
    }
    double flash_duration() const {
        return flash_duration_;
    }

    /* Speed-of-light replay toggle: while on, calls to feed() are the only
     * source and begin_frame() pulls matching-frame events from the log. */
    void set_replaying(bool on) {
        replaying_ = on;
    }
    bool replaying() const {
        return replaying_;
    }

    void log_reaction(const std::string &msg);

  private:
    int width_;
    int height_;
    uint64_t seed_;

    cl_rng rng_;

    /* The single engine arena, constructed before anything that borrows it. */
    struct ArenaOwner {
        std::vector<uint8_t> storage;
        cl_arena a;
        explicit ArenaOwner(size_t bytes)
            : storage(bytes > 0 ? bytes : (1u << 20)) {
            cl_arena_init(&a, storage.data(), storage.size());
        }
    };
    ArenaOwner arena_;

    cl_bus bus_;
    cl_input_state input_state_;
    cl_input_log input_log_;
    AudioMixer audio_;
    Hub hub_;
    ActionMap actions_;
    InputSystem inputs_;
    CommandLog commands_;
    World world_;
    SystemGraph systems_;
    ReactionEngine reactions_;
    RendererSW renderer_;
    RenderSystem *render_system_ = nullptr;
    RenderSystem *scene_restore_render_system_ = nullptr;
    bool scene_restore_valid_ = false;
    std::unique_ptr<ClayScene> scene_;
    std::unique_ptr<Scene3DRenderSystem> scene_system_;

    uint64_t frame_ = 0;
    double sim_time_ = 0.0;
    double sim_dt_ = 1.0 / 60.0;
    double time_scale_ = 1.0;

    bool replaying_ = false;
    Replayer replay_;

    Color flash_color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    double flash_remaining_ = 0.0;
    double flash_duration_ = 0.0;

    std::vector<Action> pending_actions_;

    void publish_input_event(const cl_input_event &e);
    void execute_action(const Action &a, bool record_command);
    void pump_replay_events();
    Event to_event(const cl_event &ev);
};

} // namespace clay

#endif /* CLAY_ENGINE_RUNTIME_HPP */
