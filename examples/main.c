#include <clay/engine_c.h>

#include <stdio.h>

int main(int argc, char **argv) {
    cl_engine_runtime *runtime = cl_engine_runtime_create(320, 240, 0xC0FFEE);
    if (!runtime) {
        fputs("clay_host_c: could not create runtime\n", stderr);
        return 1;
    }

    const char *actions =
        "{\"actions\": {\"primary\": {\"key\": \"SPACE\"}}}";
    if (cl_engine_runtime_load_actions(runtime, actions) != CLAY_OK ||
        cl_engine_runtime_install_builtin_systems(runtime) != CLAY_OK ||
        cl_engine_runtime_spawn_species(runtime, "animal", 160, 120, 0.7f,
                                         0.9f, 0.6f, 1.0f, 60.0f) != CLAY_OK) {
        fputs("clay_host_c: setup failed\n", stderr);
        cl_engine_runtime_destroy(runtime);
        return 1;
    }

    for (int i = 0; i < 60; i++) {
        cl_engine_runtime_feed_motion(runtime, 160, 120, 0, 0);
        if (cl_engine_runtime_step(runtime, 1.0 / 60.0) != CLAY_OK) {
            fputs("clay_host_c: step failed\n", stderr);
            cl_engine_runtime_destroy(runtime);
            return 1;
        }
    }

    size_t pixels = 0;
    const uint32_t *frame = cl_engine_runtime_pixels(runtime, &pixels);
    if (argc > 1 &&
        cl_engine_runtime_save_png(runtime, argv[1]) != CLAY_OK) {
        fputs("clay_host_c: could not save PNG\n", stderr);
        cl_engine_runtime_destroy(runtime);
        return 1;
    }
    printf("clay_host_c: frame=%llu pixels=%zu\n",
           (unsigned long long)cl_engine_runtime_frame(runtime), pixels);
    int ok = frame != NULL && pixels == 320u * 240u;
    cl_engine_runtime_destroy(runtime);
    return ok ? 0 : 1;
}
