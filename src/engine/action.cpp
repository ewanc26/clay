#include "action.hpp"

namespace clay {

void ActionMap::bind(const std::string &action, cl_key key) {
    if (key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT) return;
    ActionBinding b;
    b.action = action;
    b.key = key;
    b.consumed_hold = false;
    bindings_[key] = std::move(b);
}

void ActionMap::bind_hold(const std::string &action, cl_key key) {
    if (key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT) return;
    ActionBinding b;
    b.action = action;
    b.key = key;
    b.consumed_hold = true;
    bindings_[key] = std::move(b);
}

bool ActionMap::has_key(cl_key key) const {
    return bindings_.count(key) != 0;
}

const ActionBinding *ActionMap::find_by_key(cl_key key) const {
    auto it = bindings_.find(key);
    return it == bindings_.end() ? nullptr : &it->second;
}

void ActionMap::bind_from_json(cl_json_node *root) {
    cl_json_node *acts = cl_json_get_cstr(root, "actions");
    if (acts == nullptr || acts->kind != CLAY_J_OBJ) return;
    for (size_t i = 0; i < acts->obj.n; i++) {
        const cl_json_pair &p = acts->obj.pairs[i];
        cl_json_node *val = p.val;
        if (val == nullptr || val->kind != CLAY_J_OBJ) continue;
        const char *name = p.key.data;
        std::string action(name, p.key.len);

        cl_json_node *key_node = cl_json_get_cstr(val, "key");
        if (key_node == nullptr || key_node->kind != CLAY_J_STR) continue;
        cl_key key = cl_key_from_str(key_node->s);
        if (key <= CLAY_KEY_NONE) continue;

        cl_variant hold = cl_json_lookup(val, cl_str_c("hold"), nullptr);
        bool is_hold = cl_variant_truthy(hold);
        if (is_hold) {
            bind_hold(action, key);
        } else {
            bind(action, key);
        }
    }
}

} // namespace clay