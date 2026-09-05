# Third-party dependencies

Clay is bespoke by design, but not anti-middleware: the C23 core stays
stdlib-only (that is the point of the seam), while the C++ engine layer leans
on a deliberately tiny set of battle-tested libraries. All of them are
single-header or fetched at configure time — no lock-in, no build branches.

| Library | Use | Where | License |
| --- | --- | --- | --- |
| [GLFW 3.4](https://github.com/glfw/glfw) | window + input presentation | `src/engine/platform/window_glfw.cpp` | zlib/libpng |
| [stb_image](https://github.com/nothings/stb) | PNG decode | `src/engine/imageio.cpp` | public domain / MIT |
| [stb_image_write](https://github.com/nothings/stb) | PNG encode (`--dump`, screenshots) | `src/engine/imageio.cpp` | public domain / MIT |
| [doctest](https://github.com/doctest/doctest) | C++ unit tests | `test/*.cpp` | MIT |
| [miniaudio 0.11.21](https://miniaud.io/) | optional audio device and WAV/FLAC/MP3 decoding | `src/engine/audio/audio_device.cpp` | public domain / MIT-0 |

## Vendored, single-file

`third_party/stb/stb_image.h`, `third_party/stb/stb_image_write.h`,
`third_party/doctest/doctest.h`, and `third_party/miniaudio.h` are vendored
verbatim so the build never
needs network beyond CMake's optional GLFW fetch.

## GLFW

GLFW is the one non-header dependency, and only a *presenter*: the engine
always renders through its own software rasterizer, and the window merely
blits the finished frame. On a machine with `libglfw3-dev`/`glfw` installed
CMake finds it (`-DCLAY_USE_SYSTEM_GLFW=ON`); otherwise it silently fetches
GLFW 3.4 and builds it. Set `-DCLAY_BUILD_INTERACTIVE=OFF` to drop GLFW
entirely (the `clay_player` app then runs headless with simulated input).

## The seam stays honest

Nothing in `src/core` may include any of these headers. The C ABI (`clay.h`)
is the only boundary — that rule is what "bespoke" protects, and what keeps
the engine portable to a dial of a toaster if the mood ever strikes.
