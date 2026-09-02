#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ui/imgui.hpp"
#include "render/renderer.hpp"

#include <cmath>

using namespace clay;

namespace {

/* A minimal IRenderer that records fill_rect calls for hit-testing checks. */
struct TestRenderer : IRenderer {
    int w = 320, h = 240;
    int fill_count = 0;
    void begin_frame(Rgba) override {}
    void end_frame() override {}
    int width() const override { return w; }
    int height() const override { return h; }
    void fill_rect(float, float, float, float, Rgba) override {
        fill_count++;
    }
    void fill_circle(float, float, float, Rgba) override {
        fill_count++;
    }
    void draw_line(float, float, float, float, Rgba) override {}
    void fill_triangle(float, float, float, float, float, float, Rgba) override {}
    void draw_image(int, int, const uint32_t *, int, int) override {}
    Mesh3DStats draw_mesh(const Mesh3D &, cl_m4, cl_m4, cl_m4, Rgba,
                          cl_v3, float, cl_v3, float, float, float) override {
        return {};
    }
};

} // namespace

TEST_CASE("ui: button returns true on click") {
    TestRenderer r;
    UIContext ui(r);
    ui.begin_frame(15, 15, false);
    ui.begin_column(10, 10);
    bool clicked = ui.button("OK");
    CHECK_FALSE(clicked);
    
    ui.begin_frame(15, 15, true);
    ui.begin_column(10, 10);
    clicked = ui.button("OK");
    CHECK(clicked);
}

TEST_CASE("ui: button ignores click outside bounds") {
    TestRenderer r;
    UIContext ui(r);
    ui.begin_frame(200, 200, true);  /* far from button at (10,10) */
    ui.begin_column(10, 10);
    bool clicked = ui.button("OK");
    CHECK_FALSE(clicked);
}

TEST_CASE("ui: checkbox toggles on click") {
    TestRenderer r;
    UIContext ui(r);
    bool value = false;

    ui.begin_frame(15, 15, true);
    ui.begin_column(10, 10);
    bool toggled = ui.checkbox("Check", &value);
    CHECK(toggled);
    CHECK(value);
}

TEST_CASE("ui: checkbox does not toggle without click") {
    TestRenderer r;
    UIContext ui(r);
    bool value = false;

    ui.begin_frame(50, 50, true);  /* cursor not over checkbox */
    ui.begin_column(10, 10);
    ui.checkbox("Check", &value);
    CHECK_FALSE(value);
}

TEST_CASE("ui: slider updates value when dragged") {
    TestRenderer r;
    UIContext ui(r);
    float val = 0.0f;

    /* Click near the right end of the slider track (track at x=80, width=100,
     * so x=170 is 90% across). */
    ui.begin_frame(170, 18, true);
    ui.begin_column(0, 10);
    ui.slider("Vol", &val, 0.0f, 100.0f);
    CHECK(val > 50.0f);
}

TEST_CASE("ui: slider clamps to min") {
    TestRenderer r;
    UIContext ui(r);
    float val = 50.0f;

    /* Click at the left edge of the slider track. */
    ui.begin_frame(80, 18, true);
    ui.begin_column(0, 10);
    ui.slider("Vol", &val, 0.0f, 100.0f);
    CHECK(val == doctest::Approx(0.0f).epsilon(0.05f));
}

TEST_CASE("ui: cursor_y advances after widgets") {
    TestRenderer r;
    UIContext ui(r);
    ui.begin_frame(0, 0, false);

    ui.begin_column(10, 10);
    float y0 = ui.cursor_y();
    ui.label("Hello");
    float y1 = ui.cursor_y();
    CHECK(y1 > y0);

    ui.button("OK");
    float y2 = ui.cursor_y();
    CHECK(y2 > y1);
}

TEST_CASE("ui: same_line advances horizontally") {
    TestRenderer r;
    UIContext ui(r);
    ui.begin_frame(0, 0, false);

    ui.begin_column(10, 10);
    ui.label("Name:");
    float y_after_label = ui.cursor_y();
    CHECK(y_after_label > 10.0f);  /* label advanced cursor */

    ui.same_line(10.0f);
    /* same_line resets cursor_y to the row baseline. */
    CHECK(ui.cursor_y() == doctest::Approx(10.0f));
}
