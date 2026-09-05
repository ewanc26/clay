#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "runtime.hpp"
#include "systems/reaction.hpp"

#include <clay/clay.h>

#include <array>
#include <string>
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

TEST_CASE("reaction: audio effects trigger and stop mixer voices") {
    Runtime rt(200, 200, 5);
    const auto clip = rt.audio().add_clip(AudioClip{48000, 1, {0.75F}});
    REQUIRE(clip.has_value());
    const std::string rules = std::string(R"json({"rules":[
          {"name":"play music","on":"input.key",
           "match":{"value":"SPACE","kind":"press"},
           "do":[{"effect":"audio_play","clip":)json") +
                              std::to_string(*clip) +
                              R"json(,"bus":"music","loop":true,"gain":0.5}]},
          {"name":"play sfx","on":"input.key",
           "match":{"value":"SPACE","kind":"press"},
           "do":[{"effect":"audio_play","clip":)json" +
                              std::to_string(*clip) +
                              R"json(,"bus":"sfx","loop":true}]},
          {"name":"stop music","on":"input.key",
           "match":{"value":"E","kind":"press"},
           "do":[{"effect":"audio_music_stop"}]},
          {"name":"stop all","on":"input.key",
           "match":{"value":"R","kind":"press"},
           "do":[{"effect":"audio_stop_all"}]}
        ]})json";
    REQUIRE(rt.reactions().load_text(rules));

    rt.begin_frame(1.0 / 60.0);
    rt.feed_press(CLAY_KEY_SPACE);
    CHECK(rt.audio().voice_count() == 2);
    std::array<float, 2> samples{};
    REQUIRE(rt.audio().mix_stereo(samples));
    CHECK(samples[0] == doctest::Approx(1.0F));
    CHECK(samples[1] == doctest::Approx(1.0F));

    rt.feed_release(CLAY_KEY_SPACE);
    rt.feed_press(CLAY_KEY_E);
    CHECK(rt.audio().voice_count() == 1);
    rt.feed_release(CLAY_KEY_E);
    rt.feed_press(CLAY_KEY_R);
    CHECK(rt.audio().voice_count() == 0);
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

TEST_CASE(
    "reaction: successful reload replaces rules and parse failure preserves") {
    Runtime rt(200, 200, 4);
    rt.reactions().load_text(kRules);
    CHECK_EQ(rt.reactions().rule_count(), 4);

    CHECK(rt.reactions().load_text(
        R"json({"rules":[{"name":"replacement","on":"input.key",
                         "match":{"value":"SPACE","kind":"press"},
                         "do":[{"effect":"flash"}]}]})json"));
    CHECK_EQ(rt.reactions().rule_count(), 1);

    CHECK(!rt.reactions().load_text("{ malformed"));
    CHECK_EQ(rt.reactions().rule_count(), 1);
}
