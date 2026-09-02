#ifndef CLAY_ENGINE_ECS_WORLD_HPP
#define CLAY_ENGINE_ECS_WORLD_HPP

#include "ecs/components.hpp"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
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

/* Type-erased handle to a component pool, so World can store arbitrary
 * ComponentStorage<T> instances in a single map without templating the
 * World class itself. */
struct ComponentStorageBase {
    virtual ~ComponentStorageBase() = default;
    virtual uint32_t count() const = 0;
    virtual bool erase(Entity e) = 0;
    virtual void clear() = 0;
};

/* Packed (SoA-style) facade thrown transparently on top of plain vectors:
 * dense entities in insertion order, sparse lookup by entity index, swap-and-
 * pop erase with the moved entity's sparse slot fixed up. Good enough for the
 * Garden's few hundred entities, and honest — no magic. */
template <class T> struct ComponentStorage : ComponentStorageBase {
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

/* The Garden world: an Entity registry plus type-erased component pools,
 * reached through the typed storage<T>() template. Storage is created on
 * first access so arbitrary component types work without editing World;
 * components are trivially copyable, stored in compact dense arrays. */
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

    /* Typed storage; creates the pool on first access, so a game can add
     * arbitrary component types without editing World. The pool is compact
     * (dense arrays + sparse lookup), created once per World. */
    template <class T> ComponentStorage<T> &storage() {
        auto it = storages_.find(std::type_index(typeid(T)));
        if (it == storages_.end()) {
            auto ptr = std::make_unique<ComponentStorage<T>>();
            it = storages_.emplace(std::type_index(typeid(T)),
                                   std::move(ptr)).first;
        }
        return *static_cast<ComponentStorage<T> *>(it->second.get());
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

    std::unordered_map<std::type_index, std::unique_ptr<ComponentStorageBase>>
        storages_;

    void erase_every_component(Entity e);
};

} // namespace clay

#endif /* CLAY_ENGINE_ECS_WORLD_HPP */