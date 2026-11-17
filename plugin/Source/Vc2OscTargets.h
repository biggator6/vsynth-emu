#pragma once
// The OSC voice's targets (v232): rom.4's 0x893af9f0…0x893aff60, which the kinds' note-ons and updates read: each the
// voice's words with its parts' outputs (the bound envelopes' and LFOs', the vibrato's, the ramps').  0x893afa50 and
// 0x893afab0, two more copies of the cents, have no caller in rom.4 and aren't ported.  Bit for bit: xt_vc2osc, each
// against rom.4's own.  HARDWARE_FIRMWARE.md, "The VC-2's OSC voice's note-on and targets".  Audio thread.
#include "Vc2OscTables.h"
#include "Vc2OscVoice.h"
#include <span>

// rom.4's bounds: the upper first, then the lower, a NaN passing
float vc2OscBounded(float x, Vc2OscBounds b);
// The pitch's sum before its bounds: +0x5c, the first envelope × +0x70, the vibrato × +0xfc, ramp 0, the offset (the
// pitch getter's too)
float vc2OscPitchSum(const Vc2OscVoice& v, Vc2OscParts& p);

// The targets of one voice, on its parts and the wiring's shared math
class Vc2OscTargets {
public:
    Vc2OscTargets(const Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s) : v_(v), p_(p), s_(s) {}
    float cents() const;           // 0x893af9f0: within ±6000
    float ratio() const;           // 0x893afb40: 2^(cents/1200) × +0x60 × the rate's ratio, within ±32
    float tunedRatio() const;      // 0x893afb80, 0x893afd10: that × 2^(−detune/1200)
    float walkTarget() const;      // 0x893afc30: (+0x64 + LFO 2 × +0x98 + envelope 2 × +0x78) × ramp 2, within ±5
    float formantTarget() const;   // 0x893afca0: ramp 1 × 2^(+0x68 + LFO 3 × +0xa0 + envelope 3 × +0x80), [0.5, 2]
    float walk() const;            // 0x893afdf0: the walk's target × its factor (1 for some waves)
    float formant() const;         // 0x893afeb0: the formant's × its factor
    float gain() const;            // 0x893aff30: (1 + the vibrato's level × +0x100) × +0x88 × +0xb0 × ramp 0's factor
    float level() const;           // 0x893aff60: that with envelope 4's level for ramp 0's, within ±1, × +0xb0
    // The ratio on a pool's cents' bounds and 1/1200, held to `ratio`: 0x893afb40's, and the kinds' note-on's own
    float ratioOn(const Vc2OscTargetPool& pool, Vc2OscBounds ratio) const;

    // An envelope and an LFO bound to a target, each with the block's depth on it (the walk's, the formant's)
    struct Mod {
        size_t envelope;
        uint32_t envelopeDepth;
        size_t lfo;
        uint32_t lfoDepth;
    };

    // A target a note-on writes into one of the voice's words (v242: LITE's and the analog's)
    struct Word {
        uint32_t at;
        float (Vc2OscTargets::*target)() const;
    };
    void writeInto(Vc2OscVoice& v, std::span<const Word> words) const;   // each in order

private:
    float modulation(const Mod& m) const;   // the envelope × its depth + the LFO × its depth
    float levelOne() const;        // 1 + the vibrato's level × +0x100
    const Vc2OscVoice& v_;
    Vc2OscParts& p_;
    const Vc2ToneShared& s_;
};
