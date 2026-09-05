# Open issues

## Godot GDExtension adapter

The Mono/P/Invoke integration is validated across the supported desktop
platforms. macOS runs the real Godot .NET editor/runtime smoke, Linux runs both
the editor/runtime smoke and an exported x86_64 player, and Windows runs an
exported x86_64 player with the native DLL staged beside the executable. The
Linux and Windows workflows pin the official Godot .NET 4.7.2 editor/templates
and verify their published SHA-256 digests before use.

The optional `godot-gdextension` package now provides a loadable entry symbol,
`.gdextension` manifest, a native `ClayRuntimeNode` with owned runtime lifecycle,
and headless smoke coverage while reusing the stable C ABI. A richer resource
surface and editor lifecycle ownership remain future work in this issue.

## Audio subsystem

The standalone and managed paths now validate WAV loading, generic miniaudio
loading for FLAC, MP3, and Ogg Vorbis, decoder-backed streams with explicit
read status, headless mixing,
clip/stream unload, optional realtime playback, host sample-rate discovery,
deterministic fades, music crossfades, an optional owned playback-device
lifecycle, and deterministic 2D source/listener spatialization. Remaining work
is underflow recovery policy at the realtime device boundary and broader
format/device matrix coverage.
Track that follow-up in GitHub issue #15.

## CI workflow execution

Feature-branch push runs are currently created with a failed conclusion but no
jobs, even though manual dispatch runs execute the matrix. Track the GitHub
Actions trigger/configuration investigation in issue #49.
