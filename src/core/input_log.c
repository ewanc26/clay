#include "input_log.h"

#include "math.h"

#include <stdio.h>
#include <stdlib.h>

/* On-disk header. Events are encoded field-by-field (never raw structs), so a
 * .clayrec round-trips across compilers/platforms with the same endianness. */
#define CLAYREC_MAGIC UINT64_C(0x434C415952454301) /* "CLAYREC\x01"       */
#define CLAYREC_VERSION 2u

void cl_input_log_init(cl_input_log *log, cl_arena *a, size_t cap) {
    log->arena = a;
    log->cap = cap ? cap : 256;
    log->items = (cl_input_event *)cl_arena_alloc(
        a, log->cap * sizeof(cl_input_event), _Alignof(cl_input_event));
    log->count = 0;
    log->fingerprint = 0x9E3779B97F4A7C15ULL;
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
    log->fingerprint =
        cl_hash_bytes(e, sizeof(cl_input_event), log->fingerprint);
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

static void write_u32(FILE *f, uint32_t v) {
    fwrite(&v, sizeof(v), 1, f);
}

static void write_u64(FILE *f, uint64_t v) {
    fwrite(&v, sizeof(v), 1, f);
}

static void write_f64(FILE *f, double v) {
    fwrite(&v, sizeof(v), 1, f);
}

static uint32_t read_u32(FILE *f) {
    uint32_t v = 0;
    if (fread(&v, sizeof(v), 1, f) != 1) return 0;
    return v;
}

static uint64_t read_u64(FILE *f) {
    uint64_t v = 0;
    if (fread(&v, sizeof(v), 1, f) != 1) return 0;
    return v;
}

static double read_f64(FILE *f) {
    double v = 0;
    if (fread(&v, sizeof(v), 1, f) != 1) return 0;
    return v;
}

static void encode_event(FILE *f, const cl_input_event *e) {
    write_u32(f, e->frame);
    write_f64(f, e->time);
    write_u32(f, (uint32_t)e->type);
    write_u32(f, (uint32_t)e->key);
    write_u32(f, (uint32_t)e->mods);
    write_f64(f, e->x);
    write_f64(f, e->y);
    write_f64(f, e->dx);
    write_f64(f, e->dy);
    write_f64(f, (double)e->wheel);
    write_u32(f, e->focus ? 1u : 0u);
}

static cl_input_event decode_event(FILE *f) {
    cl_input_event e = {0};
    e.frame = read_u32(f);
    e.time = read_f64(f);
    e.type = (cl_input_kind)read_u32(f);
    e.key = (cl_key)read_u32(f);
    e.mods = (int)read_u32(f);
    e.x = read_f64(f);
    e.y = read_f64(f);
    e.dx = read_f64(f);
    e.dy = read_f64(f);
    e.wheel = (int)read_f64(f);
    e.focus = read_u32(f) != 0;
    return e;
}

cl_err cl_input_log_save(cl_input_log *log, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return CLAY_ERR_IO;
    write_u64(f, CLAYREC_MAGIC);
    write_u32(f, CLAYREC_VERSION);
    write_u64(f, log->count);
    write_u64(f, log->fingerprint);
    for (size_t i = 0; i < log->count; i++) encode_event(f, &log->items[i]);
    fclose(f);
    return CLAY_OK;
}

cl_err cl_input_log_load(cl_input_log *log, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return CLAY_ERR_IO;
    uint64_t magic = read_u64(f);
    uint32_t version = read_u32(f);
    uint64_t count = read_u64(f);
    uint64_t stamp = read_u64(f);
    if (magic != CLAYREC_MAGIC || version != CLAYREC_VERSION) {
        fclose(f);
        return CLAY_ERR_PARSE;
    }
    if (count > (uint64_t)1000000000) {
        fclose(f);
        return CLAY_ERR_PARSE;
    }
    for (uint64_t i = 0; i < count; i++) {
        cl_input_event e = decode_event(f);
        cl_input_log_append(log, &e);
    }
    fclose(f);
    (void)stamp;
    return CLAY_OK;
}