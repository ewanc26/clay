# Clay GDExtension bootstrap

This directory provides the optional native GDExtension bootstrap for Clay.
It exposes the standard `clay_gdextension_init` entry symbol and a Godot
`.gdextension` manifest and the registration code for a native
`ClayRuntimeNode` that owns and advances a Clay runtime while keeping Clay's
public runtime surface in the existing C ABI. Hosts that need the complete
runtime facade should use `godot-mono`.

Build it with `-DCLAY_BUILD_GDEXTENSION=ON`, then place the installed
`clay.gdextension` file and matching library under a Godot project's `bin/`
directory. The minimal interface declarations mirror Godot's stable C entry
contract; no godot-cpp dependency is required.

`project.godot` and `main.tscn` form a tiny headless smoke project. With the
installed library staged in `bin/`, run `godot --headless --path . --quit`
from this directory; successful startup instantiates `ClayRuntimeNode`, prints
its native class name after one process frame, and exits cleanly. The node
creates a 320x180 Clay runtime on construction and advances it from Godot's
process notifications.
Scripts extending `ClayRuntimeNode` can also call `advance_runtime()` to
explicitly advance one fixed 1/60-second frame.
The repository helper `scripts/godot_gdextension_smoke.sh` performs the same
run and fails if Godot falls back to a placeholder node.
