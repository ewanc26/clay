#ifndef CLAY_ENGINE_RENDER_RASTER_HPP
#define CLAY_ENGINE_RENDER_RASTER_HPP

#include <cstdint>

namespace clay::raster {

/* The from-scratch software rasterizer: 32-bit RGBA pixels in a row-major
 * 4-byte/pixel buffer, (0,0) top-left, +x right, +y down. Opaque compositing
 * (src-over alpha onto the destination). Conventions and edge rules are
 * documented per primitive; none of them depend on GPU state. */
using Pixel = uint32_t; /* 0x00RRGGBB (alpha channel reserved) */

Pixel px(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
Pixel blend(Pixel dst, Pixel src);

/* Bounds-scan fill of axis-aligned rect [x, x+w) x [y, y+h). */
void fill_rect(void *buf, int width, int height, int x, int y, int w, int h,
               Pixel c);

/* Circle (not ellipse) by symmetric bbox scan; radius is a pixel distance. */
void fill_circle(void *buf, int width, int height, int cx, int cy, int radius,
                 Pixel c);

/* Thick line (Bresenham-idea, fast + symmetric) between two integer points. */
void draw_line(void *buf, int width, int height, int x0, int y0, int x1,
               int y1, Pixel c);

/* Filled triangle between three points, edge-function coverage test. */
void fill_triangle(void *buf, int width, int height, const float pts[6],
                   Pixel c);

void clear(void *buf, int width, int height, Pixel c);

/* Deterministic raster of the whole region (for scattering updates later). */
void blend_pixel(void *buf, int width, int height, int x, int y, Pixel c);

} // namespace clay::raster

#endif /* CLAY_ENGINE_RENDER_RASTER_HPP */