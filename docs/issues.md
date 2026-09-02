# Open issues

## Godot Mono binding package

Clay now includes typed key/modifier constants, native library packaging for
each supported platform, a CI build job, an installed SDK payload, and a
managed `SafeHandle` wrapper under `integrations/godot-mono/`. This issue
remains open until the sample is run through Godot editor/export validation.

The macOS path is now validated locally with Godot Mono 4.7.2: the managed
assembly imports in the editor and the sample starts headlessly with the Clay
native library. The repeatable check is `scripts/godot_mono_smoke.sh`, and CI
has a macOS runtime job. Windows/Linux editor and exported-player validation
remain outstanding.

The macOS export preset and solution metadata are now committed. A real
universal export packages successfully, but the exported app still requires
`libclay_engine.dylib` to be copied into `Contents/MacOS` because Godot packs
the loose library as a resource and macOS cannot resolve P/Invoke from there.

Upstream tracking: [#5](https://github.com/ewanc26/clay/issues/5).

## Windows and Godot export validation

The shared C ABI now uses an explicit Windows export macro, and CI includes a
Windows build/test leg. The repository still needs an exported Godot Mono
sample run to validate DLL naming, dependent runtime loading, and architecture
selection.

## Godot GDExtension adapter

The current Mono integration uses a stable C ABI and P/Invoke. It does not
provide a Godot entry symbol or `.gdextension` manifest, so it cannot be
consumed as a first-class GDExtension. Decide whether to maintain a separate
GDExtension adapter after the Mono sample is validated.
