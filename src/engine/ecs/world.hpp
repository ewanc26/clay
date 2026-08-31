#ifndef CLAY_ENGINE_ECS_WORLD_HPP
#define CLAY_ENGINE_ECS_WORLD_HPP

#include "ecs/components.hpp"

#include <cstdint>
#include <type_traits>
#include <vector>

namespace clay {

/* Stable identity: index + generation. An index is recycled only after its
 * slot's generation wraps (uint32), so a stale Entity can never alias a new
 * one that happens to reuse the slot. */
struct Entity {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool operator==(const Entity &o) const {
        return index == o.index && generation == o.generation;
    }
    bool operator!=(const Entity &o) const {
        return !(*this == o);
    }
    explicit operator bool() const {
        return generation != 0;
    }
};

/* Packed (SoA-style) facade thrown transparently on top of plain vectors:
 * dense entities in insertion order, sparse lookup by entity index, swap-and-
 * pop erase with the moved entity's sparse slot fixed up. Good enough for the
 * Garden's few hundred entities, and honest — no magic. */
template <class T> struct ComponentStorage {
    std::vector<T> dense;
    std::vector<Entity> owner;   /* dense[i] -> owning entity    */
    std::vector<uint32_t> index; /* entity.index -> dense+1 (0)  */

    uint32_t count() const {
        return static_cast<uint32_t>(dense.size());
    }

    T *find(Entity e) const {
        if (e.index >= index.size()) return nullptr;
        uint32_t i = index[e.index];
        if (i == 0 || i > dense.size()) return nullptr;
        T *p = const_cast<T *>(&dense[i - 1]);
        if (owner[i - 1] != e) return nullptr; /* stale generation */
        return p;
    }

    bool contains(Entity e) const {
        return find(e) != nullptr;
    }

    T *set(Entity e, T value) {
        T *p = find(e);
        if (p) {
            *p = value;
            return p;
        }
        if (e.index >= index.size()) index.resize((size_t)e.index + 1, 0);
        size_t i = dense.size();
        dense.push_back(value);
        owner.push_back(e);
        index[e.index] = static_cast<uint32_t>(i + 1);
        return &dense[i];
    }

    bool erase(Entity e) {
        if (e.index >= index.size()) return false;
        uint32_t i = index[e.index];
        if (i == 0 || i > dense.size()) return false;
        if (owner[i - 1] != e) return false;

        size_t last = dense.size() - 1;
        if (i - 1 != last) {
            T moved = std::move(dense[last]);
            dense[i - 1] = std::move(moved);
            owner[i - 1] = owner[last];
            index[owner[i - 1].index] = static_cast<uint32_t>(i);
        }
        dense.pop_back();
        owner.pop_back();
        index[e.index] = 0;
        return true;
    }

    void clear() {
        dense.clear();
        owner.clear();
        index.clear();
    }
};

/* The Garden world: an Entity registry plus one ComponentStorage per known
 * component type, reached through the typed storage<T>() template. Storage is
 * process-lifetime but safely movable; components are trivially copyable. */
class World {
  public:
    World() = default;
    World(const World &) = delete;
    World &operator=(const World &) = delete;

    Entity create();
    void destroy(Entity e);
    bool alive(Entity e) const;
    size_t living() const;

    void clear();

    template <class T> ComponentStorage<T> &storage() {
        if constexpr (std::is_same_v<T, Transform2D>) return transforms_;
        else if constexpr (std::is_same_v<T, Velocity>) return velocities_;
        else if constexpr (std::is_same_v<T, Color>) return colors_;
        else if constexpr (std::is_same_v<T, LifeSpan>) return lives_;
        else if constexpr (std::is_same_v<T, Kind>) return kinds_;
        else if constexpr (std::is_same_v<T, Tag>) return tags_;
        else if constexpr (std::is_same_v<T, MagnetStrength>)
            return magnets_;
        else if constexpr (std::is_same_v<T, RippleRing>) return rings_;
    }

    /* Counters feed the "reactivity trace": how much the player perturbed
     * the world. */
    uint64_t spawns() const {
        return spawns_;
    }
    uint64_t destroys() const {
        return destroys_;
    }

  private:
    struct Slot {
        uint32_t generation = 0;
        bool used = false;
    };

    std::vector<Slot> slots_;
    uint64_t spawns_ = 0;
    uint64_t destroys_ = 0;

    ComponentStorage<Transform2D> transforms_;
    ComponentStorage<Velocity> velocities_;
    ComponentStorage<Color> colors_;
    ComponentStorage<LifeSpan> lives_;
    ComponentStorage<Kind> kinds_;
    ComponentStorage<Tag> tags_;
    ComponentStorage<MagnetStrength> magnets_;
    ComponentStorage<RippleRing> rings_;

    void erase_every_component(Entity e);
};

} // namespace clay

#endif /* CLAY_ENGINE_ECS_WORLD_HPP */