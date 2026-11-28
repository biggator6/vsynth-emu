#pragma once
// What the editor shows of the synth's modulation (v464; research/NEXT_ISSUES.md item 2, the user's d-5: a modulator as
// a ring beside the knob, the knob saying what's stored): the newest note's Matrix sums by slot and Multi Step
// Modulator terms by destination, as its last conversion pass left them (XtVoice::mx and msm), which XtSynth's
// converters add to the knobs (XtSynthKnobs.cpp, XtSynthEnvKnobs.cpp).  The audio thread takes them after each block;
// the editor reads them on its timer.  A sounding note is one not yet let go, else the newest still sounding; none, no
// note.  It reads XtSynth's voices (a friend) and changes nothing.  No JUCE.
#include "XtMsm.h"
#include "XtOsc.h"
#include <atomic>
#include <cstdint>

class XtSynth;

class XtSynthView {
public:
    // Audio thread, after a render; or none sounding, for a block the players didn't play
    void take(const XtSynth& s);
    void none() { sounding_.store(false, std::memory_order_release); }
    // Any thread
    bool sounding() const { return sounding_.load(std::memory_order_acquire); }
    int matrix(int slot) const { return mx_[slot].load(std::memory_order_relaxed); }
    int msm(int dest) const { return msm_[dest].load(std::memory_order_relaxed); }

private:
    std::atomic<bool> sounding_ { false };
    std::atomic<int16_t> mx_[kXtMatrixSlots] {};
    std::atomic<int16_t> msm_[kXtMsmDests] {};
};
