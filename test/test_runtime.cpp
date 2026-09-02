#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "runtime.hpp"

#include <limits>

using namespace clay;

namespace {

uint64_t fnv1a64(const Framebuffer &fb) {
    uint64_t h = 1469598103934665603ULL;
    for (uint32_t v : fb.pixels) {
        for (int j = 0; j < 4; j++) {
            h ^= (uint64_t)(v & 0xff);
            h *= 1099511628211ULL;
            v >>= 8;
        }
    }
    return h;
}

const char *kTinyReactions = R"json({
  "rules": [
    { "name": "click animal", "on": "input.key",
      "match": { "value": "MOUSE_LEFT", "kind": "press" },
      "cooldown": 0.05,
      "do": [ { "effect": "spawn", "species": "animal", "life": 90.0,
                "color": [0.7, 0.9, 0.6] } ] },
    { "name": "scroll ripple", "on": "input.wheel",
      "cooldown": 0.2,
      "do": [ { "effect": "ripple", "color": [0.6, 0.8, 0.9], "radius": 60 } ] },
    { "name": "space flash", "on": "input.key",
      "match": { "value": "SPACE", "kind": "press" },
      "cooldown": 0.1,
      "do": [ { "effect": "flash", "color": [1.0, 0.9, 0.5] } ] },
    { "name": "R clears", "on": "input.key",
      "match": { "value": "R", "kind": "press" },
      "cooldown": 0.1,
      "do": [ { "effect": "kill_radius", "radius": 300 } ] }
  ]
})json";

/* A fixed scripted transcript, equivalent to demo drive_headless_input. */
void drive(Runtime &rt, uint64_t frame) {
    switch (frame) {
    case 1:
        rt.feed_motion(120, 140, 0, 0);
        break;
    case 20:
        rt.feed_motion(140, 160, 20, 20);
        break;
    case 25: {
        cl_input_event e =
            cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_MOUSE_LEFT);
        e.x = 140;
        e.y = 160;
        rt.feed(e);
        break;
    }
    case 26: {
        cl_input_event e =
            cl_input_event_make(CLAY_IN_RELEASE, CLAY_KEY_MOUSE_LEFT);
        e.x = 140;
        e.y = 160;
        rt.feed(e);
        break;
    }
    case 34: {
        cl_input_event e = cl_input_event_make(CLAY_IN_WHEEL, CLAY_KEY_NONE);
        e.wheel = 2;
        e.x = 180;
        e.y = 200;
        rt.feed(e);
        break;
    }
    case 42:
        rt.feed_press(CLAY_KEY_SPACE);
        break;
    case 43:
        rt.feed_release(CLAY_KEY_SPACE);
        break;
    case 50:
        rt.feed_press(CLAY_KEY_R);
        rt.feed_release(CLAY_KEY_R);
        break;
    default:
        break;
    }
}

void plant(Runtime &rt) {
    rt.spawn_species("sculpture", rt.width() * 0.5f, rt.height() * 0.5f,
                     {0.9f, 0.8f, 0.5f, 1.0f}, 1e9f);
    for (int i = 0; i < 20; i++) {
        rt.spawn_species("animal", (float)(cl_rng_f64(&rt.rng()) * rt.width()),
                         (float)(cl_rng_f64(&rt.rng()) * rt.height()),
                         {0.7f, 0.9f, 0.6f, 1.0f}, 300.0f);
    }
}

void run_frames(Runtime &rt, bool scripted, uint64_t n, double dt) {
    for (uint64_t f = 1; f <= n; f++) {
        rt.begin_frame(dt);
        if (scripted) drive(rt, rt.frame());
        rt.update(rt.sim_dt());
        rt.render();
    }
}

} // namespace

TEST_CASE("runtime: reactions fire and mutate the world") {
    Runtime rt(320, 240, 42);
    rt.reactions().load_text(kTinyReactions);
    /* Route the raw edges (click, space) to actions so they leave commands. */
    rt.actions().bind("click", CLAY_KEY_MOUSE_LEFT);
    rt.actions().bind("space", CLAY_KEY_SPACE);
    plant(rt);
    size_t planted = rt.world().living();

    for (uint64_t f = 1; f <= 60; f++) {
        rt.begin_frame(1.0 / 60.0);
        drive(rt, rt.frame());
        rt.update(rt.sim_dt());
        rt.render();
    }

    CHECK(rt.reactions().fired_count() >= 4); /* click, wheel, space, R */
    CHECK(rt.commands().count() >= 2);        /* click + space edges */

    /* The click spawned an animal; wheel spawned a ripple. */
    CHECK(rt.world().spawns() >= planted + 2);
    /* R's kill_radius destroyed at least the spawned animal. */
    CHECK(rt.world().destroys() < rt.world().spawns());
}

TEST_CASE("runtime: cooldown suppresses a rapid second press") {
    Runtime rt(320, 240, 7);
    rt.reactions().load_text(kTinyReactions);

    /* Both presses happen at nearly the same sim time. */
    for (int rep = 0; rep < 3; rep++) {
        rt.begin_frame(1.0 / 60.0);
        rt.feed_press(CLAY_KEY_SPACE);
        rt.feed_release(CLAY_KEY_SPACE);
        rt.update(rt.sim_dt());
        rt.render();
    }
    /* Three presses, but the cooldown must swallow the early ones. */
    CHECK(rt.reactions().fired_count() >= 1);
    CHECK(rt.reactions().fired_count() < 3);
}

TEST_CASE("runtime: malformed direct input is ignored") {
    Runtime rt(320, 240, 8);
    rt.begin_frame(1.0 / 60.0);

    cl_input_event invalid = cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_NONE);
    rt.feed(invalid);

    CHECK_EQ(rt.input_log().count, 0);
    CHECK_EQ(rt.commands().count(), 0);
    CHECK_EQ(rt.reactions().fired_count(), 0);
}

TEST_CASE("runtime: malformed direct timing is ignored") {
    Runtime rt(320, 240, 9);
    rt.begin_frame(-1.0);
    rt.begin_frame(std::numeric_limits<double>::quiet_NaN());

    CHECK_EQ(rt.frame(), 0);
    CHECK_EQ(rt.sim_time(), 0.0);
}

TEST_CASE("runtime: same seed, same transcript, same world") {
    uint64_t seed = 0xBEEF;
    const uint64_t frames = 60;
    const double dt = 1.0 / 60.0;

    auto snapshot = [&](Runtime &rt) {
        return std::make_tuple(
            rt.commands().count(), rt.world().spawns(), rt.world().destroys(),
            rt.reactions().fired_count(), fnv1a64(rt.framebuffer()));
    };

    Runtime a(320, 240, seed);
    a.reactions().load_text(kTinyReactions);
    plant(a);
    run_frames(a, true, frames, dt);

    Runtime b(320, 240, seed);
    b.reactions().load_text(kTinyReactions);
    plant(b);
    run_frames(b, true, frames, dt);

    CHECK(snapshot(a) == snapshot(b));
}

TEST_CASE("runtime: recorded transcript replays byte-identically") {
    uint64_t seed = 0xCAFE;
    const uint64_t frames = 60;
    const double dt = 1.0 / 60.0;

    Runtime original(320, 240, seed);
    original.reactions().load_text(kTinyReactions);
    plant(original);
    run_frames(original, true, frames, dt);

    const char *path = "clay_test_runtime_replay.clayrec";
    REQUIRE(cl_input_log_save(&original.input_log(), path) == CLAY_OK);

    Runtime replay(320, 240, seed);
    replay.reactions().load_text(kTinyReactions);
    plant(replay);
    REQUIRE(cl_input_log_load(&replay.input_log(), path) == CLAY_OK);
    replay.set_replaying(true);

    run_frames(replay, false, frames, dt);

    CHECK(replay.commands().count() == original.commands().count());
    CHECK(replay.commands().fingerprint() == original.commands().fingerprint());
    CHECK(replay.world().spawns() == original.world().spawns());
    CHECK(replay.world().destroys() == original.world().destroys());
    CHECK(replay.reactions().fired_count() == original.reactions().fired_count());
    CHECK(fnv1a64(replay.framebuffer()) == fnv1a64(original.framebuffer()));
}
