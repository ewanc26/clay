#ifndef CLAY_ENGINE_REPLAY_HPP
#define CLAY_ENGINE_REPLAY_HPP

#include <clay/clay.h>

#include <cstddef>
#include <vector>

namespace clay {

/* Drives the raw input log back through the exact same path live input uses.
 * A replay is just the recorded transcript re-fed into cl_input_state_feed;
 * because every downstream layer is deterministic given the same transcript,
 * the rendered result must be byte-identical to the original run. */
class Replayer {
  public:
    explicit Replayer(const cl_input_log &log);

    void rewind();
    size_t remaining() const;

    /* All recorded events stamped with this frame, consumed. */
    std::vector<cl_input_event> events_for_frame(uint32_t frame);

  private:
    const cl_input_log &log_;
    size_t pos_ = 0;
    std::vector<cl_input_event> scratch_;
};

} // namespace clay

#endif /* CLAY_ENGINE_REPLAY_HPP */