#pragma once
// The COSM types with a pool slot that XtCosm's getters reach, adapted to rom.4's object (v258): the Compressor (15),
// the Limiter (16) and the Frequency Shifter (18), rows of vc2CosmAdapterRows.  Their XtCosm classes allocate their
// buffers when constructed, so the laws run on one object of each, made with the note-on frame (Vc2CosmNoteOn::scratch)
// and never on the tone generator's thread.  HARDWARE_FIRMWARE.md, "The VC-2's COSM adapters: the Compressor, the
// Limiter and the Frequency Shifter".
#include "Vc2CosmNoteOn.h"
#include "XtCosm.h"

struct Vc2CosmBufferedScratch {
    XtCosmComp comp;
    XtCosmLim lim;
    XtCosmFs fs;
};

void vc2CosmCompNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);     // 0x893aa6e0
void vc2CosmCompNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);   // 0x893aa4b0
void vc2CosmLimNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);      // 0x893a9c90
void vc2CosmLimNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);    // 0x893aa530
void vc2CosmFsStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);    // 0x893a8b30
void vc2CosmFsNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);       // 0x893a9df0
void vc2CosmFsNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);     // 0x893aa550
