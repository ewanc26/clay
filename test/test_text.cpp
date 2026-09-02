#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "render/renderer_sw.hpp"
#include "render/text.hpp"

using namespace clay;

namespace {
Rgba px(uint8_t r, uint8_t g, uint8_t b) {
    return Rgba{r, g, b, 255};
}
} // namespace

TEST_CASE("text: draw_text advances by char width plus spacing") {
    RendererSW r(64, 16);
    r.begin_frame(px(0, 0, 0));
    int w = draw_text(r, 2, 2, "AB", px(255, 255, 255));
    r.end_frame();
    /* 5 wide + 1 spacing per glyph, two glyphs. */
    CHECK(w == (5 + 1) * 2);
}

TEST_CASE("text: glyphs leave background transparent behind gaps") {
    RendererSW r(16, 16);
    r.begin_frame(px(10, 10, 10));
    /* '!' is a single vertical bar at column 2; neighbors stay the clear
     * colour because the glyph is blitted with alpha (transparent gaps). */
    draw_text(r, 0, 0, "!", px(255, 255, 255));
    r.end_frame();
    CHECK(r.pixel(0, 0) == 0xFF0A0A0A);  /* left of the bar                */
    CHECK(r.pixel(2, 0) == 0xFFFFFFFF);  /* the bar                        */
    CHECK(r.pixel(4, 0) == 0xFF0A0A0A);  /* right of the bar               */
}

TEST_CASE("text: empty or null string draws nothing and advances zero") {
    RendererSW r(16, 16);
    r.begin_frame(px(0, 0, 0));
    CHECK(draw_text(r, 0, 0, "", px(255, 255, 255)) == 0);
    CHECK(draw_text(r, 0, 0, "", 0, px(255, 255, 255)) == 0);
    CHECK(draw_text(r, 0, 0, nullptr, 0, px(255, 255, 255)) == 0);
    r.end_frame();
    /* All background remains the clear colour. */
    CHECK(r.pixel(0, 0) == 0xFF000000);
}

TEST_CASE("text: non-ASCII bytes render a replacement glyph") {
    RendererSW r(16, 16);
    r.begin_frame(px(0, 0, 0));
    /* 0x80 is outside printable ASCII -> replacement glyph, still draws. */
    draw_text(r, 0, 0, "\x80", 1, px(200, 200, 200));
    r.end_frame();
    bool any = false;
    for (int y = 0; y < 7 && !any; y++)
        for (int x = 0; x < 5 && !any; x++)
            any = r.pixel(x, y) == 0xFFC8C8C8;
    CHECK(any);
}