#pragma once
// What SOLO's files share (v334): rom.4's SOLO (VariPhrase types 1 and 6) on the object's words, rom.2's 0x8950cd90
// chain (XtSoloVoice's) with its record block four bytes up (rom.4's +0xec, rom.2's +0xe8) and its grains' words laid
// out otherwise again past them (rom.4's +0x128).  The render and the frame's step in Vc2OscSolo.cpp, the frame's
// reads in Vc2OscSoloFrame.cpp, the records, the grain parameters and the jumps in Vc2OscSoloRecords.cpp, the
// set-ups and the seek in Vc2OscSoloSetUp.cpp.  Every address is rom.4's, every offset the object's (the voice's
// +0x15c).  HARDWARE_FIRMWARE.md, "The VC-2's SOLO".  Audio thread.
#include "Vc2OscWalkSteps.h"

namespace vc2solo {
// [FW] the object's own: the anchor's step (×16, the grain parameters'), the pitch, the formant, the Energy, the gain's
// target, the gain and its rate (the set-ups' 1/64), Robot Voice's depth (rom.4's word at +0x94), the rate the grains
// and the filter take (the block's), the rate factor (the loops')
// one-off: rom.4's object's offsets, a declaration of its layout, not a job to template
inline constexpr uint32_t kAnchorStep = 0x30, kPitch = 0x70, kFormant = 0x74, kEnergy = 0x78, kTarget = 0x80;
// one-off: the same layout's next words
inline constexpr uint32_t kGain = 0x84, kGainRate = 0x88, kRobotDepth = 0x94, kRateNow = 0x9c;
// [FW] the record block (+0xec): the samples' address, the grain toggle (a byte), the repeat budget and the record's
// info (half words), the segment's start and the next's, the entry after the next, the next segment's first record,
// the next record, the record's start and end, the anchor, the record's length and the next's (all ×16)
// one-off: the record block's offsets, a declaration of its layout
inline constexpr uint32_t kSamples = 0xec, kToggle = 0xf0, kBudget = 0xf4, kInfo = 0xf6, kSegment = 0xf8;
// one-off: the same layout's next words
inline constexpr uint32_t kNextSegment = 0xfc, kEntryAfter = 0x100, kNextFirst = 0x104, kNextRecord = 0x108;
// one-off: the same layout's next words
inline constexpr uint32_t kRecStart = 0x10c, kRecEnd = 0x110, kAnchor = 0x114, kRecLength = 0x118, kNextLength = 0x11c;
// [FW] the grains (+0x128): the trigger's period and its timer (1/4096 sample), the grain's start offset, the last
// sample a read takes, the formant weight a grain takes, the second tap's weight, the windows' step, the Energy
// filter's cutoff, its coefficient and Q, each channel's two states, the rate as the parameters took it, Robot
// Voice's smoothed period
// one-off: the grains' offsets, a declaration of their layout
inline constexpr uint32_t kTrigger = 0x128, kOffset = 0x12c, kTimer = 0x130, kLastSample = 0x144, kWeight = 0x148;
// one-off: the same layout's next words
inline constexpr uint32_t kTap = 0x154, kWindowStep = 0x160, kCutoff = 0x16c, kCoef = 0x170, kQ = 0x174;
// one-off: the same layout's next words
inline constexpr uint32_t kBandLeft = 0x178, kLowLeft = 0x17c, kBandRight = 0x180, kLowRight = 0x184;
inline constexpr uint32_t kRateTaken = 0x188, kSmooth = 0x18c;
// [FW] byte 0's Robot Voice; byte 3's 0x08 (the backward loop turned it) and 0x80 (type 6's); the record's info: 0x04
// voiced, 0x01 and 0x02 an unvoiced record's pitch and formant kept; the budget's bits (<< 6), all of them unlimited
inline constexpr uint8_t kRobot = 0x10, kFromZero = 0x08, kTypeSix = 0x80;
// one-off: the info word's bits, a declaration
inline constexpr int16_t kVoiced = 0x04, kKeepsPitch = 0x01, kKeepsFormant = 0x02, kBudgetBits = 0x1f8;
inline constexpr int16_t kUnlimited = 0x7e00;
inline constexpr int kBudgetShift = 6;
inline constexpr int32_t kAgeStep = 0x1000;   // [FW] a sample's age, 1/4096

// A grain's words: its age (1/4096 sample), its window's place, its start (×16), its formant weight, its second tap's
// weight, its period (×16): grain A (the toggle 0) and grain B
struct Vc2OscSoloGrain {
    uint32_t age, window, start, weight, tap, period;
};
inline constexpr Vc2OscSoloGrain kGrainA { 0x134, 0x164, 0x13c, 0x14c, 0x158, 0x120 };
inline constexpr Vc2OscSoloGrain kGrainB { 0x138, 0x168, 0x140, 0x150, 0x15c, 0x124 };

// What the grains give a frame's reads: each grain's sample and its fraction, and its window's gain
struct Vc2OscSoloTaps {
    int32_t a = 0, b = 0;
    float fa = 0, fb = 0, wa = 0, wb = 0;
};

// one-off: the routines' declarations, alike only in taking the words
int32_t loadNext(Vc2WalkWords& w);                    // 0x893c9b80: the budget
int32_t loadPrev(Vc2WalkWords& w);                    // 0x893c9bf0: the budget
int32_t segmentEntry(Vc2WalkWords& w, uint32_t at);   // 0x893c9e50: the segment's start
void grainParams(Vc2WalkWords& w);                    // 0x893c9c60
int32_t repeatCost(const Vc2WalkWords& w);            // 0x893c9dd0
Vc2OscSoloTaps grains(Vc2WalkWords& w);               // 0x893c9a40 (Vc2OscSoloFrame.cpp)
void clampTaps(const Vc2WalkWords& w, Vc2OscSoloTaps& t);   // 0x893c9ae0 (the same)
void frame(Vc2WalkWords& w, const Vc2OscSamples& wave, float* lr);   // 0x893c97b0… (the same)
} // namespace vc2solo
