#ifndef CLAY_ENGINE_ACTION_HPP
#define CLAY_ENGINE_ACTION_HPP

#include <clay/clay.h>

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

    /* Zero-fill so the struct is trivially valid (avoids missing-field
     * warnings and uninitialized memory in logs). */
    Action() : value(cl_variant_nil()) {}
};

/* Physical binding for one action, from JSON:
 *   { "actions": { "primary": {"key": "MOUSE_LEFT"}, "move_up": {"key": "W"} } }
 * A missing key leaves the action unbound (unreachable, but valid). */
struct ActionBinding {
    std::string action;
    cl_key key = CLAY_KEY_NONE;
    bool consumed_hold = false; /* fire on hold (motion-adjacent)          */
};

class ActionMap {
  public:
    void bind(const std::string &action, cl_key key);
    void bind_hold(const std::string &action, cl_key key);

    /* Nothing throws on a bad binding; unrecognized keys are dropped. */
    void bind_from_json(cl_json_node *root);

    /* primary query: does any binding use this key (for edge detection)? */
    bool has_key(cl_key key) const;
    const ActionBinding *find_by_key(cl_key key) const;

    /* all distinct action names, for command pre-warming */
    size_t active_count() const {
        return bindings_.size();
    }

  private:
    std::unordered_map<cl_key, ActionBinding> bindings_;
};

} // namespace clay

#endif /* CLAY_ENGINE_ACTION_HPP */