#include "ui/imgui.hpp"

#include "render/text.hpp"

namespace clay {

UIContext::UIContext(IRenderer &r) : r_(r) {}

void UIContext::begin_frame(float cursor_x, float cursor_y, bool mouse_down) {
    mx_ = cursor_x;
    my_ = cursor_y;
    mouse_just_pressed_ = mouse_down && !mouse_was_down_;
    mouse_down_ = mouse_down;
    mouse_was_down_ = mouse_down;

    if (active_released_) {
        has_active_ = false;
        active_released_ = false;
    }
    in_row_ = false;
}

void UIContext::end_frame() {
    in_row_ = false;
    active_released_ = false;
}

void UIContext::begin_column(float x, float y) {
    column_x_ = x;
    cursor_y_ = y;
    row_y_ = y;
    row_start_x_ = x;
    item_right_ = x;
    in_row_ = false;
}

void UIContext::end_column() {
    in_row_ = false;
}

void UIContext::same_line(float offset_x) {
    if (!in_row_) {
        row_start_x_ = item_right_;
        in_row_ = true;
    }
    column_x_ = row_start_x_ + offset_x;
    cursor_y_ = row_y_;
}

void UIContext::spacing(float amount) {
    if (amount < 0.0f) amount = theme_.spacing;
    cursor_y_ += amount;
}

float UIContext::cursor_y() const {
    return cursor_y_;
}

void UIContext::label(const char *text) {
    row_y_ = cursor_y_;
    draw_text(r_, (int)column_x_, (int)cursor_y_, text, theme_.text_color);
    int width = draw_text(r_, (int)column_x_, (int)cursor_y_, text,
                          Rgba{0, 0, 0, 0});
    item_right_ = column_x_ + (float)width;
    cursor_y_ += (float)Font5x7::get().char_height() + theme_.spacing;
    in_row_ = false;
}

bool UIContext::button(const char *text) {
    row_y_ = cursor_y_;
    int text_w = draw_text(r_, 0, 0, text, Rgba{0, 0, 0, 0});
    float w = (float)text_w + 2.0f * theme_.padding;
    float h = theme_.button_height;

    bool hovered = mx_ >= column_x_ && mx_ < column_x_ + w &&
                   my_ >= cursor_y_ && my_ < cursor_y_ + h;
    Rgba bg = hovered ? theme_.button_hover : theme_.button_bg;
    r_.fill_rect(column_x_, cursor_y_, w, h, bg);
    draw_text(r_, (int)(column_x_ + theme_.padding),
              (int)(cursor_y_ + 4.0f), text, theme_.text_color);

    item_right_ = column_x_ + w;
    cursor_y_ += h + theme_.spacing;
    in_row_ = false;
    return hovered && mouse_just_pressed_;
}

bool UIContext::checkbox(const char *label, bool *value) {
    row_y_ = cursor_y_;
    float box_size = theme_.checkbox_size;
    float label_x = column_x_ + box_size + theme_.padding;
    float label_y = cursor_y_;

    draw_text(r_, (int)label_x, (int)label_y, label, theme_.text_color);

    bool hovered = mx_ >= column_x_ && mx_ < column_x_ + box_size &&
                   my_ >= cursor_y_ && my_ < cursor_y_ + box_size;
    Rgba box_bg = hovered ? theme_.button_hover : theme_.button_bg;
    r_.fill_rect(column_x_, cursor_y_, box_size, box_size, box_bg);
    if (*value) {
        r_.fill_rect(column_x_ + 2.0f, cursor_y_ + 2.0f,
                     box_size - 4.0f, box_size - 4.0f,
                     theme_.checkbox_check);
    }

    bool clicked = hovered && mouse_just_pressed_;
    if (clicked) *value = !*value;

    item_right_ = label_x;
    cursor_y_ += box_size + theme_.spacing;
    in_row_ = false;
    return clicked;
}

bool UIContext::slider(const char *label, float *value, float min,
                       float max) {
    row_y_ = cursor_y_;
    float label_w = (float)theme_.item_label_width;
    draw_text(r_, (int)column_x_, (int)cursor_y_, label, theme_.text_color);

    float track_x = column_x_ + label_w;
    float track_y = cursor_y_;
    float track_w = theme_.slider_width;
    float track_h = 8.0f;

    float t = (max > min) ? (*value - min) / (max - min) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float knob_x = track_x + t * track_w;
    float knob_r = 6.0f;

    bool hovered = mx_ >= track_x - knob_r && mx_ < track_x + track_w + knob_r &&
                   my_ >= track_y - knob_r && my_ < track_y + track_h + knob_r;

    bool just_pressed = hovered && mouse_just_pressed_;

    if (just_pressed || (has_active_ && mouse_down_)) {
        has_active_ = true;
        float new_t = (mx_ - track_x) / track_w;
        if (new_t < 0.0f) new_t = 0.0f;
        if (new_t > 1.0f) new_t = 1.0f;
        *value = min + new_t * (max - min);
    } else if (!mouse_down_) {
        has_active_ = false;
    }

    r_.fill_rect(track_x, track_y, track_w, track_h, theme_.slider_track);
    float fill_w = t * track_w;
    r_.fill_rect(track_x, track_y, fill_w, track_h, theme_.slider_fill);
    r_.fill_circle(knob_x, track_y + track_h / 2.0f, knob_r,
                   theme_.button_hover);

    item_right_ = column_x_ + label_w + track_w;
    cursor_y_ += track_h + theme_.spacing + (float)Font5x7::get().char_height();
    in_row_ = false;
    return true;
}

} // namespace clay
