# Godot Mono integration

`ClayRuntime.cs` is a small managed facade over Clay's opaque C ABI. Copy it
into a Godot C# project, place the native `clay_engine` library in the
platform's native library location, and drive the runtime from a node's
`_Process(double delta)` method.

The wrapper owns the native handle with `SafeHandle`; framebuffer data is
copied out as packed `0x00RRGGBB` pixels. The wrapper deliberately does not
marshal Clay's C++ headers or expose native pointers to managed code.

This directory is a binding source drop, not yet a complete Godot sample
project. A CI smoke project and platform packaging are tracked in `docs/issues.md`.
