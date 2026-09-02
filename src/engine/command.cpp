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

    /* Reversible commands go on the undo stack; an undo clears the redo
     * branch (standard editor semantics — new actions invalidate redos). */
    if (stored.reversible) {
        undo_stack_.push_back(stored);
        redo_stack_.clear();
        hash_feed(fingerprint_, "\x03", 1); /* undo-push marker */
    }
}

const Command *CommandLog::undo() {
    if (undo_stack_.empty()) return nullptr;
    Command cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    redo_stack_.push_back(cmd);
    hash_feed(fingerprint_, "\xff", 1); /* undo marker */
    hash_feed(fingerprint_, cmd.name.data(), cmd.name.size());
    hash_feed(fingerprint_, "\x01", 1);
    hash_feed(fingerprint_, cmd.source.data(), cmd.source.size());
    hash_feed(fingerprint_, "\x02", 1);
    double vals[5] = {get_double(cmd.value), cmd.x, cmd.y,
                      (double)cmd.frame, cmd.time};
    hash_feed(fingerprint_, (const char *)vals, sizeof(vals));
    return &redo_stack_.back();
}

const Command *CommandLog::redo() {
    if (redo_stack_.empty()) return nullptr;
    Command cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    undo_stack_.push_back(cmd);
    hash_feed(fingerprint_, "\xfe", 1); /* redo marker */
    hash_feed(fingerprint_, cmd.name.data(), cmd.name.size());
    hash_feed(fingerprint_, "\x01", 1);
    hash_feed(fingerprint_, cmd.source.data(), cmd.source.size());
    hash_feed(fingerprint_, "\x02", 1);
    double vals[5] = {get_double(cmd.value), cmd.x, cmd.y,
                      (double)cmd.frame, cmd.time};
    hash_feed(fingerprint_, (const char *)vals, sizeof(vals));
    return &undo_stack_.back();
}

void CommandLog::clear() {
    entries_.clear();
    undo_stack_.clear();
    redo_stack_.clear();
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