#include "runtime.hpp"

#include "render/raster.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <iterator>
#include <vector>

namespace clay {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr size_t kMaxFramebufferPixels = 64u << 20;

static bool valid_dimensions(int width, int height) {
    return width > 0 && height > 0 &&
           (size_t)width <=
               std::numeric_limits<size_t>::max() / (size_t)height &&
           (size_t)width * (size_t)height <= kMaxFramebufferPixels;
}

static int checked_width(int width, int height) {
    if (!valid_dimensions(width, height))
        throw std::invalid_argument("invalid runtime dimensions");
    return width;
}

/* --------------------------------------------------------------- ctor */

Runtime::Runtime(int width, int height, uint64_t seed, size_t arena_bytes)
    : width_(checked_width(width, height)), height_(height), seed_(seed),
      arena_(arena_bytes), bus_(), input_log_(), hub_(&arena_.a),
      inputs_(actions_), renderer_(width, height), replay_(input_log_) {
    cl_rng_seed(&rng_, seed_);
    cl_bus_init(&bus_, &arena_.a);
    std::memset(&input_state_, 0, sizeof(input_state_));
    cl_input_log_init(&input_log_, &arena_.a, 0);

    /* Every engine event reaches systems first, then reaction rules. */
    hub_.subscribe_all([this](const cl_event &ev) {
        systems_.dispatch_event(*this, to_event(ev));
    });
    hub_.subscribe_all([this](const cl_event &ev) {
        reactions_.on_event(*this, to_event(ev));
    });
}

Runtime::~Runtime() = default;

bool Runtime::load_actions(const std::string &text) {
    std::vector<uint8_t> storage(64u << 10);
    cl_arena arena;
    cl_arena_init(&arena, storage.data(), storage.size());
    cl_json_node root;
    if (cl_json_parse(&root, &arena, cl_str_c(text.c_str())) != CLAY_OK)
        return false;
    actions_.clear();
    actions_.bind_from_json(&root);
    return true;
}

cl_err Runtime::save_recording(const std::string &path) const {
    return cl_input_log_save(const_cast<cl_input_log *>(&input_log_),
                             path.c_str());
}

cl_err Runtime::load_recording(const std::string &path) {
    cl_err err = cl_input_log_load(&input_log_, path.c_str());
    if (err == CLAY_OK) replay_.rewind();
    return err;
}

cl_err Runtime::audio_load_wav(const std::string &path, AudioClipId *clip_id) {
    if (path.empty() || clip_id == nullptr) return CLAY_ERR_INVALID_ARG;
    std::ifstream input(path, std::ios::binary);
    if (!input) return CLAY_ERR_IO;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    auto clip = decode_wav(bytes);
    if (!clip.has_value()) return CLAY_ERR_PARSE;
    auto id = audio_.add_clip(std::move(*clip));
    if (!id.has_value()) return CLAY_ERR_INVALID_ARG;
    *clip_id = *id;
    return CLAY_OK;
}

AudioVoiceId Runtime::audio_play(AudioClipId clip_id, AudioBus bus, bool loop,
                                 float gain) {
    auto voice = audio_.play(clip_id, bus, loop, gain);
    return voice.value_or(0);
}

bool Runtime::audio_stop(AudioVoiceId voice_id) {
    return audio_.stop(voice_id);
}

bool Runtime::audio_mix_stereo(std::span<float> output) {
    return audio_.mix_stereo(output);
}

bool Runtime::resize(int width, int height) {
    if (!valid_dimensions(width, height)) return false;
    width_ = width;
    height_ = height;
    renderer_.framebuffer().resize(width, height);
    return true;
}

Event Runtime::to_event(const cl_event &ev) {
    Event out;
    out.channel = ev.channel;
    out.value = ev.value;
    out.frame = ev.frame;
    out.time = ev.time;
    return out;
}

/* -------------------------------------------------------------- input  */

void Runtime::begin_frame(double dt_seconds) {
    if (!std::isfinite(dt_seconds) || dt_seconds < 0.0) return;

    frame_ += 1;
    sim_dt_ = (time_scale_ > 0.0 ? time_scale_ : 1.0) * dt_seconds;
    sim_time_ += sim_dt_;

    cl_input_state_begin(&input_state_, frame_, sim_time_);
    pending_actions_.clear();

    if (replaying_) pump_replay_events();

    /* Hold bindings become pseudo-actions each frame (no command spam). */
    for (Action &a : inputs_.poll_holds(input_state_)) {
        pending_actions_.push_back(a);
        hub_.publish_at(channel(CLAY_CH_ACTION),
                        cl_variant_str(cl_str_c(a.name.c_str())), frame_,
                        sim_time_);
    }

    hub_.publish_at(channel(CLAY_CH_FRAME), cl_variant_nil(), frame_,
                    sim_time_);
}

void Runtime::pump_replay_events() {
    std::vector<cl_input_event> evs = replay_.events_for_frame(frame_);
    for (const cl_input_event &e : evs) feed(e);
}

void Runtime::feed_motion(double x, double y, double dx, double dy) {
    cl_input_event e = cl_input_event_make(CLAY_IN_MOTION, CLAY_KEY_NONE);
    e.x = x;
    e.y = y;
    e.dx = dx;
    e.dy = dy;
    feed(e);
}

void Runtime::feed(const cl_input_event &raw) {
    if (!cl_input_event_valid(&raw)) return;

    /* Level-triggered edge detection into the shared input state. */
    cl_input_state_feed(&input_state_, &raw);

    /* The transcript is the permanent raw record; a replay replays exactly
     * this. During a replay the events already came from the log, so they
     * are not re-appended (that would corrupt the cursor). */
    if (!replaying_) {
        cl_input_event stamped = raw;
        if (stamped.frame == 0) stamped.frame = frame_;
        if (stamped.time == 0.0) stamped.time = sim_time_;
        cl_input_log_append(&input_log_, &stamped);
    }

    publish_input_event(raw);

    /* Raw edge -> logical action -> command, the single reactivity path. */
    for (const Action &a : inputs_.process(raw)) {
        pending_actions_.push_back(a);
        execute_action(a, /*record_command=*/true);
    }
}

void Runtime::publish_input_event(const cl_input_event &e) {
    cl_variant value = cl_variant_nil();
    switch (e.type) {
        case CLAY_IN_PRESS:
        case CLAY_IN_RELEASE:
            value = cl_variant_str(cl_str_c(cl_key_name(e.key)));
            hub_.publish_at(channel(CLAY_CH_INPUT_KEY), value, frame_,
                            sim_time_);
            break;
        case CLAY_IN_MOTION:
            value = cl_variant_f64(e.dx);
            hub_.publish_at(channel(CLAY_CH_INPUT_MOTION), value, frame_,
                            sim_time_);
            break;
        case CLAY_IN_WHEEL:
            value = cl_variant_i64(e.wheel);
            hub_.publish_at(channel(CLAY_CH_INPUT_WHEEL), value, frame_,
                            sim_time_);
            break;
        case CLAY_IN_FOCUS:
            value = cl_variant_bool(e.focus);
            hub_.publish_at(channel(CLAY_CH_INPUT_FOCUS), value, frame_,
                            sim_time_);
            break;
    }
}

void Runtime::execute_action(const Action &a, bool record_command) {
    /* Undo/redo are meta-actions that manipulate the command log directly.
     * They must not themselves be recorded as commands (otherwise they would
     * push themselves onto the undo stack, creating infinite regress). */
    if (a.name == "undo") {
        undo();
        return;
    }
    if (a.name == "redo") {
        redo();
        return;
    }

    hub_.publish_at(channel(CLAY_CH_ACTION),
                    cl_variant_str(cl_str_c(a.name.c_str())),
                    a.frame ? a.frame : frame_, a.time ? a.time : sim_time_);

    if (!record_command) return;
    Command cmd;
    cmd.name = a.name;
    cmd.source = a.name;
    cmd.value = a.value;
    cmd.x = a.x;
    cmd.y = a.y;
    cmd.reversible = a.reversible;
    cmd.frame = a.frame ? a.frame : frame_;
    cmd.time = a.time ? a.time : sim_time_;
    commands_.record(cmd);
    hub_.publish_at(channel(CLAY_CH_COMMAND),
                    cl_variant_str(cl_str_c(cmd.name.c_str())), cmd.frame,
                    cmd.time);
}

/* ------------------------------------------------------------ world   */

Entity Runtime::spawn_species(const std::string &species, float x, float y,
                              Color color, float life) {
    Entity e = world_.create();
    Species kind = Species::Unknown;
    if (species == "animal")
        kind = Species::Animal;
    else if (species == "sculpture")
        kind = Species::Sculpture;
    else if (species == "pebble")
        kind = Species::Pebble;
    else if (species == "ground")
        kind = Species::Ground;

    world_.storage<Transform2D>().set(e, {x, y, 0.0f, 1.0f});
    world_.storage<Color>().set(e, color);
    world_.storage<LifeSpan>().set(e, {life, life});
    world_.storage<Kind>().set(e, {kind});

    if (kind == Species::Animal) {
        double dir = cl_rng_f64(&rng_) * kTwoPi;
        double speed = 12.0 + cl_rng_f64(&rng_) * 22.0;
        world_.storage<Velocity>().set(e, {(float)(std::cos(dir) * speed),
                                           (float)(std::sin(dir) * speed)});
        world_.storage<MagnetStrength>().set(e, {42.0f});
    }

    hub_.publish_at(channel(CLAY_CH_WORLD),
                    cl_variant_str(cl_str_c(species.c_str())), frame_,
                    sim_time_);
    return e;
}

Entity Runtime::spawn_ripple(float x, float y, float radius, Color color) {
    Entity e = world_.create();
    world_.storage<Transform2D>().set(e, {x, y, 0.0f, 1.0f});
    world_.storage<Color>().set(e, color);
    world_.storage<LifeSpan>().set(e, {1.4f, 1.4f});
    world_.storage<Kind>().set(e, {Species::Ripple});
    world_.storage<RippleRing>().set(e, {8.0f, radius / 1.4f, 2.0f});
    hub_.publish_at(channel(CLAY_CH_WORLD), cl_variant_str(cl_str_c("ripple")),
                    frame_, sim_time_);
    return e;
}

void Runtime::destroy_entity(Entity e) {
    if (!world_.alive(e)) return;
    world_.destroy(e);
    hub_.publish_at(channel(CLAY_CH_WORLD), cl_variant_str(cl_str_c("destroy")),
                    frame_, sim_time_);
}

void Runtime::kill_within(float x, float y, float radius) {
    ComponentStorage<Transform2D> &ts = world_.storage<Transform2D>();
    std::vector<Entity> victims;
    for (size_t i = 0; i < ts.count(); i++) {
        Transform2D &t = ts.dense[i];
        float dx = t.x - x;
        float dy = t.y - y;
        if (dx * dx + dy * dy > radius * radius) continue;
        Kind *k = world_.storage<Kind>().find(ts.owner[i]);
        if (k &&
            (k->species == Species::Sculpture || k->species == Species::Ground))
            continue;
        victims.push_back(ts.owner[i]);
    }
    for (Entity e : victims) destroy_entity(e);
}

void Runtime::flash(Color color, double duration) {
    flash_color_ = color;
    flash_duration_ = duration;
    flash_remaining_ = duration;
}

void Runtime::log_reaction(const std::string &msg) {
    cl_log_info("%s", msg.c_str());
}

/* ------------------------------------------------------------- update */

void Runtime::update(double dt) {
    /* flash decays in sim time (not wall time) so replays match */
    if (flash_remaining_ > 0.0) flash_remaining_ -= dt;
    systems_.update(*this, dt);
}

/* ------------------------------------------------------------- render */

void Runtime::render() {
    static GardenRenderSystem default_renderer;
    RenderSystem *system = render_system_ ? render_system_ : &default_renderer;
    system->render(*this, renderer_);
}

/* ----------------------------------------------------------- undo/redo */

const Command *Runtime::undo() {
    /* Undo is a meta-action: it manipulates the command log directly and
     * must not itself be recorded as a command. */
    const Command *cmd = commands_.undo();
    if (cmd == nullptr) return nullptr;
    /* Republish the undone command on the command channel so systems can
     * observe and revert world state. The value is prefixed "undo:" so
     * reaction rules can match undo events distinctly from fresh ones. */
    std::string event_name = "undo:" + cmd->name;
    hub_.publish_at(channel(CLAY_CH_COMMAND),
                    cl_variant_str(cl_str_c(event_name.c_str())), cmd->frame,
                    cmd->time);
    return cmd;
}

const Command *Runtime::redo() {
    const Command *cmd = commands_.redo();
    if (cmd == nullptr) return nullptr;
    /* Redone commands are republished as-is — systems re-apply the same
     * world mutation they applied originally. */
    hub_.publish_at(channel(CLAY_CH_COMMAND),
                    cl_variant_str(cl_str_c(cmd->name.c_str())), cmd->frame,
                    cmd->time);
    return cmd;
}

} // namespace clay
