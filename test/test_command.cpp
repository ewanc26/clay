#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "command.hpp"

using namespace clay;

namespace {

Command make_cmd(int n) {
    Command c;
    c.name = n % 2 == 0 ? "primary" : "move";
    c.source = c.name;
    c.value = cl_variant_i64(n);
    c.x = (double)n;
    c.y = (double)n * 0.5;
    c.frame = (uint32_t)n;
    c.time = (double)n * 0.016;
    c.reversible = n % 3 == 0;
    return c;
}

} // namespace

TEST_CASE("command_log: append-only, deterministic fingerprint") {
    CommandLog log;
    for (int i = 1; i <= 50; i++) log.record(make_cmd(i));
    CHECK(log.count() == 50);
    uint64_t fp = log.fingerprint();

    CommandLog log2;
    for (int i = 1; i <= 50; i++) log2.record(make_cmd(i));
    CHECK(log2.fingerprint() == fp);

    /* Reordering changes the fingerprint. */
    CommandLog log3;
    for (int i = 50; i >= 1; i--) log3.record(make_cmd(i));
    CHECK(log3.fingerprint() != fp);
}

TEST_CASE("command_log: find_last returns most recent matching name") {
    CommandLog log;
    for (int i = 1; i <= 10; i++) log.record(make_cmd(i));

    /* Even n -> "primary". Recent matches: 10, 8, 6, ...; last is 10. */
    const Command *last = log.find_last("primary");
    REQUIRE(last != nullptr);
    CHECK(last->frame == 10);

    const Command *first = log.at(0);
    REQUIRE(first != nullptr);
    CHECK(first->name == "move"); /* n=1 -> "move", n=2 -> "primary" */
    CHECK(first->frame == 1);

    const Command *first_primary = log.at(1);
    REQUIRE(first_primary != nullptr);
    CHECK(first_primary->name == "primary");

    CHECK(log.find_last("nonexistent") == nullptr);
}

TEST_CASE("command_log: clear resets ledger and fingerprint") {
    CommandLog log;
    for (int i = 1; i <= 5; i++) log.record(make_cmd(i));
    uint64_t before = log.fingerprint();
    log.clear();
    CHECK(log.count() == 0);
    CHECK(log.fingerprint() != before);
    log.record(make_cmd(1));
    CHECK(log.count() == 1);
}

TEST_CASE("command_log: undo/redo stacks for reversible commands") {
    CommandLog log;
    /* Record 5 commands: reversible for n % 3 == 0 (n=3,6,9,...).
     * In 1..5: n=3 is reversible. n=3 is odd -> name "move". */
    for (int i = 1; i <= 5; i++) log.record(make_cmd(i));

    /* Only n=3 was reversible, so undo stack has 1 entry. */
    CHECK(log.undo_count() == 1);
    CHECK(log.redo_count() == 0);

    /* Undo pops the last reversible command. */
    const Command *undone = log.undo();
    REQUIRE(undone != nullptr);
    CHECK(undone->name == "move"); /* n=3 -> "move" */
    CHECK(undone->frame == 3);
    CHECK(log.undo_count() == 0);
    CHECK(log.redo_count() == 1);

    /* Can't undo past the stack bottom. */
    CHECK(log.undo() == nullptr);

    /* Redo pushes it back. */
    const Command *redone = log.redo();
    REQUIRE(redone != nullptr);
    CHECK(redone->frame == 3);
    CHECK(log.undo_count() == 1);
    CHECK(log.redo_count() == 0);
}

TEST_CASE("command_log: undo/redo is deterministic across replays") {
    /* Same record/undo/redo sequence must produce identical fingerprints
     * regardless of how the stacks were reached. Undo/redo changes state
     * (the fingerprint reflects the operation), but it must be reproducible. */
    CommandLog log;
    for (int i = 1; i <= 3; i++) log.record(make_cmd(i));
    log.undo();
    uint64_t fp_after_undo = log.fingerprint();

    CommandLog log2;
    for (int i = 1; i <= 3; i++) log2.record(make_cmd(i));
    log2.undo();
    CHECK(log2.fingerprint() == fp_after_undo);

    /* Redo from the same state is also deterministic. */
    log.redo();
    log2.redo();
    CHECK(log.fingerprint() == log2.fingerprint());
}

TEST_CASE("command_log: new actions branch the redo stack") {
    /* Standard editor semantics: recording after an undo clears the redo
     * stack, just like recording on a fresh log. */
    CommandLog log;
    for (int i = 1; i <= 3; i++) log.record(make_cmd(i));
    log.undo();
    CHECK(log.redo_count() == 1);

    /* Recording a new reversible command after undo invalidates redo. */
    Command c = make_cmd(99);
    c.reversible = true;
    log.record(c);
    CHECK(log.redo_count() == 0);

    /* The new command is on the undo stack. */
    const Command *undone = log.undo();
    REQUIRE(undone != nullptr);
    CHECK(undone->frame == 99);
}