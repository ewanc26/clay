# Godot Mono integration

`ClayRuntime.cs` is a small managed facade over Clay's opaque C ABI. The
directory also contains a minimal `project.godot` + `ClayDemo.cs` sample:
copy the shared native `clay_engine` library (`.dylib`, `.so`, or `.dll`) in
the platform's native library location, open the directory in a Godot Mono
editor, and run the project. Configure Clay with `-DCLAY_BUILD_SHARED=ON`
(the default) to produce that library.

`cmake --install build --prefix sdk` also places the shared library at
`sdk/integrations/godot-mono/native/<platform>/` (`macos`, `linux`, or
`windows`). Copy that file into the Godot
project root (or its platform export layout) so `DllImport("clay_engine")` can
resolve it.

`managed/ClayRuntime.csproj` builds the wrapper independently as a `net8.0`
library, which is useful for checking the binding without the Godot editor.
`ClayGodotSample.csproj` is the Godot SDK project opened by the Mono editor.

The wrapper owns the native handle with `SafeHandle`; framebuffer data is
copied out as packed `0x00RRGGBB` pixels. The wrapper deliberately does not
marshal Clay's C++ headers or expose native pointers to managed code.

This is the Mono/P/Invoke integration path, not a Godot GDExtension. A
GDExtension requires a Godot entry symbol and a `.gdextension` manifest in
addition to a shared library; that adapter is a separate future integration.

The sample reuses its managed pixel and RGBA conversion buffers each frame;
production hosts can upload through a native texture bridge for further
optimization. CI coverage and platform packaging are tracked in
`docs/issues.md`.
