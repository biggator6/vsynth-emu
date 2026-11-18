#pragma once
// A VC-2 COSM's sound (v263): one XtCosmSection a COSM, as XtSynth holds for the V-Synth, fed what rom.4's frame reads
// at a note-on, new block and update, and rendering the routine the code's byte chose (rom.4's install 0x893aafe0).
// rom.4 keeps this state in the object's D words and a pool slot's buffers; the port keeps it in XtCosm's objects, so
// each law stays in one place (the VC-2 session's choice, 2026-11-18).  All 14 types' objects are made with this one,
// off the tone generator's thread.  The types so far: OD/Dist, the Wave Shaper, the Amp and Speaker Simulators, the
// Resonator, the Dual Filter, the TVF, the Compressor, the Limiter, the Frequency Shifter and Lo-Fi.  xt_vc2cosm's
// seventh part.  HARDWARE_FIRMWARE.md, "The VC-2's COSM DSP: the first twelve codes".
#include "Vc2CosmNoteOn.h"
#include "XtCosm.h"

// What a type's XtCosm object is given: envelope A's start level, held level and rate and LFO A's level, then B's
struct Vc2CosmDspLevels {
    float startA, heldA, rateA, lfoA, startB, heldB, rateB, lfoB;
};

// A type's parts: its code, XtCosmSection's number for it, and what its object does at each of the frame's events
struct Vc2CosmDspRow {
    int32_t code;
    int type;
    void (*noteOn)(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l);
    void (*newBlock)(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l);
    void (*update)(XtCosmSection& s, const Vc2CosmDspLevels& l);
};

class Vc2CosmDsp {
public:
    // After the frame's note-on and install: the object they left, the block, and `stereo` the install's flag
    void noteOn(const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, Vc2CosmBound& bound, bool stereo);
    void newBlock(const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, Vc2CosmBound& bound);   // after the frame's
    void update(const Vc2CosmObjectWords& o, Vc2CosmBound& bound);                                 // after the frame's
    void render(float* lr);   // 32 L/R frames in place, in the XT's units
    const XtCosmSection& section() const { return section_; }

private:
    XtCosmSection section_;
    const Vc2CosmDspRow* row_ = nullptr;   // the note's type, none for a code the port doesn't render yet
};
