# Open issues

## Godot GDExtension adapter

The Mono/P/Invoke integration is validated across the supported desktop
platforms. macOS runs the real Godot .NET editor/runtime smoke, Linux runs both
the editor/runtime smoke and an exported x86_64 player, and Windows runs an
exported x86_64 player with the native DLL staged beside the executable. The
Linux and Windows workflows pin the official Godot .NET 4.7.2 editor/templates
and verify their published SHA-256 digests before use.

The current Mono integration uses a stable C ABI and P/Invoke. It does not
provide a Godot entry symbol or `.gdextension` manifest, so it cannot be
consumed as a first-class GDExtension. Decide whether to maintain a separate
GDExtension adapter if a native Godot extension surface is needed in addition
to the validated Mono integration.
