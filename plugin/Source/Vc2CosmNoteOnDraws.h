#pragma once
// xt_vc2cosm's sixth part's draws (v250, Vc2CosmNoteOnCheck.cpp): where its object, block and bound envelopes and LFOs
// lie in Vc2Fw's RAM, the codes the port has types for, the Growl's block as its converter gives it, the bound words,
// and the noise a slot holds before a note-on takes it.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CheckRig.h"
#include "Vc2CosmNoteOn.h"

// In Vc2Fw's RAM: the object, its block, the four bound objects (envelopes A, B, LFOs A, B), 0x100 apart
inline constexpr uint32_t kNoteOnObjectAt = 0x8C075000, kNoteOnBlockAt = 0x8C075200, kNoteOnBoundAt = 0x8C075400;

// The object and its block into rom.4's memory
void vc2WriteNoteOnState(Sh4& cpu, const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b);
// A block: its code one the port has a type for (Through, the Speaker Simulator, the Growl; the adapted types from
// v252) or one the switches pass (past 20, its byte a type's with no start values or none), its words as that type's
// converter gives them
void vc2DrawNoteOnBlock(Vc2CosmBlockWords& b, std::mt19937& rng);
// A block of a code the port has a type for, as its converter gives it, for an update to follow (v262)
void vc2DrawPortedBlock(Vc2CosmBlockWords& b, std::mt19937& rng);
// The block's words as its code's converter gives them, when the port draws its type, for a code given
// after (v253: rom.4's Lo-Fi indexes its tables by a byte its converter keeps in range)
void vc2DrawTypeWords(Vc2CosmBlockWords& b, std::mt19937& rng);
// The pool a note-on is given: COSM 1's or 2's mostly, now and then past them
uint32_t vc2DrawNoteOnPool(std::mt19937& rng);
// The object: noise, its bindings the four bound objects, its slot none or one of 24, the floats the Growl reads moderate
void vc2DrawNoteOnObject(Vc2CosmObjectWords& o, std::mt19937& rng);
// The bound objects' words the note-on holds: the envelopes' levels and rates, the LFOs' levels
void vc2DrawBound(Sh4& cpu, std::mt19937& rng);
// Noise into the slot a note-on with `code` on `pool` would take, rom.4's and the port's alike: the slot, or −1
int32_t vc2FillFreeSlot(Sh4& cpu, Vc2CosmPools& pools, uint32_t pool, int32_t code, std::mt19937& rng);
#endif
