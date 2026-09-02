#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "systems/collision.hpp"

using namespace clay;

TEST_CASE("collision: circle_circle no overlap") {
    Contact2D c = circle_circle(
        cl_v2_make(0, 0), 5.0f,
        cl_v2_make(20, 0), 5.0f);
    CHECK_FALSE(c.overlap);
}

TEST_CASE("collision: circle_circle overlap") {
    Contact2D c = circle_circle(
        cl_v2_make(0, 0), 5.0f,
        cl_v2_make(8, 0), 5.0f);
    CHECK(c.overlap);
    CHECK(c.depth == doctest::Approx(2.0f).epsilon(0.01f));
    /* Contact point is midpoint of overlap region: A's right edge (5) to
     * B's left edge (3), midpoint = 4. */
    CHECK(c.point.x == doctest::Approx(4.0f).epsilon(0.01f));
    CHECK(c.point.y == doctest::Approx(0.0f).epsilon(0.01f));
}

TEST_CASE("collision: circle_circle tangent") {
    Contact2D c = circle_circle(
        cl_v2_make(0, 0), 5.0f,
        cl_v2_make(10, 0), 5.0f);
    CHECK_FALSE(c.overlap);
}

TEST_CASE("collision: aabb_aabb no overlap") {
    Contact2D c = aabb_aabb(
        cl_v2_make(0, 0), 5.0f, 5.0f,
        cl_v2_make(20, 0), 5.0f, 5.0f);
    CHECK_FALSE(c.overlap);
}

TEST_CASE("collision: aabb_aabb overlap") {
    Contact2D c = aabb_aabb(
        cl_v2_make(0, 0), 10.0f, 10.0f,
        cl_v2_make(5, 5), 10.0f, 10.0f);
    CHECK(c.overlap);
    /* Both axes overlap by 15 (half-extents 10+10, center distance 5). */
    CHECK(c.depth == doctest::Approx(15.0f).epsilon(0.01f));
}

TEST_CASE("collision: aabb_aabb edge-adjacent") {
    Contact2D c = aabb_aabb(
        cl_v2_make(0, 0), 5.0f, 5.0f,
        cl_v2_make(10, 0), 5.0f, 5.0f);
    CHECK_FALSE(c.overlap);
}

TEST_CASE("collision: aabb_circle overlap") {
    Contact2D c = aabb_circle(
        cl_v2_make(0, 0), 5.0f, 5.0f,
        cl_v2_make(3, 3), 3.0f);
    CHECK(c.overlap);
}

TEST_CASE("collision: aabb_circle no overlap") {
    Contact2D c = aabb_circle(
        cl_v2_make(0, 0), 5.0f, 5.0f,
        cl_v2_make(20, 20), 3.0f);
    CHECK_FALSE(c.overlap);
}
