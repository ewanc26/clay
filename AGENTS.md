# AGENTS.md

Guidance for AI coding agents working in this repository. Human contributors
may find it useful too, but the audience is agents.

## Project overview

Clay is a bespoke game engine: C23 core + C++23 engine, written from scratch,
no middleware, with a reactive pipeline that answers what the player did at
every layer of the stack.

- **Language:** C23 in `src/core` (the `cl_` ABI, header `include/clay/clay.h`),
  C++23 in `namespace clay` everywhere else. This split is a hard rule, not a
  style choice — see "The divide" below.
- **Build:** CMake, C23/C++23 strict, `-Wall -Wextra -Wpedantic`. Tests are
  per-file executables run through `ctest`, following the account's other
  native repos (`keepsake/`, `wolfram/`).
- **Renderer:** a from-scratch software rasterizer (`src/engine/render/`).
  GLFW is the *only* optional third-party dependency and it is purely a
  presenter — Clay always renders through its own rasterizer first.
- **Target:** macOS and Linux desktop. Windows untested (as elsewhere in this
  account). Everything except interactive `clay_player` runs headless.

## Repository layout

```
include/clay/  clay.h (the C ABI umbrella), clay_engine.hpp (C++ umbrella)
src/core/      C23: common, log, arena, math, variant, hmap, json, rng, time,
               input, event, input_log
src/engine/    C++23:
  event.hpp/cpp        typed C++ facade over the cl_bus
  action.*             logical player actions (names + action map)
  command.*            undoable commands + command log
  replay.*             recorder/replayer over the raw input log
  input_system.*       raw cl_input_event -> actions
  ecs/                 World, Entity, Storage<T>, components
  render/              renderer.hpp (IRenderer), raster.* (software),
                       renderer_sw.*; presenter lives in platform/
  systems/             system_graph.* (System + reactive graph), reaction.*
                       (JSON rules), builtin.* (Movement, CursorMagnet,
                       Lifespan, HueShift, Ripple)
  imageio.*            PNG encode/decode (stb_image*, vendored)
  platform/            window_glfw.* optional GLFW presenter + input
  runtime.*            owns bus/input/log/world/systems/reactions/renderer;
                       the loop
demo/                  "The Clay Garden" vignette + clay_player CLI
test/                  ctest targets, one executable per module
docs/                  design notes
```

## The divide — read before editing

- **Nothing in `src/core` may ever include a C++ header, use C++ features, or
  allocate with anything but the supplied arenas.** The core owns memory,
  math, containers, JSON, input, and events, and must stay linkable from C.
- **C++ code must only enter the core through `include/clay/clay.h`** — no
  direct `src/core/foo.h` includes from engine/demo/test, so the public C ABI
  never forks from what engine code actually compiles against.
- C names are `cl_`-prefixed and follow a uniform `cl_T` + `cl_T_free`
  contract (see `unique_handle` style RAII in `event.hpp`).
- All `cl_*` by-value structs are trivially copyable and safe to hold directly.
- Events are fire-and-forget on a `cl_bus`; if two systems both mutate the
  world in response to one event, order is defined by `SystemGraph` order, not
  by luck.

## Built by running

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
./build/demo/clay_player --headless --frames 60 --dump out/garden.png
```

"Verified" means: clean build (zero warnings under the strict flags),
`ctest` green, and — for anything touching the game loop — a real headless run
with `--dump` producing a non-degenerate PNG plus the deterministic replay
round-trip passing (`test_runtime`).

## Code style

- Header guards (`CLAY_CORE_<FILE>_H`, `CLAY_ENGINE_<MODULE>_<FILE>_HPP`),
  not `#pragma once`.
- `.clang-format` in this repo (LLVM base, 4-space indent, 80 columns,
  attached braces) — run `clang-format -i` on changed files.
- Comments explain *why*, sparingly; never narrate obvious code.
- No C++ exceptions for expected engine states. OOM and programmer errors are
  fatal (the arena "handle it or halt" contract); possible-but-unlikely states
  use `cl_result`/optional-style returns, mirroring wolfram's `wf_result`.
- Keep `src/core` dependency-free at the file level: a `.c` may include only
  `<std*.h>` + sibling `src/core/*.h`. No libc allocation outside arenas
  (except `time.c`, `log.c` file sinks) unless justified in a comment.

## Reactivity invariants

- Every player input must pass through exactly one path:
  `cl_input_state_feed` → bus → `InputSystem` → `CommandQueue` → systems.
  Anything interactive that skips this path is a bug.
- `input_log` records raw events at the source. `Replayer` must reproduce
  deterministic runs: with a fixed seed and no wall-clock reads in systems,
  two replays of the same transcript must render identical commits.
- Reaction rules are data (`reactions.json`-shaped), not code. Adding a rule
  must not require a rebuild.

## Commits and pull requests — MANDATORY

Matches the convention in `wolfram/AGENTS.md` / `keepsake/AGENTS.md`, but
here it is enforced by CI (`scripts/lint_commits.sh`) and cannot be skipped:

- **Scoped conventional commits, atomically.** Every commit is exactly one
  logical change and its message must match
  `^(feat|fix|docs|test|refactor|perf|chore|build|ci|revert)(\(<scope>\))?: <summary>`.
  Scope by module: `core`, `engine`, `ecs`, `render`, `systems`, `input`,
  `action`, `command`, `replay`, `event`, `demo`, `test`, `docs`, `build`,
  `ci`, `scripts`. Examples: `feat(render): ...`, `fix(core): ...`,
  `test(ecs): ...`.
- Never combine a code change with a docs update, or changes to two unrelated
  modules, in one commit. Split multi-concern work into sequential commits.
- **Feature branches, `--no-ff` merges** to `main` so branch structure
  survives in history. Rule of thumb: any commit that is not the first
  commit on `main` should have arrived via `feat/*` or `fix/*` merged with
  `git merge --no-ff`.
- **Honest attribution**: commits may carry `Co-authored-by:` trailers
  crediting the agent and model that did the work.
- **No commented-out code**; delete it or move it to a test.
- Do not open a pull request unless explicitly asked.
- Local gate before every push: `./scripts/lint_commits.sh