#include "log.h"

#include <stdarg.h>
#include <stdio.h>

static cl_log_level g_level = CLAY_LOG_INFO;
static cl_log_sink g_sink = NULL;
static void *g_sink_user = NULL;

static void default_sink(cl_log_level level, const char *msg, size_t len,
                         void *user) {
    (void)user;
    static const char *const names[] = {"debug", "info ", "warn ", "error"};
    const char *name = names[level < CLAY_LOG_OFF ? level : CLAY_LOG_WARN];
    fprintf(stderr, "[clay] %s %.*s\n", name, (int)len, msg);
}

void cl_log_set_level(cl_log_level level) {
    g_level = level;
}

cl_log_level cl_log_get_level(void) {
    return g_level;
}

void cl_log_set_sink(cl_log_sink sink, void *user) {
    g_sink = sink;
    g_sink_user = user;
}

void cl_log(cl_log_level level, const char *fmt, ...) {
    if (level < g_level || level >= CLAY_LOG_OFF) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    size_t len = (size_t)(n < (int)sizeof(buf) ? n : sizeof(buf) - 1);
    if (g_sink) {
        g_sink(level, buf, len, g_sink_user);
    } else {
        default_sink(level, buf, len, NULL);
    }
}