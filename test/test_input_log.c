#include "test_c.h"

#include <clay/clay.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_CLAYREC_MAGIC UINT64_C(0x434C415952454301)
#define TEST_CLAYREC_HEADER_BYTES 28u
#define TEST_CLAYREC_EVENT_BYTES 68u

static size_t read_file(const char *path, unsigned char *buf, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, cap, f);
    if (ferror(f)) n = 0;
    if (fclose(f) != 0) n = 0;
    return n;
}

static bool write_file(const char *path, const unsigned char *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(buf, 1, len, f) == len;
    if (fclose(f) != 0) ok = false;
    return ok;
}

static uint32_t decode_u32_le(const unsigned char *p) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; i++) v |= (uint32_t)p[i] << (i * 8);
    return v;
}

static uint64_t decode_u64_le(const unsigned char *p) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; i++) v |= (uint64_t)p[i] << (i * 8);
    return v;
}

static void reverse_field(unsigned char *p, size_t len) {
    for (size_t i = 0; i < len / 2; i++) {
        unsigned char t = p[i];
        p[i] = p[len - 1 - i];
        p[len - 1 - i] = t;
    }
}

static void clayrec_v3_to_legacy_v2(unsigned char *buf, size_t count,
                                    bool big_endian) {
    /* v2 uses the same field sizes as v3 but stores every primitive in host
     * byte order. Start from the canonical little-endian v3 bytes. */
    buf[8] = 2;
    buf[9] = 0;
    buf[10] = 0;
    buf[11] = 0;
    if (!big_endian) return;

    reverse_field(buf + 0, 8);
    reverse_field(buf + 8, 4);
    reverse_field(buf + 12, 8);
    reverse_field(buf + 20, 8);

    static const size_t widths[] = {4, 8, 4, 4, 4, 8, 8, 8, 8, 8, 4};
    size_t off = TEST_CLAYREC_HEADER_BYTES;
    for (size_t event = 0; event < count; event++) {
        for (size_t field = 0; field < sizeof(widths) / sizeof(widths[0]);
             field++) {
            reverse_field(buf + off, widths[field]);
            off += widths[field];
        }
    }
}

static int test_input_log(void) {
    unsigned char arena_buf[1 << 16];
    cl_arena arena;
    cl_arena_init(&arena, arena_buf, sizeof(arena_buf));

    cl_input_log log;
    cl_input_log_init(&log, &arena, 4);

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

    const char *path = "clay_test_input_log.clayrec";
    CHECK(cl_input_log_save(&log, path) == CLAY_OK);

    unsigned char bytes[512];
    size_t file_len = read_file(path, bytes, sizeof(bytes));
    CHECK_EQ_INT(file_len,
                 TEST_CLAYREC_HEADER_BYTES + 2 * TEST_CLAYREC_EVENT_BYTES);
    CHECK(decode_u64_le(bytes) == TEST_CLAYREC_MAGIC);
    CHECK_EQ_INT(decode_u32_le(bytes + 8), 3);
    CHECK_EQ_INT(decode_u64_le(bytes + 12), 2);

    /* Round-trip through the canonical v3 format. */
    cl_input_log back;
    cl_input_log_init(&back, &arena, 4);
    CHECK(cl_input_log_load(&back, path) == CLAY_OK);
    CHECK_EQ_INT(cl_input_log_count(&back), 2);
    CHECK(cl_input_log_fingerprint(&back) == fp);
    const cl_input_event *b1 = cl_input_log_at(&back, 0);
    CHECK(b1 != NULL && b1->key == CLAY_KEY_SPACE && b1->frame == 5);

    /* A complete but modified v3 recording must fail its fingerprint. */
    const char *corrupt_path = "clay_test_input_log_corrupt.clayrec";
    unsigned char corrupt[512];
    memcpy(corrupt, bytes, file_len);
    corrupt[TEST_CLAYREC_HEADER_BYTES] ^= 1u;
    CHECK(write_file(corrupt_path, corrupt, file_len));
    cl_input_log corrupt_log;
    cl_input_log_init(&corrupt_log, &arena, 4);
    CHECK(cl_input_log_load(&corrupt_log, corrupt_path) == CLAY_ERR_PARSE);
    CHECK_EQ_INT(cl_input_log_count(&corrupt_log), 0);

    /* Truncated files are rejected before partially populating the log. */
    const char *truncated_path = "clay_test_input_log_truncated.clayrec";
    CHECK(write_file(truncated_path, bytes, file_len - 1));
    cl_input_log truncated;
    cl_input_log_init(&truncated, &arena, 4);
    CHECK(cl_input_log_load(&truncated, truncated_path) == CLAY_ERR_PARSE);
    CHECK_EQ_INT(cl_input_log_count(&truncated), 0);

    /* Existing v2 recordings remain readable in either legacy byte order.
     * Their stored fingerprint is deliberately not required because older v2
     * writers hashed raw struct padding. */
    const char *legacy_le_path = "clay_test_input_log_v2_le.clayrec";
    unsigned char legacy_le[512];
    memcpy(legacy_le, bytes, file_len);
    clayrec_v3_to_legacy_v2(legacy_le, 2, false);
    CHECK(write_file(legacy_le_path, legacy_le, file_len));
    cl_input_log legacy_le_log;
    cl_input_log_init(&legacy_le_log, &arena, 4);
    CHECK(cl_input_log_load(&legacy_le_log, legacy_le_path) == CLAY_OK);
    CHECK_EQ_INT(cl_input_log_count(&legacy_le_log), 2);
    CHECK(cl_input_log_fingerprint(&legacy_le_log) == fp);

    const char *legacy_be_path = "clay_test_input_log_v2_be.clayrec";
    unsigned char legacy_be[512];
    memcpy(legacy_be, bytes, file_len);
    clayrec_v3_to_legacy_v2(legacy_be, 2, true);
    CHECK(write_file(legacy_be_path, legacy_be, file_len));
    cl_input_log legacy_be_log;
    cl_input_log_init(&legacy_be_log, &arena, 4);
    CHECK(cl_input_log_load(&legacy_be_log, legacy_be_path) == CLAY_OK);
    CHECK_EQ_INT(cl_input_log_count(&legacy_be_log), 2);
    CHECK(cl_input_log_fingerprint(&legacy_be_log) == fp);

    /* Loading a missing file remains a clean, reported I/O failure. */
    cl_input_log missing;
    cl_input_log_init(&missing, &arena, 4);
    CHECK(cl_input_log_load(&missing, "definitely/missing.clayrec") == CLAY_ERR_IO);

    remove(path);
    remove(corrupt_path);
    remove(truncated_path);
    remove(legacy_le_path);
    remove(legacy_be_path);
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_input_log)
