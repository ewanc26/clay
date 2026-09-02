#include "imageio.hpp"
#include "garden.hpp"

#ifdef CLAY_PLAYER_INTERACTIVE
#include "platform/window_glfw.hpp"
#endif

#include <clay/clay.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {

struct Options {
    bool headless = false;
    bool replay = false;
    bool record = false;
    int width = 640;
    int height = 480;
    uint64_t frames = 150;
    uint64_t seed = 0xCAFEBABE;
    std::string dump;    /* .png path */
    std::string record_to; /* .clayrec path */
    std::string replay_from; /* .clayrec path */
    std::string rules;   /* optional reactions.json; builtin default otherwise */
};

void usage() {
    std::fputs(
        "clay_player - The Clay Garden\n"
        "usage: clay_player [options]\n"
        "  --headless           run without a window (default when built "
        "without GLFW)\n"
        "  --width N            canvas width (default 640)\n"
        "  --height N           canvas height (default 480)\n"
        "  --frames N           frames to run (headless default 150)\n"
        "  --seed N             simulation seed (default 0xCAFEBABE)\n"
        "  --dump out.png       write the final framebuffer to a PNG\n"
        "  --rules file.json    load a reaction rules table\n"
        "  --record out.clayrec append this run's input transcript\n"
        "  --replay in.clayrec  feed a recorded transcript instead of live "
        "input\n",
        stdout);
}

bool parse_options(int argc, char **argv, Options &o) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&](std::string &out) -> bool {
            if (i + 1 >= argc) return false;
            out = argv[++i];
            return true;
        };
        if (a == "--headless") {
            o.headless = true;
        } else if (a == "--replay") {
            o.replay = true;
            if (!next(o.replay_from)) return false;
        } else if (a == "--record") {
            o.record = true;
            if (!next(o.record_to)) return false;
        } else if (a == "--dump") {
            if (!next(o.dump)) return false;
        } else if (a == "--rules") {
            if (!next(o.rules)) return false;
        } else if (a == "--width") {
            if (!next(a)) return false;
            o.width = atoi(a.c_str());
        } else if (a == "--height") {
            if (!next(a)) return false;
            o.height = atoi(a.c_str());
        } else if (a == "--frames") {
            if (!next(a)) return false;
            o.frames = strtoull(a.c_str(), nullptr, 10);
        } else if (a == "--seed") {
            if (!next(a)) return false;
            o.seed = strtoull(a.c_str(), nullptr, 0);
        } else if (a == "-h" || a == "--help") {
            usage();
            return false;
        } else {
            std::fprintf(stderr, "clay_player: unknown option '%s'\n",
                         a.c_str());
            return false;
        }
    }
    return true;
}

std::string read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

uint64_t fnv1a64(const clay::Framebuffer &fb) {
    uint64_t h = 1469598103934665603ULL;
    for (uint32_t v : fb.pixels) {
        for (int j = 0; j < 4; j++) {
            h ^= (uint64_t)(v & 0xff);
            h *= 1099511628211ULL;
            v >>= 8;
        }
    }
    return h;
}

void run_headless(clay::Garden &garden, clay::Runtime &rt,
                  const Options &o) {
    const double dt = 1.0 / 60.0;
    for (uint64_t frame = 1; frame <= o.frames; frame++) {
        rt.begin_frame(dt);
        if (!o.replay) garden.drive_headless_input(rt.frame());
        rt.update(rt.sim_dt());
        rt.render();
        if (!o.dump.empty() && frame == o.frames) {
            clay::save_png(o.dump, rt.framebuffer().width,
                           rt.framebuffer().height,
                           rt.framebuffer().pixels.data());
            std::fprintf(stdout, "clay_player: dumped %s\n", o.dump.c_str());
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    Options o;
    if (!parse_options(argc, argv, o)) return 1;
    if (o.width <= 0 || o.height <= 0) {
        std::fputs("clay_player: bad canvas dimensions\n", stderr);
        return 1;
    }

    clay::Garden garden(o.width, o.height, o.seed);
    clay::Runtime &rt = garden.runtime();

    if (o.record && o.replay) {
        std::fputs("clay_player: --record and --replay are mutually exclusive\n",
                   stderr);
        return 1;
    }

    std::string custom = o.rules.empty() ? std::string() : read_file(o.rules);
    if (!o.rules.empty() && custom.empty()) {
        std::fprintf(stderr, "clay_player: could not read rules file '%s'\n",
                     o.rules.c_str());
        return 1;
    }
    garden.seed(custom.empty() ? clay::kGardenReactions : custom);
    garden.plant();

    if (o.replay) {
        cl_err err = cl_input_log_load(&rt.input_log(),
                                       o.replay_from.c_str());
        if (err != CLAY_OK) {
            std::fprintf(stderr, "clay_player: failed to load %s\n",
                         o.replay_from.c_str());
            return 1;
        }
        rt.set_replaying(true);
    }

    const bool interactive = !o.headless;
    if (interactive) {
#ifdef CLAY_PLAYER_INTERACTIVE
        clay::WindowGLFW window(o.width, o.height,
                                "The Clay Garden - clay_player");
        if (window.should_close()) {
            std::fputs("clay_player: could not open a GLFW window\n", stderr);
            return 1;
        }
        const double dt = 1.0 / 60.0;
        while (!window.should_close()) {
            rt.begin_frame(dt);
            window.poll_events();
            for (const cl_input_event &e : window.drain_events()) rt.feed(e);
            rt.update(rt.sim_dt());
            rt.render();
            window.present(rt.framebuffer());
        }
#else
        std::fputs("clay_player: built without GLFW; run with --headless\n",
                   stderr);
        return 1;
#endif
    } else {
        run_headless(garden, rt, o);
    }

    if (o.record && o.replay == false) {
        cl_err err = cl_input_log_save(&rt.input_log(), o.record_to.c_str());
        std::fprintf(stdout,
                     "clay_player: %s transcript to %s (%zu events)\n",
                     err == CLAY_OK ? "recorded" : "FAILED to record",
                     o.record_to.c_str(), cl_input_log_count(&rt.input_log()));
        if (err != CLAY_OK) return 1;
    }

    std::printf(
        "clay_player: seed=%llu frames=%llu commands=%zu spawns=%llu "
        "destroys=%llu reactions=%llu systems=%zu touched=%llu "
        "input_fp=%016llx fb_hash=%016llx\n",
        (unsigned long long)o.seed, (unsigned long long)rt.frame(),
        rt.commands().count(), (unsigned long long)rt.world().spawns(),
        (unsigned long long)rt.world().destroys(),
        (unsigned long long)rt.reactions().fired_count(),
        rt.systems().size(),
        (unsigned long long)rt.renderer().touched(),
        (unsigned long long)cl_input_log_fingerprint(&rt.input_log()),
        (unsigned long long)fnv1a64(rt.framebuffer()));

    return 0;
}
