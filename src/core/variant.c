#include "variant.h"

#include <math.h>

const char *cl_var_kind_str(cl_var_kind k) {
    switch (k) {
    case CLAY_VAR_NIL: return "nil";
    case CLAY_VAR_BOOL: return "bool";
    case CLAY_VAR_I64: return "i64";
    case CLAY_VAR_F64: return "f64";
    case CLAY_VAR_STR: return "str";
    case CLAY_VAR_PTR: return "ptr";
    }
    return "unknown";
}

double cl_variant_to_double(cl_variant v) {
    switch (v.kind) {
    case CLAY_VAR_BOOL: return v.b ? 1.0 : 0.0;
    case CLAY_VAR_I64: return (double)v.i;
    case CLAY_VAR_F64: return v.f;
    default: return NAN;
    }
}

int64_t cl_variant_to_i64(cl_variant v) {
    switch (v.kind) {
    case CLAY_VAR_BOOL: return v.b ? 1 : 0;
    case CLAY_VAR_I64: return v.i;
    case CLAY_VAR_F64: return (int64_t)v.f;
    default: return 0;
    }
}

bool cl_variant_truthy(cl_variant v) {
    switch (v.kind) {
    case CLAY_VAR_NIL: return false;
    case CLAY_VAR_BOOL: return v.b;
    case CLAY_VAR_I64: return v.i != 0;
    case CLAY_VAR_F64: return v.f != 0.0;
    case CLAY_VAR_STR: return v.s.len != 0;
    case CLAY_VAR_PTR: return v.p != NULL;
    }
    return false;
}

bool cl_variant_eq(cl_variant a, cl_variant b) {
    if (a.kind == CLAY_VAR_I64 && b.kind == CLAY_VAR_F64) {
        return (double)a.i == b.f;
    }
    if (a.kind == CLAY_VAR_F64 && b.kind == CLAY_VAR_I64) {
        return a.f == (double)b.i;
    }
    if (a.kind != b.kind) return false;
    switch (a.kind) {
    case CLAY_VAR_NIL: return true;
    case CLAY_VAR_BOOL: return a.b == b.b;
    case CLAY_VAR_I64: return a.i == b.i;
    case CLAY_VAR_F64: return a.f == b.f;
    case CLAY_VAR_STR: return cl_str_eq(a.s, b.s);
    case CLAY_VAR_PTR: return a.p == b.p;
    }
    return false;
}