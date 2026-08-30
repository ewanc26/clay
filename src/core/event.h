#ifndef CLAY_CORE_EVENT_H
#define CLAY_CORE_EVENT_H

#include "arena.h"
#include "common.h"
#include "variant.h"

#include <stdbool.h>
#include <stdint.h>

/* -------------------------------------------------- channels (interned) --- */

typedef uint32_t cl_channel;
#define CLAY_CHANNEL_NONE ((cl_channel)0)

/* Interns a channel name to a stable id for the process lifetime. The name is
 * copied into a process-wide arena; guards against two spellings of the same
 * channel ("input.key" vs "input.Key") silently diverging. */
cl_channel cl_channel_intern(cl_str name);
cl_channel cl_channel_intern_cstr(const char *name);
const char *cl_channel_name(cl_channel ch);

/* -------------------------------------------------------------- event bus */

typedef struct cl_event {
    cl_channel channel;
    const char *channel_name;
    cl_variant value;
    uint32_t frame;
    double time;
} cl_event;

typedef struct cl_sub cl_sub;

typedef void (*cl_event_fn)(void *ctx, const cl_event *ev);

typedef struct cl_bus {
    cl_arena *arena;
    cl_sub **heads; /* indexed by cl_channel                       */
    size_t nchannels;
    size_t count;   /* total live subscriptions                    */
} cl_bus;

void cl_bus_init(cl_bus *b, cl_arena *a);

cl_sub *cl_bus_subscribe(cl_bus *b, cl_channel ch, cl_event_fn fn, void *ctx);
/* Unsubscribes and removes the node from the bus. Safe on a live bus between
 * publishes. */
void cl_bus_unsubscribe(cl_sub *s);

void cl_bus_publish(cl_bus *b, cl_channel ch, cl_variant value);
void cl_bus_publish_at(cl_bus *b, cl_channel ch, cl_variant value, uint32_t frame,
                       double time);

size_t cl_bus_subscriber_count(const cl_bus *b);
size_t cl_bus_channel_count(const cl_bus *b);

#endif /* CLAY_CORE_EVENT_H */