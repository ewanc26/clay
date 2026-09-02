#include "garden.hpp"

#include "systems/builtin.hpp"

#include <cmath>
#include <cstring>

namespace clay {

/* The garden's reaction table. Data, not code — swap this string and the
 * whole vignette changes without a rebuild. */
const char *kGardenReactions = R"json({
  "rules": [
    {
      "name": "click ripples",
      "on": "input.key",
      "match": { "value": "MOUSE_LEFT", "kind": "press" },
      "cooldown": 0.05,
      "do": [
        { "effect": "ripple", "color": [0.95, 0.72, 0.40], "radius": 42 }
      ]
    },
    {
      "name": "space bloom",
      "on": "input.key",
      "match": { "value": "SPACE", "kind": "press" },
      "cooldown": 0.15,
      "do": [
        { "effect": "flash", "color": [1.0, 0.85, 0.55] },
        { "effect": "ripple", "color": [0.95, 0.55, 0.40], "radius": 90 }
      ]
    },
    {
      "name": "scroll embiggen rings",
      "on": "input.wheel",
      "cooldown": 0.2,
      "do": [
        { "effect": "ripple", "color": [0.55, 0.75, 0.95], "radius": 70 }
      ]
    },
    {
      "name": "motion moss",
      "on": "input.motion",
      "cooldown": 0.35,
      "do": [
        { "effect": "spawn", "species": "pebble", "life": 6.0,
          "color": [0.45, 0.55, 0.40] }
      ]
    },
    {
      "name": "grow the herd",
      "on": "input.key",
      "match": { "value": "E", "kind": "press" },
      "cooldown": 0.2,
      "do": [
        { "effect": "spawn", "species": "animal", "life": 60.0,
          "color": [0.70, 0.85, 0.60] }
      ]
    },
    {
      "name": "calm the herd",
      "on": "input.key",
      "match": { "value": "R", "kind": "press" },
      "cooldown": 0.2,
      "do": [
        { "effect": "kill_radius", "radius": 220 }
      ]
    }
  ]
})json";

Garden::Garden(int width, int height, uint64_t seed)
    : width_(width), height_(height),
      rt_(std::make_unique<Runtime>(width, height, seed)) {}

Garden::~Garden() = default;

void Garden::seed(const std::string &reactions_json) {
    rt_->reactions().load_text(reactions_json);

    rt_->actions().bind_hold("move_up", CLAY_KEY_W);
    rt_->actions().bind_hold("move_down", CLAY_KEY_S);
    rt_->actions().bind("primary", CLAY_KEY_MOUSE_LEFT, CLAY_MOD_NONE, true);
    rt_->actions().bind("undo", CLAY_KEY_Z, CLAY_MOD_CTRL);
    rt_->actions().bind("redo", CLAY_KEY_Y, CLAY_MOD_CTRL);

    rt_->systems().add(std::make_unique<MovementSystem>());
    rt_->systems().add(std::make_unique<CursorMagnetSystem>());
    rt_->systems().add(std::make_unique<LifespanSystem>());
    rt_->systems().add(std::make_unique<HueShiftSystem>());
    rt_->systems().add(std::make_unique<RippleSystem>());
}

void Garden::plant() {
    /* The sculpture anchors the scene; the herd orbits it. */
    rt_->spawn_species("sculpture", width_ * 0.5f, height_ * 0.5f,
                       {0.95f, 0.80f, 0.55f, 1.0f}, 1e9f);

    for (int i = 0; i < 26; i++) {
        float x = (float)(cl_rng_f64(&rt_->rng()) * width_);
        float y = (float)(cl_rng_f64(&rt_->rng()) * height_);
        float h = (float)cl_rng_f64(&rt_->rng());
        float v = (float)cl_rng_f64(&rt_->rng());
        Color color = (i % 2 == 0)
                          ? Color{0.75f + h * 0.25f, 0.85f - v * 0.3f, 0.55f, 1.0f}
                          : Color{0.90f - h * 0.3f, 0.70f + v * 0.2f, 0.55f, 1.0f};
        rt_->spawn_species("animal", x, y, color, 300.0f);
    }
    for (int i = 0; i < 18; i++) {
        float x = (float)(cl_rng_f64(&rt_->rng()) * width_);
        float y = (float)(cl_rng_f64(&rt_->rng()) * height_);
        rt_->spawn_species("pebble", x, y,
                           {0.40f, 0.42f, 0.36f, 1.0f}, 1e6f);
    }
}

void Garden::drive_headless_input(uint64_t frame) {
    Runtime &rt = *rt_;
    const double cx = width_ * 0.5;
    const double cy = height_ * 0.5;

    switch (frame) {
    case 1: {
        /* plant then place the cursor over the herd */
        rt.feed_motion(90, 130, 0, 0);
        break;
    }
    case 20:
        rt.feed_motion(120, 150, 30, 20);
        break;
    case 26: {
        cl_input_event e = cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_MOUSE_LEFT);
        e.x = 120;
        e.y = 150;
        rt.feed(e);
        break;
    }
    case 27: {
        cl_input_event e = cl_input_event_make(CLAY_IN_RELEASE, CLAY_KEY_MOUSE_LEFT);
        e.x = 120;
        e.y = 150;
        rt.feed(e);
        break;
    }
    case 36: {
        cl_input_event e = cl_input_event_make(CLAY_IN_WHEEL, CLAY_KEY_NONE);
        e.x = 200;
        e.y = 180;
        e.wheel = 2;
        rt.feed(e);
        break;
    }
    case 44:
        rt.feed_press(CLAY_KEY_SPACE);
        break;
    case 45:
        rt.feed_release(CLAY_KEY_SPACE);
        break;
    case 52:
        rt.feed_press(CLAY_KEY_E);
        rt.feed_release(CLAY_KEY_E);
        break;
    case 58:
        rt.feed_press(CLAY_KEY_R);
        rt.feed_release(CLAY_KEY_R);
        break;
    default: break;
    }

    if (frame % 9 == 0 && frame >= 60) {
        double a = (double)(frame / 9) * 0.13;
        double x = cx + std::cos(a) * 120.0;
        double y = cy + std::sin(a * 1.7) * 60.0;
        rt.feed_motion(x, y, 0, 0);
    }
}

} // namespace clay