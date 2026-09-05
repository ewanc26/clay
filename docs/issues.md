# Open issues

## Godot GDExtension adapter

The Mono/P/Invoke integration is validated across the supported desktop
platforms. macOS runs the real Godot .NET editor/runtime smoke, Linux runs both
the editor/runtime smoke and an exported x86_64 player, and Windows runs an
exported x86_64 player with the native DLL staged beside the executable. The
Linux and Windows workflows pin the official Godot .NET 4.7.2 editor/templates
and verify their published SHA-256 digests before use.

The optional `godot-gdextension` package now provides a loadable entry symbol
and `.gdextension` manifest while reusing the stable C ABI. It intentionally
registers no Godot classes yet, so exposing a first-class Clay node/resource
surface, lifecycle ownership, and a Godot headless-frame smoke test remain
future work in this issue.

## Audio subsystem

The standalone and managed paths now validate WAV loading, generic miniaudio
loading for FLAC, MP3, and Ogg Vorbis, headless mixing, clip unload, optional
realtime playback, and host sample-rate discovery. Streaming, crossfades, and
a C-ABI-owned audio
device lifecycle remain future work. Track these in GitHub issue #15.
