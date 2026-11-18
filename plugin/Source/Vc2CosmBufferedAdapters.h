#pragma once
// The COSM types with a pool slot, adapted to rom.4's object: the Compressor (15), the Limiter (16) and the Frequency
// Shifter (18) (v258), the Side Band Filters (7, 8) (v260), rows of vc2CosmAdapterRows.  Their XtCosm classes allocate
// their buffers when constructed, so the laws run on one object of each, made with the note-on frame
// (Vc2CosmNoteOn::scratch) and never on the tone generator's thread.  HARDWARE_FIRMWARE.md, "The VC-2's COSM adapters:
// the Compressor, the Limiter and the Frequency Shifter" and "the Side Band Filters".
#include "Vc2CosmNoteOn.h"
#include "XtCosm.h"

struct Vc2CosmBufferedScratch {
    XtCosmComp comp;
    XtCosmLim lim;
    XtCosmFs fs;
    XtCosmSbf sbf;
};

void vc2CosmSbfStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);   // 0x893a88a0, …8960
void vc2CosmSbfNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);      // 0x893a9310, …9410
void vc2CosmSbfNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);    // 0x893aa310, …a3d0
void vc2CosmSbfUpdate(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o);   // the update's cases 0x893aab00, 0x893aab50 (v262)
void vc2CosmFsUpdate(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o);    // the update's case 0x893aaea0 (v262)

void vc2CosmCompNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);     // 0x893aa6e0
void vc2CosmCompNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);   // 0x893aa4b0
void vc2CosmLimNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);      // 0x893a9c90
void vc2CosmLimNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);    // 0x893aa530
void vc2CosmFsStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);    // 0x893a8b30
void vc2CosmFsNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);       // 0x893a9df0
void vc2CosmFsNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);     // 0x893aa550
