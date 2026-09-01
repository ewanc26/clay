#ifndef CLAY_TEST_C_H
#define CLAY_TEST_C_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Minimal per-file test harness, mirroring keepsake's CHECK style. One static
 * counter per translation unit; each test file is its own executable. */

static int clay_test_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            clay_test_failures++;                                           \
        }                                                                   \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                     \
    do {                                                                       \
        long long av = (long long)(a);                                         \
        long long bv = (long long)(b);                                         \
        if (av != bv) {                                                        \
            fprintf(stderr, "FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__, \
                    __LINE__, #a, #b, av, bv);                                 \
            clay_test_failures++;                                              \
        }                                                                      \
    } while (0)

#define CHECK_EQ_DBL(a, b, eps)                                                \
    do {                                                                       \
        double av = (double)(a);                                               \
        double bv = (double)(b);                                               \
        double d = av - bv;                                                    \
        if (d < 0) d = -d;                                                     \
        if (d > (eps)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s ~ %s (%f != %f)\n", __FILE__,      \
                    __LINE__, #a, #b, av, bv);                                 \
            clay_test_failures++;                                              \
        }                                                                      \
    } while (0)

#define CLAY_C_TEST_MAIN(run)                                                  \
    int main(void) {                                                           \
        int n = (run)();                                                       \
        if (n != 0) {                                                          \
            fprintf(stderr, "  %d failure(s)\n", n);                           \
            return 1;                                                          \
        }                                                                      \
        printf("ok: %s\n", #run);                                              \
        return 0;                                                              \
    }

#endif /* CLAY_TEST_C_H */