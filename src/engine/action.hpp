#ifndef CLAY_ENGINE_ACTION_HPP
#define CLAY_ENGINE_ACTION_HPP

#include <clay/clay.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace clay {

/* A logical player action: the *meaning* of an input, decoupled from the
 * physical binding. Action names come from config (JSON input bindings), so
 * nothing in game code hard-codes a key. */
struct Action {
    std::string name;
    cl_variant value; /* optional payload (axis value, etc.)     */
    double x = 0;     /* cursor position when the action fired   */
    double y = 0;
    uint32_t frame = 0;
    double time = 0;
    bool reversible = false; /* push onto undo stack when recorded */

    Action() : value(cl_variant_nil()) {}
};

/* Physical binding for one action, from JSON:
 *   { "actions": { "primary": {"key": "MOUSE_LEFT"}, "move_up": {"key": "W"} } }
 * A missing key leaves the action unbound (unreachable, but valid). */
struct ActionBinding {
    std::string action;
    cl_key key = CLAY_KEY_NONE;
    cl_mods mods = CLAY_MOD_NONE;
    bool consumed_hold = false; /* fire on hold (motion-adjacent)          */
    bool reversible = false;   /* undo pushes this command on the undo stack */
};

class ActionMap {
  public:
    void clear() { bindings_.clear(); }
    void bind(const std::string &action, cl_key key,
              cl_mods mods = CLAY_MOD_NONE, bool reversible = false);
    void bind_hold(const std::string &action, cl_key key,
                   cl_mods mods = CLAY_MOD_NONE);

    /* Nothing throws on a bad binding; unrecognized keys are dropped. */
    void bind_from_json(cl_json_node *root);

    /* primary query: does any binding use this key+mods combo? */
    bool has_key(cl_key key) const;
    const ActionBinding *find(cl_key key, cl_mods mods) const;
    const ActionBinding *find_by_key(cl_key key) const {
        return find(key, CLAY_MOD_NONE);
    }

    /* all distinct action names, for command pre-warming */
    size_t active_count() const {
        return bindings_.size();
    }

  private:
    /* Keyed by (key << 8) | mods so a single key can have distinct
     * bindings for different modifier combos (e.g. Z and Ctrl+Z). */
    std::unordered_map<uint64_t, ActionBinding> bindings_;
};

} // namespace clay

#endif /* CLAY_ENGINE_ACTION_HPP */
