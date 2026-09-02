/* clock_gettime and CLOCK_MONOTONIC are POSIX, not C standard. macOS
 * exposes them from <time.h> by default, but glibc hides them until
 * _POSIX_C_SOURCE is defined. Must be set before any system header. */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 199309L
#endif

#include "time.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static double windows_frequency(void) {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (double)frequency.QuadPart;
}

double cl_time_seconds(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / windows_frequency();
}

int64_t cl_time_millis(void) {
    return (int64_t)(cl_time_seconds() * 1000.0);
}

#elif defined(__APPLE__) || defined(__linux__) || defined(__unix__)

#include <time.h>

double cl_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int64_t cl_time_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

#endif
