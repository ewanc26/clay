# CLAY

A bespoke game engine from scratch: **C23 core, C++23 engine, no middleware.**
The renderer is our own software rasterizer; the only optional dependency is
GLFW, and only as a presenter for an already-rendered frame.

Clay is *reactive to everything the player does*. Every raw input event is
pushed onto a public event bus, recorded into an append-only log, promoted
into logical *actions*, and turned into undoable *commands*. Systems declare
what they watch, and JSON **reaction rules** bind player input to world
effects without any scripting-language dependency.

```
raw input ──► cl_input_state ──► event bus ──► actions ──► commands ──► ECS systems ──► raster
   │                │                                   │                            │
   └──► input_log (record)                replay ◄──────┘                    framebuffer → GLFW | PNG
```

## The language divide (the "clay divide")

- `src/core/*.h, *.c` — **C23**. Zero C++ headers. One C ABI header,
  `include/clay/clay.h`, compiled as `extern "C"` for the C++ side. All names
  prefixed `cl_`; every owned object obeys the `cl_T` + `cl_T_free` contract.
- `src/engine/*`, `demo/*`, `test/*` — **C++23**, `namespace clay`, consuming
  only `clay.h` for the core layer. Header guards, not `#pragma once`.
- Enforced in CMake: the core target standard is C23, the engine target is
  C++23, and `-Wpedantic` keeps the seam honest.

## Building

```bash
brew install glfw   # optional; interactive windowed app only
cmake -S . -B build
cmake --build build
ctest --test-dir build            # unit tests, all headless
./build/demo/clay_player --help
./build/demo/clay_player --dump out/garden.png
./build/demo/clay_player --scene demo/scenes/tabletop.clay --dump out/scene.png
./build/demo/clay_player --headless --actions actions.json --frames 120
./build/demo/clay_player --record out/take.clayrec --frames 90
./build/demo/clay_player --replay out/take.clayrec --dump out/replay.png
./build/examples/clay_host_c [optional-frame.png]
```

## What's inside

- `src/core` — memory arenas, math, string-keyed maps, JSON, RNG, monotonic
  time, input state, event bus, input recorder/replayer.
- `src/engine` — `Runtime` orchestration, ECS (`World`, typed `Storage<T>`),
  input→action mapping (rebindable from JSON), command log + replay,
  reactive `SystemGraph`, data-driven `ReactionRule`s, and both the 2D
  garden rasterizer and a from-scratch **3D software rasterizer** (depth
  buffer, flat shading, directional + point lights, perspective camera).
- `demo` — *The Clay Garden*: a living, fully headless scene where everything
  reacts to the player. A sculpture anchors the world; a herd of
  cursor-magnet animals drifts toward the mouse; clicks emit ripples, space
  blooms, scrolling embiggens the rings, and every action lands on the
  record. `--scene out.clay` runs a 3D scene from the JSON-based
  **.clay file format** instead.
- `examples` — standalone C host using only the stable `engine_c.h` ABI.
- `docs/` — design notes including the [.clay file format spec](docs/file-format.md).

## Host integrations

For standalone C/C++ hosts, see [the integration guide](docs/integration.md).
For Godot Mono, see [`integrations/godot-mono`](integrations/godot-mono/README.md);
the CMake install also packages the managed sample and platform native
library layout.

## License

AGPL-3.0 — see [LICENSE](LICENSE).
