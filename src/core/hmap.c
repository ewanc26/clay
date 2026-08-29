#include "hmap.h"

#include "math.h"

#include <stdlib.h>

static size_t round_pow2(size_t v) {
    size_t p = 1;
    while (p < v) p <<= 1;
    return p < 8 ? 8 : p;
}

static size_t hmap_slot(const cl_hmap *m, uint64_t hash) {
    return (size_t)(hash & (uint64_t)(m->cap - 1));
}

static void hmap_rehash(cl_hmap *m, size_t new_cap) {
    cl_hmap old = *m;
    m->entries = (cl_hmap_entry *)cl_arena_alloc(m->arena,
                                                  new_cap * sizeof(cl_hmap_entry),
                                                  _Alignof(cl_hmap_entry));
    m->cap = new_cap;
    m->len = 0;
    m->tombstones = 0;
    for (size_t i = 0; i < old.cap; i++) {
        cl_hmap_entry *e = &old.entries[i];
        if (!e->used || e->tombstone) continue;
        cl_hmap_put(m, e->key, e->value);
    }
}

void cl_hmap_create(cl_hmap *m, cl_arena *a, size_t cap) {
    m->arena = a;
    m->cap = round_pow2(cap);
    m->entries = (cl_hmap_entry *)cl_arena_alloc(
        a, m->cap * sizeof(cl_hmap_entry), _Alignof(cl_hmap_entry));
    m->len = 0;
    m->tombstones = 0;
}

size_t cl_hmap_len(const cl_hmap *m) {
    return m->len;
}

static cl_hmap_entry *hmap_find_slot(cl_hmap *m, cl_str key, uint64_t hash,
                                     bool *found) {
    size_t i = hmap_slot(m, hash);
    size_t start = i;
    cl_hmap_entry *tomb = NULL;
    for (;;) {
        cl_hmap_entry *e = &m->entries[i];
        if (!e->used) {
            *found = false;
            return tomb ? tomb : e;
        }
        if (e->tombstone) {
            if (!tomb) tomb = e;
        } else if (e->hash == hash && cl_str_eq(e->key, key)) {
            *found = true;
            return e;
        }
        i = (i + 1) & (m->cap - 1);
        if (i == start) {
            *found = false;
            return tomb ? tomb : e; /* full table; caller will grow */
        }
    }
}

cl_variant *cl_hmap_get(cl_hmap *m, cl_str key) {
    uint64_t hash = cl_hash_str(key, 0);
    bool found = false;
    cl_hmap_entry *e = hmap_find_slot(m, key, hash, &found);
    return found ? &e->value : NULL;
}

cl_variant *cl_hmap_get_cstr(cl_hmap *m, const char *key) {
    return cl_hmap_get(m, cl_str_c(key));
}

cl_variant *cl_hmap_put(cl_hmap *m, cl_str key, cl_variant value) {
    /* Keep occupancy (live + tombstones) under 7/8 so probing terminates. */
    if ((m->len + m->tombstones + 1) * 8 >= m->cap * 7) {
        hmap_rehash(m, m->cap * 2);
    }
    uint64_t hash = cl_hash_str(key, 0);
    bool found = false;
    cl_hmap_entry *e = hmap_find_slot(m, key, hash, &found);
    if (found) {
        e->value = value;
        return &e->value;
    }
    e->hash = hash;
    e->key = cl_str_make(cl_arena_strcpy(m->arena, key), key.len);
    e->value = value;
    e->used = true;
    e->tombstone = false;
    m->len += 1;
    return &e->value;
}

cl_variant *cl_hmap_put_cstr(cl_hmap *m, const char *key, cl_variant value) {
    return cl_hmap_put(m, cl_str_c(key), value);
}

bool cl_hmap_remove(cl_hmap *m, cl_str key) {
    uint64_t hash = cl_hash_str(key, 0);
    bool found = false;
    cl_hmap_entry *e = hmap_find_slot(m, key, hash, &found);
    if (!found) return false;
    e->tombstone = true;
    e->used = true;
    m->len -= 1;
    m->tombstones += 1;
    return true;
}

void cl_hmap_clear(cl_hmap *m) {
    for (size_t i = 0; i < m->cap; i++) {
        m->entries[i].used = false;
        m->entries[i].tombstone = false;
    }
    m->len = 0;
    m->tombstones = 0;
}

cl_hmap_entry *cl_hmap_next(cl_hmap *m, cl_hmap_iter *it) {
    while (it->index < m->cap) {
        cl_hmap_entry *e = &m->entries[it->index++];
        if (e->used && !e->tombstone) return e;
    }
    return NULL;
}