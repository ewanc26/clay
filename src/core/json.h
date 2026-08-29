#ifndef CLAY_CORE_JSON_H
#define CLAY_CORE_JSON_H

#include "arena.h"
#include "common.h"
#include "variant.h"

#include <stdbool.h>
#include <stdint.h>

/* Arena-backed JSON value tree. Root/live nodes are translucent views into
 * arena-allocated key and string storage (paths "a.b[2].c"). Objects keep
 * insertion order in a flat pair array (no hashing needed for config-sized
 * files). */
typedef enum cl_json_kind {
    CLAY_J_NIL = 0,
    CLAY_J_BOOL,
    CLAY_J_I64,
    CLAY_J_F64,
    CLAY_J_STR,
    CLAY_J_OBJ,
    CLAY_J_ARR
} cl_json_kind;

typedef struct cl_json_pair {
    cl_str key;
    struct cl_json_node *val;
} cl_json_pair;

typedef struct cl_json_object {
    cl_json_pair *pairs;
    size_t n;
} cl_json_object;

typedef struct cl_json_array {
    struct cl_json_node **items;
    size_t n;
} cl_json_array;

typedef struct cl_json_node {
    cl_json_kind kind;
    union {
        bool b;
        int64_t i;
        double f;
        cl_str s;
        cl_json_object obj;
        cl_json_array arr;
    };
} cl_json_node;

cl_err cl_json_parse(cl_json_node *out, cl_arena *a, cl_str text);
cl_err cl_json_write(cl_json_node *root, cl_arena *a, cl_str *out);

/* dotted path lookup ("player.velocity[1]"), returns NULL on traversal miss */
cl_json_node *cl_json_get(cl_json_node *root, cl_str path);
cl_json_node *cl_json_get_cstr(cl_json_node *root, const char *path);
/* scalar coercion of whatever sits at path (nil if missing) */
cl_variant cl_json_lookup(cl_json_node *root, cl_str path, bool *ok);

cl_variant cl_json_to_variant(cl_json_node *node);

#endif /* CLAY_CORE_JSON_H */