#pragma once
// The VC-2's pitch tracker's frame (v223): the module from 0x893edca0, on its state in rom.4's .bss (0x899d9b70…
// 0x899d9d1c), word for word.  Its two static initializers (0x893edf90's two words, the table setter 0x893ef240), the
// reset 0x893ee040, the mode setter 0x893ee100, the getters and setter 0x893ee010/0x893ee020/0x893ee030, and the block
// entry 0x893ee280: two state-variable stages whose coefficients glide toward their targets, the peaks and counters a
// sample, the fallback period, the noise count and the noisy-hold flag.  Each zero crossing of the band-passed signal
// goes to the analysis (the crossing routines and the estimator, v224) through Vc2TrackerCrossings.  Bit for bit:
// xt_vc2tracker against rom.4's routines in Sh4Interp.  HARDWARE_FIRMWARE.md, "The pitch tracker's frame";
// research/VC2_VOCAL_DESIGNER.md, "The pitch tracker, run", "The pitch tracker's analysis".  Audio thread.
#include "Vc2TrackerTables.h"
#include <cstdint>

// A state-variable stage: low += f·band, then band += f·(in − band − low)
struct Vc2TrackerStage {
    float band, low;
};

// The tracker's state as rom.4 lays it out from 0x899d9b70
struct Vc2TrackerWords {
    static constexpr int kBlock = 32;
    float rate[2];                          // b70, b74: the static initializer's 44100/440 and 880/44100
    Vc2TrackerSettings settings;            // b78…bc4: the table setter's
    float modeHighpass, modeLowpass;        // bc8, bcc: the mode's two f, the targets the band-pass falls back to
    float period;                           // bd0: P, the tracked period in samples, what the block entry returns
    float fallback;                         // bd4: P as it stood when no crossing came in the longest candidate
    float estimate, analysisBdc;            // bd8: the last estimate taken; bdc: the analysis's (v224)
    float filtered[kBlock + 1];             // be0: the band-passed signal, the last block's last sample, then this one's
    int32_t lastUp;                         // c64: 1 after an up crossing, 0 after a down one
    int32_t lock, failures;                 // c68, c6c: agreeing estimates and estimates refused, each up to 256
    int32_t follow, handOff;                // c70, c74: whose estimates move P (2 both, 0 up, 1 down); the hand-off flag
    float peak, recentPeak;                 // c78, c7c: the largest |x| since the last crossing; the recent peaks'
    float framePeak, lastFramePeak;         // c80, c84: the largest |x| this frame of 881 samples, and the last frame's
    float crossing[8];                      // c88…ca4: the crossing routines' (v224)
    int32_t counts[3];                      // ca8…cb0: the crossing routines' (v224)
    float lowpassTarget, highpassTarget;    // cb4, cb8: the two coefficients' targets
    float analysisCbc;                      // cbc: the analysis's (v224)
    int32_t sinceCrossing, lastSince;       // cc0, cc4: samples since the last valid crossing; the analysis's copy
    int32_t sinceUp, sinceDown;             // cc8, ccc: samples since the last crossing of each direction
    int32_t frameCount;                     // cd0: samples into the frame
    float smoothing;                        // cd4: how far P moves toward an estimate
    float shortest, longest;                // cd8, cdc: a candidate period's limits, the mode's
    float lowpassF, highpassF;              // ce0, ce4: the band-pass's coefficients, 2πF/44100
    Vc2TrackerStage highpass, lowpass;      // ce8, cf0: the two stages
    float analysisCf8;                      // cf8: 0 from the mode setter
    float halves[4];                        // cfc…d08: the half-intervals' history
    int32_t noisyHoldOn, noisyHold;         // d0c, d10: the noisy-hold flag's switch and the flag
    int32_t noise;                          // d14: the raw input's crossings a block, smoothed
    float lastInput;                        // d18: the raw input's last sample
};
static_assert(sizeof(Vc2TrackerWords) == 0x1ac, "rom.4's layout, 0x899d9b70…0x899d9d1c");

// The analysis each zero crossing of the band-passed signal goes to: the crossing routines (v224), on the state
class Vc2TrackerCrossings {
public:
    virtual ~Vc2TrackerCrossings() = default;
    virtual void up(Vc2TrackerWords& s, float previous, float now) = 0;     // 0x893ee570: previous < 0 ≤ now
    virtual void down(Vc2TrackerWords& s, float previous, float now) = 0;   // 0x893ee7d0: now < 0 ≤ previous
};

class Vc2Tracker {
public:
    explicit Vc2Tracker(Vc2TrackerCrossings& crossings);   // the static initializers' words, the rest 0, as .bss
    void reset();                                           // 0x893ee040, the mode setter's mode 0 among it
    void setMode(int mode);                                 // 0x893ee100: 0…3; any other leaves all as it was
    float block(const float* in, int n);                    // 0x893ee280: n (1…32) samples, 1 full scale; P
    float period() const { return s.period; }               // 0x893ee010
    int32_t noisyHold() const { return s.noisyHold; }       // 0x893ee020
    void setNoisyHold(int32_t on) { s.noisyHoldOn = s.noisyHold = on; }   // 0x893ee030

    Vc2TrackerWords s {};

private:
    void filter(const float* in, int n);    // the coefficients' glide and the two stages into filtered[1…n]
    void track(int n);                      // the peaks, counters, fallback and crossings a sample
    void countNoise(const float* in, int n);   // the raw input's crossings, and the noisy-hold flag

    Vc2TrackerCrossings& crossings_;
};
