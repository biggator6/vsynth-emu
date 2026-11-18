#pragma once
// The COSM types XtCosm ports, adapted to rom.4's object (v252): rom.4's per-type code is rom.2's opcode for opcode, so
// each type's parts are XtCosm's laws on rom.4's words, a word XtCosm holds no member for carried as rom.4's
// (HARDWARE_FIRMWARE.md, "The VC-2's COSM adapters").  A row a type, with its start values (0x893a85f0's case: the
// bound envelopes' start levels +0x58 and LFOs' levels +0x38 into D+0xbc, +0xc0) and new block (its routine: the held
// levels into the targets D+0xa4, +0xa8, and the block's words into the object's).  v252: the Resonator (6) and the
// TVF (13); v253: Lo-Fi (19), by XtCosmLofi's getters for the words its new block forms.
#include "Vc2CosmNoteOn.h"
#include <span>

// The adapted types' rows, which Vc2CosmNoteOn::rowOf finds after its own
std::span<const Vc2CosmTypeRow> vc2CosmAdapterRows();
