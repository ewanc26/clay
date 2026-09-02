#ifndef CLAY_ENGINE_RENDER_RENDER_SYSTEM_HPP
#define CLAY_ENGINE_RENDER_RENDER_SYSTEM_HPP

#include "render/renderer.hpp"

namespace clay {

class Runtime;

/* A pluggable draw pass. A game installs its own RenderSystem to control how
 * the world becomes pixels; the runtime never hardcodes a scene again. The
 * default GardenRenderSystem reproduces the original vignette so existing
 * demos, tests, and recordings render byte-for-byte identically. */
class RenderSystem {
  public:
    virtual ~RenderSystem() = default;

    /* Called once per frame after update(); must begin_frame/end_frame on the
     * renderer. `rt` exposes the world, cursor, flash, and timers. */
    virtual void render(Runtime &rt, IRenderer &renderer) = 0;
};

/* Default vignette renderer: reproduces the original "Clay Garden" shapes so
 * existing demos, tests, and recordings keep rendering byte-for-byte. A game
 * installs its own RenderSystem instead. */
class GardenRenderSystem final : public RenderSystem {
  public:
    void render(Runtime &rt, IRenderer &renderer) override;
};

} // namespace clay

#endif /* CLAY_ENGINE_RENDER_RENDER_SYSTEM_HPP */