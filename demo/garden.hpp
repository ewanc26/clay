#ifndef CLAY_DEMO_GARDEN_HPP
#define CLAY_DEMO_GARDEN_HPP

#include "runtime.hpp"

#include <memory>
#include <string>

namespace clay {

/* The default reaction-rules table, embedded as data in garden.cpp. */
extern const char *kGardenReactions;

/* "The Clay Garden": the proof scene. A sculpture the world watches over, a
 * herd of cursor-magnet animals, pebbles, ambient life, and a reaction-rules
 * table (pure data) that turns every input into world change. */
class Garden {
  public:
    /* Uses same seed semantics as Runtime: same seed -> same garden. */
    Garden(int width, int height, uint64_t seed);
    ~Garden();

    Garden(const Garden &) = delete;
    Garden &operator=(const Garden &) = delete;

    Runtime &runtime() {
        return *rt_;
    }
    const Runtime &runtime() const {
        return *rt_;
    }

    void seed(const std::string &reactions_json);
    void plant(); /* default garden composition */

    /* Scripted headless drive used by --headless and the test suite. */
    void drive_headless_input(uint64_t frame);

  private:
    int width_;
    int height_;
    std::unique_ptr<Runtime> rt_;
};

} // namespace clay

#endif /* CLAY_DEMO_GARDEN_HPP */