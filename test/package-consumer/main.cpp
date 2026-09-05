#include <clay/engine.hpp>

#include <array>

int main() {
    clay::Runtime runtime(24, 18, 42);
    runtime.step(1.0 / 60.0);
    const auto &frame = runtime.framebuffer();
    if (frame.width != 24 || frame.height != 18 ||
        frame.pixels.size() != 24u * 18u || runtime.frame() != 1)
        return 1;

    clay::AudioMixer mixer;
    const auto clip = mixer.add_clip(clay::AudioClip{48000, 1, {0.5F}});
    if (!clip.has_value() || !mixer.play(*clip).has_value()) return 1;
    std::array<float, 2> samples{};
    if (!mixer.mix_stereo(samples) || samples[0] != 0.5F ||
        samples[1] != 0.5F)
        return 1;
    return 0;
}
