#include "input_system.hpp"

namespace clay {

std::vector<Action> InputSystem::process(const cl_input_event &e) const {
    std::vector<Action> out;
    if (e.type != CLAY_IN_PRESS && e.type != CLAY_IN_RELEASE) return out;
    const ActionBinding *b = actions_.find_by_key(e.key);
    if (b == nullptr || b->consumed_hold) return out;
    Action a;
    a.name = b->action;
    a.value = cl_variant_bool(e.type == CLAY_IN_PRESS);
    a.x = e.x;
    a.y = e.y;
    a.frame = e.frame;
    a.time = e.time;
    out.push_back(std::move(a));
    return out;
}

std::vector<Action> InputSystem::poll_holds(const cl_input_state &s) const {
    std::vector<Action> out;
    for (cl_key key = (cl_key)1; key < CLAY_KEY_COUNT; key = (cl_key)(key + 1)) {
        if (!cl_input_down(&s, key)) continue;
        const ActionBinding *b = actions_.find_by_key(key);
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