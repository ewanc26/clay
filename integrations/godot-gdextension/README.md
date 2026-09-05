# Clay GDExtension bootstrap

This directory provides the optional native GDExtension bootstrap for Clay.
It exposes the standard `clay_gdextension_init` entry symbol and a Godot
`.gdextension` manifest while keeping Clay's public runtime surface in the
existing C ABI. The bootstrap deliberately registers no Godot classes yet;
hosts that need the full managed facade should use `godot-mono`.

Build it with `-DCLAY_BUILD_GDEXTENSION=ON`, then place the installed
`clay.gdextension` file and matching library under a Godot project's `bin/`
directory. The minimal interface declarations mirror Godot's stable C entry
contract; no godot-cpp dependency is required.

`project.godot` and `main.tscn` form a tiny headless smoke project. With the
installed library staged in `bin/`, run `godot --headless --path . --quit`
from this directory; successful startup prints `Clay GDExtension smoke
loaded` and exits cleanly.
