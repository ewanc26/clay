#include "replay.hpp"

namespace clay {

Replayer::Replayer(const cl_input_log &log) : log_(log) {}

void Replayer::rewind() {
    pos_ = 0;
}

size_t Replayer::remaining() const {
    return cl_input_log_count(&log_) - pos_;
}

std::vector<cl_input_event> Replayer::events_for_frame(uint32_t frame) {
    scratch_.clear();
    size_t n = cl_input_log_count(&log_);
    while (pos_ < n) {
        const cl_input_event *e = cl_input_log_at(&log_, pos_);
        if (e->frame != frame) break;
        scratch_.push_back(*e);
        pos_ += 1;
    }
    return scratch_;
}

} // namespace clay