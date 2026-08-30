#ifndef CLAY_CORE_INPUT_H
#define CLAY_CORE_INPUT_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

/* --------------------------------------------------------------------- keys */

/* Stable enum; order encodes the on-disk .clayrec format, so never renumber.
 * Keyboard, mouse buttons, wheel-as-key, then a compact gamepad set. */
typedef enum cl_key {
    CLAY_KEY_NONE = 0,

    CLAY_KEY_ESCAPE,
    CLAY_KEY_ENTER,
    CLAY_KEY_TAB,
    CLAY_KEY_SPACE,
    CLAY_KEY_BACKSPACE,
    CLAY_KEY_DELETE,
    CLAY_KEY_HOME,
    CLAY_KEY_END,
    CLAY_KEY_PAGE_UP,
    CLAY_KEY_PAGE_DOWN,
    CLAY_KEY_ARROW_UP,
    CLAY_KEY_ARROW_DOWN,
    CLAY_KEY_ARROW_LEFT,
    CLAY_KEY_ARROW_RIGHT,

    CLAY_KEY_A, CLAY_KEY_B, CLAY_KEY_C, CLAY_KEY_D, CLAY_KEY_E, CLAY_KEY_F,
    CLAY_KEY_G, CLAY_KEY_H, CLAY_KEY_I, CLAY_KEY_J, CLAY_KEY_K, CLAY_KEY_L,
    CLAY_KEY_M, CLAY_KEY_N, CLAY_KEY_O, CLAY_KEY_P, CLAY_KEY_Q, CLAY_KEY_R,
    CLAY_KEY_S, CLAY_KEY_T, CLAY_KEY_U, CLAY_KEY_V, CLAY_KEY_W, CLAY_KEY_X,
    CLAY_KEY_Y, CLAY_KEY_Z,

    CLAY_KEY_0, CLAY_KEY_1, CLAY_KEY_2, CLAY_KEY_3, CLAY_KEY_4, CLAY_KEY_5,
    CLAY_KEY_6, CLAY_KEY_7, CLAY_KEY_8, CLAY_KEY_9,

    CLAY_KEY_F1, CLAY_KEY_F2, CLAY_KEY_F3, CLAY_KEY_F4, CLAY_KEY_F5,
    CLAY_KEY_F6, CLAY_KEY_F7, CLAY_KEY_F8, CLAY_KEY_F9, CLAY_KEY_F10,
    CLAY_KEY_F11, CLAY_KEY_F12,

    CLAY_KEY_QUOTE, CLAY_KEY_COMMA, CLAY_KEY_PERIOD, CLAY_KEY_SLASH,
    CLAY_KEY_SEMICOLON, CLAY_KEY_MINUS, CLAY_KEY_EQUALS,
    CLAY_KEY_BRACKET_LEFT, CLAY_KEY_BRACKET_RIGHT, CLAY_KEY_BACKSLASH,
    CLAY_KEY_GRAVE,

    CLAY_KEY_MOUSE_LEFT,
    CLAY_KEY_MOUSE_RIGHT,
    CLAY_KEY_MOUSE_MIDDLE,
    CLAY_KEY_MOUSE_X1,
    CLAY_KEY_MOUSE_X2,

    CLAY_KEY_WHEEL_UP,
    CLAY_KEY_WHEEL_DOWN,

    CLAY_KEY_GP_A,
    CLAY_KEY_GP_B,
    CLAY_KEY_GP_X,
    CLAY_KEY_GP_Y,
    CLAY_KEY_GP_LB,
    CLAY_KEY_GP_RB,
    CLAY_KEY_GP_BACK,
    CLAY_KEY_GP_START,
    CLAY_KEY_GP_LEFT_STICK,
    CLAY_KEY_GP_RIGHT_STICK,
    CLAY_KEY_GP_DPAD_UP,
    CLAY_KEY_GP_DPAD_DOWN,
    CLAY_KEY_GP_DPAD_LEFT,
    CLAY_KEY_GP_DPAD_RIGHT,

    CLAY_KEY_LEFT_SHIFT,
    CLAY_KEY_LEFT_CTRL,
    CLAY_KEY_LEFT_ALT,
    CLAY_KEY_LEFT_META,

    CLAY_KEY_COUNT
} cl_key;

typedef enum cl_mods {
    CLAY_MOD_NONE = 0,
    CLAY_MOD_SHIFT = 1 << 0,
    CLAY_MOD_CTRL = 1 << 1,
    CLAY_MOD_ALT = 1 << 2,
    CLAY_MOD_META = 1 << 3
} cl_mods;

/* Human names for bindings/serialization: "SPACE", "MOUSE_LEFT", "GP_A". */
const char *cl_key_name(cl_key key);
cl_key cl_key_from_str(cl_str s);

/* ------------------------------------------------------------------ events */

typedef enum cl_input_kind {
    CLAY_IN_PRESS = 1,
    CLAY_IN_RELEASE = 2,
    CLAY_IN_MOTION = 3,
    CLAY_IN_WHEEL = 4,
    CLAY_IN_FOCUS = 5
} cl_input_kind;

typedef struct cl_input_event {
    uint32_t frame;   /* frame the event was generated in                   */
    double time;      /* cl_time_seconds() at generation                    */
    cl_input_kind type;
    cl_key key;
    int mods;
    double x, y;      /* cursor position (canvas pixels, top-left origin)   */
    double dx, dy;    /* cursor delta since last motion                     */
    int wheel;        /* wheel clicks, +up -down                            */
    bool focus;       /* meaningful for CLAY_IN_FOCUS                       */
} cl_input_event;

/* consumable value for synthetic drives (replay, autotests) */
static inline cl_input_event cl_input_event_make(cl_input_kind type, cl_key key) {
    cl_input_event e;
    e.frame = 0;
    e.time = 0.0;
    e.type = type;
    e.key = key;
    e.mods = 0;
    e.x = 0.0;
    e.y = 0.0;
    e.dx = 0.0;
    e.dy = 0.0;
    e.wheel = 0;
    e.focus = false;
    return e;
}

/* ------------------------------------------------------------------- state */

#define CLAY_WHEEL_ACCUM_FACTOR 20.0

typedef struct cl_input_state {
    uint32_t frame;
    double time;
    bool down[CLAY_KEY_COUNT];
    uint32_t pressed_frame[CLAY_KEY_COUNT];
    uint32_t released_frame[CLAY_KEY_COUNT];
    double press_x[CLAY_KEY_COUNT];
    double press_y[CLAY_KEY_COUNT];
    double cursor_x, cursor_y;
    double dx, dy;
    double wheel_accum;
    bool focus;
} cl_input_state;

/* Call at the start of each frame before feeding any events: bumps frame,
 * clears per-frame deltas, keeps held keys. */
void cl_input_state_begin(cl_input_state *s, uint32_t frame, double time);

/* Returns true for PRESS events that changed state (level-triggered: repeat
 * presses while already down are ignored). */
bool cl_input_state_feed(cl_input_state *s, const cl_input_event *e);

bool cl_input_down(const cl_input_state *s, cl_key key);
bool cl_input_just_pressed(const cl_input_state *s, cl_key key);
bool cl_input_just_released(const cl_input_state *s, cl_key key);

/* Cursor position recorded when `key` was pressed this frame. */
bool cl_input_press_point(const cl_input_state *s, cl_key key, double *x,
                          double *y);

bool cl_input_any_key_down(const cl_input_state *s);

#endif /* CLAY_CORE_INPUT_H */