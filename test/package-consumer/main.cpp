#include <clay/engine.hpp>

#include <array>
#include <filesystem>
#include <fstream>

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

    const auto scene_path = std::filesystem::temp_directory_path() /
                            "clay-package-consumer.clay";
    {
        std::ofstream scene(scene_path, std::ios::binary);
        scene << R"({"version":1,"settings":{"render":{"width":10,"height":6}},"scene":[]})";
    }
    if (!runtime.load_scene_file(scene_path.string()) || !runtime.has_scene() ||
        runtime.width() != 10 || runtime.height() != 6)
        return 1;
    runtime.unload_scene();
    std::filesystem::remove(scene_path);

    const auto actions_path = std::filesystem::temp_directory_path() /
                              "clay-package-consumer-actions.json";
    const auto reactions_path = std::filesystem::temp_directory_path() /
                                "clay-package-consumer-reactions.json";
    {
        std::ofstream actions(actions_path, std::ios::binary);
        std::ofstream reactions(reactions_path, std::ios::binary);
        actions << R"({"actions":{"primary":{"key":"SPACE"}}})";
        reactions << R"({"rules":[]})";
    }
    if (!runtime.load_actions_file(actions_path.string()) ||
        !runtime.load_reactions_file(reactions_path.string()))
        return 1;
    std::filesystem::remove(actions_path);
    std::filesystem::remove(reactions_path);
    return 0;
}
