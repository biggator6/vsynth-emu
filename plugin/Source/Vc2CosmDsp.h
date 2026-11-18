#pragma once
// A VC-2 COSM's sound (v263, v264): one XtCosmSection a COSM, as XtSynth holds for the V-Synth, fed what rom.4's frame
// reads at a note-on, new block and update, and rendering the routine the code's byte chose (rom.4's install
// 0x893aafe0).  rom.4 keeps this state in the object's D words and a pool slot's buffers; the port keeps it in XtCosm's objects, so
// each law stays in one place (the VC-2 session's choice, 2026-11-18).  All 14 types' objects are made with this one,
// off the tone generator's thread.  Every code a converter gives has its sound; the Growl's is v234's, on the object's
// own words.  One thing crosses from COSM to COSM: a Side Band Filter's second line reads up to 136 words before its
// buffer, the end of the memory in front of it, so each render keeps its rings' ends in the pools (Vc2CosmPools) and a
// Side Band Filter reads them back.  xt_vc2cosm's seventh part.  HARDWARE_FIRMWARE.md, "The VC-2's COSM DSP".
#include "Vc2CosmNoteOn.h"
#include "XtCosm.h"
#include <array>

// What a type's XtCosm object is given: envelope A's start level, held level and rate and LFO A's level, then B's; the
// note's pitch ratio and its wave's root; and D+0x98 as the last note left it, where the Dynamic TVF's follower starts
struct Vc2CosmDspLevels {
    float startA, heldA, rateA, lfoA, startB, heldB, rateB, lfoB;
    float pitch, root, word98;
};

// The delay lines a type keeps, each at the place of the slot's buffer it is in rom.4 (none: null), and their length
struct Vc2CosmDspRings {
    std::array<const float*, 4> lines {};
    int length = 0;
};

// A type's parts: its code, XtCosmSection's number for it, what its object does at each of the frame's events (none
// where the frame's own words hold all of it, as the Growl's), its delay lines, and what its sound leaves in D+0x98
struct Vc2CosmDspRow {
    int32_t code;
    int type;
    void (*noteOn)(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l);
    void (*newBlock)(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l);
    void (*update)(XtCosmSection& s, const Vc2CosmDspLevels& l);
    Vc2CosmDspRings (*rings)(const XtCosmSection& s) = nullptr;
    float (*word98)(const XtCosmSection& s) = nullptr;
};

class Vc2CosmDsp {
public:
    explicit Vc2CosmDsp(Vc2CosmPools& pools) : pools_(pools) {}
    // After the frame's note-on and install: the object they left, the block, and `stereo` the install's flag
    void noteOn(const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, Vc2CosmBound& bound, bool stereo);
    void newBlock(const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, Vc2CosmBound& bound);   // after the frame's
    void update(const Vc2CosmObjectWords& o, Vc2CosmBound& bound);                                 // after the frame's
    // 32 L/R frames in place, in the XT's units.  `past`: the three words after them, which the stereo Side Band
    // Filters' follower reads.  The Growl's words are the object's, so it takes the object as well
    void render(Vc2CosmObjectWords& o, float* lr, const float* past);
    const XtCosmSection& section() const { return section_; }
    const Vc2CosmGrowl& growl() const { return growl_; }

private:
    void carryWords(const Vc2CosmObjectWords& o);
    Vc2CosmDspRings rings() const;
    Vc2CosmPools& pools_;
    XtCosmSection section_;
    Vc2CosmGrowl growl_;
    const Vc2CosmDspRow* row_ = nullptr;   // the note's type, none for Through
    bool noted_ = false;                   // a note has been here: until then the words carried are the object's own
    float word98_ = 0.0f;
};
