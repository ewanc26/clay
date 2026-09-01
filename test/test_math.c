#include "test_c.h"

#include <clay/clay.h>

static int test_math(void) {
    cl_v2 a = cl_v2_make(3.0f, 4.0f);
    CHECK_EQ_DBL(cl_v2_length(a), 5.0, 1e-5);
    cl_v2 n = cl_v2_normalize(a);
    CHECK_EQ_DBL(cl_v2_length(n), 1.0, 1e-5);

    cl_m4 tr = cl_m4_translate(3.0f, -1.0f, 0.0f);
    cl_v3 p = cl_m4_mul_vec3(tr, cl_v3_make(4.0f, 2.0f, 0.0f));
    CHECK_EQ_DBL(p.x, 7.0, 1e-4);
    CHECK_EQ_DBL(p.y, 1.0, 1e-4);

    cl_m4 rot = cl_m4_rotate_z(3.141592653589 / 2.0);
    cl_v3 r = cl_m4_mul_vec3(rot, cl_v3_make(1.0f, 0.0f, 0.0f));
    CHECK_EQ_DBL(r.x, 0.0, 1e-4);
    CHECK_EQ_DBL(r.y, 1.0, 1e-4);

    cl_m4 scl = cl_m4_scale(2.0f, 3.0f, 1.0f);
    cl_v3 sp = cl_m4_mul_vec3(scl, cl_v3_make(1.0f, 1.0f, 1.0f));
    CHECK_EQ_DBL(sp.x, 2.0, 1e-4);
    CHECK_EQ_DBL(sp.y, 3.0, 1e-4);

    cl_m4 mul = cl_m4_mul(tr, scl);
    cl_v3 q = cl_m4_mul_vec3(mul, cl_v3_make(1.0f, 1.0f, 1.0f));
    CHECK_EQ_DBL(q.x, 8.0, 1e-4); /* (1+3)*2, row-vector convention */
    CHECK_EQ_DBL(q.y, 0.0, 1e-4); /* (1-1)*3 */

    cl_v2 perp = cl_v2_perp(cl_v2_make(1.0f, 0.0f));
    CHECK_EQ_DBL(perp.x, 0.0, 1e-5);
    CHECK_EQ_DBL(perp.y, 1.0, 1e-5);
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_math)