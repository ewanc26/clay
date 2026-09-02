#include "ecs/world.hpp"

namespace clay {

Entity World::create() {
    /* Reuse a free slot when one exists (fills from the front of the free
     * list via the moving slot trick: find a used slot, but keep it simple —
     * linear scan is fine at Garden scale). */
    for (size_t i = 0; i < slots_.size(); i++) {
        if (!slots_[i].used) {
            slots_[i].used = true;
            slots_[i].generation += 1;
            spawns_ += 1;
            return Entity{static_cast<uint32_t>(i), slots_[i].generation};
        }
    }
    slots_.push_back(Slot{1, true});
    spawns_ += 1;
    return Entity{static_cast<uint32_t>(slots_.size() - 1), 1};
}

void World::destroy(Entity e) {
    if (!alive(e)) return;
    erase_every_component(e);
    slots_[e.index].used = false;
    /* generation steps forward so stale Entity handles can't collide */
    slots_[e.index].generation += 1;
    destroys_ += 1;
}

bool World::alive(Entity e) const {
    return e.index < slots_.size() && slots_[e.index].used &&
           slots_[e.index].generation == e.generation;
}

size_t World::living() const {
    size_t n = 0;
    for (const Slot &s : slots_) {
        if (s.used) n += 1;
    }
    return n;
}

void World::clear() {
    for (size_t i = 0; i < slots_.size(); i++) {
        if (slots_[i].used) slots_[i].generation += 1;
        slots_[i].used = false;
    }
    for (auto &[_, storage] : storages_) {
        storage->clear();
    }
}

void World::erase_every_component(Entity e) {
    for (auto &[_, storage] : storages_) {
        storage->erase(e);
    }
}

} // namespace clay