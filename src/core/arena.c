#include "arena.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cl_arena_init(cl_arena *a, void *base, size_t bytes) {
    a->base = (char *)base;
    a->cap = bytes;
    a->off = 0;
    a->used = 0;
}

void cl_arena_reset(cl_arena *a) {
    a->off = 0;
}

void *cl_arena_alloc(cl_arena *a, size_t size, size_t align) {
    if (align == 0) align = 1;
    size_t off = (a->off + align - 1) & ~(align - 1);
    if (size > a->cap - off) {
        CL_PANIC("cl_arena_alloc: arena exhausted");
    }
    void *p = a->base + off;
    memset(p, 0, size);
    a->off = off + size;
    if (a->off > a->used) a->used = a->off;
    return p;
}

void *cl_arena_reserve(cl_arena *a, size_t size, size_t align) {
    if (align == 0) align = 1;
    size_t off = (a->off + align - 1) & ~(align - 1);
    if (size > a->cap - off) {
        CL_PANIC("cl_arena_reserve: arena exhausted");
    }
    void *p = a->base + off;
    a->off = off + size;
    if (a->off > a->used) a->used = a->off;
    return p;
}

size_t cl_arena_remaining_bytes(cl_arena *a) {
    return a->cap - a->off;
}

static size_t align_up(size_t v, size_t align) {
    return (v + align - 1) & ~(align - 1);
}

char *cl_arena_strcpy(cl_arena *a, cl_str s) {
    size_t n = align_up(s.len + 1, 1);
    char *p = (char *)cl_arena_alloc(a, n, 1);
    if (s.len > 0) memcpy(p, s.data, s.len);
    p[s.len] = '\0';
    return p;
}

char *cl_arena_cstr(cl_arena *a, const char *s) {
    return cl_arena_strcpy(a, cl_str_c(s));
}

cl_arena_frame cl_arena_temp(cl_arena *a) {
    cl_arena_frame f = {a, a->off};
    return f;
}

void cl_arena_return(cl_arena_frame f) {
    f.a->off = f.saved;
}

cl_arena_buf cl_arena_buf_make(cl_arena *a, size_t reserve) {
    cl_arena_buf b;
    b.a = a;
    b.frame = cl_arena_temp(a);
    b.cap = reserve;
    b.data = (char *)cl_arena_alloc(a, reserve, 1);
    b.len = 0;
    return b;
}

static void cl_arena_buf_grow(cl_arena_buf *b, size_t need) {
    if (b->len + need <= b->cap) return;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->len + need) cap *= 2;
    /* Linear arena: can only grow by moving forward. The old bytes stay on
     * the arena and the buffer is rewritten from the new offset. */
    char *p = (char *)cl_arena_alloc(b->a, cap, 1);
    memcpy(p, b->data, b->len);
    b->data = p;
    b->cap = cap;
}

void cl_arena_buf_append(cl_arena_buf *b, const char *data, size_t len) {
    cl_arena_buf_grow(b, len);
    memcpy(b->data + b->len, data, len);
    b->len += len;
}

void cl_arena_buf_printf(cl_arena_buf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        va_end(ap2);
        return;
    }
    cl_arena_buf_grow(b, (size_t)n + 1);
    vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)n;
}

cl_str cl_arena_buf_deposit(cl_arena_buf *b) {
    /* Persist the accumulated bytes in the arena and hand back a slice that
     * stays valid for the arena's lifetime. Deliberately does NOT rewind the
     * frame: deposit is "commit to arena", not "forget the scratch". */
    cl_str out = {b->data, b->len};
    return out;
}