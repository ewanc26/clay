#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "runtime.hpp"
#include "systems/reaction.hpp"

#include <clay/clay.h>

#include <vector>

using namespace clay;

namespace {

const char *kRules = R"json({
  "rules": [
    { "name": "click spawns", "on": "input.key",
      "match": { "value": "MOUSE_LEFT", "kind": "press" },
      "cooldown": 0.05,
      "do": [ { "effect": "spawn", "species": "animal", "life": 60.0,
                "color": [0.5, 0.8, 0.6] } ] },
    { "name": "release nothing", "on": "input.key",
      "match": { "value": "MOUSE_LEFT", "kind": "release" },
      "do": [ { "effect": "flash", "color": [1, 1, 1] } ] },
    { "name": "wheel ripple", "on": "input.wheel",
      "do": [ { "effect": "ripple", "radius": 50 } ] },
    { "name": "motion scatters", "on": "input.motion",
      "cooldown": 0.4,
      "do": [ { "effect": "spawn", "species": "pebble" } ] }
  ]
})json";

} // namespace

TEST_CASE("reaction: press value matches, release does not") {
    Runtime rt(200, 200, 1);
    rt.reactions().load_text(kRules);

    rt.begin_frame(1.0 / 60.0);
    rt.feed_press(CLAY_KEY_MOUSE_LEFT);
    CHECK_EQ(rt.reactions().fired_count(), 1); /* "click spawns" only */
    CHECK_EQ(rt.world().spawns(), 1);

    rt.feed_release(CLAY_KEY_MOUSE_LEFT);
    /* press rule won't fire on release; release rule fires (flash). */
    CHECK_EQ(rt.reactions().fired_count(), 2);
}

TEST_CASE("reaction: cooldown clocks from sim time, not wall time") {
    Runtime rt(200, 200, 2);
    rt.reactions().load_text(kRules);

    /* Two presses one frame apart: second is inside the 0.05 cooldown. */
    rt.begin_frame(1.0 / 60.0);
    rt.feed_press(CLAY_KEY_MOUSE_LEFT);
    rt.feed_release(CLAY_KEY_MOUSE_LEFT);
    rt.begin_frame(1.0 / 60.0);
    rt.feed_press(CLAY_KEY_MOUSE_LEFT);
    rt.feed_release(CLAY_KEY_MOUSE_LEFT);
    CHECK_LT(rt.reactions().fired_count(), 4);
}

TEST_CASE("reaction: rules survive a clear and re-load") {
    Runtime rt(200, 200, 3);
    rt.reactions().load_text(kRules);
    CHECK_EQ(rt.reactions().rule_count(), 4);

    rt.reactions().reset();
    CHECK_EQ(rt.reactions().rule_count(), 0);
    rt.reactions().load_text(kRules);
    CHECK_EQ(rt.reactions().rule_count(), 4);
}