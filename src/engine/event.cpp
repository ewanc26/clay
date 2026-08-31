#include "event.hpp"

#include <algorithm>

namespace clay {

cl_channel channel(const std::string &name) {
    return cl_channel_intern(cl_str_c(name.c_str()));
}

std::string channel_name(cl_channel ch) {
    const char *n = cl_channel_name(ch);
    return n ? std::string(n) : std::string();
}

/* ----------------------------------------------------------------- Subscrib. */

Subscription::~Subscription() {
    reset();
}

Subscription &Subscription::operator=(Subscription &&o) noexcept {
    if (this != &o) {
        reset();
        sub_ = o.sub_;
        o.sub_ = nullptr;
    }
    return *this;
}

void Subscription::reset() {
    if (sub_) {
        cl_bus_unsubscribe(sub_);
        sub_ = nullptr;
    }
}

/* -------------------------------------------------------------------- Hub */

Hub::Hub(cl_arena *arena) : arena_(arena) {
    cl_bus_init(&bus_, arena_);
}

Hub::~Hub() {
    handlers_.clear();
}

/* A channel of CLAY_CHANNEL_NONE is the "every channel" broadcast. */
uint64_t Hub::subscribe(cl_channel ch, Handler fn) {
    uint64_t id = next_id_++;
    handlers_[id] = Entry{ch, std::move(fn)};
    return id;
}

uint64_t Hub::subscribe_all(Handler fn) {
    return subscribe(CLAY_CHANNEL_NONE, std::move(fn));
}

void Hub::unsubscribe(uint64_t id) {
    auto it = handlers_.find(id);
    if (it == handlers_.end()) return;
    it->second.fn = nullptr; /* tombstone in flight; erased on next publish */
    handlers_.erase(it);
}

void Hub::publish(cl_channel ch, cl_variant value) {
    publish_at(ch, value, 0, 0.0);
}

void Hub::publish_at(cl_channel ch, cl_variant value, uint32_t frame,
                     double time) {
    /* Core subscribers (C side) hear every event first. */
    cl_bus_publish_at(&bus_, ch, value, frame, time);

    const cl_event core_ev = {.channel = ch,
                              .channel_name = cl_channel_name(ch),
                              .value = value,
                              .frame = frame,
                              .time = time};

    /* Snapshot the handler ids so a handler subscribing/unsubscribing within
     * a callback cannot reorder or corrupt the walk. */
    std::vector<uint64_t> ids;
    ids.reserve(handlers_.size());
    for (const auto &pair : handlers_) {
        if (pair.second.channel == CLAY_CHANNEL_NONE || pair.second.channel == ch) {
            ids.push_back(pair.first);
        }
    }
    for (uint64_t id : ids) {
        auto it = handlers_.find(id);
        if (it == handlers_.end() || !it->second.fn) continue;
        it->second.fn(core_ev);
    }
}

size_t Hub::handler_count() const {
    return handlers_.size();
}

} // namespace clay