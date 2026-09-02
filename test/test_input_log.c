#include "test_c.h"

#include <clay/clay.h>

#include <stdio.h>

static int test_input_log(void) {
    unsigned char buf[1 << 16];
    cl_arena arena;
    cl_arena_init(&arena, buf, sizeof(buf));

    cl_input_log log;
    cl_input_log_init(&log, &arena, 0);

    cl_input_event e1 = cl_input_event_make(CLAY_IN_PRESS, CLAY_KEY_SPACE);
    e1.frame = 5;
    e1.time = 1.25;
    cl_input_event e2 = cl_input_event_make(CLAY_IN_RELEASE, CLAY_KEY_SPACE);
    e2.frame = 6;
    e2.time = 1.30;
    e2.x = 42.0;
    e2.y = 17.0;
    cl_input_log_append(&log, &e1);
    cl_input_log_append(&log, &e2);

    CHECK_EQ_INT(cl_input_log_count(&log), 2);
    const cl_input_event *got = cl_input_log_at(&log, 1);
    CHECK(got != NULL);
    CHECK(got->type == CLAY_IN_RELEASE);
    CHECK_EQ_DBL(got->x, 42.0, 1e-9);
    uint64_t fp = cl_input_log_fingerprint(&log);

    /* Round-trip through the on-disk format; fingerprint and content must
     * match exactly. */
    const char *path = "clay_test_input_log.clayrec";
    CHECK(cl_input_log_save(&log, path) == CLAY_OK);

    cl_input_log back;
    cl_input_log_init(&back, &arena, 0);
    CHECK(cl_input_log_load(&back, path) == CLAY_OK);
    CHECK_EQ_INT(cl_input_log_count(&back), 2);
    CHECK(cl_input_log_fingerprint(&back) == fp);
    const cl_input_event *b1 = cl_input_log_at(&back, 0);
    CHECK(b1 != NULL && b1->key == CLAY_KEY_SPACE && b1->frame == 5);

    cl_input_log bad;
    cl_input_log_init(&bad, &arena, 0);

    FILE *corrupt = fopen(path, "r+b");
    CHECK(corrupt != NULL);
    if (corrupt != NULL) {
        CHECK(fseek(corrupt, 20, SEEK_SET) == 0);
        int first = fgetc(corrupt);
        CHECK(first != EOF);
        CHECK(fseek(corrupt, 20, SEEK_SET) == 0);
        CHECK(fputc(first ^ 1, corrupt) != EOF);
        fclose(corrupt);
        CHECK(cl_input_log_load(&bad, path) == CLAY_ERR_PARSE);
    }

    /* Loading a bad file must be a clean, reported failure. */
    CHECK(cl_input_log_load(&bad, "definitely/missing.clayrec") == CLAY_ERR_IO);

    FILE *truncated = fopen("clay_test_input_log_truncated.clayrec", "wb");
    CHECK(truncated != NULL);
    if (truncated != NULL) {
        fputs("CLAY", truncated);
        fclose(truncated);
        CHECK(cl_input_log_load(&bad, "clay_test_input_log_truncated.clayrec") ==
              CLAY_ERR_PARSE);
    }
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_input_log)
