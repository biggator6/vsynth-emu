#pragma once
// The COSM types XtCosm ports, adapted to rom.4's object (v252): rom.4's per-type code is rom.2's opcode for opcode, so
// each type's parts are XtCosm's laws on rom.4's words, a word XtCosm holds no member for carried as rom.4's
// (HARDWARE_FIRMWARE.md, "The VC-2's COSM adapters").  A row a type, with its start values (0x893a85f0's case: the
// bound envelopes' start levels +0x58 and LFOs' levels +0x38 into D+0xbc, +0xc0) and new block (its routine: the held
// levels into the targets D+0xa4, +0xa8, and the block's words into the object's).  v252: the Resonator (6) and the
// TVF (13); v253: Lo-Fi (19), by XtCosmLofi's getters for the words its new block forms; v254: OD/Dist (1, 2), the Wave
// Shaper (3) and the Amp Simulator (4), with their note-on cases (the object's words into D's, rom.4's copies); v255:
// the Dual Filter (12), the Dynamic TVF (14), the TVF on XtCosmTvf as the Dynamic TVF, and the TB Filter (20) in
// Vc2CosmTbAdapter.cpp, its new block's targets and its note-on case's coefficients XtCosmTb's.
#include "Vc2CosmNoteOn.h"
#include <span>

// The adapted types' rows, which Vc2CosmNoteOn::rowOf finds after its own
std::span<const Vc2CosmTypeRow> vc2CosmAdapterRows();
// The TB Filter's new block (0x893b9530) and note-on case (0x893b9140), its row's parts (v255)
void vc2CosmTbNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);
void vc2CosmTbNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);
