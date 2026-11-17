#pragma once
// rom.4's COSM note-on frame (v250), bit for bit: the note-on 0x893aa0e0 (the pool, the code, D's floats cleared, the
// lines' place, the code's byte, the allocator 0x893a8c50, the start values 0x893a85f0, the bound envelopes' and LFOs'
// levels and rates held, the new block, the code's own case) and the new block 0x893aa5d0's frame.  Each type's parts
// are rows by code (Vc2CosmTypeRow): v250's Through (0), the Speaker Simulator (5) and the Growl (9, Vc2CosmGrowl on the
// object's words); the types XtCosm ports come as adapters over it, a few a version.  A code whose type has no row yet
// does the frame alone.  xt_vc2cosm's sixth part.  HARDWARE_FIRMWARE.md, "The VC-2's COSM note-on frame".
#include "Vc2CosmGrowl.h"
#include "Vc2CosmPools.h"

// A converter's block as the note-on and new block read it: its code, then the type's words (the object's +0x10…)
struct Vc2CosmBlockWords {
    int32_t code;
    uint32_t words[17];
};
static_assert(sizeof(Vc2CosmBlockWords) == 0x48, "rom.4's layout, a COSM block");

// What the note-on reads beyond the object: a word of the tone's memory, an envelope's +0x4c (level) or +0x54 (rate)
// or an LFO's +0x38 (level), by its address (the object's bindings)
class Vc2CosmBound {
public:
    virtual ~Vc2CosmBound() = default;
    virtual float word(uint32_t at) = 0;
};

class Vc2CosmNoteOn;

// A type's parts the frame calls, any of them none: its start values (0x893a85f0's case), new block (0x893aa5d0's),
// and its own case of the note-on
struct Vc2CosmTypeRow {
    int32_t code;
    void (*startValues)(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);
    void (*newBlock)(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);
    void (*noteOnCase)(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o);
};

class Vc2CosmNoteOn {
public:
    Vc2CosmNoteOn(Vc2CosmPools& pools, Vc2CosmBound& bound) : pools_(pools), bound_(bound) {}
    // 0x893aa0e0: a note on the object with the converter's block, its pool COSM 1's (0) or COSM 2's (1)
    void noteOn(Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, uint32_t pool);
    // 0x893aa5d0: a new block, when its code is the object's
    void newBlock(Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);
    // A type's row, or none
    static const Vc2CosmTypeRow* rowOf(int32_t code);

    Vc2CosmGrowl growl;   // the Growl's parts, on the object's words for each call

private:
    void allocate(Vc2CosmObjectWords& o);   // 0x893a8c50
    void hold(Vc2CosmObjectWords& o);       // the bound levels and rates
    Vc2CosmPools& pools_;
    Vc2CosmBound& bound_;
};
