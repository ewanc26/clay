#ifndef CLAY_CORE_INPUT_LOG_H
#define CLAY_CORE_INPUT_LOG_H

#include "arena.h"
#include "common.h"
#include "input.h"

#include <stdint.h>

/* Append-only transcript of every raw input event, in generation order. The
 * log is what makes the engine answer "what did the player do?" in
 * permanent, replayable form: a saved .clayrec is a *complete* history. */
typedef struct cl_input_log {
    cl_arena *arena;
    cl_input_event *items;
    size_t count;
    size_t cap;
    uint64_t fingerprint; /* running hash, deterministic across builds */
} cl_input_log;

void cl_input_log_init(cl_input_log *log, cl_arena *a, size_t cap);
void cl_input_log_append(cl_input_log *log, const cl_input_event *e);

size_t cl_input_log_count(const cl_input_log *log);
const cl_input_event *cl_input_log_at(const cl_input_log *log, size_t i);

uint64_t cl_input_log_fingerprint(const cl_input_log *log);

cl_err cl_input_log_save(cl_input_log *log, const char *path);
cl_err cl_input_log_load(cl_input_log *log, const char *path);

#endif /* CLAY_CORE_INPUT_LOG_H */