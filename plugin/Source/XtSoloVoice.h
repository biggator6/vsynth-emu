#pragma once
// The V-Synth XT's SOLO grain player as a block-rendering voice, for the
// offline engine (VariphraseEngine::soloFirmwareResynthOffline) and the
// plugin's voices alike.  Behaviour as described in VariphraseEngine.cpp
// (firmware 0x8950cd90 chain; checked sample by sample against the firmware
// in Ghidra's P-code emulator).
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
    // stretch = time stretch, energy = SOLO Energy g (0 = OFF).
    void noteOn(const XtSoloPlan& plan, double A, double F, double stretch, double energy);
    // Pitch, formant and time may change while the note plays.
    void setRates(double A, double F, double stretch);
    void setEnergy(double g) { energy = g; }
    // Renders `count` samples: mono (a stereo plan's left channel), or both
    // channels (a mono plan plays L = R, as the firmware writes it).
    void render(float* out, int count) { render(out, nullptr, count); }
    void render(float* left, float* right, int count);
    bool ended() const { return ended_; }
    FILE* trace = nullptr;              // VSE_TRACE: per-sample state, as EmuSolo.java writes it

private:
    struct Grain { double start, v, frac, C; };
    struct Chan { double start = 0, age = 0, v = 1, e = 1.0, tap = 0, tapW = 0; };
    const XtSoloPlan* p = nullptr;
    double A = 1, F = 1, walk = 1, energy = 0;
    Chan ch[2];
    int next = 1;
    double slope = 0.0;
    double P = 0.0, Pq = 0.0, timer = 0.0, ppw = 0.0;
    bool ended_ = false;
    size_t seg = 0;
    double anchor = 0.0, anchorStep = 1.0, budget = 0.0;
    double low[2] = {}, band[2] = {}, fCoef = 0, q = 1.2;   // SVF state per channel
    double f0n = 0.0, anchorStepInit = 1.0;
    long t = 0;

    double readGrain(const std::vector<float>& x, double pos, double tap, double tapW) const;
    double budgetOf(size_t i) const;
    static double repeatCost(double a, double tw);
    Grain grainParams(size_t sg, bool setF0);
};
