#ifndef CLAY_ENGINE_RENDER_TEXT_HPP
#define CLAY_ENGINE_RENDER_TEXT_HPP

#include <cstddef>
#include <cstdint>

namespace clay {

class IRenderer;
struct Rgba;

/* Minimal 5x7 fixed-width bitmap font. Covers printable ASCII (32..126); any
 * other byte renders as a replacement glyph. No external font file, so text
 * renders headlessly and byte-for-byte reproducibly — the same guarantee the
 * software rasterizer gives every other primitive. */
struct Font5x7 {
    /* Rasterize one glyph into a caller-owned Pixel buffer (char_width x
     * char_height), tinted by color and composited with the given per-pixel
     * alpha. The buffer must hold char_width*char_height entries. */
    void tint(uint32_t *out, unsigned char c, uint32_t color) const;

    int char_width() const {
        return 5;
    }
    int char_height() const {
        return 7;
    }
    static const Font5x7 &get();
};

/* Draw a line of text (no newline handling) at an integer top-left position,
 * src-over tinted into the current color. Returns the advance width. */
int draw_text(IRenderer &r, int x, int y, const char *text, std::size_t len,
              const Rgba &color);
int draw_text(IRenderer &r, int x, int y, const char *text,
              const Rgba &color);

} // namespace clay

#endif /* CLAY_ENGINE_RENDER_TEXT_HPP */