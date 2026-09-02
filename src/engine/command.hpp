#ifndef CLAY_ENGINE_COMMAND_HPP
#define CLAY_ENGINE_COMMAND_HPP

#include <clay/clay.h>

#include <cstdint>
#include <string>
#include <vector>

namespace clay {

/* A command is the engine's one permanent account of what the player did:
 * "the player pressed primary at (x, y)", in a form that is logged, can be
 * replayed, and can never change once recorded. Systems consume commands
 * indirectly by watching the CLAY_CH_COMMAND events they produce. */
struct Command {
    std::string name;   /* "primary", "move", "toggle_pause"            */
    std::string source; /* originating action name, or "system"          */
    cl_variant value;
    double x = 0;
    double y = 0;
    uint32_t frame = 0;
    double time = 0;
    bool reversible = false;

    Command() : value(cl_variant_nil()) {}
};

/* Append-only command ledger. Deterministic across builds: the fingerprint
 * hashes the stable serialized form of every entry in arrival order.
 *
 * Reversible commands (cmd.reversible == true) are also pushed onto an undo
 * stack at record time. undo()/redo() move commands between the undo and redo
 * stacks and fold each transition into the fingerprint, so the log stays a
 * single deterministic linear history — replay of an undo sequence reproduces
 * the exact same world state. */
class CommandLog {
  public:
    void record(const Command &cmd);
    void clear();

    const std::vector<Command> &all() const {
        return entries_;
    }
    size_t count() const {
        return entries_.size();
    }
    uint64_t fingerprint() const {
        return fingerprint_;
    }
    const Command *at(size_t i) const;

    /* Return the last command whose name matches (recent-first). */
    const Command *find_last(const std::string &name) const;

    /* --- undo/redo --- */
    size_t undo_count() const {
        return undo_stack_.size();
    }
    size_t redo_count() const {
        return redo_stack_.size();
    }
    /* Pop the most recently recorded reversible command onto the redo stack.
     * Returns the command that was undone (or nullptr if nothing to undo). */
    const Command *undo();
    /* Replay the most recently undone command back onto the undo stack.
     * Returns the command that was redone (or nullptr if nothing to redo). */
    const Command *redo();

  private:
    std::vector<Command> entries_;
    std::vector<Command> undo_stack_; /* reversible commands, most-recent last */
    std::vector<Command> redo_stack_;
    uint64_t fingerprint_ = 0x517CC1B727220A95ULL;
    /* double coercion that keeps NaN out of fingerprints */
    static double get_double(const cl_variant &v);
};

} // namespace clay

#endif /* CLAY_ENGINE_COMMAND_HPP */