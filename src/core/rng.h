#ifndef CLAY_CORE_RNG_H
#define CLAY_CORE_RNG_H

#include <stdint.h>

/* Deterministic splitmix64 stream. Same seed -> same sequence on every
 * platform; this is what makes recorded replay reproducible. */
typedef struct cl_rng {
    uint64_t state;
} cl_rng;

void cl_rng_seed(cl_rng *r, uint64_t seed);
uint64_t cl_rng_next_u64(cl_rng *r);
uint32_t cl_rng_next_u32(cl_rng *r);
double cl_rng_f64(cl_rng *r);        /* [0, 1)                     */
float cl_rng_f32(cl_rng *r);         /* [0, 1)                     */
double cl_rng_centered(cl_rng *r);   /* [-1, 1)                    */
int64_t cl_rng_range(cl_rng *r, int64_t lo, int64_t hi); /* [lo, hi) */

#endif /* CLAY_CORE_RNG_H */