# Third-party dependencies

Clay is bespoke by design, but not anti-middleware: the C23 core stays
stdlib-only (that is the point of the seam), while the C++ engine layer leans
on a deliberately tiny set of battle-tested libraries. Dependencies are
vendored single files or pinned configure-time fetches.

| Library | Use | Where | License |
| --- | --- | --- | --- |
| [GLFW 3.4](https://github.com/glfw/glfw) | window + input presentation | `src/engine/platform/window_glfw.cpp` | zlib/libpng |
| [miniaudio 0.11.25](https://github.com/mackron/miniaudio) | audio decode/resampling + native device output | `src/engine/audio/audio_system.cpp` | public domain / MIT-0 |
| [stb_vorbis](https://github.com/nothings/stb) | OGG/Vorbis short-effect decode (via miniaudio's pinned `extras/stb_vorbis.c`) | `src/engine/audio/audio_system.cpp` | public domain / MIT |
| [stb_image](https://github.com/nothings/stb) | PNG decode | `src/engine/imageio.cpp` | public domain / MIT |
| [stb_image_write](https://github.com/nothings/stb) | PNG encode (`--dump`, screenshots) | `src/engine/imageio.cpp` | public domain / MIT |
| [doctest](https://github.com/doctest/doctest) | C++ unit tests | `test/*.cpp` | MIT |

## Vendored, single-file

`third_party/stb/stb_image.h`, `third_party/stb/stb_image_write.h`, and
`third_party/doctest/doctest.h` are vendored verbatim. Audio deliberately uses
a pinned miniaudio checkout instead of copying another generated single-header
snapshot into the repository; that checkout also supplies the exact
`stb_vorbis.c` implementation used for Vorbis effect decoding.

## Configure-time dependencies

GLFW is only a *presenter*: the engine always renders through its own software
rasterizer, and the window merely blits the finished frame. On a machine with
`libglfw3-dev`/`glfw` installed CMake finds it (`-DCLAY_USE_SYSTEM_GLFW=ON`);
otherwise it fetches GLFW 3.4. Set `-DCLAY_BUILD_INTERACTIVE=OFF` to drop GLFW
entirely.

The C++ audio layer pins miniaudio 0.11.25. It owns decoding/resampling and the
cross-platform playback-device seam; device startup remains opt-in, so the
same build and runtime are valid on headless hosts. WAV/music streaming uses
miniaudio's decoder path, while OGG/Vorbis short effects use the bundled
`stb_vorbis` decoder before entering Clay's owned float-PCM mixer.

## The seam stays honest

Nothing in `src/core` may include any of these headers. The C ABI (`clay.h` /
`engine_c.h`) is the boundary: that rule is what "bespoke" protects, and what
keeps the engine portable to a dial of a toaster if the mood ever strikes.
