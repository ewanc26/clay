#include "test_c.h"

#include <clay/clay.h>

#include <string.h>

static int test_json(void) {
    cl_err err;

    /* Round-trip a small object. */
    {
        unsigned char buf[2048];
        cl_arena a;
        cl_arena_init(&a, buf, sizeof(buf));
        cl_json_node root;
        err = cl_json_parse(&root, &a,
                            cl_str_c("{\"name\":\"clay\",\"count\":3,"
                                     "\"ratio\":1.5,\"ok\":true}"));
        CHECK(err == CLAY_OK);
        cl_json_node *name = cl_json_get_cstr(&root, "name");
        CHECK(name != NULL && name->kind == CLAY_J_STR);
        CHECK(name->s.len == 4);
        CHECK(memcmp(name->s.data, "clay", 4) == 0);
        cl_json_node *count = cl_json_get_cstr(&root, "count");
        CHECK(count != NULL && count->kind == CLAY_J_I64 && count->i == 3);
        cl_json_node *ratio = cl_json_get_cstr(&root, "ratio");
        CHECK(ratio != NULL && ratio->kind == CLAY_J_F64);
        CHECK_EQ_DBL(ratio->f, 1.5, 1e-9);
        cl_json_node *ok = cl_json_get_cstr(&root, "ok");
        CHECK(ok != NULL && ok->kind == CLAY_J_BOOL && ok->b);
    }

    /* Arrays + nested paths. */
    {
        unsigned char buf[2048];
        cl_arena a;
        cl_arena_init(&a, buf, sizeof(buf));
        cl_json_node root;
        err = cl_json_parse(&root, &a,
                            cl_str_c("{\"rules\":[{\"name\":\"a\"},"
                                     "{\"name\":\"b\"}]}"));
        CHECK(err == CLAY_OK);
        cl_json_node *rules = cl_json_get_cstr(&root, "rules");
        CHECK(rules != NULL && rules->kind == CLAY_J_ARR);
        CHECK_EQ_INT(rules->arr.n, 2);
        cl_json_node *second = rules->arr.items[1];
        cl_json_node *name2 = cl_json_get_cstr(second, "name");
        CHECK(name2 != NULL && name2->s.len == 1 && name2->s.data[0] == 'b');
    }

    /* Dangerous input: truncated JSON must fail cleanly, not overrun. */
    {
        unsigned char buf[2048];
        cl_arena a;
        cl_arena_init(&a, buf, sizeof(buf));
        cl_json_node root;
        err = cl_json_parse(&root, &a, cl_str_c("{\"a\":[1,2,"));
        CHECK(err == CLAY_ERR_PARSE || err == CLAY_ERR_OOM);
    }

    /* Missing path -> NULL without side effects. */
    {
        unsigned char buf[1024];
        cl_arena a;
        cl_arena_init(&a, buf, sizeof(buf));
        cl_json_node root;
        err = cl_json_parse(&root, &a, cl_str_c("{}"));
        CHECK(err == CLAY_OK);
        CHECK(cl_json_get_cstr(&root, "absent") == NULL);
    }
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_json)