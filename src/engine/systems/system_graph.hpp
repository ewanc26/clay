#ifndef CLAY_ENGINE_SYSTEMS_SYSTEM_GRAPH_HPP
#define CLAY_ENGINE_SYSTEMS_SYSTEM_GRAPH_HPP

#include "event.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace clay {

class Runtime;

/* Reactive unit. Every System instance hears every bus event; whether it
 * reacts is its own decision, but the SystemGraph counts the reactions so
 * the engine can always answer "how did the world respond to that?" */
class System {
  public:
    virtual ~System() = default;

    virtual const char *name() const = 0;

    /* Called for every event on the bus, in registration order. */
    virtual void on_event(Runtime &rt, const Event &ev) {
        (void)rt;
        (void)ev;
    }

    /* Continuous simulation step at fixed dt after event dispatch. */
    virtual void update(Runtime &rt, double dt) {
        (void)rt;
        (void)dt;
    }

    uint64_t reactions() const {
        return reactions_;
    }
    uint64_t last_reaction_frame() const {
        return last_reaction_frame_;
    }

  protected:
    void note_reaction(Runtime &rt);
    void note_reaction_at(Runtime &rt, uint64_t frame);

  private:
    uint64_t reactions_ = 0;
    uint64_t last_reaction_frame_ = 0;
};

class SystemGraph {
  public:
    void add(std::unique_ptr<System> system);
    System *at(size_t i);

    void dispatch_event(Runtime &rt, const Event &ev);
    void update(Runtime &rt, double dt);

    size_t size() const {
        return systems_.size();
    }
    uint64_t total_reactions() const;

  private:
    std::vector<std::unique_ptr<System>> systems_;
};

} // namespace clay

#endif /* CLAY_ENGINE_SYSTEMS_SYSTEM_GRAPH_HPP */