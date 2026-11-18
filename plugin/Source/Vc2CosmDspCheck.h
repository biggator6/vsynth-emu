#pragma once
// xt_vc2cosm's seventh part's kinds and slots (v264): each block code Vc2CosmDsp renders, what its object holds by the
// place rom.4 holds it, and its delay lines (Vc2CosmDspKinds.cpp); the pool as a note finds it (Vc2CosmDspSlots.cpp).
// XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CheckRig.h"
#include "Vc2CosmDsp.h"
#include <random>
#include <span>
#include <string>
#include <vector>

// A word of the state: its place in D, and what the port holds there
struct Vc2CosmDspWord {
    uint32_t at;
    float value;
};
using Vc2CosmDspWords = std::vector<Vc2CosmDspWord>;

// A type's delay lines: the write index, each line at the place of the slot's buffer it is in rom.4, and their length
struct Vc2CosmDspRing {
    int index;
    std::array<const float*, 4> lines;
    int length;
};

// A block code: what its object holds, and its delay lines if it takes a slot
struct Vc2CosmDspKind {
    int32_t code;
    void (*state)(const Vc2CosmDsp& d, const Vc2CosmObjectWords& o, bool stereo, Vc2CosmDspWords& w);
    Vc2CosmDspRing (*ring)(const Vc2CosmDsp& d, const Vc2CosmObjectWords& o);
};

std::span<const Vc2CosmDspKind> vc2CosmDspKinds();
const Vc2CosmDspKind& vc2DspKindOf(int32_t code);
// The next note's kind: the two types that read what the last note left, that note's own again, one with delay lines,
// or any
const Vc2CosmDspKind& vc2DspDrawKind(const Vc2CosmDspKind* last, std::mt19937& rng);

inline constexpr int kDspSlots = Vc2CosmPoolWords::kSlots;
// The first `taken` slots of `pool` in use, rom.4's and the port's alike, so a note takes the one after them
void vc2DspHoldSlots(Vc2Fw& fw, Vc2CosmPools& pools, uint32_t pool, int taken);
// Noise in the words in front of a slot's two line 2 buffers, rom.4's and the port's alike
void vc2DspDrawEnds(Vc2Fw& fw, Vc2CosmPools& pools, uint32_t pool, int slot, std::mt19937& rng);
// Four buffers of RAM in the stack words rom.4's allocator takes when every slot is in use
void vc2DspDrawStaleStack(Vc2Fw& fw);
// That rom.4 played on those words, and neither side has a slot; empty when so
std::string vc2DspStaleDifference(Vc2Fw& fw, const Vc2CosmObjectWords& o);
#endif
