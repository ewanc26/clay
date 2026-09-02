#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "action.hpp"
#include "input_system.hpp"

#include <clay/clay.h>

#include <cstring>

using namespace clay;

TEST_CASE("action_map: json bindings") {
    const char *binding_json = R"json({
      "actions": {
        "primary": { "key": "MOUSE_LEFT" },
        "move_up": { "key": "W", "hold": true },
        "bogus": { "key": "NO_SUCH_KEY" }
      }
    })json";

    unsigned char buf[4096];
    cl_arena arena;
    cl_arena_init(&arena, buf, sizeof(buf));
    cl_json_node root;
    REQUIRE(cl_json_parse(&root, &arena, cl_str_c(binding_json)) == CLAY_OK);

    ActionMap m;
    m.bind_from_json(&root);
    const ActionBinding *mm = m.find_by_key(CLAY_KEY_MOUSE_LEFT);
    CHECK(mm != nullptr);
    const ActionBinding *w = m.find_by_key(CLAY_KEY_W);
    CHECK(w != nullptr);
    CHECK(m.has_key(CLAY_KEY_MOUSE_LEFT));
    CHECK(!m.has_key(CLAY_KEY_F5));
    CHECK(w->consumed_hold);
}

TEST_CASE("input_system: edges become actions") {
    ActionMap m;
    m.bind("primary", CLAY_KEY_MOUSE_LEFT);
    m.bind("exit", CLAY_KEY_ESCAPE);
    InputSystem is(m);

    cl_input_event press = cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_MOUSE_LEFT);
    press.x = 77.0;
    press.y = 88.0;
    std::vector<Action> acts = is.process(press);
    REQUIRE(acts.size() == 1);
    CHECK(acts[0].name == "primary");
    CHECK(acts[0].value.kind == CLAY_VAR_BOOL);
    CHECK(acts[0].value.b);
    CHECK(acts[0].x == doctest::Approx(77.0));

    cl_input_event release =
        cl_input_event_make(CLAY_IN_RELEASE, CLAY_KEY_MOUSE_LEFT);
    acts = is.process(release);
    REQUIRE(acts.size() == 1);
    CHECK(acts[0].value.b == false);

    /* Unbound keys map to nothing. */
    cl_input_event motion = cl_input_event_make(CLAY_IN_MOTION, CLAY_KEY_NONE);
    CHECK(is.process(motion).empty());
}

TEST_CASE("input_system: hold bindings poll, never flood") {
    ActionMap m;
    m.bind_hold("move_up", CLAY_KEY_W);
    InputSystem is(m);

    cl_input_event press = cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_W);
    CHECK(is.process(press).empty()); /* hold bindings ignore edges */

    cl_input_state s;
    std::memset(&s, 0, sizeof(s));
    cl_input_state_begin(&s, 1, 0.0);
    cl_input_state_feed(&s, &press);
    cl_input_state_begin(&s, 2, 0.0);

    std::vector<Action> holds = is.poll_holds(s);
    REQUIRE(holds.size() == 1);
    CHECK(holds[0].name == "move_up");
}

TEST_CASE("action_map: modifier bindings are distinct from key-only") {
    ActionMap m;
    m.bind("undo", CLAY_KEY_Z, CLAY_MOD_CTRL);
    m.bind("redo", CLAY_KEY_Y, CLAY_MOD_CTRL);

    /* Ctrl+Z finds the undo binding, not the plain-Z binding. */
    const ActionBinding *cz = m.find(CLAY_KEY_Z, CLAY_MOD_CTRL);
    REQUIRE(cz != nullptr);
    CHECK(cz->action == "undo");

    /* Plain Z (no Ctrl) does not match — there is no plain-Z binding. */
    CHECK(m.find(CLAY_KEY_Z, CLAY_MOD_NONE) == nullptr);

    /* Ctrl+Y finds redo. */
    const ActionBinding *cy = m.find(CLAY_KEY_Y, CLAY_MOD_CTRL);
    REQUIRE(cy != nullptr);
    CHECK(cy->action == "redo");
}

TEST_CASE("action_map: reversible flag propagates to bindings") {
    ActionMap m;
    m.bind("spawn", CLAY_KEY_MOUSE_LEFT, CLAY_MOD_NONE, true);
    const ActionBinding *b = m.find(CLAY_KEY_MOUSE_LEFT, CLAY_MOD_NONE);
    REQUIRE(b != nullptr);
    CHECK(b->reversible);
}