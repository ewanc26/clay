#include "systems/system_graph.hpp"

#include "runtime.hpp"

namespace clay {

void System::note_reaction(Runtime &rt) {
    note_reaction_at(rt, rt.frame());
}

void System::note_reaction_at(Runtime &rt, uint64_t frame) {
    (void)rt;
    reactions_ += 1;
    last_reaction_frame_ = frame;
}

void SystemGraph::add(std::unique_ptr<System> system) {
    systems_.push_back(std::move(system));
}

System *SystemGraph::at(size_t i) {
    return i < systems_.size() ? systems_[i].get() : nullptr;
}

void SystemGraph::dispatch_event(Runtime &rt, const Event &ev) {
    for (auto &s : systems_) s->on_event(rt, ev);
}

void SystemGraph::update(Runtime &rt, double dt) {
    for (auto &s : systems_) s->update(rt, dt);
}

uint64_t SystemGraph::total_reactions() const {
    uint64_t n = 0;
    for (const auto &s : systems_) n += s->reactions();
    return n;
}

} // namespace clay