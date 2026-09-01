#include "test_c.h"

#include <clay/clay.h>

static int test_rng(void) {
    cl_rng a, b;
    cl_rng_seed(&a, 12345);
    cl_rng_seed(&b, 12345);
    for (int i = 0; i < 1000; i++) {
        CHECK(cl_rng_next_u64(&a) == cl_rng_next_u64(&b));
    }

    cl_rng c;
    cl_rng_seed(&c, 99);
    uint64_t first = cl_rng_next_u64(&c);
    cl_rng_seed(&c, 99);
    CHECK(cl_rng_next_u64(&c) == first); /* resumable */

    cl_rng r;
    cl_rng_seed(&r, 7);
    double lo = 1.0, hi = 0.0;
    for (int i = 0; i < 10000; i++) {
        double u = cl_rng_f64(&r);
        CHECK(u >= 0.0 && u < 1.0);
        if (u < lo) lo = u;
        if (u > hi) hi = u;
    }
    CHECK(lo < 0.01);
    CHECK(hi > 0.99);

    int64_t v = cl_rng_range(&r, 10, 20);
    CHECK(v >= 10 && v < 20);
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_rng)