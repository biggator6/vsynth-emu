#pragma once
// The COSM adapters' access to rom.4's words (v255, moved out of Vc2CosmAdapters.cpp as they stood, that file at its
// line limit): a block's float, its bytes into the object, and the levels a type's laws take, the bound start levels
// or the held.  Every offset the object's or its block's.  The tone generator's thread.
#include "Vc2CosmNoteOn.h"
#include <cstddef>

namespace Vc2CosmAdapter {
// The levels a type's laws take: envelope A's and LFO A's, envelope B's and LFO B's (the bound start levels for its
// start values, the held levels for its new block)
struct Vc2CosmStarts {
    float envA, lfoA, envB, lfoB;
};

// A block's word k as a float (the block's +4 + 4k)
float blockFloat(const Vc2CosmBlockWords& b, int k);
// The bound envelopes' start levels (+0x58) and LFOs' levels (+0x38), through the note-on's bound words
Vc2CosmStarts startsOf(Vc2CosmNoteOn& n, const Vc2CosmObjectWords& o);
// The levels the note-on and update held (+0x104…+0x110)
Vc2CosmStarts heldOf(const Vc2CosmObjectWords& o);
// The block's bytes from `from` into the object's at the same place past its code (+0x10 + from)
void copyBlock(Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, size_t from, size_t bytes);
} // namespace Vc2CosmAdapter
