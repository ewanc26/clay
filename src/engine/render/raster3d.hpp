#ifndef CLAY_ENGINE_RENDER_RASTER3D_HPP
#define CLAY_ENGINE_RENDER_RASTER3D_HPP

#include "clay/clay.h"
#include "render/renderer.hpp"

#include <cstdint>
#include <vector>

namespace clay {

/* CPU 3D renderer. Owns a depth buffer sized like the framebuffer and writes
 * shaded pixels directly into a 32-bit RGBA buffer (0xAARRGGBB, top-left,
 * +y down — same convention as raster.hpp). */
class Renderer3D {
  public:
    Renderer3D() = default;

    void resize(int width, int height);
    int width() const {
        return width_;
    }
    int height() const {
        return height_;
    }

    /* Reset the depth buffer to far (1.0). Color clearing happens on the
     * caller's color buffer (see raster::clear). */
    void clear();

    /* Rasterize `mesh` into `dst` (0xAARRGGBB pixels, row-major, top-left).
     * `proj * view * model` maps object-space points to clip space. Normals
     * are transformed by the model (upper 3x3) then lit by `light_dir`
     * (world space, normalized): diffuse = max(0, dot(n, l)), plus `ambient`.
     * Winding is CCW (front) and backfaces are culled. */
    Mesh3DStats draw_mesh(uint32_t *dst, int dst_pitch, const Mesh3D &mesh,
                          cl_m4 model, cl_m4 view, cl_m4 proj, Rgba color,
                          cl_v3 light_dir = {0.3f, 0.5f, 0.8f},
                          float ambient = 0.35f);

  private:
    int width_ = 0;
    int height_ = 0;
    std::vector<float> depth_;
};

} // namespace clay

#endif /* CLAY_ENGINE_RENDER_RASTER3D_HPP */