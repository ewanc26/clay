#ifndef CLAY_ENGINE_SYSTEMS_BUILTIN_HPP
#define CLAY_ENGINE_SYSTEMS_BUILTIN_HPP

#include "event.hpp"
#include "systems/system_graph.hpp"

namespace clay {

/* MovementSystem integrates Velocity into Transform2D for every Species that
 * has one (animals, pebbles) and expands RippleRing radius over time. World
 * edges wrap, so nothing ever leaves the canvas. */
class MovementSystem final : public System {
  public:
    const char *name() const override {
        return "movement";
    }
    void update(Runtime &rt, double dt) override;
};

/* LifespanSystem ages every life-span until it expires, then destroys the
 * entity through the reactive path (publishing world.destroy). Sculptures and
 * the ground band are immortal. */
class LifespanSystem final : public System {
  public:
    const char *name() const override {
        return "lifespan";
    }
    void update(Runtime &rt, double dt) override;
};

/* CursorMagnetSystem pulls magnet-flagged creatures toward the cursor so that
 * simply steering the mouse steers the garden. Reactivity with literally zero
 * discrete input. */
class CursorMagnetSystem final : public System {
  public:
    const char *name() const override {
        return "cursor_magnet";
    }
    void update(Runtime &rt, double dt) override;
};

/* HueShiftSystem drifts creature colors slowly and deterministically with sim
 * time, so a long headless run is still visibly alive while staying
 * reproducible. */
class HueShiftSystem final : public System {
  public:
    const char *name() const override {
        return "hue_shift";
    }
    void update(Runtime &rt, double dt) override;
};

/* RippleSystem expands ring thickness/alpha and reacts to the wheel to embiggen
 * every live ring (scroll = world microphone). */
class RippleSystem final : public System {
  public:
    const char *name() const override {
        return "ripple";
    }
    void on_event(Runtime &rt, const Event &ev) override;
    void update(Runtime &rt, double dt) override;

  private:
    double last_wheel_ = 0.0;
    bool saw_wheel_ = false;
};

} // namespace clay

#endif /* CLAY_ENGINE_SYSTEMS_BUILTIN_HPP */