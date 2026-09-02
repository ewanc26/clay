# Godot Mono integration

`ClayRuntime.cs` is a small managed facade over Clay's opaque C ABI. The
directory also contains a minimal `project.godot` + `ClayDemo.cs` sample:
copy the shared native `clay_engine` library (`.dylib`, `.so`, or `.dll`) in
the platform's native library location, open the directory in a Godot Mono
editor, and run the project. Configure Clay with `-DCLAY_BUILD_SHARED=ON`
(the default) to produce that library.

`ClayRuntime.csproj` builds the wrapper independently as a `netstandard2.1`
library, which is useful for checking the binding without the Godot editor.

The wrapper owns the native handle with `SafeHandle`; framebuffer data is
copied out as packed `0x00RRGGBB` pixels. The wrapper deliberately does not
marshal Clay's C++ headers or expose native pointers to managed code.

This is the Mono/P/Invoke integration path, not a Godot GDExtension. A
GDExtension requires a Godot entry symbol and a `.gdextension` manifest in
addition to a shared library; that adapter is a separate future integration.

The sample allocates one managed RGBA conversion buffer per frame for clarity;
production hosts should reuse a packed pixel buffer or upload through a native
texture bridge. CI coverage and platform packaging are tracked in
`docs/issues.md`.
