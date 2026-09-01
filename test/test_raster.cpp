#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "render/renderer_sw.hpp"

using namespace clay;

namespace {

Rgba px(uint8_t r, uint8_t g, uint8_t b) {
    return Rgba{r, g, b, 255};
}

} // namespace

TEST_CASE("raster: clear paints every pixel") {
    RendererSW r(8, 4);
    r.begin_frame(px(10, 20, 30));
    r.end_frame();
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 8; x++)
            CHECK(r.pixel(x, y) == 0x0A141E);
}

TEST_CASE("raster: fill_rect bounds") {
    RendererSW r(16, 16);
    r.begin_frame(px(0, 0, 0));
    r.fill_rect(2.0f, 3.0f, 5.0f, 4.0f, px(255, 0, 0));
    r.end_frame();

    REQUIRE(r.pixel(2, 3) == 0xFF0000);
    CHECK(r.pixel(6, 6) == 0xFF0000);  /* inside */
    CHECK(r.pixel(1, 3) == 0x000000);  /* left of */
    CHECK(r.pixel(7, 3) == 0x000000);  /* right of */
    CHECK(r.pixel(2, 2) == 0x000000);  /* above */
    CHECK(r.pixel(2, 7) == 0x000000);  /* below */
}

TEST_CASE("raster: fill_circle center and edge") {
    RendererSW r(16, 16);
    r.begin_frame(px(0, 0, 0));
    r.fill_circle(8.0f, 8.0f, 3.0f, px(0, 255, 0));
    r.end_frame();

    CHECK(r.pixel(8, 8) == 0x00FF00);
    CHECK(r.pixel(8, 5) == 0x00FF00); /* top edge included */
    CHECK(r.pixel(8, 12) == 0x000000); /* just outside (dist 4) */
    CHECK(r.pixel(12, 8) == 0x000000);
}

TEST_CASE("raster: draw_line horizontal") {
    RendererSW r(16, 16);
    r.begin_frame(px(0, 0, 0));
    r.draw_line(2.0f, 8.0f, 13.0f, 8.0f, px(0, 0, 255));
    r.end_frame();

    CHECK(r.pixel(2, 8) == 0x0000FF);
    CHECK(r.pixel(13, 8) == 0x0000FF);
    CHECK(r.pixel(7, 8) == 0x0000FF);
    CHECK(r.pixel(7, 7) == 0x000000);
    CHECK(r.pixel(7, 9) == 0x000000);
}

TEST_CASE("raster: touched counts pixels written per frame") {
    RendererSW r(16, 16);
    r.begin_frame(px(1, 1, 1));
    r.fill_rect(0, 0, 4, 4, px(9, 9, 9));
    r.end_frame();
    uint64_t first_rect = r.touched(); /* gauge is per-frame, reset by begin */
    CHECK(first_rect == 16);

    /* A second overlapping rect reports the same per-frame count. */
    r.begin_frame(px(1, 1, 1));
    r.fill_rect(0, 0, 4, 4, px(9, 9, 9));
    r.end_frame();
    uint64_t second_rect = r.touched();
    CHECK(second_rect == first_rect);

    /* A clear-only frame writes nothing through the primitives. */
    r.begin_frame(px(1, 1, 1));
    r.end_frame();
    CHECK(r.touched() == 0);
}