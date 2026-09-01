#include "test_c.h"

#include <clay/clay.h>

static int test_variant(void) {
    cl_variant n = cl_variant_nil();
    CHECK(n.kind == CLAY_VAR_NIL);
    CHECK(!cl_variant_truthy(n));

    cl_variant t = cl_variant_bool(true);
    CHECK(cl_variant_truthy(t));
    CHECK_EQ_DBL(cl_variant_to_double(t), 1.0, 1e-9);

    cl_variant i = cl_variant_i64(-7);
    CHECK(cl_variant_to_double(i) == -7.0);
    CHECK_EQ_INT(cl_variant_to_i64(t), 1);

    cl_variant f = cl_variant_f64(2.5);
    CHECK_EQ_DBL(cl_variant_to_double(f), 2.5, 1e-9);
    CHECK_EQ_INT(cl_variant_to_i64(f), 2);

    cl_variant s = cl_variant_str(cl_str_c("x"));
    CHECK(cl_variant_truthy(s));
    CHECK(cl_variant_to_double(s) != cl_variant_to_double(s)); /* NaN */

    CHECK(cl_variant_eq(cl_variant_f64(1.0), cl_variant_i64(1)));
    CHECK(!cl_variant_eq(cl_variant_bool(true), cl_variant_bool(false)));
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_variant)