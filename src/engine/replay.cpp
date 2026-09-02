#include "replay.hpp"

namespace clay {

Replayer::Replayer(const cl_input_log &log) : log_(log) {}

void Replayer::rewind() {
    pos_ = 0;
}

size_t Replayer::remaining() const {
    const size_t count = cl_input_log_count(&log_);
    return pos_ < count ? count - pos_ : 0;
}

std::vector<cl_input_event> Replayer::events_for_frame(uint32_t frame) {
    scratch_.clear();
    size_t n = cl_input_log_count(&log_);
    while (pos_ < n) {
        const cl_input_event *e = cl_input_log_at(&log_, pos_);
        if (e->frame >= frame) break;
        pos_ += 1;
    }
    while (pos_ < n) {
        const cl_input_event *e = cl_input_log_at(&log_, pos_);
        if (e->frame != frame) break;
        scratch_.push_back(*e);
        pos_ += 1;
    }
    return scratch_;
}

} // namespace clay
