#include "test_c.h"

#include <clay/clay.h>

#include <string.h>

static struct {
    cl_channel ch;
    cl_variant v;
    int n;
} sink;

static void on_event(void *ctx, const cl_event *ev) {
    (void)ctx;
    sink.n++;
    sink.ch = ev->channel;
    sink.v = ev->value;
}

static int test_event(void) {
    memset(&sink, 0, sizeof(sink));

    unsigned char buf[8192];
    cl_arena arena;
    cl_arena_init(&arena, buf, sizeof(buf));

    cl_bus bus;
    cl_bus_init(&bus, &arena);

    cl_channel ch = cl_channel_intern(cl_str_c("sampled"));
    cl_channel other = cl_channel_intern(cl_str_c("other"));
    CHECK(ch != other);
    CHECK(strcmp(cl_channel_name(ch), "sampled") == 0);
    /* Interned: same string -> same channel id. */
    CHECK(cl_channel_intern(cl_str_c("sampled")) == ch);

    cl_sub *sub = cl_bus_subscribe(&bus, ch, on_event, &sink);
    CHECK(sub != NULL);

    cl_bus_publish(&bus, ch, cl_variant_i64(41));
    CHECK_EQ_INT(sink.n, 1);
    CHECK(sink.ch == ch);
    CHECK(sink.v.kind == CLAY_VAR_I64 && sink.v.i == 41);

    /* Channel-specific listener does not hear other channels. */
    cl_bus_publish(&bus, other, cl_variant_bool(true));
    CHECK_EQ_INT(sink.n, 1);

    /* Unsubscribing stops delivery. */
    cl_bus_unsubscribe(sub);
    cl_bus_publish(&bus, ch, cl_variant_i64(1));
    CHECK_EQ_INT(sink.n, 1);
    CHECK_EQ_INT(cl_bus_subscriber_count(&bus), 0);
    return clay_test_failures;
}

CLAY_C_TEST_MAIN(test_event)