# Godot Mono integration

`ClayRuntime.cs` is a small managed facade over Clay's opaque C ABI. The
directory also contains a minimal `project.godot` + `ClayDemo.cs` sample:
copy the shared native `clay_engine` library (`.dylib`, `.so`, or `.dll`) in
the platform's native library location, open the directory in a Godot Mono
editor, and run the project. Configure Clay with `-DCLAY_BUILD_SHARED=ON`
(the default) to produce that library.

`cmake --install build --prefix sdk` places the shared library beside the
sample project at `sdk/integrations/godot-mono/`, where
`DllImport("clay_engine")` can resolve it immediately. It also keeps a copy at
`sdk/integrations/godot-mono/native/<platform>/` (`macos`, `linux`, or
`windows`) for platform-specific export staging.

`managed/ClayRuntime.csproj` builds the wrapper independently as a `net8.0`
library, which is useful for checking the binding without the Godot editor.
`ClayGodotSample.csproj` is the Godot SDK project opened by the Mono editor.
`ClayRuntime` checks the exported ABI version during construction and fails
early if the native library does not match the wrapper contract.
Building the sample project requires the Godot .NET SDK to be installed or
available through NuGet; the standalone wrapper project does not require it.
`ClayKey.cs` provides typed key constants for `ClayRuntime.FeedKey` instead of
requiring hosts to pass numeric ABI values.
`ClayModifiers.cs` provides typed modifier flags for the `FeedKeyAt` overload.
`ClayError` provides symbolic names for native failures reported by the
managed facade, so invalid arguments, parse errors, I/O failures, and arena
exhaustion are distinguishable in host exceptions.

After building Clay and installing Godot Mono, the bundled smoke test stages a
temporary project and verifies the managed build, Godot solution import, and
headless game startup:

```sh
GODOT_MONO_BIN=/path/to/godot-mono ./scripts/godot_mono_smoke.sh
```

The script defaults to `godot-mono` on `PATH` and auto-detects the standard
macOS, Linux, or Windows library in `build/`; set `CLAY_NATIVE_LIBRARY` for a
custom platform or build directory.
`ClayRuntime.IsKeyDown` exposes the authoritative held-key state.
`ClayRuntime.IsFocused` exposes focus state, and `FeedKeyAt` preserves canvas
coordinates and modifier bits for positioned key or mouse-button events.
`ClayRuntime.IsReplaying` reports whether the runtime is currently driven by a
loaded transcript.
`ClayRuntime.SimDelta` and `ClayRuntime.TimeScale` expose the effective timing
values used by the deterministic simulation.
`RecordingCount` and `RecordingFingerprint` let hosts verify replay data after
loading a recording.
The sample also tracks the Godot viewport and recreates its RGBA upload buffer
when the viewport changes. It forwards the supported keyboard, mouse-button,
mouse-motion, wheel, digital joypad, and application-focus events through the
managed facade. Joypad A/B/X/Y, shoulders, start/back, stick buttons, and the
four D-pad directions map to Clay's corresponding `ClayKey` values.

The wrapper owns the native handle with `SafeHandle`; framebuffer data is
copied out as packed `0x00RRGGBB` pixels. The wrapper deliberately does not
marshal Clay's C++ headers or expose native pointers to managed code.

This is the Mono/P/Invoke integration path, not a Godot GDExtension. A
GDExtension requires a Godot entry symbol and a `.gdextension` manifest in
addition to a shared library; that adapter is a separate future integration.

The committed `export_presets.cfg` provides a reproducible macOS export. After
exporting, use the bundled macOS helper to stage the platform library into the
app's native search path:

```sh
./scripts/godot_mono_export_macos.sh build/ClayGodotSample.app
```

The reusable `scripts/godot_mono_stage_native.sh` helper accepts an exported
player path and native library path, placing the library beside a desktop
executable or in `Contents/MacOS` for a macOS app bundle.

Godot currently packs arbitrary `.dylib` files as project resources; macOS
cannot resolve a P/Invoke library from inside that resource pack. The helper
keeps the macOS export and staging steps together. Linux and Windows exported
players still require platform-specific validation and staging coverage; see
`docs/issues.md`.

The sample reuses its managed pixel and RGBA conversion buffers each frame;
production hosts can upload through a native texture bridge for further
optimization. CI coverage and platform packaging are tracked in
`docs/issues.md`.
