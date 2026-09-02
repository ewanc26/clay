#include "input_log.h"

#include "math.h"

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* On-disk header. Events are encoded field-by-field (never raw structs) in a
 * fixed little-endian representation, so .clayrec is cross-platform. */
#define CLAYREC_MAGIC UINT64_C(0x434C415952454301) /* "CLAYREC\x01"       */
#define CLAYREC_VERSION 3u

void cl_input_log_init(cl_input_log *log, cl_arena *a, size_t cap) {
    log->arena = a;
    log->cap = cap ? cap : 256;
    log->items = (cl_input_event *)cl_arena_alloc(
        a, log->cap * sizeof(cl_input_event), _Alignof(cl_input_event));
    log->count = 0;
    log->fingerprint = 0x9E3779B97F4A7C15ULL;
}

static uint64_t fingerprint_event(const cl_input_event *e, uint64_t seed) {
    uint32_t type = (uint32_t)e->type;
    uint32_t key = (uint32_t)e->key;
    uint32_t mods = (uint32_t)e->mods;
    uint32_t wheel = (uint32_t)e->wheel;
    uint32_t focus = e->focus ? 1u : 0u;
    seed = cl_hash_bytes(&e->frame, sizeof(e->frame), seed);
    seed = cl_hash_bytes(&e->time, sizeof(e->time), seed);
    seed = cl_hash_bytes(&type, sizeof(type), seed);
    seed = cl_hash_bytes(&key, sizeof(key), seed);
    seed = cl_hash_bytes(&mods, sizeof(mods), seed);
    seed = cl_hash_bytes(&e->x, sizeof(e->x), seed);
    seed = cl_hash_bytes(&e->y, sizeof(e->y), seed);
    seed = cl_hash_bytes(&e->dx, sizeof(e->dx), seed);
    seed = cl_hash_bytes(&e->dy, sizeof(e->dy), seed);
    seed = cl_hash_bytes(&wheel, sizeof(wheel), seed);
    return cl_hash_bytes(&focus, sizeof(focus), seed);
}

void cl_input_log_append(cl_input_log *log, const cl_input_event *e) {
    if (log->count == log->cap) {
        size_t cap = log->cap * 2;
        cl_input_event *items = (cl_input_event *)cl_arena_alloc(
            log->arena, cap * sizeof(cl_input_event), _Alignof(cl_input_event));
        for (size_t i = 0; i < log->count; i++) items[i] = log->items[i];
        log->items = items;
        log->cap = cap;
    }
    log->items[log->count++] = *e;
    log->fingerprint = fingerprint_event(e, log->fingerprint);
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

static bool write_u32(FILE *f, uint32_t v) {
    uint8_t bytes[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16),
                        (uint8_t)(v >> 24)};
    return fwrite(bytes, sizeof(bytes), 1, f) == 1;
}

static bool write_u64(FILE *f, uint64_t v) {
    uint8_t bytes[8];
    for (size_t i = 0; i < sizeof(bytes); i++)
        bytes[i] = (uint8_t)(v >> (i * 8));
    return fwrite(bytes, sizeof(bytes), 1, f) == 1;
}

static bool write_f64(FILE *f, double v) {
    uint64_t bits = 0;
    memcpy(&bits, &v, sizeof(bits));
    return write_u64(f, bits);
}

static bool read_u32(FILE *f, uint32_t *out) {
    uint8_t bytes[4];
    if (fread(bytes, sizeof(bytes), 1, f) != 1) return false;
    *out = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return true;
}

static bool read_u64(FILE *f, uint64_t *out) {
    uint8_t bytes[8];
    if (fread(bytes, sizeof(bytes), 1, f) != 1) return false;
    *out = 0;
    for (size_t i = 0; i < sizeof(bytes); i++)
        *out |= (uint64_t)bytes[i] << (i * 8);
    return true;
}

static bool read_f64(FILE *f, double *out) {
    uint64_t bits = 0;
    if (!read_u64(f, &bits)) return false;
    memcpy(out, &bits, sizeof(bits));
    return true;
}

static bool encode_event(FILE *f, const cl_input_event *e) {
    return write_u32(f, e->frame) && write_f64(f, e->time) &&
           write_u32(f, (uint32_t)e->type) && write_u32(f, (uint32_t)e->key) &&
           write_u32(f, (uint32_t)e->mods) && write_f64(f, e->x) &&
           write_f64(f, e->y) && write_f64(f, e->dx) && write_f64(f, e->dy) &&
           write_f64(f, (double)e->wheel) && write_u32(f, e->focus ? 1u : 0u);
}

static bool decode_event(FILE *f, cl_input_event *e, double *wheel_value) {
    uint32_t type = 0;
    uint32_t key = 0;
    uint32_t mods = 0;
    uint32_t focus = 0;
    double decoded_wheel = 0.0;
    *e = (cl_input_event){0};
    if (!read_u32(f, &e->frame) || !read_f64(f, &e->time) ||
        !read_u32(f, &type) || !read_u32(f, &key) || !read_u32(f, &mods) ||
        !read_f64(f, &e->x) || !read_f64(f, &e->y) || !read_f64(f, &e->dx) ||
        !read_f64(f, &e->dy) || !read_f64(f, &decoded_wheel) ||
        !read_u32(f, &focus))
        return false;
    e->type = (cl_input_kind)type;
    e->key = (cl_key)key;
    e->mods = (int)mods;
    *wheel_value = decoded_wheel;
    e->wheel = 0;
    e->focus = focus != 0;
    return true;
}

static bool valid_decoded_event(const cl_input_event *e, double wheel_value) {
    if (e->type < CLAY_IN_PRESS || e->type > CLAY_IN_FOCUS) return false;
    if (e->key < CLAY_KEY_NONE || e->key >= CLAY_KEY_COUNT) return false;
    if ((e->type == CLAY_IN_PRESS || e->type == CLAY_IN_RELEASE) &&
        e->key == CLAY_KEY_NONE)
        return false;
    if (!isfinite(e->time) || !isfinite(e->x) || !isfinite(e->y) ||
        !isfinite(e->dx) || !isfinite(e->dy) || !isfinite(wheel_value))
        return false;
    if (wheel_value < (double)INT_MIN || wheel_value > (double)INT_MAX ||
        wheel_value != trunc(wheel_value))
        return false;
    return true;
}

cl_err cl_input_log_save(cl_input_log *log, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return CLAY_ERR_IO;
    bool ok = write_u64(f, CLAYREC_MAGIC) && write_u32(f, CLAYREC_VERSION) &&
              write_u64(f, log->count) && write_u64(f, log->fingerprint);
    for (size_t i = 0; ok && i < log->count; i++)
        ok = encode_event(f, &log->items[i]);
    if (fclose(f) != 0) ok = false;
    return ok ? CLAY_OK : CLAY_ERR_IO;
}

cl_err cl_input_log_load(cl_input_log *log, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return CLAY_ERR_IO;
    uint64_t magic = 0;
    uint32_t version = 0;
    uint64_t count = 0;
    uint64_t stamp = 0;
    if (!read_u64(f, &magic) || !read_u32(f, &version) ||
        !read_u64(f, &count) || !read_u64(f, &stamp)) {
        fclose(f);
        return CLAY_ERR_PARSE;
    }
    if (magic != CLAYREC_MAGIC || version != CLAYREC_VERSION) {
        fclose(f);
        return CLAY_ERR_PARSE;
    }
    if (count > (uint64_t)1000000000) {
        fclose(f);
        return CLAY_ERR_PARSE;
    }
    if (count > SIZE_MAX / sizeof(cl_input_event)) {
        fclose(f);
        return CLAY_ERR_PARSE;
    }
    size_t capacity = count > 256 ? (size_t)count : 256;
    if (capacity >
        cl_arena_remaining_bytes(log->arena) / sizeof(cl_input_event)) {
        fclose(f);
        return CLAY_ERR_PARSE;
    }
    cl_arena_frame scratch = cl_arena_temp(log->arena);
    cl_input_event *items = (cl_input_event *)cl_arena_alloc(
        log->arena, capacity * sizeof(cl_input_event),
        _Alignof(cl_input_event));
    uint64_t loaded_fingerprint = 0x9E3779B97F4A7C15ULL;
    for (uint64_t i = 0; i < count; i++) {
        double wheel_value = 0.0;
        /* Decode once more through a temporary so the on-disk floating-point
         * wheel value can be range-checked before its int conversion. */
        cl_input_event decoded;
        if (!decode_event(f, &decoded, &wheel_value)) {
            fclose(f);
            cl_arena_return(scratch);
            return CLAY_ERR_PARSE;
        }
        if (!valid_decoded_event(&decoded, wheel_value)) {
            fclose(f);
            cl_arena_return(scratch);
            return CLAY_ERR_PARSE;
        }
        decoded.wheel = (int)wheel_value;
        items[i] = decoded;
        loaded_fingerprint = fingerprint_event(&items[i], loaded_fingerprint);
    }
    fclose(f);
    if (loaded_fingerprint != stamp) {
        cl_arena_return(scratch);
        return CLAY_ERR_PARSE;
    }
    if (capacity <= log->cap) {
        for (size_t i = 0; i < (size_t)count; i++) log->items[i] = items[i];
        cl_arena_return(scratch);
    } else {
        log->items = items;
        log->cap = capacity;
    }
    log->count = (size_t)count;
    log->fingerprint = loaded_fingerprint;
    return CLAY_OK;
}
