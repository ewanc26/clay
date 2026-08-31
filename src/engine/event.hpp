#ifndef CLAY_ENGINE_EVENT_HPP
#define CLAY_ENGINE_EVENT_HPP

#include <clay/clay.h>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace clay {

/* Interned channel helpers over the core registry. */
cl_channel channel(const std::string &name);
std::string channel_name(cl_channel ch);

/* C++-side copy of a core event (safe to keep). */
struct Event {
    cl_channel channel;
    cl_variant value;
    uint32_t frame;
    double time;

    std::string channel_str() const {
        return channel_name(channel);
    }
};

/* Movable RAII over a core cl_sub*. */
class Subscription {
  public:
    Subscription() = default;
    explicit Subscription(cl_sub *sub) : sub_(sub) {}
    ~Subscription();
    Subscription(const Subscription &) = delete;
    Subscription &operator=(const Subscription &) = delete;
    Subscription(Subscription &&o) noexcept : sub_(o.sub_) {
        o.sub_ = nullptr;
    }
    Subscription &operator=(Subscription &&o) noexcept;
    void reset();
    explicit operator bool() const {
        return sub_ != nullptr;
    }

  private:
    cl_sub *sub_ = nullptr;
};

/* The engine's event hub: a facade over the core cl_bus that also runs
 * std::function handlers, so C-core subscribers and C++ systems hear the
 * exact same event stream in the exact same registered order. */
class Hub {
  public:
    explicit Hub(cl_arena *arena);
    ~Hub();

    Hub(const Hub &) = delete;
    Hub &operator=(const Hub &) = delete;

    using Handler = std::function<void(const cl_event &)>;

    /* Returns an opaque id; unsubscribe(id) detaches. */
    uint64_t subscribe(cl_channel ch, Handler fn);
    uint64_t subscribe_channel_name(cl_channel ch, Handler fn) {
        return subscribe(ch, std::move(fn));
    }
    void unsubscribe(uint64_t id);

    /* Register a handler that sees every event, whatever its channel. */
    uint64_t subscribe_all(Handler fn);

    void publish(cl_channel ch, cl_variant value);
    void publish_at(cl_channel ch, cl_variant value, uint32_t frame,
                    double time);

    cl_bus *bus() {
        return &bus_;
    }
    const cl_bus *bus() const {
        return &bus_;
    }

    size_t handler_count() const;

  private:
    struct Entry {
        cl_channel channel;
        Handler fn;
    };

    cl_arena *arena_;
    cl_bus bus_;
    uint64_t next_id_ = 1;
    std::map<uint64_t, Entry> handlers_;
};

} // namespace clay

#endif /* CLAY_ENGINE_EVENT_HPP */