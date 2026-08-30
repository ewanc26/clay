#include "event.h"

#include <stdlib.h>
#include <string.h>

/* Process-wide channel registry. Interning keeps channel identity stable for
 * the whole lifetime; nothing is ever freed, because a channel name that an
 * event loop may still hold must not dangle. */
static cl_arena *g_chan_arena = NULL;
static cl_channel g_chan_count = 0;
static const char **g_chan_names = NULL;
static size_t g_chan_cap = 64;

static void chan_room(cl_channel want) {
    if (g_chan_arena == NULL) {
        g_chan_arena = (cl_arena *)malloc(sizeof(cl_arena));
        size_t bytes = 256 * 1024;
        cl_arena_init(g_chan_arena, malloc(bytes), bytes);
        g_chan_names = (const char **)malloc(g_chan_cap * sizeof(char *));
    }
    if ((size_t)want + 1 > g_chan_cap) {
        size_t cap = g_chan_cap * 2;
        while (cap < (size_t)want + 1) cap *= 2;
        g_chan_cap = cap;
        g_chan_names = (const char **)realloc(g_chan_names, cap * sizeof(char *));
    }
}

cl_channel cl_channel_intern(cl_str name) {
    /* Channel 0 is the CLAY_CHANNEL_NONE sentinel — never handed out. The
     * registry is 1-based: the first real channel is id 1. */
    for (cl_channel i = 1; i <= g_chan_count; i++) {
        if (cl_str_eq(cl_str_c(g_chan_names[i]), name)) return i;
    }
    cl_channel id = g_chan_count + 1;
    chan_room(id);
    const char *copy = cl_arena_strcpy(g_chan_arena, name);
    g_chan_names[id] = copy;
    g_chan_count = id;
    return id;
}

cl_channel cl_channel_intern_cstr(const char *name) {
    return cl_channel_intern(cl_str_c(name));
}

const char *cl_channel_name(cl_channel ch) {
    if (ch == 0 || ch > g_chan_count) return "(unknown)";
    return g_chan_names[ch];
}

/* ------------------------------------------------------------------- bus */

struct cl_sub {
    cl_sub *next;
    cl_bus *bus;
    cl_channel channel;
    cl_event_fn fn;
    void *ctx;
    bool live;
};

cl_sub *cl_bus_alloc_sub(cl_bus *b, cl_channel ch, cl_event_fn fn, void *ctx) {
    cl_sub *s = (cl_sub *)cl_arena_alloc(b->arena, sizeof(cl_sub),
                                         _Alignof(cl_sub));
    s->next = NULL;
    s->bus = b;
    s->channel = ch;
    s->fn = fn;
    s->ctx = ctx;
    s->live = true;
    return s;
}

void cl_bus_init(cl_bus *b, cl_arena *a) {
    b->arena = a;
    b->heads = NULL;
    b->nchannels = 0;
    b->count = 0;
}

cl_sub *cl_bus_subscribe(cl_bus *b, cl_channel ch, cl_event_fn fn, void *ctx) {
    if (ch >= b->nchannels) {
        size_t cap = b->nchannels ? b->nchannels * 2 : 16;
        while (cap <= ch) cap *= 2;
        cl_sub **heads =
            (cl_sub **)cl_arena_alloc(b->arena, cap * sizeof(cl_sub *),
                                      _Alignof(cl_sub *));
        memcpy(heads, b->heads, b->nchannels * sizeof(cl_sub *));
        b->heads = heads;
        b->nchannels = cap;
    }
    cl_sub *s = cl_bus_alloc_sub(b, ch, fn, ctx);
    cl_sub **tail = &b->heads[ch];
    while (*tail) tail = &(*tail)->next;
    *tail = s;
    b->count += 1;
    return s;
}

void cl_bus_unsubscribe(cl_sub *s) {
    if (!s->live || s->bus == NULL) return;
    cl_sub **cur = &s->bus->heads[s->channel];
    while (*cur && *cur != s) cur = &(*cur)->next;
    if (*cur == s) {
        *cur = s->next;
        s->bus->count -= 1;
    }
    s->live = false;
    s->next = NULL;
    s->bus = NULL;
}

static void publish_internal(cl_bus *b, cl_channel ch, cl_variant value,
                             uint32_t frame, double time) {
    if (ch >= b->nchannels) return;
    const cl_event ev = {.channel = ch,
                         .channel_name = cl_channel_name(ch),
                         .value = value,
                         .frame = frame,
                         .time = time};
    /* Iterate over a copy of the chain so callbacks may unsubscribe (even
     * themselves) while we walk. */
    cl_sub **snapshot = NULL;
    size_t n = 0;
    for (cl_sub *s = b->heads[ch]; s; s = s->next) n += 1;
    if (n > 0) {
        snapshot = (cl_sub **)cl_arena_alloc(b->arena, n * sizeof(cl_sub *),
                                             _Alignof(cl_sub *));
        size_t i = 0;
        for (cl_sub *s = b->heads[ch]; s; s = s->next) snapshot[i++] = s;
    }
    for (size_t i = 0; i < n; i++) {
        cl_sub *s = snapshot[i];
        if (s->live) s->fn(s->ctx, &ev);
    }
}

void cl_bus_publish(cl_bus *b, cl_channel ch, cl_variant value) {
    publish_internal(b, ch, value, 0, 0.0);
}

void cl_bus_publish_at(cl_bus *b, cl_channel ch, cl_variant value, uint32_t frame,
                       double time) {
    publish_internal(b, ch, value, frame, time);
}

size_t cl_bus_subscriber_count(const cl_bus *b) {
    return b->count;
}

size_t cl_bus_channel_count(const cl_bus *b) {
    return b->nchannels;
}