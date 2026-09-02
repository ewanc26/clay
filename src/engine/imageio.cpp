#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <stb/stb_image.h>

#include "imageio.hpp"

#include <cstdio>
#include <cstring>

namespace clay {

/* PNG encode/decode backed by stb_image / stb_image_write (vendored). The
 * framebuffer is 0xAARRGGBB packed; these helpers keep the alpha channel so
 * sprites composited from images blend src-over exactly like text glyphs. */

std::vector<uint32_t> load_png_rgba(const std::string &path, int &out_width,
                                    int &out_height) {
    int comp = 0;
    unsigned char *data =
        stbi_load(path.c_str(), &out_width, &out_height, &comp, 4);
    std::vector<uint32_t> out;
    if (!data) return out;
    out.resize((size_t)out_width * (size_t)out_height);
    for (size_t i = 0; i < out.size(); i++) {
        unsigned char *p = data + i * 4;
        out[i] = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) |
                 ((uint32_t)p[1] << 8) | (uint32_t)p[2];
    }
    stbi_image_free(data);
    return out;
}

namespace {

void png_write_row(void *ctx, void *data, int len) {
    std::vector<uint8_t> *sink = static_cast<std::vector<uint8_t> *>(ctx);
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    sink->insert(sink->end(), bytes, bytes + len);
}

/* Packed 0xAARRGGBB -> 4-channel rows for stb. */
void flatten_rgba(const uint32_t *src, size_t count, std::vector<uint8_t> &rgba) {
    rgba.resize(count * 4);
    for (size_t i = 0; i < count; i++) {
        rgba[i * 4 + 0] = (uint8_t)(src[i] >> 16);
        rgba[i * 4 + 1] = (uint8_t)(src[i] >> 8);
        rgba[i * 4 + 2] = (uint8_t)(src[i]);
        rgba[i * 4 + 3] = (uint8_t)(src[i] >> 24);
    }
}

} // namespace

bool save_png(const std::string &path, int width, int height,
              const uint32_t *rgba_pixels) {
    std::vector<uint8_t> rgba;
    flatten_rgba(rgba_pixels, (size_t)width * (size_t)height, rgba);

    std::vector<uint8_t> bytes;
    bool ok = stbi_write_png_to_func(
        png_write_row, &bytes, width, height, 4, rgba.data(), width * 4);

    if (!ok) return false;
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t written = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return written == bytes.size();
}

} // namespace clay