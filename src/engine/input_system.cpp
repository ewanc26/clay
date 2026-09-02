#include "input_system.hpp"

namespace clay {

std::vector<Action> InputSystem::process(const cl_input_event &e) const {
    std::vector<Action> out;
    if (e.type != CLAY_IN_PRESS && e.type != CLAY_IN_RELEASE) return out;
    const ActionBinding *b = actions_.find(e.key, (cl_mods)e.mods);
    if (b == nullptr || b->consumed_hold) return out;
    Action a;
    a.name = b->action;
    a.value = cl_variant_bool(e.type == CLAY_IN_PRESS);
    a.reversible = b->reversible;
    a.x = e.x;
    a.y = e.y;
    a.frame = e.frame;
    a.time = e.time;
    out.push_back(std::move(a));
    return out;
}

std::vector<Action> InputSystem::poll_holds(const cl_input_state &s) const {
    std::vector<Action> out;
    /* Compute current modifier state from held modifier keys. */
    cl_mods cur_mods = CLAY_MOD_NONE;
    if (cl_input_down(&s, CLAY_KEY_LEFT_SHIFT))
        cur_mods = (cl_mods)(cur_mods | CLAY_MOD_SHIFT);
    if (cl_input_down(&s, CLAY_KEY_LEFT_CTRL))
        cur_mods = (cl_mods)(cur_mods | CLAY_MOD_CTRL);
    if (cl_input_down(&s, CLAY_KEY_LEFT_ALT))
        cur_mods = (cl_mods)(cur_mods | CLAY_MOD_ALT);
    if (cl_input_down(&s, CLAY_KEY_LEFT_META))
        cur_mods = (cl_mods)(cur_mods | CLAY_MOD_META);

    for (cl_key key = (cl_key)1; key < CLAY_KEY_COUNT; key = (cl_key)(key + 1)) {
        if (!cl_input_down(&s, key)) continue;
        /* Skip modifier keys themselves. */
        if (key == CLAY_KEY_LEFT_SHIFT || key == CLAY_KEY_LEFT_CTRL ||
            key == CLAY_KEY_LEFT_ALT || key == CLAY_KEY_LEFT_META)
            continue;
        /* Try exact modifier match first, then fall back to no-mods. */
        const ActionBinding *b = actions_.find(key, cur_mods);
        if (b == nullptr) b = actions_.find(key, CLAY_MOD_NONE);
        if (b == nullptr || !b->consumed_hold) continue;
        Action a;
        a.name = b->action;
        a.value = cl_variant_f64(1.0); /* held: magnitude placeholder */
        a.x = s.cursor_x;
        a.y = s.cursor_y;
        a.frame = s.frame;
        a.time = s.time;
        out.push_back(std::move(a));
    }
    return out;
}

} // namespace clay