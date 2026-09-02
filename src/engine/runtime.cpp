#include "runtime.hpp"

#include "render/raster.hpp"

#include <cmath>
#include <cstring>
#include <vector>

namespace clay {

constexpr double kTwoPi = 6.28318530717958647692;

static Rgba u8c(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    return Rgba{(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a};
}
static Rgba f32c(float r, float g, float b, float a) {
    return u8c((uint32_t)(r * 255.0f), (uint32_t)(g * 255.0f),
               (uint32_t)(b * 255.0f), (uint32_t)(a * 255.0f));
}

/* --------------------------------------------------------------- ctor */

Runtime::Runtime(int width, int height, uint64_t seed, size_t arena_bytes)
    : width_(width),
      height_(height),
      seed_(seed),
      arena_(arena_bytes),
      bus_(),
      input_log_(),
      hub_(&arena_.a),
      inputs_(actions_),
      renderer_(width, height),
      replay_(input_log_) {
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

void Runtime::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    width_ = width;
    height_ = height;
    renderer_.framebuffer().resize(width, height);
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

    if (replaying_) pump_replay_events();

    cl_input_state_begin(&input_state_, frame_, sim_time_);
    pending_actions_.clear();

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
        hub_.publish_at(channel(CLAY_CH_INPUT_KEY), value, frame_, sim_time_);
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
    world_.storage<RippleRing>().set(
        e, {8.0f, radius / 1.4f, 2.0f});
    hub_.publish_at(channel(CLAY_CH_WORLD), cl_variant_str(cl_str_c("ripple")),
                frame_, sim_time_);
    return e;
}

void Runtime::destroy_entity(Entity e) {
    if (!world_.alive(e)) return;
    world_.destroy(e);
    hub_.publish_at(channel(CLAY_CH_WORLD),
                    cl_variant_str(cl_str_c("destroy")), frame_, sim_time_);
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
        if (k && (k->species == Species::Sculpture || k->species == Species::Ground))
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

namespace {

/* Circle drawn as connected line segments (no stroke primitive in raster). */
void draw_ring(RendererSW &r, float cx, float cy, float radius, Rgba c) {
    const int segments = 48;
    float prev_x = cx + radius;
    float prev_y = cy;
    for (int i = 1; i <= segments; i++) {
        float a = (float)i / (float)segments * (float)kTwoPi;
        float x = cx + std::cos(a) * radius;
        float y = cy + std::sin(a) * radius;
        r.draw_line(prev_x, prev_y, x, y, c);
        prev_x = x;
        prev_y = y;
    }
}

} // namespace

void Runtime::render() {
    Rgba soil = u8c(38, 32, 27, 255);
    renderer_.begin_frame(soil);

    ComponentStorage<Transform2D> &ts = world_.storage<Transform2D>();
    ComponentStorage<Color> &cs = world_.storage<Color>();
    ComponentStorage<RippleRing> &rs = world_.storage<RippleRing>();

    /* Ground band, pinned to the bottom of the canvas. */
    renderer_.fill_rect(0.0f, height_ - 46.0f, width_, 46.0f,
                        u8c(62, 52, 38, 255));

    for (size_t i = 0; i < ts.count(); i++) {
        Entity e = ts.owner[i];
        Transform2D &t = ts.dense[i];
        Color *c = cs.find(e);
        Rgba base = f32c(c ? c->r : 0.63f, c ? c->g : 0.63f, c ? c->b : 0.63f,
                         1.0f);
        Kind *k = world_.storage<Kind>().find(e);
        Species species = k ? k->species : Species::Unknown;

        switch (species) {
        case Species::Animal: {
            renderer_.fill_circle(t.x, t.y, 7.0f, base);
            Rgba outline = u8c((uint32_t)(base.r / 2), (uint32_t)(base.g / 2),
                               (uint32_t)(base.b / 2), 255);
            renderer_.draw_line(t.x - 8, t.y, t.x + 8, t.y, outline);
            Velocity *v = world_.storage<Velocity>().find(e);
            if (v) {
                float len = std::sqrt(v->x * v->x + v->y * v->y);
                if (len > 1.0f) {
                    float ux = v->x / len * 9.0f;
                    float uy = v->y / len * 9.0f;
                    renderer_.draw_line(t.x, t.y, t.x + ux, t.y + uy, outline);
                }
            }
            break;
        }
        case Species::Sculpture: {
            float s = 14.0f;
            renderer_.fill_triangle(t.x, t.y - s, t.x - s, t.y + s, t.x + s,
                                    t.y + s, base);
            Rgba shade = u8c((uint32_t)(base.r / 2), (uint32_t)(base.g / 2),
                             (uint32_t)(base.b / 3), 255);
            renderer_.fill_triangle(t.x, t.y - s, t.x - s, t.y + s, t.x,
                                    t.y + s, shade);
            renderer_.fill_circle(t.x, t.y - s, 3.0f, u8c(255, 250, 235, 255));
            break;
        }
        case Species::Ripple: {
            RippleRing *ring = rs.find(e);
            float radius = ring ? ring->radius : 20.0f;
            float alpha = c ? c->a * 255.0f : 255.0f;
            Rgba dim = u8c(base.r, base.g, base.b, (uint32_t)alpha);
            draw_ring(renderer_, t.x, t.y, radius, dim);
            break;
        }
        case Species::Pebble:
            renderer_.fill_circle(t.x, t.y, 2.2f, base);
            break;
        default:
            break;
        }
    }

    /* Mouse crosshair: the cursor is part of the world and reacts. */
    renderer_.draw_line((float)cursor_x() - 8, (float)cursor_y(),
                        (float)cursor_x() + 8, (float)cursor_y(),
                        u8c(255, 230, 190, 255));
    renderer_.draw_line((float)cursor_x(), (float)cursor_y() - 8,
                        (float)cursor_x(), (float)cursor_y() + 8,
                        u8c(255, 230, 190, 255));

    if (flash_remaining_ > 0.0) {
        float t = (float)(flash_remaining_ /
                          (flash_duration_ > 0.0 ? flash_duration_ : 1.0));
        Rgba over = f32c(flash_color_.r, flash_color_.g, flash_color_.b,
                         0.0f + t);
        renderer_.fill_rect(0, 0, width_, height_, over);
    }

    renderer_.end_frame();
}

} // namespace clay
