#ifndef CLAY_ENGINE_RENDER_RENDERER_HPP
#define CLAY_ENGINE_RENDER_RENDERER_HPP

#include <cstdint>
#include <vector>

namespace clay {

struct Rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

static inline uint32_t rgba_to_pixel(Rgba c) {
    return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/* A finished, engine-owned frame: RGBA8, top-left origin, row-major. This is
 * the *authoritative* image — a window presenter or a PNG dump is only ever a
 * copy of it. */
struct Framebuffer {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;

    void resize(int w, int h);
    void clear(Rgba c);
    uint32_t pixel(int x, int y) const;
    const uint8_t *as_rgba() const; /* stb/GL-friendly flattened bytes */
};

/* Backend-agnostic draw surface. Clay rasterizes everything itself; an
 * implementation either keeps the pixels (RendererSW) or presents them. */
class IRenderer {
  public:
    virtual ~IRenderer() = default;

    virtual void begin_frame(Rgba clear_color) = 0;
    virtual void end_frame() = 0;

    virtual int width() const = 0;
    virtual int height() const = 0;

    /* Primitives operate in canvas pixels (top-left origin, +y down). */
    virtual void fill_rect(float x, float y, float w, float h, Rgba c) = 0;
    virtual void fill_circle(float cx, float cy, float radius, Rgba c) = 0;
    virtual void draw_line(float x0, float y0, float x1, float y1, Rgba c) = 0;
    virtual void fill_triangle(float x0, float y0, float x1, float y1,
                               float x2, float y2, Rgba c) = 0;
};

} // namespace clay

#endif /* CLAY_ENGINE_RENDER_RENDERER_HPP */