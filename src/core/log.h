#ifndef CLAY_CORE_LOG_H
#define CLAY_CORE_LOG_H

#include <stdbool.h>
#include <stddef.h>

typedef enum cl_log_level {
    CLAY_LOG_DEBUG = 0,
    CLAY_LOG_INFO,
    CLAY_LOG_WARN,
    CLAY_LOG_ERROR,
    CLAY_LOG_OFF
} cl_log_level;

/* Logging hook; default sink writes "[clay] LEVEL message\n" to stderr.
 * A custom sink replaces the default. Must remain async-safe at level. */
typedef void (*cl_log_sink)(cl_log_level level, const char *msg,
                            size_t len, void *user);

void cl_log_set_level(cl_log_level level);
cl_log_level cl_log_get_level(void);

void cl_log_set_sink(cl_log_sink sink, void *user);

void cl_log(cl_log_level level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define cl_log_debug(...) cl_log(CLAY_LOG_DEBUG, __VA_ARGS__)
#define cl_log_info(...) cl_log(CLAY_LOG_INFO, __VA_ARGS__)
#define cl_log_warn(...) cl_log(CLAY_LOG_WARN, __VA_ARGS__)
#define cl_log_error(...) cl_log(CLAY_LOG_ERROR, __VA_ARGS__)

#endif /* CLAY_CORE_LOG_H */