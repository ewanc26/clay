#ifndef CLAY_ENGINE_RENDER_RENDERER_SW_HPP
#define CLAY_ENGINE_RENDER_RENDERER_SW_HPP

#include "render/renderer.hpp"

namespace clay {

/* The software renderer: every primitive goes through clay::raster straight
 * into the framebuffer. Deterministic, no GPU, no driver — a game rendered
 * here is reproducible byte-for-byte, which is what makes replay testing
 * meaningful at all. */
class RendererSW final : public IRenderer {
  public:
    RendererSW(int width, int height);

    void begin_frame(Rgba clear_color) override;
    void end_frame() override;

    int width() const override {
        return fb_.width;
    }
    int height() const override {
        return fb_.height;
    }

    void fill_rect(float x, float y, float w, float h, Rgba c) override;
    void fill_circle(float cx, float cy, float radius, Rgba c) override;
    void draw_line(float x0, float y0, float x1, float y1, Rgba c) override;
    void fill_triangle(float x0, float y0, float x1, float y1, float x2,
                       float y2, Rgba c) override;
    void draw_image(int x, int y, const uint32_t *src, int src_w,
                    int src_h) override;

    Framebuffer &framebuffer() {
        return fb_;
    }
    const Framebuffer &framebuffer() const {
        return fb_;
    }

    uint32_t pixel(int x, int y) const;

    /* Pixels touched this frame — a blunt, honest reactivity gauge. */
    uint64_t touched() const {
        return touched_;
    }

  private:
    Framebuffer fb_;
    uint64_t touched_ = 0;
};

} // namespace clay

#endif /* CLAY_ENGINE_RENDER_RENDERER_SW_HPP */