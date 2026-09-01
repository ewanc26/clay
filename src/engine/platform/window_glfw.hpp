#ifndef CLAY_ENGINE_PLATFORM_WINDOW_GLFW_HPP
#define CLAY_ENGINE_PLATFORM_WINDOW_GLFW_HPP

#include "render/renderer.hpp"

#include <clay/clay.h>

#include <vector>

struct GLFWwindow;

namespace clay {

/* GLFW presenter: a window whose soul is a CPU framebuffer. Clay still does
 * all its own rendering through RendererSW — GLFW only supplies the OS
 * surface, the key/mouse events, and a texture blit at the end of every
 * frame. Swap GLFW out for SDL/Fermaki and nothing but this file changes. */
class WindowGLFW {
  public:
    WindowGLFW(int canvas_width, int canvas_height, const char *title);
    ~WindowGLFW();

    WindowGLFW(const WindowGLFW &) = delete;
    WindowGLFW &operator=(const WindowGLFW &) = delete;

    bool should_close() const;
    void poll_events();

    /* Raw events since the caller last drained the queue. */
    std::vector<cl_input_event> drain_events();

    /* Paste the engine framebuffer into the window. */
    void present(const Framebuffer &fb);

    int canvas_width() const;
    int canvas_height() const;

  private:
    struct Impl;
    Impl *impl_;
};

} // namespace clay

#endif /* CLAY_ENGINE_PLATFORM_WINDOW_GLFW_HPP */