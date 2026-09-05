#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "runtime.hpp"
#include "systems/builtin.hpp"

#include <array>
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
            cl_input_event e =
                cl_input_event_make(CLAY_IN_WHEEL, CLAY_KEY_NONE);
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

TEST_CASE("runtime: resize reports invalid dimensions") {
    Runtime rt(32, 24, 10);
    CHECK(!rt.resize(0, 24));
    CHECK(!rt.resize(32, -1));
    CHECK(rt.width() == 32);
    CHECK(rt.height() == 24);
    CHECK(rt.resize(48, 20));
    CHECK(rt.width() == 48);
    CHECK(rt.height() == 20);
    CHECK(rt.framebuffer().pixels.size() == 48u * 20u);
}

TEST_CASE("runtime: scenes without resolution preserve host dimensions") {
    Runtime rt(32, 24, 10);
    REQUIRE(rt.load_scene("{\"version\":1,\"scene\":[]}"));
    CHECK(rt.width() == 32);
    CHECK(rt.height() == 24);
    CHECK(rt.has_scene());
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
    CHECK(replay.reactions().fired_count() ==
          original.reactions().fired_count());
    CHECK(fnv1a64(replay.framebuffer()) == fnv1a64(original.framebuffer()));
}

TEST_CASE("runtime: replay applies input on its recorded frame") {
    Runtime original(64, 64, 17);
    original.begin_frame(1.0 / 60.0);
    original.feed_press(CLAY_KEY_SPACE);
    original.update(original.sim_dt());
    original.render();
    original.begin_frame(1.0 / 60.0);
    original.feed_release(CLAY_KEY_SPACE);
    original.update(original.sim_dt());
    original.render();

    const char *path = "clay_test_runtime_input_timing.clayrec";
    REQUIRE(cl_input_log_save(&original.input_log(), path) == CLAY_OK);

    Runtime replay(64, 64, 17);
    REQUIRE(cl_input_log_load(&replay.input_log(), path) == CLAY_OK);
    replay.set_replaying(true);
    replay.begin_frame(1.0 / 60.0);
    CHECK(replay.is_key_down(CLAY_KEY_SPACE));
    CHECK(cl_input_just_pressed(&replay.input_state(), CLAY_KEY_SPACE));
    replay.update(replay.sim_dt());
    replay.render();
    replay.begin_frame(1.0 / 60.0);
    CHECK(!replay.is_key_down(CLAY_KEY_SPACE));
    CHECK(cl_input_just_released(&replay.input_state(), CLAY_KEY_SPACE));
}

TEST_CASE("replayer: stale frames do not block later events") {
    Runtime source(32, 32, 3);
    cl_input_event stale = cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_A);
    stale.frame = 0;
    cl_input_log_append(&source.input_log(), &stale);
    cl_input_event current = cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_B);
    current.frame = 2;
    cl_input_log_append(&source.input_log(), &current);

    Runtime replay(32, 32, 3);
    replay.input_log() = source.input_log();
    replay.set_replaying(true);
    replay.begin_frame(1.0 / 60.0);
    CHECK(!replay.is_key_down(CLAY_KEY_B));
    replay.begin_frame(1.0 / 60.0);
    CHECK(replay.is_key_down(CLAY_KEY_B));
}

TEST_CASE("runtime: rejects unsafe framebuffer dimensions before allocation") {
    CHECK_THROWS_AS(clay::Runtime(8193, 8193, 1), std::invalid_argument);
    CHECK_THROWS_AS(clay::Runtime(0, 10, 1), std::invalid_argument);
}

TEST_CASE("runtime: a custom render system replaces the default draw pass") {
    class BlankRenderSystem final : public RenderSystem {
      public:
        int calls = 0;
        void render(Runtime &, IRenderer &) override {
            calls++;
        }
    };

    Runtime rt(64, 64, 99);
    BlankRenderSystem blank;
    rt.set_render_system(&blank);

    rt.begin_frame(1.0 / 60.0);
    rt.render();
    (void)rt.update(0.0);

    CHECK(blank.calls == 1);
    CHECK(fnv1a64(rt.framebuffer()) ==
          fnv1a64(Runtime(64, 64, 99).framebuffer()));
}

TEST_CASE("runtime: unloading a scene restores the host render system") {
    class BlankRenderSystem final : public RenderSystem {
      public:
        void render(Runtime &, IRenderer &) override {}
    };

    Runtime rt(64, 64, 99);
    BlankRenderSystem blank;
    rt.set_render_system(&blank);
    REQUIRE(rt.load_scene("{\"version\":1,\"scene\":[]}"));
    CHECK(rt.render_system() != &blank);
    rt.unload_scene();
    CHECK(rt.render_system() == &blank);
}

TEST_CASE("runtime: replacing a scene tracks a host renderer override") {
    class BlankRenderSystem final : public RenderSystem {
      public:
        void render(Runtime &, IRenderer &) override {}
    };

    Runtime rt(64, 64, 99);
    BlankRenderSystem first;
    BlankRenderSystem second;
    rt.set_render_system(&first);
    REQUIRE(rt.load_scene("{\"version\":1,\"scene\":[]}"));
    rt.set_render_system(&second);
    REQUIRE(rt.load_scene("{\"version\":1,\"scene\":[]}"));
    rt.unload_scene();
    CHECK(rt.render_system() == &second);
}

TEST_CASE("runtime: scene graph resolves parent/child world transforms") {
    Runtime rt(320, 240, 42);
    rt.systems().add(std::make_unique<SceneGraphSystem>());

    Entity parent =
        rt.spawn_species("sculpture", 100, 100, {0.5f, 0.4f, 0.3f, 1.0f}, 1e9f);
    /* Move the parent to a known local position. */
    rt.world().storage<Transform2D>().set(parent, {100, 100, 0, 1});

    Entity child =
        rt.spawn_species("animal", 110, 100, {0.7f, 0.9f, 0.6f, 1.0f}, 300.0f);
    /* Child is offset +10 x from parent in local space. */
    rt.world().storage<Transform2D>().set(child, {10, 0, 0, 1});
    rt.world().storage<Parent>().set(child, {parent});

    rt.begin_frame(1.0 / 60.0);
    rt.update(rt.sim_dt());
    /* SceneGraphSystem runs in update(), resolving world transforms. */
    WorldTransform2D *wt_child =
        rt.world().storage<WorldTransform2D>().find(child);
    REQUIRE(wt_child != nullptr);
    CHECK(wt_child->x == doctest::Approx(110.0f)); /* 100 + 10 */
    CHECK(wt_child->y == doctest::Approx(100.0f));

    WorldTransform2D *wt_parent =
        rt.world().storage<WorldTransform2D>().find(parent);
    REQUIRE(wt_parent != nullptr);
    CHECK(wt_parent->x == doctest::Approx(100.0f));
    CHECK(wt_parent->y == doctest::Approx(100.0f));
}

TEST_CASE("runtime: audio source follows resolved entity transform") {
    Runtime rt(320, 240, 42);
    rt.systems().add(std::make_unique<SceneGraphSystem>());
    rt.systems().add(std::make_unique<AudioSourceSystem>());

    const auto clip = rt.audio().add_clip(AudioClip{48000, 1, {1.0F}});
    REQUIRE(clip.has_value());
    const auto voice = rt.audio().play(*clip, AudioBus::Sfx, true);
    REQUIRE(voice.has_value());

    const Entity source = rt.world().create();
    rt.world().storage<Transform2D>().set(source, {5.0F, 0.0F, 0.0F, 1.0F});
    rt.world().storage<AudioSource2D>().set(source, {*voice, 10.0F, true});

    rt.begin_frame(1.0 / 60.0);
    rt.update(rt.sim_dt());
    std::array<float, 2> output{};
    REQUIRE(rt.audio().mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.25F));
    CHECK(output[1] == doctest::Approx(0.5F));

    rt.world().storage<Transform2D>().set(source, {10.0F, 0.0F, 0.0F, 1.0F});
    rt.begin_frame(1.0 / 60.0);
    rt.update(rt.sim_dt());
    output = {};
    REQUIRE(rt.audio().mix_stereo(output));
    CHECK(output[0] == doctest::Approx(0.0F));
    CHECK(output[1] == doctest::Approx(0.0F));
}

TEST_CASE("runtime: moving parent moves all descendants") {
    Runtime rt(320, 240, 42);
    rt.systems().add(std::make_unique<SceneGraphSystem>());

    Entity parent =
        rt.spawn_species("sculpture", 50, 50, {0.5f, 0.4f, 0.3f, 1.0f}, 1e9f);
    Entity child =
        rt.spawn_species("animal", 50, 50, {0.7f, 0.9f, 0.6f, 1.0f}, 300.0f);
    rt.world().storage<Parent>().set(child, {parent});
    rt.world().storage<Transform2D>().set(child, {10, 0, 0, 1});

    rt.begin_frame(1.0 / 60.0);
    rt.update(rt.sim_dt());
    float first_x = rt.world().storage<WorldTransform2D>().find(child)->x;
    CHECK(first_x == doctest::Approx(60.0f)); /* 50 + 10 */

    /* Move the parent by 20 in x; child should follow. */
    rt.world().storage<Transform2D>().set(parent, {70, 50, 0, 1});
    rt.begin_frame(1.0 / 60.0);
    rt.update(rt.sim_dt());
    float second_x = rt.world().storage<WorldTransform2D>().find(child)->x;
    CHECK(second_x == doctest::Approx(80.0f)); /* 70 + 10 */
}

TEST_CASE("runtime: scene graph with rotation and scale") {
    Runtime rt(320, 240, 42);
    rt.systems().add(std::make_unique<SceneGraphSystem>());

    Entity parent =
        rt.spawn_species("sculpture", 0, 0, {0.5f, 0.4f, 0.3f, 1.0f}, 1e9f);
    /* Parent at origin, rotated 90 degrees, scaled 2x. */
    rt.world().storage<Transform2D>().set(parent,
                                          {0, 0, (float)M_PI / 2.0f, 2.0f});

    /* Child at local (10, 0). After 90-deg CCW rotation and 2x scale,
     * child should be at approximately (-0, 20) = (0, 20). */
    Entity child =
        rt.spawn_species("animal", 0, 0, {0.7f, 0.9f, 0.6f, 1.0f}, 300.0f);
    rt.world().storage<Parent>().set(child, {parent});
    rt.world().storage<Transform2D>().set(child, {10, 0, 0, 1});

    rt.begin_frame(1.0 / 60.0);
    rt.update(rt.sim_dt());

    WorldTransform2D *wt = rt.world().storage<WorldTransform2D>().find(child);
    REQUIRE(wt != nullptr);
    CHECK(wt->x == doctest::Approx(0.0f).epsilon(0.01f));
    CHECK(wt->y == doctest::Approx(20.0f).epsilon(0.01f));
}
