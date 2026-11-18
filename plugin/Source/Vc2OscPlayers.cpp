// The OSC voice's players' installs and VariPhrase's note-on (v238): rom.4's OSC code beside the voice, LITE's install
// 0x893af7a0, VariPhrase's 0x893af870 and VariPhrase's note-on 0x893af420, which the frame (v230) reached by address;
// and what VariPhrase's note-on shares with LITE's (v240, Vc2OscPlayers.h; LITE's in Vc2OscLite.cpp).  The players' own
// routines (their renders, setups, seeks, updates, the event lookup) stay rom.4's by address through Vc2OscParts.
// Every address is rom.4's; every offset the voice's.  HARDWARE_FIRMWARE.md, "The VC-2's players' installs and
// VariPhrase's note-on".
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
// [FW] the block's flags byte (+0x138, the block's +0xe4; rom.2's +0xb4): 0x80 the range in samples, else in events;
// 0x20 Vari off.  The object (+0x15c) takes it as its byte 0: 0x01 takes the loop record's modes, 0x08 one-way loops
// over events
constexpr uint32_t kFlagsByte = 0x138, kCopiedFlags = 3;
constexpr uint8_t kInSamples = 0x80, kVariOff = 0x20, kLoopModes = 0x01, kEventLoops = 0x08;

// [FW] LITE's renders by the wave's type, without the flag (+0xa40) and with it: 0 Vari off's, 1 kind 1's mode 1
// (which then sets up the object, 0x893cc760), 2 LITE's two heads
struct LiteRenders {
    uint32_t mono, stereo;
};
constexpr std::array<LiteRenders, 3> kLite { { { 0x893c63c0, 0x893c66e0 }, { 0x893c9f60, 0x893c9f60 },
                                               { 0x893c6e80, 0x893c6e8c } } };
constexpr uint32_t kModeOne = 1, kModeOneSetup = 0x893cc760, kLiteStereo = 3;

// [FW] VariPhrase's by the wave's type 1…8: its render (0 none), its constant's place in its table (5 more with the
// flag), and whether the block's Vari off turns it into Vari off's (LITE's type 0)
struct Vc2OscVariRow {
    uint32_t render;
    int at;
    bool variOff;
};
constexpr std::array<Vc2OscVariRow, 8> kVari { { { 0x893c9450, 0, true }, { 0, 0, false }, { 0x893cadc0, 3, false },
                                                  { 0, 0, false }, { 0x893ca1f0, 4, false }, { 0x893c9450, 1, true },
                                                  { 0, 0, false }, { 0x893ca8b0, 2, false } } };
constexpr int kVariStereo = 5;
// [FW] the wave types whose note-on goes on to Vari off's words (the switch at 0x893af548: 1, 6, 7, 8)
constexpr std::array<bool, 8> kVariOffTypes { true, false, false, false, false, true, true, true };

// [FW] VariPhrase's note-on's words: the range's two ends and where to seek (the block's +0xe8, +0xec, +0xf0), their
// copies (×16 in samples, as they stand in events), the block's +0x60 (× 10000 into +0x1a8) and +0xb8 (the samples'
// address), the header's, loop record's and +0xe0 record's pointers; the object's player words
constexpr uint32_t kFrom = 0x13c, kTo = 0x140, kSeekTo = 0x144, kCopy16 = 0x170, kCopy32 = 0x174;
constexpr uint32_t kSelf = 0x1a0, kSelfAt = 0x108, kHeaderWord = 0x1a4, kTimes = 0x1a8, kTimesFrom = 0xb4;
constexpr uint32_t kHeaderAt = 0x128, kLoopAt = 0x130, kPairAt = 0x134, kPairWord = 0x2d0, kSamplesFrom = 0x10c;
constexpr uint32_t kPitch = 0x1cc, kFormant = 0x1d0, kLevel = 0x1d4, kLevelFrom = 0x6c, kWalk = 0x1d8, kZero = 0x1f0;
constexpr uint32_t kUpdate = 0x893c88b0, kAfterUpdate = 0x893c8670, kSeek = 0x893c6360;
constexpr uint32_t kZeroAt = 0x248, kPosition = 0x24c, kSamples = 0x250, kEnd = 0x254, kLoop = 0x258, kRateFwd = 0x264;
constexpr uint32_t kFwdMode = 0x268, kStart = 0x26c, kBwd = 0x270, kRateBwd = 0x27c, kBwdMode = 0x280, kOne = 0x284;
constexpr uint32_t kPositionCopy = 0x288;
constexpr float kTwo31 = 2147483648.0f;   // [FW] 0x893af4ae…: mov #79, shll8, shll16, as a float

// [FW] a loop's three words from `at`: the forward's start, end and span, the backward's end, start and span
void setLoop(Vc2OscVoice& v, uint32_t at, uint32_t first, uint32_t second, uint32_t span) {
    v.setU32(at, first);
    v.setU32(at + 4, second);
    v.setU32(at + 8, span);
}

// [FW] 0x893af590…: the range by events, each event's position (0x893cc0d0) in samples, the end's less 1; the loops'
// modes one-way by the object's 0x08, their rates 1; a seek to the third event's position when it isn't the first
void byEvents(Vc2OscVoice& v, Vc2OscParts& parts) {
    const auto a = (uint32_t)(parts.eventPosition(v.u16(kFrom)) >> 4);
    const auto b = (uint32_t)((parts.eventPosition(v.u16(kTo)) >> 4) - 1);
    const uint32_t mode = (v.u8(Vc2OscVoice::kObject) & kEventLoops) != 0 ? 1 : 0;
    vc2OscSetVariOff(v, { .start = a, .end = b, .loopStart = a, .loopEnd = b, .bwdEnd = b, .bwdStart = a,
                          .fwdMode = mode, .bwdMode = mode, .fwdRate = 1.0f, .bwdRate = 1.0f });
    if (v.u16(kSeekTo) != v.u16(kFrom)) parts.seek(kSeek, parts.eventPosition(v.u16(kSeekTo)));
}

// [FW] 0x893af660…: the range in samples, the end's less 1, with the loop record's words; the backward's rate factor
// times the +0xe0 record's +0x2d0; a seek to the third word when it isn't the first
void bySamples(Vc2OscVoice& v, Vc2OscParts& parts, const Vc2ToneShared& shared) {
    Vc2OscVariOff w = vc2OscVariOffOf(v, parts, shared, vc2OscTables().rateTenths, { v.u32(kFrom), v.u32(kTo) - 1 });
    w.bwdRate = mul(w.bwdRate, parts.boundFloat(v.u32(kPairAt), kPairWord));
    vc2OscSetVariOff(v, w);
    if (v.u32(kSeekTo) != v.u32(kFrom)) parts.seek(kSeek, (int32_t)v.u32(kSeekTo));
}
} // namespace

void Vc2OscVoice::installLite(Vc2OscParts& parts) {
    const uint32_t type = u32(kWave);
    if (type >= kLite.size()) return;
    tail.render = tail.flag != 0 ? kLite[type].stereo : kLite[type].mono;
    if (type == kModeOne) parts.objectCall(kModeOneSetup);
    tail.installed = vc2OscTables().liteInstalled[type + (tail.flag != 0 ? kLiteStereo : 0)];
}

void Vc2OscVoice::installVari() {
    const uint32_t at = u32(kWave) - 1;   // unsigned: type 0 and past 8 install nothing
    if (at >= kVari.size() || kVari[at].render == 0) return;
    const Vc2OscVariRow& row = kVari[at];
    const Vc2OscTables& t = vc2OscTables();
    const bool stereo = tail.flag != 0;
    if (row.variOff && (u8(kFlagsByte) & kVariOff) != 0) {
        tail.render = stereo ? kLite[0].stereo : kLite[0].mono;
        tail.installed = t.liteInstalled[stereo ? kLiteStereo : 0];
        return;
    }
    tail.render = row.render;
    tail.installed = t.variInstalled[(size_t)(row.at + (stereo ? kVariStereo : 0))];
}

// [FW] 0x893af420: the block's flags into the object, the range's copies, the voice's own address at +0x1a0, the
// targets' ratio, formant and walk into the object, kind 2's update and the routine after it; then, for Vari off on
// types 1, 6, 7 and 8, the player's words by events or samples, the Start Offset, and the position the walk's sign
// starts it from
void Vc2OscVoice::noteOnVari(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    for (uint32_t k = 0; k < kCopiedFlags; ++k) setU8(kObject + k, u8(kFlagsByte + k));
    const bool inSamples = (u8(kFlagsByte) & kInSamples) != 0;
    if (inSamples) {
        setU32(kCopy32, u32(kFrom) << 4);
        setU32(kCopy32 + 4, u32(kTo) << 4);
    } else {
        setU16(kCopy16, u16(kFrom));
        setU16(kCopy16 + 2, u16(kTo));
    }
    vc2OscPlayerHead(*this, parts, vc2OscTables().timesTenThousand);
    setF32(kPitch, Vc2OscTargets(*this, parts, shared).tunedRatio());
    setF32(kFormant, Vc2OscTargets(*this, parts, shared).formant());
    setU32(kLevel, u32(kLevelFrom));
    setF32(kZero, 0.0f);
    setF32(kWalk, Vc2OscTargets(*this, parts, shared).walk());
    parts.kindCall(kUpdate);
    parts.kindCall(kAfterUpdate);
    const uint32_t type = u32(kWave) - 1;   // unsigned: type 0 and past 8 go no further
    if (type >= kVariOffTypes.size() || !kVariOffTypes[type] || (u8(kFlagsByte) & kVariOff) == 0) return;
    if (inSamples) bySamples(*this, parts, shared);
    else byEvents(*this, parts);
    vc2OscVariOffStart(*this);
}

void vc2OscPlayerHead(Vc2OscVoice& v, Vc2OscParts& parts, float tenThousand) {
    v.setU32(kSelf, parts.address() + kSelfAt);
    float times = mul(v.f32(kTimesFrom), tenThousand);
    if (!(kTwo31 > times)) times = sub(times, add(kTwo31, kTwo31));
    v.setU32(kTimes, (uint32_t)xtf::ftrc(times));
    v.setU32(kHeaderWord, parts.waveHeader(v.u32(kHeaderAt)).word1c);
}

float vc2OscLoopRate(int16_t rate, const Vc2ToneShared& shared, float tenths) {
    return shared.exp2(div(xtf::flt(rate), tenths));
}

Vc2OscVariOff vc2OscVariOffOf(const Vc2OscVoice& v, Vc2OscParts& parts, const Vc2ToneShared& shared, float tenths,
                              Vc2OscFrames range) {
    const Vc2OscLoopRecord loop = parts.loopRecord(v.u32(kLoopAt));
    const bool modes = (v.u8(Vc2OscVoice::kObject) & kLoopModes) != 0;
    return { .start = range.first, .end = range.last, .loopStart = (uint32_t)loop.loopStart,
             .loopEnd = (uint32_t)loop.loopEnd, .bwdEnd = (uint32_t)loop.bwdEnd, .bwdStart = (uint32_t)loop.bwdStart,
             .fwdMode = modes ? (uint32_t)loop.fwdMode : 0, .bwdMode = modes ? (uint32_t)loop.bwdMode : 0,
             .fwdRate = vc2OscLoopRate(loop.rateFwd, shared, tenths),
             .bwdRate = vc2OscLoopRate(loop.rateBwd, shared, tenths) };
}

void vc2OscSetVariOff(Vc2OscVoice& v, const Vc2OscVariOff& w) {
    v.setU32(kStart, w.start);
    v.setU32(kEnd, w.end);
    setLoop(v, kLoop, w.loopStart, w.loopEnd, w.loopEnd - w.loopStart);
    setLoop(v, kBwd, w.bwdEnd, w.bwdStart, w.bwdEnd - w.bwdStart);
    v.setU32(kFwdMode, w.fwdMode);
    v.setU32(kBwdMode, w.bwdMode);
    v.setF32(kRateFwd, w.fwdRate);
    v.setF32(kRateBwd, w.bwdRate);
}

uint32_t vc2OscVariOffStart(Vc2OscVoice& v) {
    v.setU32(kSamples, v.u32(kSamplesFrom));
    v.setF32(kZeroAt, 0.0f);
    v.setF32(kOne, 1.0f);
    v.setU32(kPosition, 0.0f > v.f32(kWalk) ? v.u32(kEnd) : v.u32(kStart));
    v.setU32(kPositionCopy, v.u32(kPosition));
    return v.u32(kPosition);
}
