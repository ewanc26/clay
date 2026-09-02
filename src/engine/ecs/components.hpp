#ifndef CLAY_ENGINE_ECS_COMPONENTS_HPP
#define CLAY_ENGINE_ECS_COMPONENTS_HPP

#include <cstdint>

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

/* The component vocabulary of the Garden world. All POD, trivially copyable;
 * owned by World's dense storages. ComponentStorage<T> is created on first
 * world.storage<T>() access, so adding a component type no longer requires
 * editing world.hpp — just add the struct here. */

/* 2D placement in canvas pixels, top-left origin, +y down. */
struct Transform2D {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f; /* radians */
    float scale = 1.0f;
};

struct Velocity {
    float x = 0.0f;
    float y = 0.0f;
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct LifeSpan {
    float remaining = 0.0f;
    float max = 0.0f;
};

enum class Species : uint8_t {
    Unknown = 0,
    Sculpture, /* the player moves this                        */
    Animal,    /* drifts toward the cursor                    */
    Ripple,    /* expanding ring, pure geometry               */
    Pebble,    /* inert scenery                              */
    Ground     /* background band                            */
};

struct Kind {
    Species species = Species::Unknown;
};

struct Tag {
    const char *name = nullptr; /* arena-resident or literal; never freed */
};

/* Animals are pulled toward the cursor with this strength (px/s^2). */
struct MagnetStrength {
    float to_cursor = 0.0f;
};

struct RippleRing {
    float radius = 0.0f;
    float speed = 0.0f;
    float thickness = 0.0f;
};

/* Scene graph: Parent links an entity to its parent. Child positions,
 * rotations, and scales are resolved relative to the parent chain each
 * frame by SceneGraphSystem, which writes the result into WorldTransform2D.
 * Entities without a Parent simply copy Transform2D -> WorldTransform2D. */
struct Parent {
    Entity parent;
};

struct WorldTransform2D {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float scale = 1.0f;
};

} // namespace clay

#endif /* CLAY_ENGINE_ECS_COMPONENTS_HPP */