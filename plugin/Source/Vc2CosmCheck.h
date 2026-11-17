#pragma once
// xt_vc2cosm's parts (v235): the Growl's (Vc2CosmCheck.cpp, v234, and main) and the converters' frame's
// (Vc2CosmFrameCheck.cpp, its draws in Vc2CosmFrameDraws.cpp, v237).  XT_VC2COSM_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2CosmInput.h"
#include <array>
#include <cstring>

// The frame's cases on the run's seed: rom.4's dispatcher and the port's on the same inputs; how many differ
long vc2CosmFrameCases(Vc2Fw& fw, const Vc2Run& run);

// The tables both parts read: COSM's pool words and the Matrix's limits
bool vc2LoadCosmAndMatrix(const char* path);

// Whether the check holds type `type`'s converter ported: its own list, the types it draws parameters for
bool vc2CosmCheckPorted(int type);

// The Matrix core's cases on the run's seed (v243, Vc2MatrixCheck.cpp): rom.4's refresh, readers and getter and the
// port's on the same M; how many differ
long vc2MatrixCases(Vc2Fw& fw, const Vc2Run& run);
// The Matrix set-up's cases (v247, Vc2MatrixSetupCheck.cpp): rom.4's records, assignments, M's init, levels and head
// and the port's on the same G and M; how many differ
long vc2MatrixSetupCases(Vc2Fw& fw, const Vc2Run& run);

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
