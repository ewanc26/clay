# Clay file format (.clay)

A Clay file is a single, deterministically-loadable JSON document describing a
level (a scene), the 3D/2D geometry it references, and engine settings. It is
the on-disk form of what the reactive pipeline runs against: it feeds the
world (`Runtime`), the reaction rules, and the renderer — nothing in a `.clay`
file is code.

This document is the spec. The core `cl_json_parse` / `cl_json_write` pair in
`src/core/json.[ch]` is the only serializer; no `.clay` file is ever decoded by
hand-written parsers on either side.

## Philosophy (inherited from the reactions.json contract)

- **Data, not code.** Loading a `.clay` file must not require a rebuild.
- **Deterministic.** With a fixed seed and no wall-clock reads, the same file
  renders identical commits. JSON object ordering is *significant*: entity
  draw/update order and mesh triangle order come straight from the file.
- **Human-editable.** Plain text, meaningful names, no binary bags of floats.
  Large meshes may be external (see “External assets”), but a single-file,
  inline form always works.
- **One C ABI.** All reading goes through `cl_json_parse`; all writing through
  `cl_json_write`.

## Top-level shape

```jsonc
{
  "version": 1,

  "settings": {
    "seed": 1337,          // finite or omitted -> fixed seed (determinism)
    "fps": 60,
    "render": {
      "width": 320,
      "height": 240,
      "clear": [0x10, 0x12, 0x1a, 0xff]
    }
  },

  "reactions": [ ... ],    // optional; identical shape to reactions.json

  "meshes": [              // named, reusable 3D geometry
    { "name": "cube", "primitive": "cube" },
    { "name": "monkey", "primitive": "sphere", "segments": { "y": 16, "x": 8 } }
  ],

  "scene": [
    {
      "name": "light",
      "component": "directional_light",
      "dir": [0.3, 0.5, 0.8],
      "intensity": 1.0
    },
    {
      "name": "garden-block",
      "component": "block",
      "transform": { "pos": [0, 0, -3], "euler": [0, 0.5, 0], "scale": 1 },
      "mesh": "cube",
      "color": [180, 120, 80]
    }
  ]
}
```

## Field rules

- `version` is a required integer. Loaders must reject a `version` they do not
  understand rather than guess (mirrors the `cl_T_free`/`cl_result` contract:
  possible-but-unexpected states return errors, programmer errors are fatal).
- Unknown object keys and unknown `component` names are **ignored with a note in
  the log** (matching existing JSON lookup semantics). A malformed value for a
  key that *is* understood is an error.
- `seed` present and integer → fixed seed; omitted → non-deterministic run.
  `null`/`false` also allowed to mean “run, don’t fix the seed.”
- `scene` is an array of entity descriptors. Load order is entity order; a later
  entity may not reorder earlier ones (the pipeline is order-defined, never
  luck).
- Colors are `[r, g, b]` or `[r, g, b, a]`, 0–255. Matches the `Rgba` engine
  type and `0xAARRGGBB` pixel layout.
- 3D vectors are `[x, y, z]`; Euler angles are radians, applied YXZ (yaw,
  pitch, roll) to match the engine’s row-vector `cl_m4` convention.

## `meshes` — 3D geometry

A named mesh is either an inline surface or a reference:

```jsonc
{ "name": "cube", "positions": [[...]], "indices": [...], "color": [180,120,80] }
```

`positions` (array of `[x,y,z]`) and `indices` (flat, 3 per triangle) map one
to one onto `Mesh3D`. `primitive` selects a builtin generator (`cube`,
`sphere`, `plane`) so scenes never need to spell out tessellations by hand;
its parameters are under the generator’s own object. `color` gives the flat
shading base for the whole mesh.

The load order of a mesh’s triangle list is preserved verbatim: the z-buffer
and backface culling decide visibility; the file only decides *order in*,
which keeps replays stable.

## `scene` entities

Each entry has exactly one `component`. This is intentionally flat (no
component inventory indirection) — the ECS `Storage<T>` attaches a typed
pool per component by name. `transform` (optional) is the default
model matrix: `pos`, `euler`, `scale` (uniform or per-axis when given a
3-vector).

Entities the renderer understands:

- `directional_light`: `dir` (world-space, normalized at load) + `intensity`.
- `mesh_instance`: `mesh` (name) + `color` + `transform` → renders via
  `IRenderer::draw_mesh`.
- 2D garden primitives (`block`, `ripple`, `hueshift`, `lifespan`) are
  accepted with a 2D `pos`; the ECS already owns these components.

`reactions`: an optional array in the exact `reactions.json` shape, passed
straight to `ReactionEngine::load_json`. Keeping it inline makes a `.clay`
file self-contained; the separate reaction module still works for layered
content.

## External assets (future, reserved)

A `mesh` value that is a bare string (e.g. `"mesh": "banana"`) refers to a
mesh previously declared in `meshes`; a `"file": "res://meshes/banana.claym"`
form (reserved) would pull a standalone mesh asset. Both are the same schema
under the hood. Nothing uses `"file"` yet; it is reserved so the spec does not
need to change later.

## Errors

Load failures are a single `cl_err`/`cl_result` (success, unsupported
version, malformed JSON, unknown component, out-of-memory). Partial state
after a failed load is unspecified; callers retry with the previous world
intact. This matches `src/core`’s “handle it or halt” arena contract — the
arena that allocates the loaded world is the same one that will free it.

## Determinism note

JSON object order is preserved by `cl_json_parse`. Do not rely on hash
iteration anywhere a `.clay` file touches order (scene order, mesh order).
The `fb_hash` replay round-trip is the gate: a file change that breaks
determinism fails `test_runtime`.