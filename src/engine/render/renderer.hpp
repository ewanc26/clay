#ifndef CLAY_ENGINE_RENDER_RENDERER_HPP
#define CLAY_ENGINE_RENDER_RENDERER_HPP

#include "clay/clay.h"

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
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) |
           ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/* A triangle mesh: positions in object space, traversal by index triplets.
 * The rendering path is CPU-side and deterministic by construction (fixed
 * triangle order, no map iteration). */
struct Mesh3D {
    std::vector<cl_v3> positions;
    std::vector<unsigned> indices; /* 3 per triangle */
    bool empty() const {
        return indices.empty();
    }
    void add_triangle(cl_v3 a, cl_v3 b, cl_v3 c);
};

/* Result of rasterizing one mesh; lets callers/tests observe how much geometry
 * actually reached the framebuffer. */
struct Mesh3DStats {
    int triangles_in = 0;
    int triangles_rasterized = 0;
    int pixels_written = 0;
};

/* A finished, engine-owned frame: RGBA8, top-left origin, row-major. This is
 * the *authoritative* image — a window presenter or a PNG dump is only ever a
 * copy of it. */
struct Framebuffer {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels;
    mutable std::vector<uint8_t> rgba_cache;

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
    virtual void fill_triangle(float x0, float y0, float x1, float y1, float x2,
                               float y2, Rgba c) = 0;
    /* Composite a premultiplied source image at an integer pixel position. */
    virtual void draw_image(int x, int y, const uint32_t *src, int src_w,
                            int src_h) = 0;

    /* Rasterize a 3D mesh. `proj * view * model` maps object space to clip
     * space; lighting uses the model-transformed normals and `light_dir`
     * (world space). `intensity` scales the directional term; `ambient` is the
     * minimum fraction of the base color that always shows. An optional
     * `point_light_*` triple adds a single positional light with inverse-square
     * attenuation. Returns stats about what was actually written. */
    virtual Mesh3DStats draw_mesh(const Mesh3D &mesh, cl_m4 model, cl_m4 view,
                                  cl_m4 proj, Rgba color,
                                  cl_v3 light_dir = {0.3f, 0.5f, 0.8f},
                                  float intensity = 1.0f,
                                  cl_v3 point_light_pos = {0.0f, 0.0f, 0.0f},
                                  float point_light_intensity = 0.0f,
                                  float point_light_attenuation = 0.0f,
                                  float ambient = 0.35f) = 0;
};

} // namespace clay

#endif /* CLAY_ENGINE_RENDER_RENDERER_HPP */
