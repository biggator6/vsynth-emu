#pragma once
// xt_vc2cosm's parts (v235): the Growl's (Vc2CosmCheck.cpp, v234, and main) and the converters' frame's
// (Vc2CosmFrameCheck.cpp, its draws in Vc2CosmFrameDraws.cpp, v237).  XT_VC2COSM_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2CosmInput.h"
#include <array>
#include <cstring>

// The frame's cases on the run's seed: rom.4's dispatcher and the port's on the same inputs; how many differ
long vc2CosmFrameCases(Vc2Fw& fw, const Vc2Run& run);

// The tables the parts read: COSM's pool words and the Matrix's limits from rom.4 at `path`; and (v252) XtCosm's,
// which the adapted types run on, from the rom.2 beside it (its name with rom.4 as rom.2, as the plugin finds them)
bool vc2LoadCosmAndMatrix(const char* path);

// Whether the check holds type `type`'s converter ported: its own list, the types it draws parameters for
bool vc2CosmCheckPorted(int type);

// The Matrix core's cases on the run's seed (v243, Vc2MatrixCheck.cpp): rom.4's refresh, readers and getter and the
// port's on the same M; how many differ
long vc2MatrixCases(Vc2Fw& fw, const Vc2Run& run);
// The Matrix set-up's cases (v247, Vc2MatrixSetupCheck.cpp): rom.4's records, assignments, M's init, levels and head
// and the port's on the same G and M; how many differ
long vc2MatrixSetupCases(Vc2Fw& fw, const Vc2Run& run);
// The COSM object's frame's cases (v249, Vc2CosmObjectCheck.cpp): rom.4's constructor, set-up, binding and install and
// the port's on the same object; how many differ
long vc2CosmObjectCases(Vc2Fw& fw, const Vc2Run& run);

// The COSM note-on frame's cases (v250, Vc2CosmNoteOnCheck.cpp): rom.4's note-on, new block, destructor and pools and
// the port's on the same object and pools; how many differ
long vc2CosmNoteOnCases(Vc2Fw& fw, const Vc2Run& run);
// The COSM DSP's cases (v263, Vc2CosmDspCheck.cpp): rom.4's own DSP routines and Vc2CosmDsp on the same notes, blocks,
// updates and frames, the output and the state compared; how many differ
long vc2CosmDspCases(Vc2Fw& fw, const Vc2Run& run);

// An event of a part's table, each entry its share (v250: the object's part and the note-on's): counted, then run
template <class Rig, size_t N>
std::string vc2CosmEvent(Rig& r, const std::array<std::string (*)(Rig&, std::mt19937&), N>& events,
                         std::mt19937& rng) {
    ++r.n.events;
    return events[rng() % N](r, rng);
}

// What a routine on both sides is called in a difference, and whether its answers are compared
struct Vc2CosmAsk {
    const char* what;
    bool answered = true;
};

// A routine on both sides (v249: the set-up's part and the object's): rom.4's `routine` on `args`, then the port's
// `mine()`, its answer; what `differs()` says, else the answers when the ask has them compared
template <class Mine, class Differs>
std::string vc2CosmBoth(Vc2Fw& fw, uint32_t routine, std::initializer_list<uint32_t> args, Vc2CosmAsk ask, Mine mine,
                        Differs differs) {
    uint32_t r0 = 0;
    if (!fw.call(routine, args, &r0)) return std::string(ask.what) + " faults: " + fw.cpu.error();
    const uint32_t answer = mine();
    const std::string said = differs();
    if (!said.empty()) return std::string(ask.what) + ": " + said;
    return ask.answered && r0 != answer ? std::string(ask.what) + "'s answer" : "";
}

// Words of noise over `x`, a word a draw
template <class Words>
void vc2CosmNoise(Words& x, std::mt19937& rng) {
    std::array<uint32_t, sizeof x / 4> noise {};
    for (uint32_t& w : noise) w = (uint32_t)rng();
    std::memcpy(&x, noise.data(), sizeof x);
}

// A part of the check on its rig (v243: the frame's and the Matrix core's): `hook()` rom.4's side once, then each case
// `start(rng)` and up to `events` of `event(rng)` until one says what differs, through vc2CaseOutcome after the rig's
// `n.events`; `say(run)` its counts; how many cases differ.  Its generator the run's seed + `seedOffset`
template <class Rig>
long vc2CosmPart(Vc2Fw& fw, const Vc2Run& run, uint32_t seedOffset, int events) {
    Rig r { fw };
    r.hook();
    std::mt19937 rng((uint32_t)run.seed + seedOffset);
    Vc2Tally tally { "XT_VC2COSM_FIRST", "XT_VC2COSM_STOP" };
    for (int c = 0; c < run.cases; ++c) {
        r.start(rng);
        std::string what;
        for (int e = 0; e < events && what.empty(); ++e) what = r.event(rng);
        if (vc2CaseOutcome(tally, what, c, r.n.events)) break;
    }
    r.say(run);
    return tally.different;
}

// A converter's input drawn: noise, then the switch, the type (mostly one the port has), the curves, sensitivities,
// key, velocity and the type's parameters by their ranges; M at `matrixAt`
void vc2DrawCosmInput(Vc2CosmInput& in, std::mt19937& rng, uint32_t matrixAt);
