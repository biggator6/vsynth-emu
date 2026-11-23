// The players' beat on the OSC voice (v325): rom.4's 0x893cbf40 and 0x893cbfe0, rom.2's 0x8950f860 and 0x8950f900
// instruction for instruction, run as XtBeat's start and reset (XtMultiBandPlayer.cpp) on the object's words; and the
// seeks the voice has as its own, one table by address as its renders are: the two with no band player in them, Vari
// off's 0x893c6360 and LITE's heads' 0x893c6de0, each rom.2's on rom.4's object four bytes up (Vc2OscPlayers.cpp's and
// Vc2OscLite.cpp's) and ending in the reset, and from v329 mode 1's 0x893cc830 (Vc2OscWalkStart.cpp), which ends in it
// too, from v333 the band players' 0x893cd440 (Vc2OscBands.cpp), whose reset comes before its bands' part, which
// reads nothing of the beat.  Any other seek goes out by its address.  From v326 the beat's block,
// 0x893c8990, which a player's render runs first: XtBeat's block (rom.2's 0x8950c280, but for its steer giving up four
// steps past the target, XtBeat::pastShift), the loop's positions read through the record the object names, then the
// block's tail, the step's word by the object's byte 3; and the copy of a rom.2 port's words to and from the voice's,
// the beat's and (Vc2OscHeads.cpp) XtLitePlayer's.  Every address is rom.4's; every offset the voice's.
// HARDWARE_FIRMWARE.md, "The VC-2's players' beat and seeks", "The VC-2's LITE heads".  Audio thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscWalk.h"
#include "XtBeat.h"
#include "XtFloat.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>

namespace {
// [FW] XtBeat's words in the object (+0x15c), each at its place in the voice: the clocks and Time Trip steps not yet
// taken (+0x40, +0x41), the wave's Original Tempo and the note's, ×10000 (+0x48, +0x4c), a clock's step and the budget
// (+0x50, +0x54), the sync's, trip's, walk's and beat's positions (+0x58…+0x64), the factor and its integral (+0x8c,
// +0x90)
constexpr std::array<XtWord, 12> kBeatWords { {
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

uint32_t inVoice(uint32_t at) { return at; }   // the beat's rows hold the voice's own offsets

XtBeat beatOf(const Vc2OscVoice& v) {
    XtBeat b;
    vc2OscWordsFrom(v, &b, kBeatWords, inVoice);
    return b;
}

void setBeat(Vc2OscVoice& v, const XtBeat& b) { vc2OscWordsTo(v, &b, kBeatWords, inVoice); }

// [FW] rom.4's beat block's (0x893c8990) words besides XtBeat's: the record the object's +0x6c names (the block's +0xe0
// record's copy) and its four loop events' positions, ×16, 0xb4 bytes apart (the forward loop's start and end, the
// backward's end and start); the walk (+0x7c); the step's word (+0x34); the word the hold's end sets 1 (+0xa0, rom.2's
// +0x9c, four bytes up)
constexpr uint32_t kEventsAt = 0x1c8, kWalk = 0x1d8, kStepWord = 0x190, kResumed = 0x1fc;
constexpr std::array<uint32_t, 4> kLoopEvents { 0x4, 0xb8, 0x16c, 0x220 };
// [FW] byte 3's bits the block's tail (0x893c8b1c…, rom.2's 0x8950c3fc…) reads: 0x40 the walk held, 0x20 the way that
// lets it go, the bits a hold's end keeps; 0x01 a step of one sample, 0x08 turned going forward, 0x04 going back, 0x02
// turned
constexpr uint8_t kHeld = 0x40, kHeldBack = 0x20, kHeldKeeps = 0x8f;
constexpr uint8_t kOneSample = 0x01, kTurnFwd = 0x08, kTurnBwd = 0x04, kTurned = 0x02;
constexpr int32_t kOneStep = 0x2000000;   // [FW] the pool word 0x893c8b70: a sample, × 2^25
constexpr int kRom4Past = 2;   // [FW] rom.4's steer (0x893c8c30) gives up past four steps from the target, rom.2's one

int32_t negated(int32_t x) { return (int32_t)(0u - (uint32_t)x); }

// [FW] the step's word by byte 3: held (0x40) 0, the hold let go (bits 0x70 cleared, +0xa0 1) once the walk goes the
// way 0x20 says; else the walk's word, or a sample's with 0x01 by the walk's way, turned with 0x08 going forward (going
// back 0x08 cleared) and with 0x04 going back (going forward 0x08 cleared, sic), and with 0x02
int32_t beatStep(Vc2OscVoice& v, float w) {
    uint8_t b3 = v.u8(kByte3);
    if ((b3 & kHeld) != 0) {
        if ((b3 & kHeldBack) != 0 ? 0.0f > w : w > 0.0f) {
            v.setF32(kResumed, 1.0f);
            v.setU8(kByte3, b3 & kHeldKeeps);
        }
        return 0;
    }
    const bool back = 0.0f > w;
    int32_t word = xtf::ftrc(w);
    if ((b3 & kOneSample) != 0) word = back ? -kOneStep : kOneStep;
    if ((b3 & kTurnFwd) != 0 && back) b3 &= (uint8_t)~kTurnFwd;
    else if ((b3 & kTurnFwd) != 0) word = negated(word);
    if ((b3 & kTurnBwd) != 0 && back) word = negated(word);
    else if ((b3 & kTurnBwd) != 0) b3 &= (uint8_t)~kTurnFwd;
    v.setU8(kByte3, b3);
    return (b3 & kTurned) != 0 ? negated(word) : word;
}

// [FW] the seeks the voice has as its own, each given the word as the voice's callers give it: Vari off's and the
// heads' take the sample (the position ×16 shifted down by 4, as both begin), mode 1's (v329) and the band players'
// (v333, Vc2OscBands.cpp) the word itself
constexpr int kSampleShift = 4;
void variOffSeek(Vc2OscVoice& v, Vc2OscParts&, int32_t at) { vc2OscSeekVariOff(v, at >> kSampleShift); }
void headsSeek(Vc2OscVoice& v, Vc2OscParts&, int32_t at) { vc2OscSeekHeads(v, at >> kSampleShift); }
struct Vc2OscOwnSeek {
    uint32_t at;
    void (*seek)(Vc2OscVoice&, Vc2OscParts&, int32_t);
};
constexpr std::array<Vc2OscOwnSeek, 4> kOwnSeeks { { { 0x893c6360, variOffSeek },
                                                     { 0x893cc830, vc2OscSeekModeOne },
                                                     { 0x893c6de0, headsSeek },
                                                     { 0x893cd440, vc2OscSeekBands } } };
} // namespace

void vc2OscWordsFrom(const Vc2OscVoice& v, void* port, std::span<const XtWord> words, Vc2OscPlace place) {
    auto* to = static_cast<char*>(port);
    for (const auto& [at, member, bytes] : words) std::memcpy(to + member, v.at(place(at)), bytes);
}

void vc2OscWordsTo(Vc2OscVoice& v, const void* port, std::span<const XtWord> words, Vc2OscPlace place) {
    const auto* from = static_cast<const char*>(port);
    for (const auto& [at, member, bytes] : words) {
        std::memcpy(v.own.data() + (place(at) - Vc2OscVoice::kOwn), from + member, bytes);
    }
}

// [FW] 0x893c8990 (v326): XtBeat's block, its loop's positions read through the record, then the step's word by byte 3
int32_t Vc2OscVoice::beatBlock(Vc2OscParts& parts) {
    XtBeat b = beatOf(*this);
    b.pastShift = kRom4Past;
    std::array<int32_t, kLoopEvents.size()> lp {};
    for (size_t k = 0; k < lp.size(); ++k) {
        lp[k] = std::bit_cast<int32_t>(parts.boundFloat(u32(kEventsAt), kLoopEvents[k]));
    }
    const float w = b.block(u8(kObject), f32(kWalk), lp.data());
    setBeat(*this, b);
    const int32_t word = beatStep(*this, w);
    setU32(kStepWord, (uint32_t)word);
    return word;
}

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
    own->seek(*this, parts, at);
    beatReset();
}
