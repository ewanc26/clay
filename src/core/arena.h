#ifndef CLAY_CORE_ARENA_H
#define CLAY_CORE_ARENA_H

#include "common.h"

#include <stddef.h>

/* ------------------------------------------------------------------ arenas */

/* Linear allocator over a caller-supplied buffer. The core's "handle it or
 * halt" memory contract: allocation fail, arena exhaustion, and structural
 * misuse are fatal (CL_PANIC), never silently dropped. Reset + temp frames
 * make arena memory cheap enough to be the only allocator the engine needs.
 *
 * NOT thread-safe; one arena per thread that touches it. */
typedef struct cl_arena {
    char *base;
    size_t cap;   /* total bytes                              */
    size_t off;   /* next free offset                         */
    size_t used;  /* high-water mark of off (for diagnostics) */
} cl_arena;

void cl_arena_init(cl_arena *a, void *base, size_t bytes);
void cl_arena_reset(cl_arena *a);

void *cl_arena_alloc(cl_arena *a, size_t size, size_t align);

/* Raw reservation without clearing; for bulk blobs. */
void *cl_arena_reserve(cl_arena *a, size_t size, size_t align);

size_t cl_arena_remaining_bytes(cl_arena *a);

/* NUL-terminated copy helpers (the core's owned-string convention). */
char *cl_arena_strcpy(cl_arena *a, cl_str s);
char *cl_arena_cstr(cl_arena *a, const char *s);

typedef struct cl_arena_frame {
    cl_arena *a;
    size_t saved;
} cl_arena_frame;

cl_arena_frame cl_arena_temp(cl_arena *a);
void cl_arena_return(cl_arena_frame f);

/* Temporary scatter-safe buffer (e.g. serialization scratch). */
typedef struct cl_arena_buf {
    cl_arena *a;
    cl_arena_frame frame;
    char *data;
    size_t len;
    size_t cap;
} cl_arena_buf;

cl_arena_buf cl_arena_buf_make(cl_arena *a, size_t reserve);
cl_str cl_arena_buf_deposit(cl_arena_buf *b); /* returns + releases frame */
void cl_arena_buf_printf(cl_arena_buf *b, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
void cl_arena_buf_append(cl_arena_buf *b, const char *data, size_t len);

#endif /* CLAY_CORE_ARENA_H */