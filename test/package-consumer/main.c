#include <clay/engine_c.h>

#include <stddef.h>

int main(void) {
    cl_engine_runtime *runtime = cl_engine_runtime_create(16, 16, 42);
    if (!runtime) return 1;

    const cl_err step = cl_engine_runtime_step(runtime, 1.0 / 60.0);
    size_t pixel_count = 0;
    const uint32_t *pixels = cl_engine_runtime_pixels(runtime, &pixel_count);
    const int valid_frame = step == CLAY_OK && pixels != NULL &&
                            pixel_count == 16u * 16u &&
                            cl_engine_runtime_frame(runtime) == 1;
    cl_engine_runtime_destroy(runtime);
    return valid_frame ? 0 : 1;
}
