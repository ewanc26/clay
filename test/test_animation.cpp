#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "runtime.hpp"
#include "systems/builtin.hpp"

using namespace clay;

TEST_CASE("easing: linear interpolates evenly") {
    CHECK(cl_ease_apply(CL_EASE_LINEAR, 0.0f) == doctest::Approx(0.0f));
    CHECK(cl_ease_apply(CL_EASE_LINEAR, 0.5f) == doctest::Approx(0.5f));
    CHECK(cl_ease_apply(CL_EASE_LINEAR, 1.0f) == doctest::Approx(1.0f));
}

TEST_CASE("easing: out_quad is fast then slow") {
    float mid = cl_ease_apply(CL_EASE_OUT_QUAD, 0.5f);
    CHECK(mid > 0.5f);
    CHECK(cl_ease_apply(CL_EASE_OUT_QUAD, 0.0f) == doctest::Approx(0.0f));
    CHECK(cl_ease_apply(CL_EASE_OUT_QUAD, 1.0f) == doctest::Approx(1.0f));
}

TEST_CASE("easing: in_quad is slow then fast") {
    float mid = cl_ease_apply(CL_EASE_IN_QUAD, 0.5f);
    CHECK(mid < 0.5f);
    CHECK(cl_ease_apply(CL_EASE_IN_QUAD, 0.0f) == doctest::Approx(0.0f));
    CHECK(cl_ease_apply(CL_EASE_IN_QUAD, 1.0f) == doctest::Approx(1.0f));
}

TEST_CASE("easing: out_sine starts fast, ends slow") {
    float mid = cl_ease_apply(CL_EASE_OUT_SINE, 0.5f);
    CHECK(mid > 0.5f);
    CHECK(cl_ease_apply(CL_EASE_OUT_SINE, 0.0f) == doctest::Approx(0.0f).epsilon(0.01f));
    CHECK(cl_ease_apply(CL_EASE_OUT_SINE, 1.0f) == doctest::Approx(1.0f).epsilon(0.01f));
}

TEST_CASE("tween: one-shot reaches target and deactivates") {
    Runtime rt(64, 64, 42);
    Entity e = rt.world().create();
    rt.world().storage<Tween>().set(e,
        {0.0f, 100.0f, 1.0f, 0.0f, 0.0f, CL_EASE_LINEAR, false, true});

    AnimationSystem sys;
    rt.world().storage<Tween>().find(e)->active = true;

    sys.update(rt, 0.5);
    Tween *tw = rt.world().storage<Tween>().find(e);
    REQUIRE(tw != nullptr);
    CHECK(tw->value == doctest::Approx(50.0f).epsilon(0.01f));
    CHECK(tw->active);

    sys.update(rt, 0.5);
    CHECK_FALSE(tw->active);
    CHECK(tw->value == doctest::Approx(100.0f));
}

TEST_CASE("tween: loop restarts from beginning") {
    Runtime rt(64, 64, 42);
    Entity e = rt.world().create();
    rt.world().storage<Tween>().set(e,
        {0.0f, 10.0f, 1.0f, 0.0f, 0.0f, CL_EASE_LINEAR, true, true});

    AnimationSystem sys;

    /* Halfway through the cycle. */
    sys.update(rt, 0.5);
    Tween *tw = rt.world().storage<Tween>().find(e);
    REQUIRE(tw != nullptr);
    CHECK(tw->value == doctest::Approx(5.0f).epsilon(0.01f));
    CHECK(tw->active);

    /* Complete the cycle + loop restarts immediately. */
    sys.update(rt, 0.5);
    CHECK(tw->active); /* still active because loop=true */
    CHECK(tw->value == doctest::Approx(0.0f).epsilon(0.01f));
}

TEST_CASE("tween: eased tween does not move linearly") {
    Runtime rt(64, 64, 42);
    Entity e = rt.world().create();
    rt.world().storage<Tween>().set(e,
        {0.0f, 100.0f, 2.0f, 0.0f, 0.0f, CL_EASE_OUT_QUAD, false, true});

    AnimationSystem sys;
    sys.update(rt, 1.0);
    Tween *tw = rt.world().storage<Tween>().find(e);
    REQUIRE(tw != nullptr);
    CHECK(tw->value > 50.0f);
}

TEST_CASE("tween: inactive tweens are not updated") {
    Runtime rt(64, 64, 42);
    Entity e = rt.world().create();
    rt.world().storage<Tween>().set(e,
        {0.0f, 100.0f, 1.0f, 0.0f, 0.0f, CL_EASE_LINEAR, false, false});

    AnimationSystem sys;
    sys.update(rt, 1.0);
    Tween *tw = rt.world().storage<Tween>().find(e);
    REQUIRE(tw != nullptr);
    CHECK(tw->value == doctest::Approx(0.0f));
    CHECK_FALSE(tw->active);
}
