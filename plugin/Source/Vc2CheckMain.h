#pragma once
// What the VC-2 part's checks share from v276 (xt_vc2notes', xt_vc2keys'): a check's main (its start, its sides made on
// the interpreter, its cases and their tally, its counts said) and an event on both sides with its answer compared.
// Check builds only.
#include "Vc2CheckRig.h"
#include <memory>

// How rom.4's answer is read after a routine: its r0, or fr0's bits
inline uint32_t vc2R0(const Sh4& c) { return c.r[0]; }
inline uint32_t vc2Fr0(const Sh4& c) { return c.fb[0][0]; }
using Vc2Theirs = uint32_t (*)(const Sh4&);

// A routine of rom.4's an event runs: its address, its name, and how its answer is read (none when it answers nothing)
struct Vc2CheckRoutine {
    uint32_t routine;
    const char* name;
    Vc2Theirs theirs = nullptr;
};

// An event on both: rom.4's routine on `args` and the port's `mine()` giving its answer's word, then `differs()`; when
// nothing differs and the routine answers, rom.4's answer against the port's
template <class Mine, class Differs>
std::string vc2BothAnswer(Vc2Fw& fw, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args, Mine mine,
                          Differs differs) {
    uint32_t ours = 0;
    return vc2BothOn(fw, r.routine, args, r.name, [&] { ours = mine(); }, [&] {
        const std::string what = differs();
        if (!what.empty() || r.theirs == nullptr || r.theirs(fw.cpu) == ours) return what;
        char t[128];
        std::snprintf(t, sizeof t, "%s answers %08x, against %08x", r.name, ours, r.theirs(fw.cpu));
        return std::string(t);
    });
}

// A check's name, the port's tables' loader and its tally's switches
struct Vc2CheckStart {
    const char* name;
    bool (*load)(const char*);
    Vc2Tally tally;
};

// A check's main: rom.4 and the port's tables, the `Sides` made on the interpreter (they hold the port and its counts,
// `n`, with its events), then each case by `oneCase(sides, rng)` under the tally's switches, and the counts said by
// `say(run, counts, different)`
template <class Sides, class Case, class Say>
int vc2CheckMain(int argc, char** argv, Vc2CheckStart start, Case oneCase, Say say) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, start.name, start.load);
    if (!run) return 1;
    const auto s = std::make_unique<Sides>(fw);
    vc2RunCases(*run, start.tally, "", s->n.events, [&](std::mt19937& rng, bool) { return oneCase(*s, rng); });
    say(*run, s->n, start.tally.different);
    return 0;
}
