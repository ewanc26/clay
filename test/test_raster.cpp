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
            CHECK(r.pixel(x, y) == 0xFF0A141E /* opaque 0x0A141E */);
}

TEST_CASE("raster: RGBA view expands packed pixels in channel order") {
    RendererSW r(2, 1);
    r.begin_frame(px(0, 0, 0));
    r.fill_rect(0.0f, 0.0f, 1.0f, 1.0f, px(0x12, 0x34, 0x56));
    r.end_frame();

    const uint8_t *rgba = r.framebuffer().as_rgba();
    REQUIRE(rgba != nullptr);
    CHECK(rgba[0] == 0x12);
    CHECK(rgba[1] == 0x34);
    CHECK(rgba[2] == 0x56);
    CHECK(rgba[3] == 0xFF);
    CHECK(rgba[4] == 0x00);
    CHECK(rgba[5] == 0x00);
    CHECK(rgba[6] == 0x00);
    CHECK(rgba[7] == 0xFF);
}

TEST_CASE("raster: src-over alpha blending") {
    RendererSW r(8, 8);
    /* Opaque red base. */
    r.begin_frame(px(0, 0, 0));
    r.fill_rect(0.0f, 0.0f, 8.0f, 8.0f, px(200, 0, 0));
    /* 50% green over the red base. */
    r.fill_rect(0.0f, 0.0f, 8.0f, 8.0f, Rgba{0, 255, 0, 127});
    r.end_frame();

    /* 50% green over red -> a dark yellow-brown. Verify it is neither pure
     * red nor pure green: red channel stays, green channel appears, and the
     * result is opaque because both inputs were opaque. */
    uint32_t p = r.pixel(3, 3);
    CHECK((p >> 24) == 0xFF);
    uint8_t red = (uint8_t)(p >> 16);
    uint8_t green = (uint8_t)(p >> 8);
    CHECK(red < 200u);
    CHECK(red > 0u);
    CHECK(green > 0u);
    CHECK(green < 255u);
}

TEST_CASE("raster: fully transparent src leaves dst untouched") {
    RendererSW r(8, 8);
    r.begin_frame(px(10, 20, 30));
    r.fill_rect(0.0f, 0.0f, 8.0f, 8.0f, px(10, 20, 30)); /* opaque dst */
    r.fill_rect(0.0f, 0.0f, 8.0f, 8.0f, Rgba{255, 0, 0, 0}); /* transparent */
    r.end_frame();
    CHECK(r.pixel(0, 0) == 0xFF0A141E); /* untouched */
    CHECK(r.pixel(7, 7) == 0xFF0A141E);
}

TEST_CASE("raster: opaque src overwrites dst") {
    RendererSW r(8, 8);
    r.begin_frame(px(10, 20, 30));
    r.fill_rect(0.0f, 0.0f, 8.0f, 8.0f, px(10, 20, 30));
    r.fill_rect(0.0f, 0.0f, 8.0f, 8.0f, px(1, 2, 3));
    r.end_frame();
    CHECK(r.pixel(0, 0) == 0xFF010203);
}

TEST_CASE("raster: fill_rect bounds") {
    RendererSW r(16, 16);
    r.begin_frame(px(0, 0, 0));
    r.fill_rect(2.0f, 3.0f, 5.0f, 4.0f, px(255, 0, 0));
    r.end_frame();

    REQUIRE(r.pixel(2, 3) == 0xFFFF0000);
    CHECK(r.pixel(6, 6) == 0xFFFF0000); /* inside */
    CHECK(r.pixel(1, 3) == 0xFF000000); /* left of */
    CHECK(r.pixel(7, 3) == 0xFF000000); /* right of */
    CHECK(r.pixel(2, 2) == 0xFF000000); /* above */
    CHECK(r.pixel(2, 7) == 0xFF000000); /* below */
}

TEST_CASE("raster: fill_circle center and edge") {
    RendererSW r(16, 16);
    r.begin_frame(px(0, 0, 0));
    r.fill_circle(8.0f, 8.0f, 3.0f, px(0, 255, 0));
    r.end_frame();

    CHECK(r.pixel(8, 8) == 0xFF00FF00);
    CHECK(r.pixel(8, 5) == 0xFF00FF00);  /* top edge included */
    CHECK(r.pixel(8, 12) == 0xFF000000); /* just outside (dist 4) */
    CHECK(r.pixel(12, 8) == 0xFF000000);
}

TEST_CASE("raster: draw_line horizontal") {
    RendererSW r(16, 16);
    r.begin_frame(px(0, 0, 0));
    r.draw_line(2.0f, 8.0f, 13.0f, 8.0f, px(0, 0, 255));
    r.end_frame();

    CHECK(r.pixel(2, 8) == 0xFF0000FF);
    CHECK(r.pixel(13, 8) == 0xFF0000FF);
    CHECK(r.pixel(7, 8) == 0xFF0000FF);
    CHECK(r.pixel(7, 7) == 0xFF000000);
    CHECK(r.pixel(7, 9) == 0xFF000000);
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
