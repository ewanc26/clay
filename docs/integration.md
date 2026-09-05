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
The C++ `Runtime::resize` method returns `false` for invalid dimensions and
`true` after resizing the authoritative framebuffer.
The C++ constructor and resize path also reject non-positive dimensions,
integer-overflowing sizes, and surfaces larger than
`CLAY_ENGINE_MAX_FRAMEBUFFER_PIXELS` before allocating the framebuffer.

The install package is validated with both a C ABI consumer and a C++ consumer;
the latter includes the public `<clay/engine.hpp>` umbrella, constructs a
`Runtime`, steps one frame, and checks the installed framebuffer. Standalone
C++ hosts can also call `Runtime::load_actions_file`,
`Runtime::load_reactions_file`, `Runtime::load_scene`, or
`Runtime::load_scene_file`; the runtime owns the loaded scene and its render
system until `Runtime::unload_scene`.

For a managed binding, include `clay/engine_c.h` and P/Invoke the
`cl_engine_runtime_*` functions. The runtime handle is opaque, and framebuffer
pixels are packed `0x00RRGGBB` values in row-major order.
The default C ABI constructor reserves 4 MiB for engine state; hosts with
larger scenes can use `cl_engine_runtime_create_with_arena` to provide an
explicit arena size in bytes. A zero-sized arena is rejected.
The public `CLAY_ENGINE_MIN_ARENA_BYTES` constant gives the smallest safe
value for runtime initialization; smaller values are rejected.
`CLAY_ENGINE_MAX_FRAMEBUFFER_PIXELS` bounds C ABI framebuffer allocations;
hosts needing a larger surface should tile or use multiple runtimes.
Before creating a runtime, hosts may compare
`cl_engine_runtime_abi_version()` with `CLAY_ENGINE_ABI_VERSION` to detect a
native library/binding mismatch.
Hosts can turn any returned `cl_err` into a stable diagnostic with
`cl_engine_error_string`; the managed wrapper includes that text in its
exceptions.
For texture uploads, `cl_engine_runtime_pixels_rgba` exposes the same frame as
RGBA8 bytes and avoids a host-side format conversion.
The pixel accessors may be called with a null count pointer when the host only
needs the frame pointer; they return null for a null runtime.
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
Filesystem-backed hosts can use `cl_engine_runtime_load_actions_file` and
`cl_engine_runtime_load_reactions_file` to keep those data files outside host
source code; unreadable paths return `CLAY_ERR_IO`.
Hosts can also load a complete version-1 `.clay` document with
`cl_engine_runtime_load_scene`; its render settings resolution is applied to
the runtime and its 3D scene becomes the active render system. Use
`cl_engine_runtime_has_scene` and `cl_engine_runtime_unload_scene` to manage
that owned scene. Hosts with a filesystem path can use
`cl_engine_runtime_load_scene_file`, which reads and validates the document
inside the native boundary and returns `CLAY_ERR_IO` for an unreadable path.
The managed facade exposes these as `LoadScene`, `LoadSceneFile`, `HasScene`,
and `UnloadScene`.
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
The headless audio mixer is available through
`cl_engine_runtime_audio_load_wav`, `cl_engine_runtime_audio_play`, and
`cl_engine_runtime_audio_unload_clip`, and
`cl_engine_runtime_audio_mix_stereo`; it produces interleaved stereo float32
samples at 48 kHz for a host audio device. `ClayRuntime` exposes the same
operations as `LoadWav`, `UnloadAudio`, `PlayAudio`, and `MixAudio`. Audio loading supports
PCM and IEEE-float WAV clips at the mixer's sample rate; the mixer performs
mono expansion, looping, bus/master gain, and output clamping.
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
Standalone C++ hosts that want Clay to own the platform playback device can
construct `clay::AudioDevice device(runtime.audio())`, call `open()` and then
`start()`, and stop it before destruction. This backend is built by default
and can be disabled with `-DCLAY_BUILD_AUDIO_DEVICE=OFF`; C ABI and Godot hosts
can instead pull mixed samples into their own audio callback.
Developers with an available playback device can smoke-test the real callback
with `CLAY_TEST_AUDIO_DEVICE=1 ./build/test/test_audio`; the regular test suite
does not require hardware.
The standalone window is resizable; the demo synchronizes the runtime canvas
and recreates its presentation texture when the window size changes.
The GLFW presenter also maps auxiliary mouse buttons and keypad digits to the
corresponding Clay keys; keypad multiply has no distinct Clay key and is
ignored.
