#ifndef CLAY_CORE_COMMON_H
#define CLAY_CORE_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ errors */

typedef enum cl_err {
    CLAY_OK = 0,
    CLAY_ERR_OOM,          /* arena exhausted                                     */
    CLAY_ERR_PARSE,        /* malformed text/bytes                               */
    CLAY_ERR_NOT_FOUND,    /* key/path/element absent                            */
    CLAY_ERR_TYPE_MISMATCH /* value had an unexpected kind                        */
    ,
    CLAY_ERR_INVALID_ARG,
    CLAY_ERR_IO,
    CLAY_ERR_FULL,
    CLAY_ERR_OVERFLOW
} cl_err;

const char *cl_err_str(cl_err e);

/* ----------------------------------------------------------------- strings */

/* Length-sliced string. Never NUL-terminated, never owned. */
typedef struct cl_str {
    const char *data;
    size_t len;
} cl_str;

static inline cl_str cl_str_c(const char *s) {
    cl_str r = {s, 0};
    while (r.data && r.data[r.len] != '\0') r.len += 1;
    return r;
}

static inline cl_str cl_str_make(const char *data, size_t len) {
    cl_str r = {data, len};
    return r;
}

static inline bool cl_str_eq(cl_str a, cl_str b) {
    if (a.len != b.len) return false;
    if (a.data == b.data) return true;
    if (a.len == 0) return true;
    return a.data && b.data && __builtin_memcmp(a.data, b.data, a.len) == 0;
}

static inline bool cl_str_empty(cl_str s) {
    return s.len == 0;
}

static inline const char *cl_str_view(cl_str s) {
    return s.data ? s.data : "";
}

/* Compare ignoring length-field drift; lexicographic, memcmp-like. */
int cl_str_cmp(cl_str a, cl_str b);

/* ------------------------------------------------------------ small macros */

#define CL_ARRAY_LEN(a) (sizeof((a)) / sizeof((a)[0]))

#define CL_PI 3.14159265358979323846264338327950288

#define cl_min(a, b) ((a) < (b) ? (a) : (b))
#define cl_max(a, b) ((a) > (b) ? (a) : (b))
#define cl_clamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

/* Fatal invariant violation: arena contract, impossible state. */
void cl_panic(const char *msg, const char *file, int line);
#define CL_PANIC(msg) cl_panic((msg), __FILE__, __LINE__)

#endif /* CLAY_CORE_COMMON_H */