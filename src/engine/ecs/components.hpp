#ifndef CLAY_ENGINE_ECS_COMPONENTS_HPP
#define CLAY_ENGINE_ECS_COMPONENTS_HPP

#include <cstdint>

namespace clay {

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

} // namespace clay

#endif /* CLAY_ENGINE_ECS_COMPONENTS_HPP */