#include "input_log.h"

#include "math.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* v2 used native-endian primitive writes. v3 keeps the same field layout but
 * fixes the byte order to little-endian so recordings are portable. The
 * loader still accepts both little- and big-endian v2 files. */
#define CLAYREC_MAGIC UINT64_C(0x434C415952454301)
#define CLAYREC_LEGACY_VERSION 2u
#define CLAYREC_VERSION 3u
#define CLAYREC_EVENT_BYTES UINT64_C(68)
#define CLAYREC_FINGERPRINT_SEED UINT64_C(0x9E3779B97F4A7C15)

static uint64_t hash_word(uint64_t hash, uint64_t word) {
    return cl_hash_u64(hash ^ cl_hash_u64(word));
}

static uint64_t double_bits(double value) {
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double bits_double(uint64_t bits) {
    double value = 0.0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Hash semantic fields rather than raw struct bytes. Struct padding is
 * unspecified and can differ after a serialize/deserialize round-trip. */
static uint64_t hash_event(const cl_input_event *e, uint64_t seed) {
    uint64_t hash = seed;
    hash = hash_word(hash, (uint64_t)e->frame);
    hash = hash_word(hash, double_bits(e->time));
    hash = hash_word(hash, (uint64_t)(uint32_t)e->type);
    hash = hash_word(hash, (uint64_t)(uint32_t)e->key);
    hash = hash_word(hash, (uint64_t)(uint32_t)e->mods);
    hash = hash_word(hash, double_bits(e->x));
    hash = hash_word(hash, double_bits(e->y));
    hash = hash_word(hash, double_bits(e->dx));
    hash = hash_word(hash, double_bits(e->dy));
    hash = hash_word(hash, (uint64_t)(uint32_t)e->wheel);
    hash = hash_word(hash, e->focus ? UINT64_C(1) : UINT64_C(0));
    return hash;
}

void cl_input_log_init(cl_input_log *log, cl_arena *a, size_t cap) {
    log->arena = a;
    log->cap = cap ? cap : 256;
    log->items = (cl_input_event *)cl_arena_alloc(
        a, log->cap * sizeof(cl_input_event), _Alignof(cl_input_event));
    log->count = 0;
    log->fingerprint = CLAYREC_FINGERPRINT_SEED;
}

void cl_input_log_append(cl_input_log *log, const cl_input_event *e) {
    if (log->count == log->cap) {
        size_t cap = log->cap * 2;
        cl_input_event *items = (cl_input_event *)cl_arena_alloc(
            log->arena, cap * sizeof(cl_input_event),
            _Alignof(cl_input_event));
        for (size_t i = 0; i < log->count; i++) items[i] = log->items[i];
        log->items = items;
        log->cap = cap;
    }
    log->items[log->count++] = *e;
    log->fingerprint = hash_event(e, log->fingerprint);
}

size_t cl_input_log_count(const cl_input_log *log) {
    return log->count;
}

const cl_input_event *cl_input_log_at(const cl_input_log *log, size_t i) {
    return i < log->count ? &log->items[i] : NULL;
}

uint64_t cl_input_log_fingerprint(const cl_input_log *log) {
    return log->fingerprint;
}

static bool write_u32_le(FILE *f, uint32_t v) {
    unsigned char b[4] = {
        (unsigned char)(v & 0xffu),
        (unsigned char)((v >> 8) & 0xffu),
        (unsigned char)((v >> 16) & 0xffu),
        (unsigned char)((v >> 24) & 0xffu),
    };
    return fwrite(b, 1, sizeof(b), f) == sizeof(b);
}

static bool write_u64_le(FILE *f, uint64_t v) {
    unsigned char b[8];
    for (unsigned i = 0; i < 8; i++) {
        b[i] = (unsigned char)((v >> (i * 8)) & UINT64_C(0xff));
    }
    return fwrite(b, 1, sizeof(b), f) == sizeof(b);
}

static bool write_f64_le(FILE *f, double v) {
    return write_u64_le(f, double_bits(v));
}

static uint32_t decode_u32(const unsigned char b[4], bool little_endian) {
    uint32_t v = 0;
    if (little_endian) {
        for (unsigned i = 0; i < 4; i++) v |= (uint32_t)b[i] << (i * 8);
    } else {
        for (unsigned i = 0; i < 4; i++) v = (v << 8) | (uint32_t)b[i];
    }
    return v;
}

static uint64_t decode_u64(const unsigned char b[8], bool little_endian) {
    uint64_t v = 0;
    if (little_endian) {
        for (unsigned i = 0; i < 8; i++) v |= (uint64_t)b[i] << (i * 8);
    } else {
        for (unsigned i = 0; i < 8; i++) v = (v << 8) | (uint64_t)b[i];
    }
    return v;
}

static bool read_u32(FILE *f, bool little_endian, uint32_t *out) {
    unsigned char b[4];
    if (fread(b, 1, sizeof(b), f) != sizeof(b)) return false;
    *out = decode_u32(b, little_endian);
    return true;
}

static bool read_u64(FILE *f, bool little_endian, uint64_t *out) {
    unsigned char b[8];
    if (fread(b, 1, sizeof(b), f) != sizeof(b)) return false;
    *out = decode_u64(b, little_endian);
    return true;
}

static bool read_f64(FILE *f, bool little_endian, double *out) {
    uint64_t bits = 0;
    if (!read_u64(f, little_endian, &bits)) return false;
    *out = bits_double(bits);
    return true;
}

static bool read_magic(FILE *f, bool *little_endian) {
    unsigned char b[8];
    if (fread(b, 1, sizeof(b), f) != sizeof(b)) return false;
    if (decode_u64(b, true) == CLAYREC_MAGIC) {
        *little_endian = true;
        return true;
    }
    if (decode_u64(b, false) == CLAYREC_MAGIC) {
        *little_endian = false;
        return true;
    }
    return false;
}

static bool encode_event(FILE *f, const cl_input_event *e) {
    return write_u32_le(f, e->frame) && write_f64_le(f, e->time) &&
           write_u32_le(f, (uint32_t)e->type) &&
           write_u32_le(f, (uint32_t)e->key) &&
           write_u32_le(f, (uint32_t)e->mods) && write_f64_le(f, e->x) &&
           write_f64_le(f, e->y) && write_f64_le(f, e->dx) &&
           write_f64_le(f, e->dy) && write_f64_le(f, (double)e->wheel) &&
           write_u32_le(f, e->focus ? 1u : 0u);
}

static bool decode_event(FILE *f, bool little_endian, cl_input_event *out) {
    uint32_t frame = 0;
    uint32_t type = 0;
    uint32_t key = 0;
    uint32_t mods = 0;
    uint32_t focus = 0;
    double time = 0.0;
    double x = 0.0;
    double y = 0.0;
    double dx = 0.0;
    double dy = 0.0;
    double wheel = 0.0;

    if (!read_u32(f, little_endian, &frame) ||
        !read_f64(f, little_endian, &time) ||
        !read_u32(f, little_endian, &type) ||
        !read_u32(f, little_endian, &key) ||
        !read_u32(f, little_endian, &mods) ||
        !read_f64(f, little_endian, &x) ||
        !read_f64(f, little_endian, &y) ||
        !read_f64(f, little_endian, &dx) ||
        !read_f64(f, little_endian, &dy) ||
        !read_f64(f, little_endian, &wheel) ||
        !read_u32(f, little_endian, &focus)) {
        return false;
    }

    const uint32_t known_mods =
        (uint32_t)(CLAY_MOD_SHIFT | CLAY_MOD_CTRL | CLAY_MOD_ALT | CLAY_MOD_META);
    if (type < (uint32_t)CLAY_IN_PRESS || type > (uint32_t)CLAY_IN_FOCUS ||
        key >= (uint32_t)CLAY_KEY_COUNT || (mods & ~known_mods) != 0 ||
        focus > 1u || wheel < (double)INT_MIN || wheel > (double)INT_MAX) {
        return false;
    }

    int wheel_int = (int)wheel;
    if ((double)wheel_int != wheel) return false;

    cl_input_event e = {0};
    e.frame = frame;
    e.time = time;
    e.type = (cl_input_kind)type;
    e.key = (cl_key)key;
    e.mods = (int)mods;
    e.x = x;
    e.y = y;
    e.dx = dx;
    e.dy = dy;
    e.wheel = wheel_int;
    e.focus = focus != 0;
    *out = e;
    return true;
}

static bool body_size_matches(FILE *f, uint64_t count) {
    if (count > UINT64_MAX / CLAYREC_EVENT_BYTES) return false;
    long body_start = ftell(f);
    if (body_start < 0 || fseek(f, 0, SEEK_END) != 0) return false;
    long file_end = ftell(f);
    if (file_end < body_start) return false;
    uint64_t actual = (uint64_t)(file_end - body_start);
    uint64_t expected = count * CLAYREC_EVENT_BYTES;
    if (fseek(f, body_start, SEEK_SET) != 0) return false;
    return actual == expected;
}

cl_err cl_input_log_save(cl_input_log *log, const char *path) {
    if (!log || !path) return CLAY_ERR_INVALID_ARG;
    FILE *f = fopen(path, "wb");
    if (!f) return CLAY_ERR_IO;

    bool ok = write_u64_le(f, CLAYREC_MAGIC) &&
              write_u32_le(f, CLAYREC_VERSION) &&
              write_u64_le(f, (uint64_t)log->count) &&
              write_u64_le(f, log->fingerprint);
    for (size_t i = 0; ok && i < log->count; i++) {
        ok = encode_event(f, &log->items[i]);
    }
    if (fclose(f) != 0) ok = false;
    return ok ? CLAY_OK : CLAY_ERR_IO;
}

cl_err cl_input_log_load(cl_input_log *log, const char *path) {
    if (!log || !path) return CLAY_ERR_INVALID_ARG;
    FILE *f = fopen(path, "rb");
    if (!f) return CLAY_ERR_IO;

    size_t old_count = log->count;
    uint64_t old_fingerprint = log->fingerprint;
    bool little_endian = false;
    uint32_t version = 0;
    uint64_t count = 0;
    uint64_t stamp = 0;

    bool header_ok = read_magic(f, &little_endian) &&
                     read_u32(f, little_endian, &version) &&
                     read_u64(f, little_endian, &count) &&
                     read_u64(f, little_endian, &stamp);
    if (!header_ok ||
        (version != CLAYREC_LEGACY_VERSION && version != CLAYREC_VERSION) ||
        (version == CLAYREC_VERSION && !little_endian) ||
        count > UINT64_C(1000000000) || !body_size_matches(f, count)) {
        fclose(f);
        return CLAY_ERR_PARSE;
    }

    uint64_t file_fingerprint = CLAYREC_FINGERPRINT_SEED;
    for (uint64_t i = 0; i < count; i++) {
        cl_input_event e;
        if (!decode_event(f, little_endian, &e)) {
            log->count = old_count;
            log->fingerprint = old_fingerprint;
            fclose(f);
            return CLAY_ERR_PARSE;
        }
        file_fingerprint = hash_event(&e, file_fingerprint);
        cl_input_log_append(log, &e);
    }

    /* v2 fingerprints may have been produced by the old raw-struct hash, so
     * they cannot be required without breaking existing recordings. Every v3
     * file uses the canonical field hash and must match exactly. */
    if (version == CLAYREC_VERSION && file_fingerprint != stamp) {
        log->count = old_count;
        log->fingerprint = old_fingerprint;
        fclose(f);
        return CLAY_ERR_PARSE;
    }

    if (fclose(f) != 0) {
        log->count = old_count;
        log->fingerprint = old_fingerprint;
        return CLAY_ERR_IO;
    }
    return CLAY_OK;
}
