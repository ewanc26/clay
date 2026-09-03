# Open issues

## Godot Mono binding package

Clay includes typed key/modifier constants, native library packaging for each
supported platform, CI builds, an installed SDK payload, and a managed
`SafeHandle` wrapper under `integrations/godot-mono/`. The remaining packaging
gap is Linux runtime/export validation.

The macOS path is validated with Godot Mono 4.7.2: the managed assembly imports
in the editor and the sample starts headlessly with the Clay native library.
The repeatable check is `scripts/godot_mono_smoke.sh`, and CI runs it on macOS.

Windows x86_64 export is also validated in CI with the pinned Godot .NET 4.7.2
editor and export templates. The `godot-windows-export` workflow builds the
MSVC DLL, exports the sample, stages `clay_engine.dll` beside the player, and
launches it headlessly. The run must render through the native runtime without
`DllNotFoundException`, `EntryPointNotFoundException`, or architecture/load
errors.

The macOS and Windows export presets are committed. The macOS helper stages
`libclay_engine.dylib` into `Contents/MacOS`; the Windows smoke script stages
`clay_engine.dll` beside the exported executable. Linux runtime/export
validation remains outstanding.

Upstream tracking: [#5](https://github.com/ewanc26/clay/issues/5).

## Godot GDExtension adapter

The current Mono integration uses a stable C ABI and P/Invoke. It does not
provide a Godot entry symbol or `.gdextension` manifest, so it cannot be
consumed as a first-class GDExtension. Decide whether to maintain a separate
GDExtension adapter after the Mono sample is validated across the supported
desktop platforms.
