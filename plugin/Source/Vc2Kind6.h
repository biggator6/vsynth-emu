#pragma once
// The VC-2's voice kind 6, the mic's pitch shifter, and the mic ring it reads (v225): rom.4's module 0x893ec700…
// 0x893ecf10, a pitch-synchronous overlap-add (TD-PSOLA).  The block mixer writes each block of the mic into a ring
// of 4096 samples, with the pitch tracker's period for it into a history of 128; a voice reads 512 samples behind,
// steps an epoch marker through the ring by the recorded periods, and every T_out samples starts a grain at the
// latest epoch on one of two heads, each read with linear interpolation under a window. A lift by level, an SVF
// high-pass from the output's period, the mic itself where the input stage calls it unvoiced, and a gliding gain
// follow; both channels get the same sample.  Mode 0, the pitch (T_out = 44100 / (440 · P+0)), is the only one the
// firmware selects; mode 1, the interval (T_out = T_in / P+0), is reached by the mode setter, which nothing calls.
// Bit for bit: xt_vc2kind6 against rom.4's routines in Sh4Interp.  HARDWARE_FIRMWARE.md, "Voice kind 6";
// research/VC2_VOCAL_DESIGNER.md, "Voice kind 6, run".  Audio thread.
#include "Vc2Kind6Tables.h"
#include <cstdint>

// The mic ring as rom.4 lays it out from 0x8a0f31cc, word for word
struct Vc2MicRingWords {
    static constexpr int kSamples = 4096, kBlocks = 128;
    int32_t writeIndex;        // 31cc: where the next block goes
    float ring[kSamples];      // 31d0: the mic
    int32_t historyIndex;      // 71d0: where the next period goes
    float history[kBlocks];    // 71d4: the tracker's period as each block went in, samples
    int32_t started;           // 73d4: a voice has started since the reset or the mode's change: the renders run
    int32_t pitchMode;         // 73d8: mode 0's flag, T_out from P+0 as a pitch
};
static_assert(sizeof(Vc2MicRingWords) == 0x4210, "rom.4's layout, 0x8a0f31cc…0x8a0f73dc");

// The module's globals as rom.4 lays them out from 0x899d9968
struct Vc2Kind6Globals {
    float unvoiceLevel;   // 9968: Unvoice Level, the mic's share where unvoiced (a converter's, 0x893a73e4)
    float perA440;        // 996c: 440/44100, mode 0's
    float a440Period;     // 9970: 44100/440, the history's value at the reset
    float unread;         // 9974: 880/44100, which nothing reads
};
static_assert(sizeof(Vc2Kind6Globals) == 0x10, "rom.4's layout, 0x899d9968…0x899d9978");

class Vc2MicRing {
public:
    static constexpr int32_t kDataMode = 2;   // [FW] 0x8993fe54's .data value, which the first reset makes 0
    void setGlobals();                 // 0x893ecef0, the init table's
    void reset();                      // 0x893ec7d0: the ring cleared, the history at 44100/440, mode 0 if it wasn't
    void write(const float* block, float period);   // 0x893ec820(in, 32): 32 samples, the tracker's period
    void setMode(int32_t mode);        // 0x893ec890: 0 or 1 if new, and no voice renders till one starts again

    Vc2MicRingWords w {};
    Vc2Kind6Globals g {};
    int32_t mode = kDataMode;          // 0x8993fe54
};

// What the setters read from P, the voice's + 0x104
struct Vc2Kind6Params {
    float ratio;   // P+0: the note's pitch over 440 Hz in mode 0, the interval in mode 1
    float rate;    // P+4: the heads' read rate
    float gain;    // P+8: the output's gain, 1 the bus's full scale
    float level;   // P+0xc: the lift's and the filter's depth
};

// A head: where it is in its grain and its window, where it reads from; its period and position are written at a
// grain's start and read by nothing in the module
struct Vc2Kind6Head {
    float count, window, base, period, frac;
};

// The voice's object at the voice + 0xafc, word for word
struct Vc2Kind6Words {
    int32_t position;                       // +00: the read point in the ring, 512 behind the write point at the start
    float epoch;                            // +04: the epoch marker in the ring
    float lastPeriod, period;               // +08: the period at the last epoch; +0c: the next epoch's step
    float ratioTarget, rateTarget, gainTarget, levelTarget;   // +10…+1c: from P, the gain's × 2^31
    float unused;                           // +20: nothing in the module writes or reads it
    int8_t head;                            // +24: the head the last grain started on
    uint8_t pad[3];
    float grain;                            // +28: T_out, samples a grain
    float sinceGrain;                       // +2c: samples since the last grain's start
    float windowStep;                       // +30: a window's step a sample, ½ over the shorter of T_in/rate and T_out
    Vc2Kind6Head a, b;                      // +34, +48: the two heads
    float frac;                             // +5c: where the grain lies between the last two epochs, 0…1
    float ratio, rate, gain, level;         // +60…+6c: the targets as they glide
    float f, damping, band, low;            // +70…+7c: the SVF
    float effective;                        // +80: r′, T_in over T_out
    float lift;                             // +84: the lift's envelope
};
static_assert(sizeof(Vc2Kind6Words) == 0x88, "rom.4's layout, the voice + 0xafc…+0xb84");

class Vc2Kind6 {
public:
    static constexpr int kBlock = 32;
    void set(const Vc2Kind6Params* p);                       // 0x893ec790 (the update's): nothing for none
    void setAndStart(Vc2MicRing& ring, const Vc2Kind6Params& p);   // 0x893ec700 (the note-on's)
    void start(Vc2MicRing& ring);                            // 0x893ec8d0
    // 0x893ec730: a block of 32 frames of two words into out, the unvoiced decision a sample (the input stage's,
    // 0x893ed500); nothing at all until a voice has started
    void render(const Vc2MicRing& ring, const float* unvoiced, float* out);

    Vc2Kind6Words w {};

private:
    void grainLength(const Vc2MicRing& ring);    // 0x893ecd50: T_in, r′, T_out and the window's step
    void filterTargets();                        // 0x893ecde0: the SVF's f and damping glide toward r′'s
    void core(const Vc2MicRing& ring, const float* unvoiced, float* out);   // 0x893eca10
    void stepEpoch(const Vc2MicRing& ring);      // the core's first part (0x893ece80 a copy with no caller)
    void startGrain(const Vc2MicRing& ring);     // the next head's grain at the latest epoch
    float headSample(const Vc2MicRing& ring, Vc2Kind6Head& h);   // a head's read, its count on by one
};
