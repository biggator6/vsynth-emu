#pragma once
// XtSynth's constants, shared by its parts: the TVA's as rom.2 stores them, the truncate fade's step and the clock's per
// block (moved from XtSynth.cpp as they stood, v200).
#include "XtFloat.h"
#include <cstdint>

namespace xtsynth {
// The TVA's constants as rom.2 stores them (each one below the nearest float)
constexpr float kInvMs = 0x1.7384eep-6f;      // 1/44.1
constexpr float kFramesPerMs = 0x1.60cccap+5f; // 44.1
constexpr float kSilent = 0x1.0000fep-16f;     // 2^-16
constexpr float kLand = 0x1.f81f8p-6f;         // 1/32.5
constexpr float kNoAttackMs = 0x1.99999ap-4f;  // 0.1
constexpr float kSendGlide = 0x1.eb851ep-11f;  // 0.0009375: the output bus's level, a share of the gap per sample
// The truncate fade's step per sample (0x894ea380): 1/(T/100·44.1), T the
// Truncate Time (system 0x51f, 1/100 ms; its default 70), in the SH-4's
// arithmetic.
inline float truncateStep() {
    static const float s = xtf::div(1.0f, xtf::mul(xtf::div(xtf::flt(70), 100.0f), kFramesPerMs));
    return s;
}
constexpr uint64_t kClockPerBlock = 32 * 24000;   // a block's frames × 24 clocks a quarter × 1000 ms; a clock is clockPeriod() × 44100
// The XT's control clock: the tone update (envelope step, TVA targets)
// runs between blocks once per 441 frames (10 ms).  (From XtSynth.h, v200.)
constexpr int kTickFrames = 441;
}  // namespace xtsynth
