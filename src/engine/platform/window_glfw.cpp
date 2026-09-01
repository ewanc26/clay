/* The presenter uses legacy fixed-function GL to blit; the API is deprecated
 * on macOS but remains the portability-neutral path for this optional backend.
 * Must be defined before any OpenGL framework header is pulled in (GLFW/glfw3.h
 * includes OpenGL on macOS), since OpenGLAvailability.h guards on first include. */
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION_RECURSIVE
#endif

#include "window_glfw.hpp"

#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <cstdio>

namespace clay {

namespace {

/* GLFW -> cl_key. Table first for the non-contiguous keys, then the
 * contiguous runs (A-Z, 0-9, F1-F12) as offsets. */
cl_key glfw_to_key(int key) {
    switch (key) {
    case GLFW_KEY_ESCAPE: return CLAY_KEY_ESCAPE;
    case GLFW_KEY_ENTER: return CLAY_KEY_ENTER;
    case GLFW_KEY_TAB: return CLAY_KEY_TAB;
    case GLFW_KEY_SPACE: return CLAY_KEY_SPACE;
    case GLFW_KEY_BACKSPACE: return CLAY_KEY_BACKSPACE;
    case GLFW_KEY_DELETE: return CLAY_KEY_DELETE;
    case GLFW_KEY_HOME: return CLAY_KEY_HOME;
    case GLFW_KEY_END: return CLAY_KEY_END;
    case GLFW_KEY_PAGE_UP: return CLAY_KEY_PAGE_UP;
    case GLFW_KEY_PAGE_DOWN: return CLAY_KEY_PAGE_DOWN;
    case GLFW_KEY_UP: return CLAY_KEY_ARROW_UP;
    case GLFW_KEY_DOWN: return CLAY_KEY_ARROW_DOWN;
    case GLFW_KEY_LEFT: return CLAY_KEY_ARROW_LEFT;
    case GLFW_KEY_RIGHT: return CLAY_KEY_ARROW_RIGHT;
    case GLFW_KEY_LEFT_SHIFT: return CLAY_KEY_LEFT_SHIFT;
    case GLFW_KEY_LEFT_CONTROL: return CLAY_KEY_LEFT_CTRL;
    case GLFW_KEY_LEFT_ALT: return CLAY_KEY_LEFT_ALT;
    case GLFW_KEY_LEFT_SUPER: return CLAY_KEY_LEFT_META;
    default: break;
    }
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return (cl_key)(CLAY_KEY_A + (key - GLFW_KEY_A));
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        return (cl_key)(CLAY_KEY_0 + (key - GLFW_KEY_0));
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12)
        return (cl_key)(CLAY_KEY_F1 + (key - GLFW_KEY_F1));
    return CLAY_KEY_NONE;
}

struct EventQueue {
    std::vector<cl_input_event> events;
    double last_cursor_x = 0.0;
    double last_cursor_y = 0.0;
};

EventQueue *queue_for(GLFWwindow *wnd) {
    return static_cast<EventQueue *>(glfwGetWindowUserPointer(wnd));
}

void key_cb(GLFWwindow *wnd, int key, int, int action, int) {
    EventQueue *q = queue_for(wnd);
    if (!q) return;
    cl_key k = glfw_to_key(key);
    if (k == CLAY_KEY_NONE) return;
    cl_input_event e = cl_input_event_make(
        action == GLFW_PRESS ? CLAY_IN_PRESS : CLAY_IN_RELEASE, k);
    glfwGetCursorPos(wnd, &e.x, &e.y);
    q->events.push_back(e);
}

void mouse_button_cb(GLFWwindow *wnd, int button, int action, int) {
    EventQueue *q = queue_for(wnd);
    if (!q) return;
    cl_key k = CLAY_KEY_NONE;
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT: k = CLAY_KEY_MOUSE_LEFT; break;
    case GLFW_MOUSE_BUTTON_RIGHT: k = CLAY_KEY_MOUSE_RIGHT; break;
    case GLFW_MOUSE_BUTTON_MIDDLE: k = CLAY_KEY_MOUSE_MIDDLE; break;
    default: return;
    }
    cl_input_event e = cl_input_event_make(
        action == GLFW_PRESS ? CLAY_IN_PRESS : CLAY_IN_RELEASE, k);
    glfwGetCursorPos(wnd, &e.x, &e.y);
    q->events.push_back(e);
}

void cursor_cb(GLFWwindow *wnd, double x, double y) {
    EventQueue *q = queue_for(wnd);
    if (!q) return;
    cl_input_event e = cl_input_event_make(CLAY_IN_MOTION, CLAY_KEY_NONE);
    e.x = x;
    e.y = y;
    e.dx = x - q->last_cursor_x;
    e.dy = y - q->last_cursor_y;
    q->last_cursor_x = x;
    q->last_cursor_y = y;
    q->events.push_back(e);
}

void scroll_cb(GLFWwindow *wnd, double, double yoff) {
    EventQueue *q = queue_for(wnd);
    if (!q) return;
    if (yoff == 0.0) return;
    cl_input_event e = cl_input_event_make(CLAY_IN_WHEEL, CLAY_KEY_NONE);
    e.wheel = (int)(yoff * 20.0);
    glfwGetCursorPos(wnd, &e.x, &e.y);
    q->events.push_back(e);
}

} // namespace

struct WindowGLFW::Impl {
    GLFWwindow *window = nullptr;
    EventQueue queue;
    int width = 0;
    int height = 0;
    unsigned int texture = 0;
};

WindowGLFW::WindowGLFW(int canvas_width, int canvas_height, const char *title)
    : impl_(new Impl()) {
    impl_->width = canvas_width;
    impl_->height = canvas_height;

    if (!glfwInit()) {
        std::fputs("clay: glfwInit failed\n", stderr);
        delete impl_;
        impl_ = nullptr;
        return;
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    impl_->window = glfwCreateWindow(canvas_width, canvas_height, title,
                                     nullptr, nullptr);
    if (!impl_->window) {
        std::fputs("clay: glfwCreateWindow failed\n", stderr);
        glfwTerminate();
        delete impl_;
        impl_ = nullptr;
        return;
    }

    glfwSetWindowUserPointer(impl_->window, &impl_->queue);
    glfwSetKeyCallback(impl_->window, key_cb);
    glfwSetMouseButtonCallback(impl_->window, mouse_button_cb);
    glfwSetCursorPosCallback(impl_->window, cursor_cb);
    glfwSetScrollCallback(impl_->window, scroll_cb);

    glfwMakeContextCurrent(impl_->window);
    glfwSwapInterval(1);

    glGenTextures(1, &impl_->texture);
    glBindTexture(GL_TEXTURE_2D, impl_->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, canvas_width, canvas_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

WindowGLFW::~WindowGLFW() {
    if (!impl_) return;
    if (impl_->texture) glDeleteTextures(1, &impl_->texture);
    if (impl_->window) glfwDestroyWindow(impl_->window);
    glfwTerminate();
    delete impl_;
}

bool WindowGLFW::should_close() const {
    return impl_ == nullptr || impl_->window == nullptr ||
           glfwWindowShouldClose(impl_->window);
}

void WindowGLFW::poll_events() {
    if (impl_ && impl_->window) glfwPollEvents();
}

std::vector<cl_input_event> WindowGLFW::drain_events() {
    std::vector<cl_input_event> out;
    if (!impl_) return out;
    out.swap(impl_->queue.events);
    return out;
}

int WindowGLFW::canvas_width() const {
    return impl_ ? impl_->width : 0;
}
int WindowGLFW::canvas_height() const {
    return impl_ ? impl_->height : 0;
}

void WindowGLFW::present(const Framebuffer &fb) {
    if (!impl_ || !impl_->window || fb.width != impl_->width ||
        fb.height != impl_->height)
        return;

    glBindTexture(GL_TEXTURE_2D, impl_->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb.width, fb.height, GL_RGBA,
                    GL_UNSIGNED_BYTE, fb.as_rgba());

    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, fb.width, fb.height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f((float)fb.width, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f((float)fb.width, (float)fb.height);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, (float)fb.height);
    glEnd();

    glfwSwapBuffers(impl_->window);
}

} // namespace clay