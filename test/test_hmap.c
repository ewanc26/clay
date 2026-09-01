#include "test_c.h"

#include <clay/clay.h>

#include <string.h>

static int test_hmap(void) {
    unsigned char buf[8192];
    cl_arena arena;
    cl_arena_init(&arena, buf, sizeof(buf));

    cl_hmap hm;
    cl_hmap_create(&hm, &arena, 8);
    CHECK(cl_hmap_len(&hm) == 0);
    CHECK(cl_hmap_get_cstr(&hm, "hello") == NULL);

    CHECK(cl_hmap_put_cstr(&hm, "hello", cl_variant_str(cl_str_c("world"))));
    cl_variant *v = cl_hmap_get_cstr(&hm, "hello");
    CHECK(v != NULL && v->kind == CLAY_VAR_STR && v->s.len == 5 &&
          memcmp(v->s.data, "world", 5) == 0);

    cl_hmap_put_cstr(&hm, "count", cl_variant_i64(42));
    cl_variant *c = cl_hmap_get_cstr(&hm, "count");
    CHECK(c != NULL && c->kind == CLAY_VAR_I64 && c->i == 42);
    CHECK(cl_hmap_len(&hm) == 2);

    /* Oversize non-NUL keys hashed from arbitrary bytes. */
    unsigned char longkey[300];
    for (size_t i = 0; i < sizeof(longkey); i++)
        longkey[i] = (unsigned char)(i * 7);
    cl_str big = cl_str_make((const char *)longkey, sizeof(longkey));
    cl_hmap_put(&hm, big, cl_variant_bool(true));
    cl_variant *b = cl_hmap_get(&hm, big);
    CHECK(b != NULL && b->kind == CLAY_VAR_BOOL && b->b);
    CHECK(cl_hmap_len(&hm) == 3);

    /* Remove + ordered scan. */
    CHECK(cl_hmap_remove(&hm, cl_str_c("count")));
    CHECK(cl_hmap_get_cstr(&hm, "count") == NULL);
    CHECK(cl_hmap_len(&hm) == 2);

    cl_hmap_iter it = {0};
    int seen = 0;
    cl_hmap_entry *e;
    while ((e = cl_hmap_next(&hm, &it)) != NULL) {
        seen++;
        CHECK(e->used);
    }
    CHECK_EQ_INT(seen, 2);

    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_hmap)