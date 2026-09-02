#include "action.hpp"

namespace clay {

static uint64_t binding_key(cl_key key, int mods) {
    return (uint64_t((unsigned)key) << 8) | (uint64_t(mods) & 0xff);
}

void ActionMap::bind(const std::string &action, cl_key key, cl_mods mods,
                     bool reversible) {
    if (key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT) return;
    ActionBinding b;
    b.action = action;
    b.key = key;
    b.mods = mods;
    b.consumed_hold = false;
    b.reversible = reversible;
    bindings_[binding_key(key, mods)] = std::move(b);
}

void ActionMap::bind_hold(const std::string &action, cl_key key, cl_mods mods) {
    if (key <= CLAY_KEY_NONE || key >= CLAY_KEY_COUNT) return;
    ActionBinding b;
    b.action = action;
    b.key = key;
    b.mods = mods;
    b.consumed_hold = true;
    bindings_[binding_key(key, mods)] = std::move(b);
}

bool ActionMap::has_key(cl_key key) const {
    return bindings_.count(binding_key(key, 0)) != 0;
}

const ActionBinding *ActionMap::find(cl_key key, cl_mods mods) const {
    auto it = bindings_.find(binding_key(key, mods));
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

        cl_mods mods = CLAY_MOD_NONE;
        cl_json_node *mods_node = cl_json_get_cstr(val, "mods");
        if (mods_node && mods_node->kind == CLAY_J_STR)
            mods = cl_mods_from_str(mods_node->s);

        cl_variant hold = cl_json_lookup(val, cl_str_c("hold"), nullptr);
        bool is_hold = cl_variant_truthy(hold);
        cl_variant rev = cl_json_lookup(val, cl_str_c("reversible"), nullptr);
        bool is_reversible = is_hold ? false : cl_variant_truthy(rev);
        if (is_hold) {
            bind_hold(action, key, mods);
        } else {
            bind(action, key, mods, is_reversible);
        }
    }
}

} // namespace clay