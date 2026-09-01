#include "systems/reaction.hpp"

#include "runtime.hpp"

#include <cmath>
#include <cstring>

namespace clay {

ReactionEngine::ReactionEngine()
    : arena_storage_(1u << 20) {
    cl_arena_init(&arena_, arena_storage_.data(), arena_storage_.size());
}

ReactionEngine::~ReactionEngine() = default;

namespace {

std::string node_str(cl_json_node *node) {
    if (node == nullptr || node->kind != CLAY_J_STR) return std::string();
    return std::string(node->s.data, node->s.len);
}

double node_num(cl_json_node *node, double fallback) {
    if (node == nullptr) return fallback;
    switch (node->kind) {
    case CLAY_J_I64: return (double)node->i;
    case CLAY_J_F64: return node->f;
    default: return fallback;
    }
}

} // namespace

void ReactionEngine::load_json(cl_json_node *root) {
    cl_json_node *rules = cl_json_get_cstr(root, "rules");
    if (rules == nullptr || rules->kind != CLAY_J_ARR) return;
    for (size_t i = 0; i < rules->arr.n; i++) {
        cl_json_node *r = rules->arr.items[i];
        if (r == nullptr || r->kind != CLAY_J_OBJ) continue;

        ReactionEngine::Rule rule;
        rule.name = node_str(cl_json_get_cstr(r, "name"));
        if (rule.name.empty()) rule.name = "untitled";
        rule.on_channel = node_str(cl_json_get_cstr(r, "on"));
        rule.cooldown = node_num(cl_json_get_cstr(r, "cooldown"), 0.0);

        cl_json_node *match = cl_json_get_cstr(r, "match");
        if (match != nullptr && match->kind == CLAY_J_OBJ) {
            rule.match_value = node_str(cl_json_get_cstr(match, "value"));
            std::string kind = node_str(cl_json_get_cstr(match, "kind"));
            if (kind == "press")
                rule.match_kind = MatchKind::Press;
            else if (kind == "release")
                rule.match_kind = MatchKind::Release;
        }

        cl_json_node *do_node = cl_json_get_cstr(r, "do");
        if (do_node == nullptr || do_node->kind != CLAY_J_ARR) continue;
        for (size_t j = 0; j < do_node->arr.n; j++) {
            cl_json_node *e = do_node->arr.items[j];
            if (e == nullptr || e->kind != CLAY_J_OBJ) continue;
            std::string effect = node_str(cl_json_get_cstr(e, "effect"));

            Effect out;
            if (effect == "spawn") {
                out.type = Effect::Type::Spawn;
                out.species = node_str(cl_json_get_cstr(e, "species"));
            } else if (effect == "ripple") {
                out.type = Effect::Type::Ripple;
            } else if (effect == "flash") {
                out.type = Effect::Type::Flash;
            } else if (effect == "log") {
                out.type = Effect::Type::Log;
            } else if (effect == "kill_radius") {
                out.type = Effect::Type::KillRadius;
            } else {
                continue;
            }

            cl_json_node *xn = cl_json_get_cstr(e, "x");
            cl_json_node *yn = cl_json_get_cstr(e, "y");
            if (xn != nullptr && yn != nullptr &&
                (xn->kind == CLAY_J_I64 || xn->kind == CLAY_J_F64) &&
                (yn->kind == CLAY_J_I64 || yn->kind == CLAY_J_F64)) {
                out.use_cursor = false;
                out.x = node_num(xn, 0.0);
                out.y = node_num(yn, 0.0);
            }

            out.life = node_num(cl_json_get_cstr(e, "life"), 3.0);
            out.radius = node_num(cl_json_get_cstr(e, "radius"), 40.0);
            out.alpha = (float)node_num(cl_json_get_cstr(e, "alpha"), 1.0);

            cl_json_node *color = cl_json_get_cstr(e, "color");
            if (color != nullptr && color->kind == CLAY_J_ARR &&
                color->arr.n >= 3) {
                out.color[0] = (float)node_num(color->arr.items[0], 1.0);
                out.color[1] = (float)node_num(color->arr.items[1], 1.0);
                out.color[2] = (float)node_num(color->arr.items[2], 1.0);
            }
            rule.effects.push_back(std::move(out));
        }
        if (!rule.effects.empty()) rules_.push_back(std::move(rule));
    }
}

void ReactionEngine::load_text(const std::string &text) {
    cl_json_node root;
    cl_err err = cl_json_parse(&root, &arena_, cl_str_c(text.c_str()));
    if (err != CLAY_OK) return;
    load_json(&root);
}

namespace {

const char *fallback_channel_name(cl_channel ch) {
    return cl_channel_name(ch);
}

} // namespace

bool ReactionEngine::rule_matches(Runtime &rt, const Rule &rule,
                                  const Event &ev) const {
    if (fallback_channel_name(ev.channel) == nullptr) return false;
    std::string chan = channel_name(ev.channel);
    if (chan != rule.on_channel) return false;

    cl_variant v = ev.value;

    if (rule.match_kind != MatchKind::Any) {
        /* Press/release semantics only make sense for key events; the input
         * state has already absorbed this frame's feed by the time the event
         * is on the bus, so down[key] tells us which edge this was. */
        if (chan != "input.key") return false;
        if (v.kind != CLAY_VAR_STR) return false;
        cl_key key = cl_key_from_str(v.s);
        bool press = rt.is_key_down(key);
        if (rule.match_kind == MatchKind::Press && !press) return false;
        if (rule.match_kind == MatchKind::Release && press) return false;
    }

    if (!rule.match_value.empty()) {
        if (v.kind != CLAY_VAR_STR) return false;
        if (v.s.len != rule.match_value.size() ||
            std::memcmp(v.s.data, rule.match_value.data(), v.s.len) != 0)
            return false;
    }
    return true;
}

void ReactionEngine::on_event(Runtime &rt, const Event &ev) {
    for (Rule &rule : rules_) {
        if (!rule_matches(rt, rule, ev)) continue;
        if (ev.time - rule.last_fired < rule.cooldown) continue;
        rule.last_fired = ev.time;
        fired_ += 1;
        for (const Effect &effect : rule.effects) {
            apply_effect(rt, rule, effect, ev);
        }
    }
}

void ReactionEngine::apply_effect(Runtime &rt, const Rule &rule,
                                  const Effect &effect, const Event &ev) {
    double x = effect.use_cursor ? rt.cursor_x() : effect.x;
    double y = effect.use_cursor ? rt.cursor_y() : effect.y;
    Color color{effect.color[0], effect.color[1], effect.color[2],
                effect.alpha};

    switch (effect.type) {
    case Effect::Type::Spawn:
        rt.spawn_species(effect.species, (float)x, (float)y, color,
                         (float)effect.life);
        break;
    case Effect::Type::Ripple:
        rt.spawn_ripple((float)x, (float)y, (float)effect.radius, color);
        break;
    case Effect::Type::Flash:
        rt.flash(color, 0.5);
        break;
    case Effect::Type::KillRadius:
        rt.kill_within((float)x, (float)y, (float)effect.radius);
        break;
    case Effect::Type::Log:
        rt.log_reaction(rule.name + " fired on " + ev.channel_str() +
                        " @ (" + std::to_string((int)x) + ", " +
                        std::to_string((int)y) + ")");
        break;
    }
}

void ReactionEngine::reset() {
    rules_.clear();
    fired_ = 0;
}

} // namespace clay