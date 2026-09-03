# Godot Mono integration

`ClayRuntime.cs` is a small managed facade over Clay's opaque C ABI. The
directory also contains a minimal `project.godot` + `ClayDemo.cs` sample:
copy the shared native `clay_engine` library (`.dylib`, `.so`, or `.dll`) in
the platform's native library location, open the directory in a Godot Mono
editor, and run the project. Configure Clay with `-DCLAY_BUILD_SHARED=ON`
(the default) to produce that library.

`cmake --install build --prefix sdk` places the shared library beside the
sample project at `sdk/integrations/godot-mono/`, where
`DllImport("clay_engine")` can resolve it immediately on platforms whose native
loader searches that location. It also keeps a copy at
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
temporary project and verifies the managed build, Godot solution import, native
library loading, and headless game startup:

```sh
GODOT_MONO_BIN=/path/to/godot-mono ./scripts/godot_mono_smoke.sh
```

The script defaults to `godot-mono` on `PATH` and auto-detects the standard
macOS, Linux, or Windows library in `build/`; set `CLAY_NATIVE_LIBRARY` for a
custom platform or build directory. On Linux the temporary staging directory is
added to `LD_LIBRARY_PATH` for the editor/runtime process because the dynamic
loader does not search the project directory by default.
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
copied out as packed `0x00RRGGBB` pixels. The sample reuses its managed pixel
and RGBA conversion buffers each frame, reallocating only when the render
surface changes. The wrapper deliberately does not marshal Clay's C++ headers
or expose native pointers to managed code.

This is the Mono/P/Invoke integration path, not a Godot GDExtension. A
GDExtension requires a Godot entry symbol and a `.gdextension` manifest in
addition to a shared library; that adapter is a separate future integration.

The committed `export_presets.cfg` provides reproducible macOS, Windows Desktop
and Linux/X11 exports. After exporting on macOS, use the bundled helper to stage
the platform library into the app's native search path:

```sh
./scripts/godot_mono_export_macos.sh build/ClayGodotSample.app
```

The reusable `godot_mono_stage_native.sh` helper is also installed beside the
sample. It accepts an exported player path and native library path, placing
the library beside a desktop executable or in `Contents/MacOS` for a macOS app
bundle:

```sh
./godot_mono_stage_native.sh build/ClayGodotSample.app build/libclay_engine.dylib
```

Windows x86_64 export and native loading are validated by
`.github/workflows/godot-windows-export.yml`. The workflow pins the official
Godot .NET editor and matching export templates, exports the `Windows Desktop`
preset, stages `clay_engine.dll` beside the player, launches it headlessly, and
requires the sample to report a frame rendered through the native Clay runtime.
The same check can be run from PowerShell when the editor and native DLL paths
are available:

```powershell
$env:GODOT_MONO_BIN = 'C:\path\to\Godot_v4.7.2-stable_mono_win64.exe'
$env:CLAY_NATIVE_LIBRARY = "$PWD\build\Debug\clay_engine.dll"
./scripts/godot_mono_windows_export_smoke.ps1
```

Linux x86_64 export and native loading are validated by
`.github/workflows/godot-linux-export.yml`. It uses the pinned official Godot
.NET editor and matching export templates, exports the `Linux/X11` preset,
stages `libclay_engine.so` beside the player, supplies that directory to the
Linux dynamic loader, launches the export headlessly, and requires a frame
rendered through Clay. With the editor/templates installed, the same check is:

```sh
GODOT_MONO_BIN=/path/to/Godot_v4.7.2-stable_mono_linux.x86_64 \
CLAY_NATIVE_LIBRARY="$PWD/build/libclay_engine.so" \
./scripts/godot_mono_linux_export_smoke.sh
```

Godot currently packs arbitrary `.dylib` files as project resources; macOS
cannot resolve a P/Invoke library from inside that resource pack. The macOS
helper keeps the export and staging steps together. Linux likewise needs an
explicit native-loader search path when running a loose staged `.so`; the
runtime and exported-player smoke scripts set that path for their child
processes. Windows resolves the staged DLL from the executable directory.

Desktop Mono validation now covers real native loading on macOS, Linux, and
Windows, including exported players on Linux and Windows. A first-class
GDExtension remains a separate integration decision rather than a prerequisite
for the Mono/P/Invoke package.
