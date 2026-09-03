#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *cl_err_str(cl_err e) {
    switch (e) {
    case CLAY_OK: return "ok";
    case CLAY_ERR_OOM: return "out of arena memory";
    case CLAY_ERR_PARSE: return "parse error";
    case CLAY_ERR_NOT_FOUND: return "not found";
    case CLAY_ERR_TYPE_MISMATCH: return "type mismatch";
    case CLAY_ERR_INVALID_ARG: return "invalid argument";
    case CLAY_ERR_IO: return "i/o error";
    case CLAY_ERR_FULL: return "capacity exhausted";
    case CLAY_ERR_OVERFLOW: return "overflow";
    }
    return "unknown";
}

int cl_str_cmp(cl_str a, cl_str b) {
    size_t n = a.len < b.len ? a.len : b.len;
    int c = 0;
    if (n > 0) c = memcmp(a.data, b.data, n);
    if (c != 0) return c;
    return a.len < b.len ? -1 : (a.len > b.len ? 1 : 0);
}

void cl_panic(const char *msg, const char *file, int line) {
    fprintf(stderr, "[clay] panic: %s (%s:%d)\n", msg, file, line);
    abort();
}
