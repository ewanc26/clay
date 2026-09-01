#include "test_c.h"

#include <clay/clay.h>

#include <stdint.h>

static int test_arena(void) {
    _Alignas(16) unsigned char backing[4096];
    cl_arena a;
    cl_arena_init(&a, backing, sizeof(backing));

    void *p1 = cl_arena_alloc(&a, 128, 8);
    CHECK(p1 != NULL);
    CHECK((uintptr_t)p1 % 8 == 0);

    /* Allocations do not overlap and respect alignment. */
    void *p2 = cl_arena_alloc(&a, 512, 16);
    CHECK(p2 != NULL);
    CHECK((uintptr_t)p2 % 16 == 0);
    CHECK((const char *)p2 >= (const char *)p1 + 128);

    /* Reset unwinds to the start; an alloc immediately after lands low. */
    cl_arena_reset(&a);
    size_t first_remaining = cl_arena_remaining_bytes(&a);
    CHECK(first_remaining >= 4080);
    void *p3 = cl_arena_alloc(&a, 64, 4);
    CHECK(p3 != NULL);
    CHECK((const char *)p3 < (const char *)p2);

    /* Temp frame returns memory, so scratch never leaks. */
    cl_arena_frame f = cl_arena_temp(&a);
    cl_arena_alloc(&a, 1024, 8);
    size_t before_return = cl_arena_remaining_bytes(&a);
    cl_arena_return(f);
    CHECK(cl_arena_remaining_bytes(&a) > before_return);

    /* String helpers copy into arena ownership. */
    char *owned = cl_arena_cstr(&a, "clay");
    CHECK(owned != NULL && owned[0] == 'c' && owned[4] == '\0');

    /* Exhaustion is a fatal "handle it or halt" condition: CL_PANIC, not a
     * NULL return. A request beyond capacity terminates (not asserted here). */
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_arena)