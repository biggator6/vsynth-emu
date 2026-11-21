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
// [FW] the block's words: the samples' address (+0x10c), the loop record's and +0xe0 record's pointers, the level (+0x6c);
// types 0 and 2: the first sample (+0x14c), the frames (+0x150), type 0's backward mode (+0x148's low byte); type 1:
// +0x14c set for a range in samples, the range (+0x150, +0x154)
constexpr uint32_t kSamplesFrom = 0x10c, kLoopAt = 0x130, kPairAt = 0x134, kLevelFrom = 0x6c;
constexpr uint32_t kFirst = 0x14c, kFrames = 0x150, kBwdModeByte = 0x148, kInSamples = 0x14c, kFrom = 0x150;
constexpr uint32_t kTo = 0x154;
// [FW] the object's words: the pitch, formant, level, walk, gain and level out; the +0xe0 record's copy; a half word
// cleared; the position ×16 (+0x38); type 1's byte 2 and its range as half words or words
constexpr uint32_t kPitch = 0x1cc, kFormant = 0x1d0, kLevel = 0x1d4, kWalk = 0x1d8, kGain = 0x1dc, kLevelOut = 0x1e0;
constexpr uint32_t kPairCopy = 0x1c8, kHalf = 0x168, kPosition16 = 0x194, kByte2 = 0x15e, kCopy16 = 0x170;
constexpr uint32_t kCopy32 = 0x174;
// [FW] the targets into the object, in rom.4's order: the tuned ratio (0x893afb80), the walk's (0x893afc30) and the
// formant's (0x893afca0) targets, the gain (0x893aff30) and the level (0x893aff60)
constexpr std::array<Vc2OscTargets::Word, 5> kTargets { { { kPitch, &Vc2OscTargets::tunedRatio },
                                                         { kWalk, &Vc2OscTargets::walkTarget },
                                                         { kFormant, &Vc2OscTargets::formantTarget },
                                                         { kGain, &Vc2OscTargets::gain },
                                                         { kLevelOut, &Vc2OscTargets::level } } };
// [FW] type 2's words, by rom.2's XtLitePlayer (its P + 4): the loops' modes, the samples' address, the heads, the
// filter's rate and state, the heads' fractions, the walk's position, the windows' phases and clocks, the last
// sample, the forward loop (start, end, span) and its rate, the first sample, the backward loop and its rate, the
// period and its inverse
constexpr uint32_t kFwdMode = 0x248, kBwdMode = 0x24c, kSamples = 0x250, kHeadA = 0x254, kHeadB = 0x258;
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

// [FW] FLOAT on the word as signed, then `two32`, a routine's own pool word 2^32, added when it reads negative: the word
// unsigned; the note-on's by its copy
float unsignedFloatBy(uint32_t x, float two32) {
    const float f = xtf::flt((int32_t)x);
    return (int32_t)x < 0 ? add(f, two32) : f;
}
float unsignedFloat(uint32_t x) { return unsignedFloatBy(x, vc2OscTables().liteTwo32); }

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

// [FW] the two heads' range and loops from the loop record: the first and last sample as floats by the routine's own
// 2^32, the loops as floats, their modes by the object's 0x01; the note-on's (0x893c7f70) and from v324 the settings'
// (0x893c84aa)
void headsLoops(Vc2OscVoice& v, const Vc2OscLoopRecord& loop, float two32) {
    v.setF32(kFirstF, unsignedFloatBy(v.u32(kFirst), two32));
    v.setF32(kLast, unsignedFloatBy(v.u32(kFrames) - 1, two32));
    floatLoop(v, kFwdLoop, loop.loopStart, loop.loopEnd, spanOf(loop.loopEnd, loop.loopStart));
    floatLoop(v, kBwdLoop, loop.bwdEnd, loop.bwdStart, spanOf(loop.bwdEnd, loop.bwdStart));
    const bool modes = (v.u8(Vc2OscVoice::kObject) & kLoopModes) != 0;
    v.setU32(kFwdMode, modes ? (uint32_t)loop.fwdMode : 0);
    v.setU32(kBwdMode, modes ? (uint32_t)loop.bwdMode : 0);
}

// [FW] 0x893c7f70: type 2, LITE's two heads: the period 88200 over the root's Hz, the windows half a period apart,
// the heads' fractions the formant's, the filter's rate the root × the pitch × its pool word, the range and loops
void twoHeads(Vc2OscVoice& v, Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscTables& t = vc2OscTables();
    const Vc2OscLoopRecord loop = parts.loopRecord(v.u32(kLoopAt));
    v.setU32(kSamples, v.u32(kSamplesFrom));
    headsLoops(v, loop, t.liteTwo32);
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
    startWalk(v);
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
    Vc2OscTargets(*this, parts, shared).writeInto(*this, kTargets);
    setU16(kHalf, 0);
    setU32(kPairCopy, u32(kPairAt));
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

namespace {
// [FW] v324: 0x893c8280's targets for types 1 and 2, the walk and formant themselves (0x893afdf0, 0x893afeb0); type 0
// takes the note-on's first four, the walk's and formant's targets
constexpr std::array<Vc2OscTargets::Word, 4> kUpdateTargets { { { kPitch, &Vc2OscTargets::tunedRatio },
                                                               { kWalk, &Vc2OscTargets::walk },
                                                               { kFormant, &Vc2OscTargets::formant },
                                                               { kGain, &Vc2OscTargets::gain } } };
constexpr size_t kUpdated = 4;
// [FW] the settings' (v324): the count's and the matched count's bytes; the settings' word that, with the object's 0x04
// held and its +0x3c clear, asks a seek with the gain's target 0; the types it writes, Vari off's and the heads'
constexpr uint32_t kCountByte = 0x19c, kMatchedByte = 0x19d, kAskSeek = 0x40, kHoldFrom = 0x3c, kCountFrom = 0x38;
constexpr uint8_t kHoldBit = 0x04, kCountBit = 0x02;
constexpr uint32_t kVariOffType = 0, kHeadsType = 2;

uint32_t givenWord(const uint32_t* from, uint32_t at) { return from[at / 4]; }   // a settings word as it stands
} // namespace

// [FW] 0x893c8280: by the wave's type, a type past 2 doing nothing: the targets into the object; type 2 then the
// filter's rate, the root's Hz × the pitch × its own pool word
void Vc2OscVoice::updateLite(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const uint32_t type = u32(kWave);
    if (type >= kWays.size()) return;
    const Vc2OscTargets targets(*this, parts, shared);
    targets.writeInto(*this, type == 0 ? std::span(kTargets).first(kUpdated) : std::span(kUpdateTargets));
    if (type == kHeadsType) setF32(kFilterRate, mul(mul(tail.hz, f32(kPitch)), vc2OscTables().liteUpdateFilter));
}

// [FW] 0x893c8390 (the voice, the settings' +0xb0, whose words are the block's from +0xb0): with the settings' +0x40
// set, the object's 0x04 held and the +0x3c clear, a seek pending and the gain's target 0; with 0x04 clear and +0x3c
// set, the matched count's byte 1; with 0x02 clear and +0x38 set, the count's byte 0; then the object's three bits
// from the settings, as the note-on's from the block; then by the settings' type, through the loop record the note-on's
// head leaves (+0x1a0, the voice's own +0x108's address, at +0x28: the voice's +0x130): type 0 Vari off's range and the
// side the walk's sign plays, type 2 the heads' range and loops; type 1 and past 2 nothing
void Vc2OscVoice::settingsLite(const uint32_t* from, Vc2OscParts& parts) {
    const uint8_t flags = u8(kObject);
    const bool held = (flags & kHoldBit) != 0;
    if (givenWord(from, kAskSeek) != 0 && held && givenWord(from, kHoldFrom) == 0) {
        tail.seekPending = 1;
        setF32(kGain, 0.0f);
    }
    if (!held && givenWord(from, kHoldFrom) != 0) setU8(kMatchedByte, 1);
    if ((flags & kCountBit) == 0 && givenWord(from, kCountFrom) != 0) setU8(kCountByte, 0);
    uint8_t b = flags;
    for (const auto& [bit, at] : kBits) b = withBit(b, bit, givenWord(from, at - kWave));
    setU8(kObject, b);
    const Vc2OscLoopRecord loop = parts.loopRecord(u32(kLoopAt));
    const uint32_t type = from[0];
    if (type == kVariOffType) {
        const Vc2OscVariOff w = vc2OscVariOffLoops(*this, loop, { u32(kFirst), u32(kFrames) - 1 });
        vc2OscSetVariOffSide(*this, w, 0.0f > f32(kWalk));
    }
    if (type == kHeadsType) headsLoops(*this, loop, vc2OscTables().liteSettingsTwo32);
}
