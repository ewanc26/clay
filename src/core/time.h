#ifndef CLAY_CORE_TIME_H
#define CLAY_CORE_TIME_H

#include <stdint.h>

/* Monotonic seconds since an unspecified epoch (CLOCK_MONOTONIC), used for
 * frame timing. Not affected by wall-clock changes. */
double cl_time_seconds(void);

/* Milliseconds fine-resolution monotonic timestamp. */
int64_t cl_time_millis(void);

#endif /* CLAY_CORE_TIME_H */