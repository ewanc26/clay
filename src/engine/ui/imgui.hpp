#ifndef CLAY_ENGINE_UI_IMGUI_HPP
#define CLAY_ENGINE_UI_IMGUI_HPP

#include "render/renderer.hpp"
#include "render/text.hpp"

#include <cstdint>

namespace clay {

struct UITheme {
    Rgba text_color{255, 255, 255, 255};
    Rgba button_bg{60, 60, 70, 255};
    Rgba button_hover{80, 80, 100, 255};
    Rgba button_active{100, 100, 130, 255};
    Rgba checkbox_check{200, 200, 200, 255};
    Rgba slider_track{50, 50, 60, 255};
    Rgba slider_fill{100, 100, 130, 255};
    Rgba bg_color{30, 30, 35, 255};
    float padding = 6.0f;
    float spacing = 4.0f;
    float button_height = 24.0f;
    float slider_width = 100.0f;
    float checkbox_size = 12.0f;
    float item_label_width = 80.0f;
};

class UIContext {
public:
    explicit UIContext(IRenderer &r);

    void begin_frame(float cursor_x, float cursor_y, bool mouse_down);
    void end_frame();

    void begin_column(float x, float y);
    void end_column();
    void same_line(float offset_x = 0.0f);
    void spacing(float amount = 0.0f);

    float cursor_y() const;
    void set_theme(const UITheme &t) {
        theme_ = t;
    }
    const UITheme &theme() const {
        return theme_;
    }

    void label(const char *text);
    bool button(const char *text);
    bool checkbox(const char *label, bool *value);
    bool slider(const char *label, float *value, float min, float max);

private:
    IRenderer &r_;
    UITheme theme_;

    float mx_ = 0.0f;
    float my_ = 0.0f;
    bool mouse_down_ = false;
    bool mouse_was_down_ = false;
    bool mouse_just_pressed_ = false;

    float column_x_ = 0.0f;
    float cursor_y_ = 0.0f;
    float row_start_x_ = 0.0f;
    float row_y_ = 0.0f;
    bool in_row_ = false;
    float item_right_ = 0.0f;

    bool has_active_ = false;
    bool active_released_ = false;
};

} // namespace clay

#endif /* CLAY_ENGINE_UI_IMGUI_HPP */
