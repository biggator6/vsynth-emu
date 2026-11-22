#pragma once
// The VC-2 part's conversion sets' COSM inputs (v390, item 8's sixth child): rom.4's 48 of 60 bytes from 0x899955a8
// word for word, two a voice (0x89399ae0), each a COSM converter's input (Vc2CosmInput.h) converting a COSM block of
// the note's input (W + 0x658, + 0x6a0).  The constructor 0x89399b10: the kind and the index at +0 and +1, the voice's
// two bytes at +2, M at +0x38, the part's 42 bytes for the COSM at +6 by the kind's and the index's words, then the
// dispatcher 0x89399920 (vc2CosmConvert, its converters vc2CosmPortedConvert's); and on the same dispatcher the pool's
// conversion 0x89399b90 (the voice's bytes again), update 0x89399bb0 (the part's bytes again, by the object's) and copy
// 0x89399c00 (Vc2SetChildHead).  Its end 0x89399b80 is a bare return.  The part's bytes, the converters' shared
// routines and the Matrix's COSM readers (0x893a2960, 0x893a2aa0) through Vc2SetReads.  Bit for bit: xt_vc2sets'
// seventh part.  HARDWARE_FIRMWARE.md, "The VC-2 part's sets' COSM inputs".  The notes' task.
#include "Vc2CosmInput.h"
#include "Vc2SetChildHead.h"

// [FW] the pool at 0x899955a8, two a voice, the voice's bytes at +2
class Vc2SetCosmInputs
    : public Vc2SetChildPool<Vc2SetCosmInputs, Vc2CosmInput, 2, 0x899955a8, &Vc2SetReads::cosmInputOf, 2> {
public:
    // 0x89399b10 on the call's object: its kind, index, voice's bytes, M and the part's bytes, then the block
    void construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads);
    // 0x89399920, the dispatcher: the block, and the input's velocity factors
    static void blockOf(Vc2CosmInput& w, uint32_t* block, Vc2SetReads& reads);
};
static_assert(Vc2SetCosmInputs::kBytes == 60, "rom.4's layout, C's COSM input");
