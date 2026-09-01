#include "render/raster.hpp"

#include <algorithm>
#include <cmath>

namespace clay::raster {

namespace {

inline Pixel *pix(void *buf, int width, int x, int y) {
    return reinterpret_cast<Pixel *>(buf) + (size_t)y * (size_t)width + (size_t)x;
}

inline bool in_bounds(int width, int height, int x, int y) {
    return x >= 0 && y >= 0 && x < width && y < height;
}

} // namespace

Pixel px(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    /* Alpha is reserved (not stored): keep the high byte clear. */
    (void)a;
    return ((Pixel)r << 16) | ((Pixel)g << 8) | (Pixel)b;
}

/* Pixel is opaque 0x00RRGGBB — no alpha channel is stored (see raster.hpp).
 * "Blending" is therefore a plain overwrite: the incoming colour fully covers
 * the destination. */
Pixel blend(Pixel dst, Pixel src) {
    (void)dst;
    return src;
}

void blend_pixel(void *buf, int width, int height, int x, int y, Pixel c) {
    if (!in_bounds(width, height, x, y)) return;
    *pix(buf, width, x, y) = blend(*pix(buf, width, x, y), c);
}

void fill_rect(void *buf, int width, int height, int x, int y, int w, int h,
               Pixel c) {
    int x0 = std::max(x, 0);
    int y0 = std::max(y, 0);
    int x1 = std::min(x + w, width);
    int y1 = std::min(y + h, height);
    if (x1 <= x0 || y1 <= y0) return;
    for (int cy = y0; cy < y1; cy++) {
        for (int cx = x0; cx < x1; cx++) {
            *pix(buf, width, cx, cy) = blend(*pix(buf, width, cx, cy), c);
        }
    }
}

void fill_circle(void *buf, int width, int height, int cx, int cy, int radius,
                 Pixel c) {
    if (radius <= 0) return;
    int x0 = std::max(cx - radius, 0);
    int y0 = std::max(cy - radius, 0);
    int x1 = std::min(cx + radius + 1, width);
    int y1 = std::min(cy + radius + 1, height);
    const float r2 = (float)radius * (float)radius;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            /* distance test conservative at the boundary edges */
            if (dx * dx + dy * dy <= r2) {
                *pix(buf, width, x, y) = blend(*pix(buf, width, x, y), c);
            }
        }
    }
}

void draw_line(void *buf, int width, int height, int x0, int y0, int x1,
               int y1, Pixel c) {
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (in_bounds(width, height, x0, y0)) {
            *pix(buf, width, x0, y0) = blend(*pix(buf, width, x0, y0), c);
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fill_triangle(void *buf, int width, int height, const float pts[6],
                   Pixel c) {
    float ax = pts[0], ay = pts[1];
    float bx = pts[2], by = pts[3];
    float cx = pts[4], cy = pts[5];

    int min_x = std::max((int)std::floor(std::min({ax, bx, cx})), 0);
    int min_y = std::max((int)std::floor(std::min({ay, by, cy})), 0);
    int max_x = std::min((int)std::ceil(std::max({ax, bx, cx})), width - 1);
    int max_y = std::min((int)std::ceil(std::max({ay, by, cy})), height - 1);

    /* edge functions with consistent winding: a point is inside when all
     * three edges agree in sign (either all >= 0 or all <= 0). */
    auto edge = [](float px, float py, float x0, float y0, float x1, float y1) {
        return (px - x0) * (y1 - y0) - (py - y0) * (x1 - x0);
    };

    for (int y = min_y; y <= max_y; y++) {
        float py = (float)y + 0.5f;
        for (int x = min_x; x <= max_x; x++) {
            float px = (float)x + 0.5f;
            float a = edge(px, py, ax, ay, bx, by);
            float b = edge(px, py, bx, by, cx, cy);
            float d = edge(px, py, cx, cy, ax, ay);
            bool inside = (a >= 0 && b >= 0 && d >= 0) ||
                          (a <= 0 && b <= 0 && d <= 0);
            if (inside) {
                *pix(buf, width, x, y) = blend(*pix(buf, width, x, y), c);
            }
        }
    }
}

void clear(void *buf, int width, int height, Pixel c) {
    Pixel *p = reinterpret_cast<Pixel *>(buf);
    for (size_t i = 0; i < (size_t)width * (size_t)height; i++) p[i] = c;
}

} // namespace clay::raster