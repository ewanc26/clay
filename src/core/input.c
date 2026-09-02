#include "input.h"

static const char *const k_key_names[CLAY_KEY_COUNT] = {
    "NONE",
    "ESCAPE", "ENTER", "TAB", "SPACE", "BACKSPACE", "DELETE", "HOME", "END",
    "PAGE_UP", "PAGE_DOWN", "ARROW_UP", "ARROW_DOWN", "ARROW_LEFT",
    "ARROW_RIGHT",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "QUOTE", "COMMA", "PERIOD", "SLASH", "SEMICOLON", "MINUS", "EQUALS",
    "BRACKET_LEFT", "BRACKET_RIGHT", "BACKSLASH", "GRAVE",
    "MOUSE_LEFT", "MOUSE_RIGHT", "MOUSE_MIDDLE", "MOUSE_X1", "MOUSE_X2",
    "WHEEL_UP", "WHEEL_DOWN",
    "GP_A", "GP_B", "GP_X", "GP_Y", "GP_LB", "GP_RB", "GP_BACK", "GP_START",
    "GP_LEFT_STICK", "GP_RIGHT_STICK", "GP_DPAD_UP", "GP_DPAD_DOWN",
    "GP_DPAD_LEFT", "GP_DPAD_RIGHT",
    "LEFT_SHIFT", "LEFT_CTRL", "LEFT_ALT", "LEFT_META",
};

_Static_assert((int)CLAY_KEY_COUNT ==
                   (int)(sizeof(k_key_names) / sizeof(k_key_names[0])),
               "key table length matches enum");

const char *cl_key_name(cl_key key) {
    if (key >= CLAY_KEY_COUNT || key < 0) return "?";
    return k_key_names[(int)key];
}

cl_key cl_key_from_str(cl_str s) {
    for (int i = 0; i < CLAY_KEY_COUNT; i++) {
        if (cl_str_eq(s, cl_str_c(k_key_names[i]))) return (cl_key)i;
    }
    return CLAY_KEY_NONE;
}

void cl_input_state_begin(cl_input_state *s, uint32_t frame, double time) {
    s->frame = frame;
    s->time = time;
    s->dx = 0.0;
    s->dy = 0.0;
    s->wheel_accum = 0.0;
}

bool cl_input_state_feed(cl_input_state *s, const cl_input_event *e) {
    switch (e->type) {
    case CLAY_IN_PRESS:
        if (e->key >= CLAY_KEY_COUNT || e->key < 0) return false;
        if (s->down[e->key]) return false; /* repeat press, not a new edge */
        s->down[e->key] = true;
        s->pressed_frame[e->key] = s->frame;
        s->press_x[e->key] = e->x;
        s->press_y[e->key] = e->y;
        return true;
    case CLAY_IN_RELEASE:
        if (e->key >= CLAY_KEY_COUNT || e->key < 0) return false;
        if (!s->down[e->key]) return false;
        s->down[e->key] = false;
        s->released_frame[e->key] = s->frame;
        return true;
    case CLAY_IN_MOTION:
        s->dx += e->dx;
        s->dy += e->dy;
        s->cursor_x = e->x;
        s->cursor_y = e->y;
        return false;
    case CLAY_IN_WHEEL:
        s->wheel_accum += (double)e->wheel * CLAY_WHEEL_ACCUM_FACTOR;
        return false;
    case CLAY_IN_FOCUS:
        s->focus = e->focus;
        if (!e->focus) {
            /* A window can lose focus without delivering matching releases.
             * Clear held inputs so a host cannot leave movement or actions
             * latched after returning from another application. */
            for (int key = 1; key < CLAY_KEY_COUNT; key++) {
                if (!s->down[key]) continue;
                s->down[key] = false;
                s->released_frame[key] = s->frame;
            }
        }
        return false;
    }
    return false;
}

bool cl_input_down(const cl_input_state *s, cl_key key) {
    return key >= 0 && key < CLAY_KEY_COUNT && s->down[key];
}

bool cl_input_just_pressed(const cl_input_state *s, cl_key key) {
    return key >= 0 && key < CLAY_KEY_COUNT && s->pressed_frame[key] == s->frame;
}

bool cl_input_just_released(const cl_input_state *s, cl_key key) {
    return key >= 0 && key < CLAY_KEY_COUNT && s->released_frame[key] == s->frame;
}

bool cl_input_press_point(const cl_input_state *s, cl_key key, double *x,
                          double *y) {
    if (!cl_input_just_pressed(s, key)) return false;
    if (x) *x = s->press_x[key];
    if (y) *y = s->press_y[key];
    return true;
}

bool cl_input_any_key_down(const cl_input_state *s) {
    for (int i = 1; i < CLAY_KEY_COUNT; i++) {
        if (s->down[i]) return true;
    }
    return false;
}
