#ifndef CLAY_ENGINE_SYSTEMS_REACTION_HPP
#define CLAY_ENGINE_SYSTEMS_REACTION_HPP

#include "event.hpp"

#include <clay/clay.h>

#include <cstdint>
#include <string>
#include <vector>

namespace clay {

class Runtime;

/* A reaction rule is a pure piece of *data* that says "when this player
 * thing happens, do this to the world". Rules are loaded from reactions.json
 * and executed by ReactionEngine; adding one never requires a rebuild. This
 * is the engine's scriptable reactivity layer, minus a scripting language. */
class ReactionEngine {
  public:
    ReactionEngine();
    ~ReactionEngine();

    ReactionEngine(const ReactionEngine &) = delete;
    ReactionEngine &operator=(const ReactionEngine &) = delete;

    /* Load rules; JSON shape:
     *   { "rules": [
     *       { "name": "click ripples", "on": "input.key",
     *         "match": { "value": "MOUSE_LEFT" },
     *         "cooldown": 0.05,
     *         "do": [ { "effect": "ripple", "color": [0.9,0.6,0.3],
     *                  "radius": 36 } ] } ] }
     * `on` channel values: input.key / input.motion / input.wheel /
     * input.focus / action / command / world. `match.value` filters payload
     * (key or action name); `match.kind` filters "press"/"release".
     * Effects: spawn (species, x, y, life, color), ripple, flash, log,
     * kill_radius. Positioning: without x/y, cursor position is used. */
    void load_json(cl_json_node *root);
    void load_text(const std::string &text);

    void on_event(Runtime &rt, const Event &ev);

    size_t rule_count() const {
        return rules_.size();
    }
    uint64_t fired_count() const {
        return fired_;
    }
    void reset();

  private:
    enum class MatchKind { Any, Press, Release };

    struct Effect {
        enum class Type { Spawn, Ripple, Flash, Log, KillRadius };
        Type type = Type::Spawn;
        std::string species;
        bool use_cursor = true;
        double x = 0.0;
        double y = 0.0;
        double life = 3.0;
        double radius = 40.0;
        float color[3] = {1.0f, 1.0f, 1.0f};
        float alpha = 1.0f;
    };

    struct Rule {
        std::string name;
        std::string on_channel;
        std::string match_value;
        MatchKind match_kind = MatchKind::Any;
        double cooldown = 0.0;
        double last_fired = -1e300;
        std::vector<Effect> effects;
    };

    std::vector<Rule> rules_;
    uint64_t fired_ = 0;

    std::vector<uint8_t> arena_storage_;
    cl_arena arena_;

    bool rule_matches(Runtime &rt, const Rule &rule, const Event &ev) const;
    void apply_effect(Runtime &rt, const Rule &rule, const Effect &effect,
                      const Event &ev);
};

} // namespace clay

#endif /* CLAY_ENGINE_SYSTEMS_REACTION_HPP */