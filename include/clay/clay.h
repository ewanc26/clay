#ifndef CLAY_ENGINE_CORE_UMBRELLA_CLAY_H
#define CLAY_ENGINE_CORE_UMBRELLA_CLAY_H

/* ---------------------------------------------------------------------------
 * clay.h — the one public C ABI of the Clay engine.
 *
 * The C23 core (src/core) declares all its types and functions here and only
 * here for consumers. Compiles as strict C23 on the C side and as `extern
 * "C"` from the C++23 engine; no C++ header is ever visible to the core.
 *
 * Names: cl_ prefix, cl_T + cl_T_free ownership contract for heap-ish
 * objects, value structs trivially copyable.
 * ------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

#if __has_include("clay/core/common.h")
#include "clay/core/common.h"
#include "clay/core/arena.h"
#include "clay/core/log.h"
#include "clay/core/math.h"
#include "clay/core/variant.h"
#include "clay/core/hmap.h"
#include "clay/core/json.h"
#include "clay/core/rng.h"
#include "clay/core/time.h"
#include "clay/core/input.h"
#include "clay/core/event.h"
#include "clay/core/input_log.h"
#else
#include "../src/core/common.h"
#include "../src/core/arena.h"
#include "../src/core/log.h"
#include "../src/core/math.h"
#include "../src/core/variant.h"
#include "../src/core/hmap.h"
#include "../src/core/json.h"
#include "../src/core/rng.h"
#include "../src/core/time.h"
#include "../src/core/input.h"
#include "../src/core/event.h"
#include "../src/core/input_log.h"
#endif

/* Shared event-channel names. Both sides publish and subscribe using these
 * exact strings so C and C++ can never drift apart. */
#define CLAY_CH_INPUT_KEY "input.key"
#define CLAY_CH_INPUT_MOTION "input.motion"
#define CLAY_CH_INPUT_BUTTON "input.button"
#define CLAY_CH_INPUT_WHEEL "input.wheel"
#define CLAY_CH_INPUT_FOCUS "input.focus"
#define CLAY_CH_ACTION "action"
#define CLAY_CH_COMMAND "command"
#define CLAY_CH_FRAME "frame"
#define CLAY_CH_RULE "rule"
#define CLAY_CH_WORLD "world"

#define CLAY_CH_COLLISION "collision"

#define CLAY_CH_DEBUG "debug"
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CLAY_ENGINE_CORE_UMBRELLA_CLAY_H */
