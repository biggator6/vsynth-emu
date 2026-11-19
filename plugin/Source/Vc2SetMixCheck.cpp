// xt_vc2sets' fifth part (v284): C's mix, Vc2SetMixes, against rom.4's own routines on its own mixes (0x89996c34) and
// notes W, after the LFOs' part on the seed after its, nothing hooked (the sets' part's hooks on the mix's routines
// taken off: they call nothing); the level run rom.4's own.  The voices, the parts and the parts' mix words
// (0x89e32a60) are drawn in rom.4's memory, which the port reads too.  Each case noise in the mixes, then 400 events:
// the placement, the constructor, the end, the conversion, the update, the copy and the block writer alone; after each
// the mixes, W and the answer.  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetMix.h"
#include "Vc2SetsChildCheck.h"

namespace {
using Mixes = Vc2SetMixes;
// [FW] the mix's routines, which the sets' part hooks
constexpr uint32_t kRoutines[] = { 0x8939bd90, 0x8939bdb0, 0x8939be30, 0x8939be40, 0x8939be60, 0x8939bec0 };

struct MixCounts : SetChildCounts<7> {
    static constexpr const char* kPool = "the mixes";
    static constexpr const char* kSaid = "mix: %ld events: %ld placements, %ld constructors, %ld ends, %ld "
                                         "conversions, %ld updates, %ld copies, %ld block writes\n";
};

using MixSides = SetChildSides<Mixes, MixCounts>;
using MixEvent = std::string (*)(MixSides&, std::mt19937&);

uint32_t anyMixBlock(std::mt19937& rng) { return setsNote(rng) + Vc2ToneInput::kMix; }

// [FW] a byte the gains read a table by: either side of 127, the ends, any
constexpr auto mixByte = setsByteDrawn<0, 1, 126, 127, 128, 254, 255>;

// A mix's words: on (usually), a type of the four, 0 or 5 (now and then any), the three bytes
void mixWordsOf(Vc2MixWords& w, std::mt19937& rng) {
    setsNoise(w, rng);
    w.on = rng() % 4 ? 1 : rng() % 2 ? 0 : (uint8_t)rng();
    w.type = (uint8_t)(rng() % 4 ? rng() % 6 : rng());
    w.level = mixByte(rng);
    w.first = mixByte(rng);
    w.second = mixByte(rng);
}

constexpr auto mixDraw = setsChildDraw<MixSides, kSetsMixTable, 11, mixWordsOf, false>;

// [FW] the events by routine; the copy and the block writer 0x8939bcc0 alone, the same work
const MixEvent kEvents[] = {
    setsPlaceEvent<MixSides, 0x8939bd90, 32, 0>,
    setsMakeEvent<MixSides, anyMixBlock, 0x8939bdb0, 1, Mixes::kPerVoice>,
    setsMakeEvent<MixSides, anyMixBlock, 0x8939bdb0, 1, Mixes::kPerVoice>,
    setsEndEvent<MixSides, 0x8939be30, 2>,
    setsConvertEvent<MixSides, &Mixes::convert, anyMixBlock, 0x8939be40, 3>,
    setsConvertEvent<MixSides, &Mixes::convert, anyMixBlock, 0x8939be40, 3>,
    setsUpdateEvent<MixSides, &Mixes::update, 0x8939be60, 4>,
    setsCopyEvent<MixSides, &Mixes::copy, anyMixBlock, 0x8939bec0, 5>,
    setsCopyEvent<MixSides, &Mixes::copy, anyMixBlock, 0x8939bcc0, 6>,
};
} // namespace

long vc2SetMixPart(SetSides& s, const Vc2Run& run) {
    return setsChildPart<MixSides, mixDraw, kEvents>(s, run, 4, kRoutines);
}
#endif
