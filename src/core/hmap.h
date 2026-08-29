#ifndef CLAY_CORE_HMAP_H
#define CLAY_CORE_HMAP_H

#include "arena.h"
#include "common.h"
#include "variant.h"

#include <stdbool.h>
#include <stdint.h>

/* String-keyed open-addressing map from arena memory, values are cl_variant.
 * Tombstones instead of compaction on remove (linear arena). Grows by
 * allocating a fresh, larger table from the same arena and rehashing; the old
 * table bytes are simply abandoned. Not thread-safe. */
typedef struct cl_hmap_entry {
    uint64_t hash;
    cl_str key;
    cl_variant value;
    bool used;
    bool tombstone;
} cl_hmap_entry;

typedef struct cl_hmap {
    cl_arena *arena;
    cl_hmap_entry *entries;
    size_t cap; /* always a power of two                */
    size_t len; /* live entries (excluding tombstones)  */
    size_t tombstones;
} cl_hmap;

void cl_hmap_create(cl_hmap *m, cl_arena *a, size_t cap);
size_t cl_hmap_len(const cl_hmap *m);

/* Returns a pointer to the stored value, or NULL. Stored keys are arena
 * copies, so passing a transient cl_str is safe. */
cl_variant *cl_hmap_get(cl_hmap *m, cl_str key);
cl_variant *cl_hmap_get_cstr(cl_hmap *m, const char *key);

/* Returns the value pointer (existing, or newly inserted value zeroed). */
cl_variant *cl_hmap_put(cl_hmap *m, cl_str key, cl_variant value);
cl_variant *cl_hmap_put_cstr(cl_hmap *m, const char *key, cl_variant value);

bool cl_hmap_remove(cl_hmap *m, cl_str key);
void cl_hmap_clear(cl_hmap *m);

/* Ordered forward scan; index must start at 0 and stay in range. */
typedef struct cl_hmap_iter {
    size_t index;
} cl_hmap_iter;

cl_hmap_entry *cl_hmap_next(cl_hmap *m, cl_hmap_iter *it);

#endif /* CLAY_CORE_HMAP_H */