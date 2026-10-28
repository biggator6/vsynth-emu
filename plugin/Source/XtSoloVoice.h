#pragma once
// The V-Synth XT's SOLO grain player as a block-rendering voice, for the
// offline engine (VariphraseEngine::soloFirmwareResynthOffline) and the
// plugin's voices alike.  Behaviour as described in VariphraseEngine.cpp
// (firmware 0x8950cd90 chain; checked sample by sample against the firmware
// in Ghidra's P-code emulator).
#include <cstdio>
#include <vector>

// A sample prepared for SOLO playback: the samples and their grain records.
struct XtSoloPlan {
    std::vector<float> in;              // samples (mono)
    std::vector<double> segStart, segLen;
    std::vector<char> voiced;
    std::vector<int> info;              // record info word, −1 if unknown
    // Diagnostics (read from the environment where the plan is built)
    bool anchorMode = true;             // VSE_SCHED=pos: record containing P
    bool forceUnvoiced = false;         // VSE_LITE=solo-np
    int forcedInfo = -1;                // VSE_LITEINFO
    size_t size() const { return segStart.size(); }
};

class XtSoloVoice {
public:
    // A = pitch ratio, F = formant ratio (clamped to [0.5, 2] by the caller),
    // stretch = time stretch, energy = SOLO Energy g (0 = OFF).
    void noteOn(const XtSoloPlan& plan, double A, double F, double stretch, double energy);
    // Pitch, formant and time may change while the note plays.
    void setRates(double A, double F, double stretch);
    // Renders `count` samples.
    void render(float* out, int count);
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
    double low = 0, band = 0, fCoef = 0, q = 1.2;
    double f0n = 0.0, anchorStepInit = 1.0;
    long t = 0;

    double at(int i) const;
    double readGrain(double pos, double tap, double tapW) const;
    double budgetOf(size_t i) const;
    static double repeatCost(double a, double tw);
    Grain grainParams(size_t sg, bool setF0);
};
