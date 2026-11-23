#pragma once
// What the period-jump player's files share (v335): rom.4's player of VariPhrase's type 8 on the object's words, rom.2's
// 0x8950e1d0 four bytes up from +0x94: one walk through the events, and a read that keeps within a period of it,
// jumping a period at a time, its two taps a period apart crossfaded by how far behind the walk the read is.  The
// render in Vc2OscJump.cpp, the records, jumps, set-up and seek in Vc2OscJumpRecords.cpp.  Every address is rom.4's,
// every offset the object's (the voice's +0x15c).  HARDWARE_FIRMWARE.md, "The VC-2's period-jump player".  Audio thread.
#include "Vc2OscWalkSteps.h"

namespace vc2jump {
// [FW] the object's own: the read's rate (×2^25 a sample, the block's), the pitch, the gain's target, the gain and its
// rate (the set-up's 1/64)
inline constexpr uint32_t kReadRate = 0x30, kPitch = 0x70, kTarget = 0x80, kGain = 0x84, kGainRate = 0x88;
// [FW] the record block (+0xec): the samples' address, the repeat budget and the record's info (half words), the
// segment's start and the next's, the entry after the next, the next segment's first record, the next record, the
// record's start and end (×16)
// one-off: the record block's offsets, a declaration of its layout
inline constexpr uint32_t kSamples = 0xec, kBudget = 0xf0, kInfo = 0xf2, kSegment = 0xf4, kNextSegment = 0xf8;
// one-off: the same layout's next words
inline constexpr uint32_t kEntryAfter = 0xfc, kNextFirst = 0x100, kNextRecord = 0x104, kRecStart = 0x108;
// [FW] then the read's: the record's end, the period (×16, the glide's), the read (×16) and its fraction (20 bits),
// the beat's step as the block took it and the walk's step, the record's length's inverse, its length and the next's
// (floats, samples ×16)
// one-off: the same layout's next words
inline constexpr uint32_t kRecEnd = 0x10c, kPeriod = 0x110, kRead = 0x114, kReadFrac = 0x118, kBeatStep = 0x11c;
inline constexpr uint32_t kWalkStep = 0x120, kInverse = 0x124, kLength = 0x128, kNextLength = 0x12c;
} // namespace vc2jump
