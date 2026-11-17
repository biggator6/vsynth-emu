#pragma once
// The VOCODER OSC's converter's own part and kind 5's update (v222): what the OSC's IDs become in P (the voice + 0x104,
// the OSC block + 0xb0), and what kind 5's update makes of P at each update before the carrier's (Vc2Carrier).  The
// converter 0x8939db00 (the VOCODER OSC's part of 0x8939fbd0) from its P stores: the levels at v/127; Growl Level with
// the Matrix's OSC Time sum and its keyfollow; the envelope depths through sign·(d/63)² (0x8939c0b0) × a velocity
// factor; the LFO depths with the Matrix's OSC LFO Time or Formant sum, held to ±63, through the same curve; the
// balance with the OSC Formant sum.  Its first half, the OSC block shared with the analog kind, comes with the voice.
// Kind 5's update 0x893afff0: the growl and the balance from four modulators, the gain's product, the pitch's cents
// through exp2, then the carrier's update.  Bit for bit: xt_vc2carrier's second part against rom.4's routines.
// HARDWARE_FIRMWARE.md, "The VOCODER OSC's converter and kind 5's update".  The update is the audio thread's.
#include "Vc2Carrier.h"
#include <cstdint>

// The tone generator's routines rom.4 shares with rom.2, as the VOCODER OSC's code calls them (research/VC2_MODE_PLAN.md,
// step 1b: equal to rom.2's, so XtOsc's ports serve the VC-2)
class Vc2ToneShared {
public:
    virtual ~Vc2ToneShared() = default;
    virtual int velocity(int velocity, int sens, int curve) const = 0;   // 0x893a7fb0: a factor 0…32768
    virtual float exp2(float x) const = 0;                                // 0x893a8340
    // 0x895065f4, the library's logf: its core 0x89509a78 is rom.2's 0x89531b78 instruction for instruction (v236,
    // the pitch ramp's; COSM's converters' too); the wrapper's errno (0x89add990) for an infinite or NaN result isn't
    // the caller's
    virtual float log(float x) const = 0;
};

// The Matrix's sums for the OSC as its two getters give them (rom.4's 0x893a23b0/0x893a2440 and 0x893a26b0/0x893a28b0,
// rom.2's 0x894e7010…): the OSC group, M + 0xca on (Pitch, Time, Formant, Level), and its LFO's (Rate, Pitch, Time,
// Formant, Level)
struct Vc2MatrixGroups {
    int16_t osc[4];
    int16_t lfo[5];
};

// P from +0x30: the converter's terms, which kind 5's update reads
struct Vc2VocoderOscTerms {
    float growl, growlEnv, growlLfo;          // +0x30…+0x38: Growl Level (0…1), its envelope and LFO depths
    int32_t growlZero[2];                     // +0x3c, +0x40: 0
    float balance, balanceEnv, balanceLfo;    // +0x44…+0x4c: Saw-Noise Mix Balance (0…2), its envelope and LFO depths
    int32_t balanceIndex, balanceZero;        // +0x50: 3 on OSC 1, 7 on OSC 2; +0x54: 0
};

struct Vc2VocoderOscP {
    Vc2CarrierParams carrier;   // +0x00…+0x30: what the carrier reads
    Vc2VocoderOscTerms terms;   // +0x30…+0x58
};
static_assert(sizeof(Vc2VocoderOscP) == 0x58, "rom.4's P, the voice + 0x104…+0x15c");

// The converter's own part: P+0x0c…+0x1c, +0x24, +0x28 and the terms, from the converter's input as the note-on makes
// it (0x8939fe08: +2 the OSC, +3 the key, +4 the velocity, the OSC's record from +8), the Matrix's groups for the OSC
// and the shared velocity curve.  P+0x00…+0x0b, +0x20 and +0x2c are kind 5's update's.
void vc2VocoderOscConvert(const uint8_t* input, const Vc2MatrixGroups& sums, const Vc2ToneShared& shared,
                          Vc2VocoderOscP& p);

// The voice's words kind 5's update reads, by their offset in the voice
struct Vc2Kind5Voice {
    float cents;                       // +0x5c: the pitch's cents, where their sum starts
    float pitchEnv, pitchEnvDepth;     // [+0xa4c]+0x4c, +0x70: an envelope's output and its depth
    float pitchMod, pitchModDepth;     // +0x2c, +0xfc
    float ramp;                        // the pitch ramp's value (0x893b1160 on the voice + 0x90c)
    float centsOffset;                 // +0xa70
    float ratio, ratio2;               // +0x60, +0xa6c: the ratio's two factors after exp2
    float gainMod, gainModDepth;       // +0x30, +0x100: the gain's 1 + their product
    float gain[3];                     // +0x88, +0xb0, +0x960: the gain's other factors
    float growlEnv, growlLfo;          // [+0xa50]+0x4c, [+0xa60]+0x38: the growl's envelope and LFO outputs
    float balanceEnv, balanceLfo;      // [+0xa54]+0x4c, [+0xa64]+0x38: the balance's
};

// 0x893afff0: P's growl and balance from its terms and the modulators, its gain and pitch, then the carrier's update on
// P with the input stage's peak
void vc2Kind5Update(const Vc2Kind5Voice& v, const Vc2ToneShared& shared, Vc2VocoderOscP& p, Vc2Carrier& carrier,
                    float peak);
