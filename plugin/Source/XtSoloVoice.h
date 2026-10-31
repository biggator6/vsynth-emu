#pragma once
// The V-Synth XT's SOLO grain player as a block-rendering voice, for the
// offline engine (VariphraseEngine::soloFirmwareResynthOffline) and the
// plugin's voices alike.  Behaviour as described in VariphraseEngine.cpp
// (firmware 0x8950cd90 chain; checked sample by sample against the firmware
// in Ghidra's P-code emulator, and forward, held and backward against the
// firmware run in the SH-4 interpreter: xt_fw solocmp).
#include <cstdint>
#include <cstdio>
#include <vector>

// A sample prepared for SOLO playback: the samples and their grain records.
// A stereo wave has one record stream (encoded from (L+R) >> 1); the player
// reads both channels through the same grains (0x8950d1f0).
struct XtSoloPlan {
    std::vector<float> in;              // samples: mono, or the left channel
    std::vector<float> inR;             // the right channel of a stereo wave, else empty
    std::vector<double> segStart, segLen;
    std::vector<char> voiced;
    std::vector<int> info;              // record info word, −1 if unknown
    // Diagnostics (read from the environment where the plan is built)
    bool anchorMode = true;             // VSE_SCHED=pos: record containing P
    bool forceUnvoiced = false;         // VSE_LITE=solo-np
    int forcedInfo = -1;                // VSE_LITEINFO
    size_t size() const { return segStart.size(); }
    bool stereo() const { return !inR.empty(); }
};

class XtSoloVoice {
public:
    // A = pitch ratio, F = formant ratio (clamped to [0.5, 2] by the caller),
    // walk = playback speed (player +0x7c: 1 as recorded, 0 holds, < 0 plays
    // backward from the end), energy = SOLO Energy g (0 = OFF).
    void noteOn(const XtSoloPlan& plan, double A, double F, double walk, double energy);
    // Pitch, formant and time may change while the note plays.
    void setRates(double A, double F, double walk);
    void setEnergy(double g) { energy = g; }
    // The output gain (player +0x80 target, +0x84 current): each sample the
    // gain moves 1/64 toward the target, then scales the output (1 = unity;
    // the firmware's int16 scale has 1/65536).  resetGain sets both (note-on).
    void setGain(float target) { gainTarget = target; }
    void resetGain(float g) { gainTarget = gain = g; }
    // Robot Voice (OSC flag 0x10, player byte 0 bit 0x10; set before noteOn):
    // A is then the pitch against A440 and voiced grains come at that fixed
    // pitch (0x8950d5a0).
    void setRobot(bool on) { robot = on; }
    // Renders `count` samples: mono (a stereo plan's left channel), or both
    // channels (a mono plan plays L = R, as the firmware writes it).
    void render(float* out, int count) { render(out, nullptr, count); }
    void render(float* left, float* right, int count);
    bool ended() const { return ended_ != 0; }
    FILE* trace = nullptr;              // VSE_TRACE: per-sample state, as EmuSolo.java writes it

private:
    struct Grain { double start, v, frac, C; };
    struct Chan { double start = 0, age = 0, v = 1, e = 1.0, tap = 0, tapW = 0; };
    const XtSoloPlan* p = nullptr;
    double A = 1, F = 1, walk = 1, energy = 0;
    Chan ch[2];
    int next = 1;
    double slope = 0.0;
    // The phrase position in the firmware's 12.20 fixed point (player +0x38
    // in 1/16 samples, +0x3c its 20-bit fraction), stepped per sample by
    // half the block's walk step (+0x34 = trunc(walk·2^25), 0x8950c280).
    int32_t Pi = 0, Pf = 0, walkStep = 0;
    double Pq = 0.0, timer = 0.0, ppw = 0.0;
    int ended_ = 0;                     // +4: 1 at the end, −1 at the start
    // The current record (+0xe8 block: +0x20 start, +0x24 end, +8 the repeat
    // budget), as 0x8950d4c0 loads the next and 0x8950d530 the previous.
    // Index size() is the stream's (256, 1) terminator.
    size_t seg = 0;
    double recStart = 0.0, recEnd = 0.0;
    double anchor = 0.0, anchorStep = 1.0, budget = 0.0;
    double low[2] = {}, band[2] = {}, fCoef = 0, q = 1.2;   // SVF state per channel
    float gainTarget = 1.0f, gain = 1.0f;
    bool robot = false;
    double f0n = 0.0, anchorStepInit = 1.0;
    long t = 0;

    double readGrain(const std::vector<float>& x, double pos, double tap, double tapW) const;
    double lenOf(size_t i) const;
    double budgetOf(size_t i) const;
    void loadNext();
    void loadPrev();
    double repeatCost() const;
    void anchorForward();
    void anchorBackward();
    Grain grainParams(bool setF0);
};
