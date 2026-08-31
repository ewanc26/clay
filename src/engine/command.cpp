#include "command.hpp"

#include <cmath>

namespace clay {

static void hash_feed(uint64_t &h, const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)data[i];
        h = cl_hash_u64(h);
    }
}

void CommandLog::record(const Command &cmd) {
    entries_.push_back(cmd);
    Command &stored = entries_.back();

    hash_feed(fingerprint_, stored.name.data(), stored.name.size());
    hash_feed(fingerprint_, "\x01", 1);
    hash_feed(fingerprint_, stored.source.data(), stored.source.size());
    hash_feed(fingerprint_, "\x02", 1);
    double vals[5] = {get_double(stored.value), stored.x, stored.y,
                      (double)stored.frame, stored.time};
    hash_feed(fingerprint_, (const char *)vals, sizeof(vals));
}

void CommandLog::clear() {
    entries_.clear();
    fingerprint_ = 0x517CC1B727220A95ULL;
}

const Command *CommandLog::at(size_t i) const {
    return i < entries_.size() ? &entries_[i] : nullptr;
}

const Command *CommandLog::find_last(const std::string &name) const {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->name == name) return &*it;
    }
    return nullptr;
}

double CommandLog::get_double(const cl_variant &v) {
    double d = cl_variant_to_double(v);
    return d == d ? d : 0.0; /* NaN -> 0 keeps fingerprints stable */
}

} // namespace clay