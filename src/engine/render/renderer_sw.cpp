#include "render/renderer.hpp"

#include "render/raster.hpp"

#include "render/renderer_sw.hpp"

#include <algorithm>

namespace clay {

void Framebuffer::resize(int w, int h) {
    width = w;
    height = h;
    pixels.assign((size_t)w * (size_t)h, 0);
    rgba_cache.clear();
}

void Framebuffer::clear(Rgba c) {
    uint32_t p = rgba_to_pixel(c);
    std::fill(pixels.begin(), pixels.end(), p);
}

uint32_t Framebuffer::pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return 0;
    return pixels[(size_t)y * (size_t)width + (size_t)x];
}

const uint8_t *Framebuffer::as_rgba() const {
    rgba_cache.resize(pixels.size() * 4u);
    for (size_t i = 0; i < pixels.size(); i++) {
        const uint32_t pixel = pixels[i];
        rgba_cache[i * 4u + 0u] = (uint8_t)(pixel >> 16);
        rgba_cache[i * 4u + 1u] = (uint8_t)(pixel >> 8);
        rgba_cache[i * 4u + 2u] = (uint8_t)pixel;
        rgba_cache[i * 4u + 3u] = (uint8_t)(pixel >> 24);
    }
    return rgba_cache.data();
}

RendererSW::RendererSW(int width, int height) {
    fb_.resize(width, height);
}

void RendererSW::begin_frame(Rgba clear_color) {
    fb_.clear(clear_color);
    touched_ = 0;
}

void RendererSW::end_frame() {}

void RendererSW::fill_rect(float x, float y, float w, float h, Rgba c) {
    int iw = (int)w, ih = (int)h;
    raster::fill_rect(fb_.pixels.data(), fb_.width, fb_.height, (int)x, (int)y,
                      iw, ih, rgba_to_pixel(c));
    touched_ += (uint64_t)(std::max(0, iw) * std::max(0, ih));
}

void RendererSW::fill_circle(float cx, float cy, float radius, Rgba c) {
    int r = (int)radius;
    raster::fill_circle(fb_.pixels.data(), fb_.width, fb_.height, (int)cx,
                        (int)cy, r, rgba_to_pixel(c));
    touched_ += (uint64_t)r * (uint64_t)r * 3;
}

void RendererSW::draw_line(float x0, float y0, float x1, float y1, Rgba c) {
    raster::draw_line(fb_.pixels.data(), fb_.width, fb_.height, (int)x0,
                      (int)y0, (int)x1, (int)y1, rgba_to_pixel(c));
    touched_ += 8;
}

void RendererSW::fill_triangle(float x0, float y0, float x1, float y1, float x2,
                               float y2, Rgba c) {
    const float pts[6] = {x0, y0, x1, y1, x2, y2};
    raster::fill_triangle(fb_.pixels.data(), fb_.width, fb_.height, pts,
                          rgba_to_pixel(c));
    touched_ += 64;
}

void RendererSW::draw_image(int x, int y, const uint32_t *src, int src_w,
                            int src_h) {
    if (!src || src_w <= 0 || src_h <= 0) return;
    raster::blit(fb_.pixels.data(), fb_.width, fb_.height, x, y, src, src_w,
                 src_h);
    touched_ += (uint64_t)src_w * (uint64_t)src_h;
}

Mesh3DStats RendererSW::draw_mesh(const Mesh3D &mesh, cl_m4 model, cl_m4 view,
                                  cl_m4 proj, Rgba color, cl_v3 light_dir,
                                  float ambient) {
    r3d_.resize(fb_.width, fb_.height);
    r3d_.clear();
    Mesh3DStats s = r3d_.draw_mesh(fb_.pixels.data(), fb_.width, mesh, model,
                                   view, proj, color, light_dir, ambient);
    touched_ += (uint64_t)s.pixels_written;
    return s;
}

uint32_t RendererSW::pixel(int x, int y) const {
    return fb_.pixel(x, y);
}

} // namespace clay
