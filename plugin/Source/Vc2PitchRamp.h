#pragma once
// The VC-2's portamento (v236): rom.4's pitch ramp, 0x64 bytes, three in an OSC voice (+0x90c, +0x970, +0x9d4, built
// with types 0, 2 and 1: the pitch's, the formant's, the walk's).  Its words word for word; the constructor 0x893b0dc0
// on a ramp given, the start 0x893b0e10, the restart 0x893b0b10 (the settings' 0x893b0f20 is it), the legato update
// 0x893b0e30, the tick 0x893b0f30 (Vc2PitchRampTick.cpp) and the value 0x893b1160.  rom.4's own law, not rom.2's
// (VC2_VOCAL_DESIGNER.md, "The OSC block in rom.4"): under 100 ms the span is held to ±500 cents and type 0 eases in,
// and type 0 carries a gain factor (+0x54) over the glide's start.  Types 1 and 2 glide a ratio, in cents through logf
// and back through exp2, both the wiring's shared math.  Bit for bit: xt_vc2osc's ramp part against rom.4's routines.
// HARDWARE_FIRMWARE.md, "The VC-2's portamento".  Audio thread.
#include "Vc2VocoderOsc.h"
#include <cstdint>

struct Vc2RampTables;

// The six words a start copies (the voice start's argument, the tone's input + 0xc), at the ramp's +0xc
struct Vc2RampSettings {
    uint32_t on = 0;          // +0x0c: the portamento's switch, at 1
    uint32_t formantOn = 0;   // +0x10: type 2's own switch beside it (the formant's ramp)
    uint32_t walkOn = 0;      // +0x14: type 1's (the walk's)
    float time = 0;           // +0x18: the time, or RATE's time per 1200 cents of the span
    uint32_t timed = 0;       // +0x1c: 1 TIME, any other RATE
    uint32_t unread = 0;      // +0x20: copied, read by nothing here
};

// rom.4's words, its single-precision phases and steps among them ([FW]: the ramp's own floats, bit for bit)
struct Vc2RampWords {
    int32_t type = 0;          // +0x00: 0 glides cents and adds them; 1 and 2 glide a ratio
    uint32_t on = 0;           // +0x04: the switch the restart sets by type; off, the value is 0 (1 for a ratio)
    uint32_t rising = 0;       // +0x08: 1 unless from > to
    Vc2RampSettings settings;  // +0x0c…+0x23
    float from = 0, to = 0;    // +0x24, +0x28: in cents
    float fromGiven = 0, toGiven = 0;   // +0x2c, +0x30: as given (a ratio's before logf)
    int32_t state = 0;         // +0x34: 0 idle, 1 started, 2 gliding, 3 done
    float span = 0;            // +0x38: to − from, held to ±500 under 100 ms
    float value = 0;           // +0x3c: what the targets add (type 0) or multiply by
    float step = 0;            // [FW] +0x40: the phase's step a tick, 655360 / the time, at most 65536
    float phase = 0;           // [FW] +0x44: 0…65536
    float ease = 0;            // +0x48: type 0's ease: 1 under 50 ms, (100 − ms) × 0.02 up to 100, else 0
    float factorStep = 0;      // [FW] +0x4c: the step held to 5000…10000
    float factorPhase = 0;     // [FW] +0x50: 0…65536
    float factor = 0;          // +0x54: type 0's gain factor, which the gain target reads
    float factorDepth = 0;     // +0x58: 1 from a restart (0x893b11a0, which nothing calls, gives it)
    float factorRise = 0;      // +0x5c: 0.25, the factor's scale over the curve's second half
    float factorFall = 0;      // +0x60: 0.5, under its first half, negated
};
static_assert(sizeof(Vc2RampWords) == 0x64, "rom.4's layout");

class Vc2PitchRamp {
public:
    void construct(int32_t type);   // 0x893b0dc0 on a ramp given
    // 0x893b0e10: the six words at `settings`, then the restart
    void start(const uint32_t* settings, float from, float to, const Vc2ToneShared& shared);
    void restart(float from, float to, const Vc2ToneShared& shared);   // 0x893b0b10, the settings' 0x893b0f20
    void legato(const uint32_t* settings);                             // 0x893b0e30: four of them, the step again
    void tick(int32_t n, const Vc2ToneShared& shared);                 // 0x893b0f30 (Vc2PitchRampTick.cpp)
    float value() const;                                               // 0x893b1160
    float factor() const { return w.factor; }                          // +0x54, read in place

    Vc2RampWords w {};

private:
    void stepFactor(int32_t n, const Vc2RampTables& t);
    void glide(int32_t n, const Vc2ToneShared& shared, const Vc2RampTables& t);
};
