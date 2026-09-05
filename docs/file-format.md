# Clay file format (`.clay`)

A `.clay` file is a versioned JSON document describing a deterministic 3D
scene: renderer settings, reusable meshes, lights, a camera, and mesh
instances. Files are syntax-parsed through the public C ABI
`cl_json_parse`/`cl_json_write`; schema validation and scene ownership remain in
the C++ engine.

Version 1 is intentionally small. Features that are not implemented are not
part of the v1 contract simply because they may be useful later.

## Design rules

- **Data, not code.** Loading a scene never executes code from the file.
- **Versioned.** `version` is required and must be an integer understood by the
  loader. A v1 loader rejects other versions rather than guessing.
- **Stable references.** Meshes use explicit string `name` values and may also
  carry a stable `uid`. There are no derived header counts or positional IDs.
- **UID first.** When an instance supplies both `mesh_uid` and `mesh`, UID
  resolution wins; `mesh` is the rename/path fallback.
- **Order preserving.** Mesh and scene arrays retain file order. Rendering never
  depends on hash-map iteration order.
- **Strict recognised fields.** Unknown object keys are ignored for forward
  compatibility, but a recognised key with the wrong type or invalid value is
  a load error. Unknown `component` names are ignored.

## Top-level shape

```jsonc
{
  "version": 1,
  "settings": {
    "seed": 1337,
    "fps": 60,
    "render": {
      "width": 640,
      "height": 480,
      "clear": [24, 26, 34, 255]
    }
  },
  "meshes": [ ... ],
  "scene": [ ... ]
}
```

`settings`, `meshes`, and `scene` are optional. If present, they must have the
shapes described below.

## Settings

`settings.seed` is either a non-negative integer for a fixed seed, or `null` /
`false` for no fixed seed. Omitting it also means no fixed seed.
When a scene with a fixed seed is loaded, the runtime adopts that seed for
subsequent deterministic operations.

`settings.fps` is a positive integer. It is metadata for the scene/runtime; the
renderer itself does not read wall-clock time.

`settings.render` may contain:

- `width` and `height`: positive integer dimensions, currently limited to 8192.
- `clear`: `[r,g,b]` or `[r,g,b,a]`, integer channels from 0 through 255.

When a host loads a scene without `settings.render.width`, `height`, or the
legacy `resolution` alias, its existing render dimensions are preserved. A
scene that supplies either form applies the authored dimensions.

The early demo spelling `"resolution": [width, height]` remains accepted as a
v1 compatibility alias. New files should use `settings.render`.

## Meshes

Each mesh requires a unique non-empty `name` and may have a unique non-empty
`uid` and a base `color`.

A mesh is either a builtin primitive or inline indexed geometry.

### Builtins

```jsonc
{ "name": "box", "primitive": "cube", "half_extent": 0.5 }
{ "name": "ball", "uid": "ball01", "primitive": "sphere",
  "radius": 0.6, "rings": 16, "slices": 12 }
{ "name": "floor", "primitive": "plane",
  "width": 8, "height": 8, "nx": 4, "ny": 4 }
```

Supported primitive names are exactly `cube`, `sphere`, and `plane`. Unknown
primitive names are errors; they are never silently substituted with another
shape. Generated windings are outward CCW for backface culling.

### Inline geometry

```jsonc
{
  "name": "triangle",
  "positions": [[0,0,0], [1,0,0], [0,1,0]],
  "indices": [0,1,2],
  "color": [10,200,10]
}
```

`positions` is a non-empty array of three-number vectors. `indices` is a
non-empty flat integer array whose length is a multiple of three; every index
must refer to an existing position. Position and triangle order are preserved.

## Scene entries

Every scene entry is an object with a string `component`. Version 1 understands
four component names.

### `directional_light`

```jsonc
{ "component": "directional_light",
  "dir": [0.3, 0.5, 0.8], "intensity": 1.0 }
```

At most one directional light is allowed. `dir`, when supplied, must be a
non-zero vector and is normalised at load time. `intensity` must be
non-negative.

### `point_light`

```jsonc
{ "component": "point_light", "pos": [2,3,1],
  "intensity": 0.7, "attenuation": 0.05 }
```

Version 1 supports at most one point light. This is explicit: v1 does not parse
an arbitrary light list and then silently render only the first one.

### `camera`

```jsonc
{ "component": "camera", "eye": [0,0,6], "target": [0,0,0],
  "up": [0,1,0], "fov": 0.9, "znear": 0.1, "zfar": 100 }
```

At most one camera is allowed. FOV is in radians, `znear` must be positive, and
`zfar` must be greater than `znear`. If no camera is present, the engine uses
its documented default camera.

### `mesh_instance`

```jsonc
{
  "component": "mesh_instance",
  "mesh": "box",
  "mesh_uid": "optional-stable-id",
  "transform": {
    "pos": [0,0,-3],
    "euler": [0,0.5,0],
    "scale": 1
  },
  "color": [180,120,80]
}
```

An instance must provide `mesh`, `mesh_uid`, or both. When both are present,
`mesh_uid` is resolved first and `mesh` is the fallback. The reference must
resolve during load.

`scale` may be a scalar or `[x,y,z]`. Euler angles are radians and rotations are
applied Y-X-Z. Clay uses row vectors, so the model transform is composed as
`S * R * T`: scale and rotation affect the object while `pos` remains its world
position.

If `color` is omitted, the instance inherits the referenced mesh's base color.

## Rendering semantics

The perspective projection uses OpenGL-style NDC depth `[-1,1]`. The software
rasterizer clips against all six homogeneous frustum planes, including both
`z >= -w` and `z <= w`, before perspective divide.

The depth buffer is frame-scoped. Separate mesh instances therefore occlude one
another by depth rather than draw order. Flat face normals are recomputed from
world-space transformed positions, which keeps lighting correct under
non-uniform scale.

## Determinism

For a fixed input scene and engine state, traversal order is explicit:

- mesh arrays preserve file order;
- inline triangle order is preserved;
- scene instances render in scene-array order;
- visibility is decided by deterministic clipping, culling and depth testing;
- no render-significant order comes from unordered-map iteration.

The test suite renders the same loaded scene repeatedly and compares framebuffer
hashes, and it separately gates depth across mesh instances, clip planes and
builtin winding on both Linux and macOS CI.

## Reserved / not in v1

External mesh files, arbitrary multi-light accumulation, inline reaction rules,
and 2D garden ECS components are not part of the version-1 loader. They may be
added by a later format version or by a backwards-compatible extension with an
explicit implementation and tests. Their absence from v1 is deliberate rather
than an implicit promise.

## Errors

`ClayScene::load` returns `false` for malformed JSON, missing/unsupported
versions, malformed recognised values, duplicate mesh names/UIDs, invalid
geometry, ambiguous v1 singleton components, and unresolved mesh references.
The underlying JSON syntax parser continues to use `cl_err` through the public C
ABI.
