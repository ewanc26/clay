#include "rng.h"

void cl_rng_seed(cl_rng *r, uint64_t seed) {
    r->state = seed + 0x9E3779B97F4A7C15ULL;
}

uint64_t cl_rng_next_u64(cl_rng *r) {
    uint64_t z = (r->state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint32_t cl_rng_next_u32(cl_rng *r) {
    return (uint32_t)(cl_rng_next_u64(r) >> 32);
}

double cl_rng_f64(cl_rng *r) {
    return (double)(cl_rng_next_u64(r) >> 11) * (1.0 / 9007199254740992.0);
}

float cl_rng_f32(cl_rng *r) {
    return (float)(cl_rng_next_u64(r) >> 40) * (1.0f / 16777216.0f);
}

double cl_rng_centered(cl_rng *r) {
    return cl_rng_f64(r) * 2.0 - 1.0;
}

int64_t cl_rng_range(cl_rng *r, int64_t lo, int64_t hi) {
    int64_t span = hi - lo;
    if (span <= 0) return lo;
    return lo + (int64_t)(cl_rng_f64(r) * (double)span);
}