#include <clay/engine/runtime.hpp>

int main() {
    clay::Runtime runtime(24, 18, 42);
    runtime.step(1.0 / 60.0);
    const auto &frame = runtime.framebuffer();
    return frame.width == 24 && frame.height == 18 &&
                   frame.pixels.size() == 24u * 18u && runtime.frame() == 1
               ? 0
               : 1;
}
