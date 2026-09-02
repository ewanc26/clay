#include "test_c.h"

#include <clay/clay.h>

#include <string.h>

static int test_input(void) {
    cl_input_state s;
    memset(&s, 0, sizeof(s));
    cl_input_state_begin(&s, 1, 0.0);

    /* Level-triggered: a press while already down does nothing. */
    cl_input_event press = cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_SPACE);
    press.x = 12.0;
    press.y = 34.0;
    CHECK(cl_input_state_feed(&s, &press));
    CHECK(cl_input_down(&s, CLAY_KEY_SPACE));
    CHECK(cl_input_just_pressed(&s, CLAY_KEY_SPACE));

    CHECK(!cl_input_state_feed(&s, &press)); /* repeat press is a no-op */
    CHECK(cl_input_down(&s, CLAY_KEY_SPACE));

    double px, py;
    CHECK(cl_input_press_point(&s, CLAY_KEY_SPACE, &px, &py));
    CHECK_EQ_DBL(px, 12.0, 1e-9);
    CHECK_EQ_DBL(py, 34.0, 1e-9);

    cl_input_state_begin(&s, 2, 0.0);
    CHECK(cl_input_down(&s, CLAY_KEY_SPACE));       /* key stays held */
    CHECK(!cl_input_just_pressed(&s, CLAY_KEY_SPACE));

    cl_input_event release =
        cl_input_event_make(CLAY_IN_RELEASE, CLAY_KEY_SPACE);
    CHECK(cl_input_state_feed(&s, &release));
    CHECK(!cl_input_down(&s, CLAY_KEY_SPACE));
    CHECK(cl_input_just_released(&s, CLAY_KEY_SPACE));

    cl_input_event focus_lost = cl_input_event_make(CLAY_IN_FOCUS, CLAY_KEY_NONE);
    focus_lost.focus = false;
    cl_input_state_begin(&s, 3, 0.0);
    CHECK(cl_input_state_feed(&s, &press));
    CHECK(cl_input_down(&s, CLAY_KEY_SPACE));
    CHECK(!cl_input_state_feed(&s, &focus_lost));
    CHECK(!cl_input_down(&s, CLAY_KEY_SPACE));
    CHECK(cl_input_just_released(&s, CLAY_KEY_SPACE));

    /* Motion moves the cursor and updates per-frame deltas. */
    cl_input_state_begin(&s, 4, 0.0);
    cl_input_event motion = cl_input_event_make(CLAY_IN_MOTION, CLAY_KEY_NONE);
    motion.x = 100.0;
    motion.y = 50.0;
    CHECK(!cl_input_state_feed(&s, &motion)); /* motion is not an edge */
    CHECK_EQ_DBL(s.cursor_x, 100.0, 1e-9);
    CHECK_EQ_DBL(s.cursor_y, 50.0, 1e-9);
    cl_input_state_begin(&s, 5, 0.0);
    CHECK_EQ_DBL(s.dx, 0.0, 1e-9); /* deltas cleared each frame */

    /* Wheel accumulates. */
    cl_input_event wheel = cl_input_event_make(CLAY_IN_WHEEL, CLAY_KEY_NONE);
    wheel.wheel = 1;
    wheel.x = 210.0;
    wheel.y = 90.0;
    CHECK(!cl_input_state_feed(&s, &wheel)); /* wheel feeds accumulate */
    CHECK_EQ_DBL(s.cursor_x, 210.0, 1e-9);
    CHECK_EQ_DBL(s.cursor_y, 90.0, 1e-9);
    wheel.wheel = -1;
    CHECK(!cl_input_state_feed(&s, &wheel));

    CHECK(cl_key_from_str(cl_str_c("SPACE")) == CLAY_KEY_SPACE);
    CHECK(cl_key_from_str(cl_str_c("MOUSE_LEFT")) == CLAY_KEY_MOUSE_LEFT);
    CHECK(cl_key_from_str(cl_str_c("GP_A")) == CLAY_KEY_GP_A);
    CHECK(cl_key_from_str(cl_str_c("DOES_NOT_EXIST")) == CLAY_KEY_NONE);
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_input)
