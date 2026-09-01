#ifndef CLAY_ENGINE_IMAGEIO_HPP
#define CLAY_ENGINE_IMAGEIO_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace clay {

/* PNG encode/decode backed by stb_image / stb_image_write (vendored). The
 * framebuffer is 0x00RRGGBB packed; these helpers handle the RGB conversion
 * so the rest of the engine never thinks about formats. */

std::vector<uint32_t> load_png_rgba(const std::string &path, int &out_width,
                                    int &out_height);
bool save_png(const std::string &path, int width, int height,
              const uint32_t *rgba_pixels);

} // namespace clay

#endif /* CLAY_ENGINE_IMAGEIO_HPP */