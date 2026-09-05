# Open issues

## Godot GDExtension adapter

The Mono/P/Invoke integration is validated across the supported desktop
platforms. macOS runs the real Godot .NET editor/runtime smoke, Linux runs both
the editor/runtime smoke and an exported x86_64 player, and Windows runs an
exported x86_64 player with the native DLL staged beside the executable. The
Linux and Windows workflows pin the official Godot .NET 4.7.2 editor/templates
and verify their published SHA-256 digests before use.

The optional `godot-gdextension` package now provides a loadable entry symbol,
`.gdextension` manifest, and CI headless startup smoke test while reusing the
stable C ABI. It intentionally registers no Godot classes yet, so exposing a
first-class Clay node/resource surface and editor lifecycle ownership remain
future work in this issue.

## Audio subsystem

The standalone and managed paths now validate WAV loading, generic miniaudio
loading for FLAC, MP3, and Ogg Vorbis, headless mixing, clip unload, optional
realtime playback, host sample-rate discovery, deterministic fades, and music
crossfades. Streaming remains future work; the C ABI now also exposes an
optional owned playback-device lifecycle that safely reports unavailable when
the backend or platform device is absent. Track streaming in GitHub issue #15.

## CI workflow execution

Feature-branch push runs are currently created with a failed conclusion but no
jobs, even though manual dispatch runs execute the matrix. Track the GitHub
Actions trigger/configuration investigation in issue #49.
