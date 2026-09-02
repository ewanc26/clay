# Integration

Clay can be consumed from an install prefix as a static C/C++ library. The
public C ABI is exposed through `clay/clay.h`; the C++ runtime headers are
installed below `clay/engine`.

```sh
cmake -S . -B build -DCLAY_BUILD_INTERACTIVE=OFF
cmake --build build
cmake --install build --prefix /path/to/clay-sdk
```

For an offline build that explicitly disables the optional GLFW presenter, use
`-DCLAY_BUILD_INTERACTIVE=ON -DCLAY_USE_SYSTEM_GLFW=OFF`; the build remains
headless and does not invoke FetchContent.

An external C or C++ target can then discover Clay with
`find_package(clay CONFIG REQUIRED)` and link `clay::clay_engine`. Host engines should drive `Runtime` from
their own update loop, feed raw `cl_input_event` values, call `update`, and
consume the framebuffer after `render`. A Godot Mono binding should keep the
managed layer at the C ABI boundary and expose an explicit native-library
lifetime handle; the C++ headers are not a binding ABI.

The install package is validated with both a C ABI consumer and a C++ consumer;
the latter includes the public `<clay/engine.hpp>` umbrella, constructs a
`Runtime`, steps one frame, and checks the installed framebuffer.

For a managed binding, include `clay/engine_c.h` and P/Invoke the
`cl_engine_runtime_*` functions. The runtime handle is opaque, and framebuffer
pixels are packed `0x00RRGGBB` values in row-major order.
Before creating a runtime, hosts may compare
`cl_engine_runtime_abi_version()` with `CLAY_ENGINE_ABI_VERSION` to detect a
native library/binding mismatch.
For texture uploads, `cl_engine_runtime_pixels_rgba` exposes the same frame as
RGBA8 bytes and avoids a host-side format conversion.
When the host viewport changes, call `cl_engine_runtime_resize` before the next
step; the framebuffer dimensions and pixel count then reflect the new size.
Hosts can query authoritative `frame`, `sim_time`, scaled simulation delta, and
cursor coordinates with the corresponding `cl_engine_runtime_*` probe
functions. `cl_engine_runtime_time_scale` reports the sanitized scale applied
by the runtime.
Focus state is available through `cl_engine_runtime_is_focused`; losing focus
also releases held keys so hosts cannot leave actions latched. For pointer
buttons and other positioned key events, use
`cl_engine_runtime_feed_key_at` to preserve canvas coordinates and modifier
bits.
Headless hosts can write the latest frame directly with
`cl_engine_runtime_save_png`; malformed arguments return `CLAY_ERR_INVALID_ARG`
and filesystem failures return `CLAY_ERR_IO`.

The host can load action bindings and reaction JSON, then seed a scene through
the C ABI with `cl_engine_runtime_load_actions`,
`cl_engine_runtime_load_reactions`, `cl_engine_runtime_spawn_species`, and
`cl_engine_runtime_spawn_ripple`. Both JSON loaders return `CLAY_ERR_PARSE` for
invalid JSON; loading an action document replaces existing bindings.
Recordings can be persisted with `cl_engine_runtime_save_recording` and
`cl_engine_runtime_load_recording`, then driven with
`cl_engine_runtime_set_replaying`; query the active mode with
`cl_engine_runtime_is_replaying`.
The `.clayrec` wire format uses an explicit little-endian encoding for its
integers and IEEE-754 doubles rather than native struct layout, so recordings
are portable across supported compilers and architectures.
Call `cl_engine_runtime_install_builtin_systems` once after creation when the
host wants movement, cursor attraction, lifespans, hue drift, and ripple
simulation.
Keyboard, motion, wheel, and focus events are available as dedicated helpers
in `engine_c.h`, avoiding managed marshaling of the C event struct. The managed
Godot facade exposes the same behavior through `IsFocused` and `FeedKeyAt`.
Use the managed `ClayKey` and `ClayModifiers` enums to avoid passing raw ABI
integers from C#.
The bundled sample additionally forwards mouse motion, wheel, and application
focus notifications, providing a complete host-loop reference.
The GLFW standalone presenter also polls the first connected GLFW-mapped
gamepad and forwards its digital face, shoulder, menu, stick, and D-pad buttons;
disconnecting it emits releases for any buttons that were held.
The standalone window is resizable; the demo synchronizes the runtime canvas
and recreates its presentation texture when the window size changes.
The GLFW presenter also maps auxiliary mouse buttons and keypad digits to the
corresponding Clay keys; keypad multiply has no distinct Clay key and is
ignored.
