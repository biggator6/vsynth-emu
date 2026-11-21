// The players' beat on the OSC voice (v325): rom.4's 0x893cbf40 and 0x893cbfe0, rom.2's 0x8950f860 and 0x8950f900
// instruction for instruction, run as XtBeat's start and reset (XtMultiBandPlayer.cpp) on the object's words; and the
// seeks the voice has as its own, one table by address as its renders are: the two with no band player in them, Vari
// off's 0x893c6360 and LITE's heads' 0x893c6de0, each rom.2's on rom.4's object four bytes up (Vc2OscPlayers.cpp's and
// Vc2OscLite.cpp's) and ending in the reset.  Any other seek goes out by its address.  Every address is rom.4's; every
// offset the voice's.  HARDWARE_FIRMWARE.md, "The VC-2's players' beat and seeks".  Audio thread.
#include "Vc2OscPlayers.h"
#include "XtBeat.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

namespace {
// [FW] XtBeat's words in the object (+0x15c), each at its place in the voice: the clocks and Time Trip steps not yet
// taken (+0x40, +0x41), the wave's Original Tempo and the note's, ×10000 (+0x48, +0x4c), a clock's step and the budget
// (+0x50, +0x54), the sync's, trip's, walk's and beat's positions (+0x58…+0x64), the factor and its integral (+0x8c,
// +0x90)
struct Vc2OscBeatWord {
    uint32_t at;
    size_t member, bytes;
};
constexpr std::array<Vc2OscBeatWord, 12> kBeatWords { {
    { 0x19c, offsetof(XtBeat, clk), sizeof(XtBeat::clk) },
    { 0x19d, offsetof(XtBeat, trip), sizeof(XtBeat::trip) },
    { 0x1a4, offsetof(XtBeat, orig), sizeof(XtBeat::orig) },
    { 0x1a8, offsetof(XtBeat, tempo), sizeof(XtBeat::tempo) },
    { 0x1ac, offsetof(XtBeat, step), sizeof(XtBeat::step) },
    { 0x1b0, offsetof(XtBeat, budget), sizeof(XtBeat::budget) },
    { 0x1b4, offsetof(XtBeat, syncPos), sizeof(XtBeat::syncPos) },
    { 0x1b8, offsetof(XtBeat, tripPos), sizeof(XtBeat::tripPos) },
    { 0x1bc, offsetof(XtBeat, walkPos), sizeof(XtBeat::walkPos) },
    { 0x1c0, offsetof(XtBeat, beatPos), sizeof(XtBeat::beatPos) },
    { 0x1e8, offsetof(XtBeat, factor), sizeof(XtBeat::factor) },
    { 0x1ec, offsetof(XtBeat, integ), sizeof(XtBeat::integ) },
} };
// [FW] the player's position ×16 (+0x38), where the start puts the beat; the object's byte 3, whose bits 1…6 the start
// clears; its byte 0's Time Trip bit, under which the reset's factor is 0
constexpr uint32_t kPosition16 = 0x194, kByte3 = 0x15f;
constexpr uint8_t kStartKeeps = 0x81, kTripBit = 0x04;

XtBeat beatOf(const Vc2OscVoice& v) {
    XtBeat b;
    auto* to = reinterpret_cast<char*>(&b);
    for (const auto& [at, member, bytes] : kBeatWords) std::memcpy(to + member, v.at(at), bytes);
    return b;
}

void setBeat(Vc2OscVoice& v, const XtBeat& b) {
    const auto* from = reinterpret_cast<const char*>(&b);
    for (const auto& [at, member, bytes] : kBeatWords) {
        std::memcpy(v.own.data() + (at - Vc2OscVoice::kOwn), from + member, bytes);
    }
}

// [FW] the seeks the voice has as its own, each given the sample (the position ×16 shifted down by 4, as both begin)
struct Vc2OscOwnSeek {
    uint32_t at;
    void (*seek)(Vc2OscVoice&, int32_t);
};
constexpr std::array<Vc2OscOwnSeek, 2> kOwnSeeks { { { 0x893c6360, vc2OscSeekVariOff },
                                                     { 0x893c6de0, vc2OscSeekHeads } } };
constexpr int kSampleShift = 4;
} // namespace

// [FW] 0x893cbf40
void Vc2OscVoice::beatStart() {
    XtBeat b = beatOf(*this);
    b.start(b.tempo, b.orig, (int32_t)u32(kPosition16));
    setBeat(*this, b);
    setU8(kByte3, u8(kByte3) & kStartKeeps);
}

// [FW] 0x893cbfe0
void Vc2OscVoice::beatReset() {
    XtBeat b = beatOf(*this);
    b.reset((u8(kObject) & kTripBit) != 0);
    setBeat(*this, b);
}

void Vc2OscVoice::seekBy(uint32_t routine, int32_t at, Vc2OscParts& parts) {
    const auto* own = std::ranges::find(kOwnSeeks, routine, &Vc2OscOwnSeek::at);
    if (own == kOwnSeeks.end()) {
        parts.seek(routine, at);
        return;
    }
    own->seek(*this, at >> kSampleShift);
    beatReset();
}
