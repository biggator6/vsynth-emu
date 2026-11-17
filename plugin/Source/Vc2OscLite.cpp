// LITE's own note-on (v240): rom.4's 0x893c7c30, kind 1's, in the players' code; the kinds' note-on reaches it by its
// row (Vc2OscNoteOn::lite).  The object's head and the targets' words, the object's byte 0 from the block, then by the
// wave's type: 0 Vari off's words (what VariPhrase's note-on writes, Vc2OscPlayers.h), 1 the mode-1 player's range,
// 2 the two heads' words (rom.2's XtLitePlayer's, four bytes up in rom.4's object); then 0x893cbf40 on the object and
// the type's seek.  The players' own routines stay rom.4's by address through Vc2OscParts.  Every address is rom.4's;
// every offset the voice's.  HARDWARE_FIRMWARE.md, "The VC-2's LITE note-on".  Audio thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscTables.h"
#include "Vc2OscTargets.h"
#include "XtFloat.h"
#include <array>

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

namespace {
// [FW] the block's words: the Start Offset (+0x10c), the loop record's and +0xe0 record's pointers, the level (+0x6c);
// types 0 and 2: the first sample (+0x14c), the frames (+0x150), type 0's backward mode (+0x148's low byte); type 1:
// +0x14c set for a range in samples, the range (+0x150, +0x154)
constexpr uint32_t kStartOffset = 0x10c, kLoopAt = 0x130, kPairAt = 0x134, kLevelFrom = 0x6c;
constexpr uint32_t kFirst = 0x14c, kFrames = 0x150, kBwdModeByte = 0x148, kInSamples = 0x14c, kFrom = 0x150;
constexpr uint32_t kTo = 0x154;
// [FW] the object's words: the pitch, formant, level, walk, gain and level out; the +0xe0 record's copy; a half word
// cleared; the position ×16 (+0x38); type 1's byte 2 and its range as half words or words
constexpr uint32_t kPitch = 0x1cc, kFormant = 0x1d0, kLevel = 0x1d4, kWalk = 0x1d8, kGain = 0x1dc, kLevelOut = 0x1e0;
constexpr uint32_t kPairCopy = 0x1c8, kHalf = 0x168, kPosition16 = 0x194, kByte2 = 0x15e, kCopy16 = 0x170;
constexpr uint32_t kCopy32 = 0x174;
// [FW] type 2's words, by rom.2's XtLitePlayer (its P + 4): the loops' modes, the Start Offset, the heads, the
// filter's rate and state, the heads' fractions, the walk's position, the windows' phases and clocks, the last
// sample, the forward loop (start, end, span) and its rate, the first sample, the backward loop and its rate, the
// period and its inverse
constexpr uint32_t kFwdMode = 0x248, kBwdMode = 0x24c, kOffset = 0x250, kHeadA = 0x254, kHeadB = 0x258;
constexpr uint32_t kFilterRate = 0x25c, kFilterL = 0x260, kFilterR = 0x264, kFracA = 0x268, kFracB = 0x26c;
constexpr uint32_t kWalkPos = 0x270, kWinA = 0x274, kClkA = 0x278, kWinB = 0x27c, kClkB = 0x280, kLast = 0x284;
constexpr uint32_t kFwdLoop = 0x288, kFwdRate = 0x294, kFirstF = 0x298, kBwdLoop = 0x29c, kBwdRate = 0x2a8;
constexpr uint32_t kPeriod = 0x2ac, kInvPeriod = 0x2b0;
constexpr uint32_t kReset = 0x893cbf40;   // [FW] the players' routine on the object before the seek
// [FW] the object's byte 0: 0x10 cleared, 0x40 the flag's (+0xa40) bit 0, and three bits each a block word's bit 0;
// 0x01 then takes the loop record's modes, and type 1's 0x80 says its range is in samples
constexpr uint8_t kCleared = 0x10, kFlagBit = 0x40, kLoopModes = 0x01, kSamplesBit = 0x80;
struct Vc2OscLiteBit {
    uint8_t bit;
    uint32_t from;
};
constexpr std::array<Vc2OscLiteBit, 3> kBits { { { 0x04, 0x140 }, { 0x01, 0x138 }, { 0x02, 0x13c } } };

uint8_t withBit(uint8_t b, uint8_t bit, uint32_t word) { return (word & 1) != 0 ? b | bit : b & (uint8_t)~bit; }

// [FW] FLOAT on the word as signed, then 2^32 added when it reads negative: the word unsigned
float unsignedFloat(uint32_t x) {
    const float f = xtf::flt((int32_t)x);
    return (int32_t)x < 0 ? add(f, vc2OscTables().liteTwo32) : f;
}

float fraction(float x) { return sub(x, xtf::flt(xtf::ftrc(x))); }   // [FW] FTRC, FLOAT, taken off

// [FW] a loop's three words as floats from `at`: the forward's start, end and span, the backward's end, start and
// span, each span the end less the start as a word
void floatLoop(Vc2OscVoice& v, uint32_t at, int32_t first, int32_t second, int32_t span) {
    v.setF32(at, xtf::flt(first));
    v.setF32(at + 4, xtf::flt(second));
    v.setF32(at + 8, xtf::flt(span));
}

int32_t spanOf(int32_t end, int32_t start) { return (int32_t)((uint32_t)end - (uint32_t)start); }

// [FW] 0x893c7d96: type 0, Vari off's words as VariPhrase's note-on writes them, on LITE's own pool words, but the
// backward mode +0x148's low byte; the position ×16
void variOff(Vc2OscVoice& v, Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscFrames frames { v.u32(kFirst), v.u32(kFrames) - 1 };
    Vc2OscVariOff w = vc2OscVariOffOf(v, parts, shared, vc2OscTables().liteTenths, frames);
    w.bwdMode = v.u8(kBwdModeByte);
    vc2OscSetVariOff(v, w);
    v.setU32(kPosition16, vc2OscVariOffStart(v) << 4);
}

// [FW] 0x893c7ec0: type 1, the mode-1 player: its byte 2 cleared (& 7, then 0xfb, 0xfd and 0xfe), the range as words
// with the object's 0x80 set, or as half words with it clear; the block's level
void modeOne(Vc2OscVoice& v, Vc2OscParts&, const Vc2ToneShared&) {
    v.setU8(kByte2, 0);
    const bool inSamples = v.u32(kInSamples) != 0;
    v.setU8(Vc2OscVoice::kObject, withBit(v.u8(Vc2OscVoice::kObject), kSamplesBit, inSamples ? 1 : 0));
    if (inSamples) {
        v.setU32(kCopy32, v.u32(kFrom));
        v.setU32(kCopy32 + 4, v.u32(kTo));
    } else {
        v.setU16(kCopy16, v.u16(kFrom));
        v.setU16(kCopy16 + 2, v.u16(kTo));
    }
    v.setU32(kLevel, v.u32(kLevelFrom));
}

// [FW] 0x893c80d0…: where the walk starts, rising from the first sample, else from the last whole half period at or
// below the last sample
void startWalk(Vc2OscVoice& v) {
    if (v.f32(kWalk) > 0.0f) {
        v.setF32(kWalkPos, unsignedFloat(v.u32(kFirst)));
        v.setU32(kHeadA, v.u32(kFirst));
        v.setU32(kHeadB, v.u32(kFirst));
    } else {
        const float half = div(v.f32(kPeriod), 2.0f);   // [FW] FLDI1, FADD: 2
        const float at = mul(xtf::flt(xtf::ftrc(div(unsignedFloat(v.u32(kFrames) - 1), half))), half);
        v.setF32(kWalkPos, at);
        v.setU32(kHeadA, (uint32_t)xtf::ftrc(at));
        v.setU32(kHeadB, (uint32_t)xtf::ftrc(v.f32(kWalkPos)));
    }
    v.setU32(kPosition16, (uint32_t)xtf::ftrc(v.f32(kWalkPos)) << 4);
}

// [FW] 0x893c7f70: type 2, LITE's two heads: the period 88200 over the root's Hz, the windows half a period apart,
// the heads' fractions the formant's, the filter's rate the root × the pitch × its pool word, the loops as floats
void twoHeads(Vc2OscVoice& v, Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscTables& t = vc2OscTables();
    const Vc2OscLoopRecord loop = parts.loopRecord(v.u32(kLoopAt));
    v.setU32(kOffset, v.u32(kStartOffset));
    v.setF32(kFirstF, unsignedFloat(v.u32(kFirst)));
    v.setF32(kLast, unsignedFloat(v.u32(kFrames) - 1));
    v.setF32(kFwdRate, vc2OscLoopRate(loop.rateFwd, shared, t.liteHeadsTenths));
    v.setF32(kBwdRate, vc2OscLoopRate(loop.rateBwd, shared, t.liteHeadsTenths));
    const float period = div(t.litePeriod, v.tail.hz);
    v.setF32(kPeriod, period);
    v.setF32(kInvPeriod, div(1.0f, period));
    v.setF32(kFracA, fraction(v.f32(kFormant)));
    v.setF32(kFracB, fraction(v.f32(kFormant)));
    for (uint32_t at : { kWinA, kClkA, kFilterL, kFilterR }) v.setF32(at, 0.0f);
    v.setF32(kWinB, mul(v.f32(kPeriod), t.liteHalf));
    v.setF32(kClkB, mul(v.f32(kPeriod), t.liteHalf));
    v.setF32(kFilterRate, mul(mul(v.tail.hz, v.f32(kPitch)), t.liteFilter));
    floatLoop(v, kFwdLoop, loop.loopStart, loop.loopEnd, spanOf(loop.loopEnd, loop.loopStart));
    floatLoop(v, kBwdLoop, loop.bwdEnd, loop.bwdStart, spanOf(loop.bwdEnd, loop.bwdStart));
    startWalk(v);
    const bool modes = (v.u8(Vc2OscVoice::kObject) & kLoopModes) != 0;
    v.setU32(kFwdMode, modes ? (uint32_t)loop.fwdMode : 0);
    v.setU32(kBwdMode, modes ? (uint32_t)loop.bwdMode : 0);
}

// [FW] each wave type's way (the switches at 0x893c7d90 and 0x893c8200; a type past 2 takes none): its words, then
// its seek (the re-seek's for kind 1), given ×16 the block's word `to` when it isn't the word `from`
struct Vc2OscLiteWay {
    void (*words)(Vc2OscVoice&, Vc2OscParts&, const Vc2ToneShared&);
    uint32_t seek, to, from;
};
constexpr std::array<Vc2OscLiteWay, 3> kWays { { { variOff, 0x893c6360, 0x154, 0x14c },
                                                  { modeOne, 0x893cc830, 0x158, 0x150 },
                                                  { twoHeads, 0x893c6de0, 0x154, 0x14c } } };
} // namespace

// [FW] 0x893c7c30
void Vc2OscVoice::noteOnLite(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    vc2OscPlayerHead(*this, parts, vc2OscTables().liteTimes);
    const Vc2OscTargets targets(*this, parts, shared);
    setF32(kPitch, targets.tunedRatio());
    setF32(kWalk, targets.walkTarget());
    setF32(kFormant, targets.formantTarget());
    setU16(kHalf, 0);
    setU32(kPairCopy, u32(kPairAt));
    setF32(kGain, targets.gain());
    setF32(kLevelOut, targets.level());
    uint8_t b = withBit(u8(kObject) & (uint8_t)~kCleared, kFlagBit, tail.flag);
    for (const auto& [bit, from] : kBits) b = withBit(b, bit, u32(from));
    setU8(kObject, b);
    const uint32_t type = u32(kWave);
    if (type < kWays.size()) kWays[type].words(*this, parts, shared);
    parts.objectCall(kReset);
    if (type >= kWays.size()) return;
    const Vc2OscLiteWay& way = kWays[type];
    if (u32(way.to) != u32(way.from)) parts.seek(way.seek, (int32_t)(u32(way.to) << 4));
}
