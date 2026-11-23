// xt_vc2sets' seventh part (v390): C's COSM inputs, Vc2SetCosmInputs, against rom.4's own routines on its own inputs
// (0x899955a8) and notes W, after the TVA's part on the seed after its; COSM's two Matrix readers (0x893a2960,
// 0x893a2aa0) hooked to log the M they're given and answer the drawn sums, the dispatcher 0x89399920 and its converters
// rom.4's own, with the velocity curve, powf and logf (the port's through SetKeptShared, its converters the ported
// ones).  The voices, the parts and the parts' COSM bytes (0x89e2003c, by xt_vc2cosm's draws of an input) are drawn in
// rom.4's memory, which the port reads too, and past the sixteen parts' rows 1024 more, which a kind or an index past
// the four and the two reach (any byte of an input's, and the constructor's kind word to 511, which places its bytes
// by the whole word).  Each case noise in the inputs, then 400 events: the placement, the constructor, the end, the
// conversion, the update, the copy and the dispatcher alone; after each the inputs, W, the calls and the answer.  Not
// drawn: a constructor's kind past 511 (its bytes past the rows drawn).  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2CosmCheck.h"
#include "Vc2SetCosmInput.h"
#include "Vc2SetsChildCheck.h"
#include "Vc2ToneInput.h"

std::array<uint16_t, 21> SetPortParts::cosmInputOf(uint32_t part, uint32_t kind, uint32_t index) {
    return partWords<21>(kSetsCosmTable, part, kind, index);
}

const int16_t* SetPortParts::matrixGroup(uint32_t reader, uint32_t matrix) {
    logged(kSetMatrixSum, reader, { matrix });
    fw_.cpu.read(kSetsSumsAt, group_.data(), sizeof group_);
    return group_.data();
}

namespace {
using Inputs = Vc2SetCosmInputs;
constexpr uint32_t kFarKinds = 512, kFarRows = 1024, kRowBytes = 42;   // the kinds drawn, the rows past the parts'
constexpr uint32_t kFarAt = kSetsCosmTable.at + 16 * kSetsCosmTable.part;    // where the parts' rows end
constexpr uint32_t kRowAt = 6;                                                // [FW] an input's bytes from the store
// [FW] the COSM inputs' routines, which the sets' part hooks; COSM's Matrix readers, which this part hooks
constexpr uint32_t kRoutines[] = { 0x89399ae0, 0x89399b10, 0x89399b80, 0x89399b90, 0x89399bb0, 0x89399c00 };
constexpr uint32_t kReaders[] = { 0x893a2960, 0x893a2aa0 };

struct CosmCounts : SetChildCounts<7> {
    static constexpr const char* kPool = "the COSM inputs";
    static constexpr const char* kSaid = "COSM inputs: %ld events: %ld placements, %ld constructors, %ld ends, %ld "
                                         "conversions, %ld updates, %ld copies, %ld dispatches (%ld Matrix sums)\n";
};

using CosmSides = SetChildSides<Inputs, CosmCounts>;
using CosmEvent = std::string (*)(CosmSides&, std::mt19937&);

// One of the note's two COSM blocks
uint32_t anyCosmBlock(std::mt19937& rng) {
    const uint32_t note = setsNote(rng);
    return note + Vc2ToneInput::kCosms[rng() % 2];
}

// The constructor's kind: the four, or now and then a word to 511, past a byte's
uint32_t cosmKind(std::mt19937& rng) { return rng() % 8 ? rng() % 4 : (uint32_t)(rng() % kFarKinds); }

// An input as xt_vc2cosm draws one, its M any word (only the hooked readers are given it)
void cosmWordsOf(Vc2CosmInput& w, std::mt19937& rng) {
    const uint32_t matrix = setsAnyWord(rng);
    vc2DrawCosmInput(w, rng, matrix);
}

// The rows past the parts' at a case's first draw, each an input's bytes as cosmWordsOf draws them
void cosmFarDrawn(Vc2Fw& fw, std::mt19937& rng) {
    for (uint32_t r = 0; r < kFarRows; ++r) {
        Vc2CosmInput w;
        cosmWordsOf(w, rng);
        fw.cpu.write(kFarAt + kRowBytes * r, reinterpret_cast<const uint8_t*>(&w) + kRowAt, kRowBytes);
    }
}

void cosmDraw(CosmSides& x, std::mt19937& rng, bool all) {
    setsChildDraw<CosmSides, kSetsCosmTable, 21, cosmWordsOf, true>(x, rng, all);
    if (all) cosmFarDrawn(x.s.fw, rng);
}

// [FW] the events by routine; the copy and the dispatcher 0x89399920 alone, the same work
const CosmEvent kEvents[] = {
    setsPlaceEvent<CosmSides, 0x89399ae0, 60, 0>,
    setsMakeEvent<CosmSides, anyCosmBlock, 0x89399b10, 1, Inputs::kPerVoice, setsAnyWord, cosmKind>,
    setsMakeEvent<CosmSides, anyCosmBlock, 0x89399b10, 1, Inputs::kPerVoice, setsAnyWord, cosmKind>,
    setsEndEvent<CosmSides, 0x89399b80, 2>,
    setsConvertEvent<CosmSides, &Inputs::convert, anyCosmBlock, 0x89399b90, 3>,
    setsConvertEvent<CosmSides, &Inputs::convert, anyCosmBlock, 0x89399b90, 3>,
    setsUpdateEvent<CosmSides, &Inputs::update, 0x89399bb0, 4>,
    setsCopyEvent<CosmSides, &Inputs::copy, anyCosmBlock, 0x89399c00, 5>,
    setsCopyEvent<CosmSides, &Inputs::copy, anyCosmBlock, 0x89399920, 6>,
};
} // namespace

long vc2SetCosmInputPart(SetSides& s, const Vc2Run& run) {
    return setsChildPart<CosmSides, cosmDraw, kEvents>(s, run, 6, kRoutines, kReaders);
}
#endif
