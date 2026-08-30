#ifndef CLAY_CORE_VARIANT_H
#define CLAY_CORE_VARIANT_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum cl_var_kind {
    CLAY_VAR_NIL = 0,
    CLAY_VAR_BOOL,
    CLAY_VAR_I64,
    CLAY_VAR_F64,
    CLAY_VAR_STR,
    CLAY_VAR_PTR
} cl_var_kind;

const char *cl_var_kind_str(cl_var_kind k);

typedef struct cl_variant {
    cl_var_kind kind;
    union {
        bool b;
        int64_t i;
        double f;
        cl_str s;
        const void *p;
    };
} cl_variant;

static inline cl_variant cl_variant_nil(void) {
    cl_variant v = {CLAY_VAR_NIL, {0}};
    return v;
}
static inline cl_variant cl_variant_bool(bool b) {
    cl_variant v = {CLAY_VAR_BOOL, {0}};
    v.b = b;
    return v;
}
static inline cl_variant cl_variant_i64(int64_t i) {
    cl_variant v = {CLAY_VAR_I64, {0}};
    v.i = i;
    return v;
}
static inline cl_variant cl_variant_f64(double f) {
    cl_variant v = {CLAY_VAR_F64, {0}};
    v.f = f;
    return v;
}
static inline cl_variant cl_variant_str(cl_str s) {
    cl_variant v = {CLAY_VAR_STR, {0}};
    v.s = s;
    return v;
}
static inline cl_variant cl_variant_ptr(const void *p) {
    cl_variant v = {CLAY_VAR_PTR, {0}};
    v.p = p;
    return v;
}

/* Numeric coercion: f64/i64/bool -> double. Returns NaN when not numeric. */
double cl_variant_to_double(cl_variant v);
int64_t cl_variant_to_i64(cl_variant v);
/* Truthiness: nil false, bool as-is, numbers nonzero, strings non-empty. */
bool cl_variant_truthy(cl_variant v);
bool cl_variant_eq(cl_variant a, cl_variant b);

#endif /* CLAY_CORE_VARIANT_H */