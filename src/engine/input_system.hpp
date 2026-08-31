#ifndef CLAY_ENGINE_INPUT_SYSTEM_HPP
#define CLAY_ENGINE_INPUT_SYSTEM_HPP

#include "action.hpp"

#include <clay/clay.h>

#include <vector>

namespace clay {

/* Turns raw cl_input_event* into logical Actions. Edge presses/releases of a
 * bound key become Actions (and then Commands, once per edge); hold bindings
 * are polled each frame instead so held movement never floods the command
 * log. Every Action carries the cursor position it fired at. */
class InputSystem {
  public:
    explicit InputSystem(const ActionMap &actions) : actions_(actions) {}

    /* One call per raw event; returns mapped actions (usually 0 or 1). */
    std::vector<Action> process(const cl_input_event &e) const;

    /* Hold-type bindings currently down, polled once per frame. */
    std::vector<Action> poll_holds(const cl_input_state &s) const;

  private:
    const ActionMap &actions_;
};

} // namespace clay

#endif /* CLAY_ENGINE_INPUT_SYSTEM_HPP */