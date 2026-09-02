# Godot Mono integration

`ClayRuntime.cs` is a small managed facade over Clay's opaque C ABI. The
directory also contains a minimal `project.godot` + `ClayDemo.cs` sample:
copy the native `clay_engine` library in the platform's native library
location, open the directory in a Godot Mono editor, and run the project.

The wrapper owns the native handle with `SafeHandle`; framebuffer data is
copied out as packed `0x00RRGGBB` pixels. The wrapper deliberately does not
marshal Clay's C++ headers or expose native pointers to managed code.

The sample allocates one managed RGBA conversion buffer per frame for clarity;
production hosts should reuse a packed pixel buffer or upload through a native
texture bridge. CI coverage and platform packaging are tracked in
`docs/issues.md`.
