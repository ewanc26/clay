#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ecs/world.hpp"

using namespace clay;

TEST_CASE("world: create/destroy/living") {
    World w;
    REQUIRE(w.living() == 0);

    Entity a = w.create();
    Entity b = w.create();
    REQUIRE(a.index != b.index);
    REQUIRE(w.living() == 2);
    REQUIRE(w.alive(a));
    REQUIRE(w.alive(b));

    w.destroy(a);
    REQUIRE(!w.alive(a));
    REQUIRE(w.alive(b));
    REQUIRE(w.living() == 1);
}

TEST_CASE("world: generation guards stale entities") {
    World w;
    Entity e = w.create();
    w.destroy(e);
    Entity fresh = w.create();

    /* Slot may be recycled, but the stale Entity must never alias it. */
    REQUIRE(e != fresh);
    REQUIRE(!w.alive(e));
    if (fresh.index == e.index) {
        REQUIRE(fresh.generation != e.generation);
    }
}

TEST_CASE("world: storage set/find/erase") {
    World w;
    Entity e = w.create();
    w.storage<Transform2D>().set(e, {1.0f, 2.0f, 0.0f, 1.0f});
    w.storage<Color>().set(e, {0.1f, 0.2f, 0.3f, 1.0f});

    ComponentStorage<Transform2D> &ts = w.storage<Transform2D>();
    REQUIRE(ts.count() == 1);
    Transform2D *t = ts.find(e);
    REQUIRE(t != nullptr);
    CHECK(t->x == doctest::Approx(1.0f));
    CHECK(t->y == doctest::Approx(2.0f));

    CHECK(ts.erase(e));
    REQUIRE(ts.find(e) == nullptr);
}

TEST_CASE("world: swap-and-pop keeps remaining entities valid") {
    World w;
    Entity e1 = w.create();
    Entity e2 = w.create();
    Entity e3 = w.create();
    w.storage<Transform2D>().set(e1, {1, 1, 0, 1});
    w.storage<Transform2D>().set(e2, {2, 2, 0, 1});
    w.storage<Transform2D>().set(e3, {3, 3, 0, 1});

    w.destroy(e2);
    REQUIRE(!w.alive(e2));
    REQUIRE(w.alive(e1));
    REQUIRE(w.alive(e3));

    ComponentStorage<Transform2D> &ts = w.storage<Transform2D>();
    REQUIRE(ts.count() == 2);
    Transform2D *t1 = ts.find(e1);
    Transform2D *t3 = ts.find(e3);
    REQUIRE(t1 != nullptr);
    REQUIRE(t3 != nullptr);
    CHECK(t1->x == doctest::Approx(1.0f));
    CHECK(t3->x == doctest::Approx(3.0f));
}