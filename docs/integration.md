# Integration

Clay can be consumed from an install prefix as a static C/C++ library. The
public C ABI is exposed through `clay/clay.h`; the C++ runtime headers are
installed below `clay/engine`.

```sh
cmake -S . -B build -DCLAY_BUILD_INTERACTIVE=OFF
cmake --build build
cmake --install build --prefix /path/to/clay-sdk
```

An external C or C++ target can then discover Clay with
`find_package(clay CONFIG REQUIRED)` and link `clay::clay_engine`. Host engines should drive `Runtime` from
their own update loop, feed raw `cl_input_event` values, call `update`, and
consume the framebuffer after `render`. A Godot Mono binding should keep the
managed layer at the C ABI boundary and expose an explicit native-library
lifetime handle; the C++ headers are not a binding ABI.

For a managed binding, include `clay/engine_c.h` and P/Invoke the
`cl_engine_runtime_*` functions. The runtime handle is opaque, and framebuffer
pixels are packed `0x00RRGGBB` values in row-major order.

The host can load reaction JSON and seed a scene through the C ABI with
`cl_engine_runtime_load_reactions`, `cl_engine_runtime_spawn_species`, and
`cl_engine_runtime_spawn_ripple`. Invalid JSON returns `CLAY_ERR_PARSE`.
Call `cl_engine_runtime_install_builtin_systems` once after creation when the
host wants movement, cursor attraction, lifespans, hue drift, and ripple
simulation.
