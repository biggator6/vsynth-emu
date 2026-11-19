// xt_vc2sets' fourth part (v283): C's LFOs, Vc2SetLfos, against rom.4's own routines on its own LFOs (0x899962b0) and
// notes W, after the envelopes' part on the seed after its; the five Matrix readers the rates take hooked to log the M
// they're given and answer one run of drawn sums (the sets' part's hooks on the LFO's routines taken off); the rate
// table and the note values' ticks rom.4's own.  The voices, the parts and the parts' LFO words (0x89e2f85c) are drawn
// in rom.4's memory, which the port reads too.  Each case noise in the LFOs, then 400 events: the placement, the
// constructor, the end, the conversion, the update, the copy and the block writer alone; after each the LFOs, W, the
// calls and the answer.  Not drawn: a rate past 255 (a note value past the table's 128, where rom.4 reads what lies
// after it).  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetLfo.h"
#include "Vc2SetsChildCheck.h"

namespace {
using Lfos = Vc2SetLfos;
// [FW] the LFO's routines, which the sets' part hooks; the Matrix readers its rates take, which this part hooks
constexpr uint32_t kRoutines[] = { 0x8939b690, 0x8939b6c0, 0x8939b740, 0x8939b750, 0x8939b770, 0x8939b7c0 };
constexpr uint32_t kReaders[] = { 0x893a26b0, 0x893a28b0, 0x893a2960, 0x893a2aa0, 0x893a2bf0 };

struct LfoCounts : SetChildCounts<7> {
    static constexpr const char* kPool = "the LFOs";
    static constexpr const char* kSaid = "LFOs: %ld events: %ld placements, %ld constructors, %ld ends, %ld "
                                         "conversions, %ld updates, %ld copies, %ld block writes (%ld Matrix sums)\n";
};

using LfoSides = SetChildSides<Lfos, LfoCounts>;
using LfoEvent = std::string (*)(LfoSides&, std::mt19937&);

uint32_t anyLfoBlock(std::mt19937& rng) {
    return setsNote(rng) + Vc2ToneInput::kLfos + Vc2ToneInput::kLfoStride * (1 + rng() % Lfos::kPerVoice);
}

// A byte the block takes 0…`most` of: one of those or one past, now and then any
uint8_t lfoChoice(std::mt19937& rng, uint32_t most) { return (uint8_t)(rng() % 4 ? rng() % (most + 2) : rng()); }

// A rate: its ends, either side of the note values' start, any of 0…255, now and then below 0
int16_t lfoRateDrawn(std::mt19937& rng) {
    const auto rates = std::to_array<int16_t>({ 0, 1, 126, 127, 128, 129, 255, (int16_t)(rng() % 256),
                                               (int16_t)(rng() % 128), (int16_t)(0x8000 | rng()) });
    return setsOneOf(rates, rng);
}

// [FW] a depth's byte at the square's edges or any
constexpr auto lfoDepthDrawn = setsByteDrawn<0, 1, 64, 126, 127, 128, 255>;

void lfoWordsOf(Vc2LfoWords& w, std::mt19937& rng) {
    setsNoise(w, rng);
    w.index = (uint8_t)(rng() % 4 ? rng() % (Lfos::kPerVoice + 1) : rng());
    w.wave = lfoChoice(rng, 7);
    w.rate = lfoRateDrawn(rng);
    w.byte0a = lfoChoice(rng, 4);
    w.depthA = lfoDepthDrawn(rng);
    w.byte0c = lfoChoice(rng, 3);
    w.depthB = lfoDepthDrawn(rng);
    w.sync = lfoChoice(rng, 1);
}

constexpr auto lfoDraw = setsChildDraw<LfoSides, kSetsLfoTable, 5, lfoWordsOf, true>;

// [FW] the events by routine; the copy and the block writer 0x8939b590 alone, the same work
const LfoEvent kEvents[] = {
    setsPlaceEvent<LfoSides, 0x8939b690, 20, 0>,
    setsMakeEvent<LfoSides, anyLfoBlock, 0x8939b6c0, 1, Lfos::kPerVoice>,
    setsMakeEvent<LfoSides, anyLfoBlock, 0x8939b6c0, 1, Lfos::kPerVoice>,
    setsEndEvent<LfoSides, 0x8939b740, 2>,
    setsConvertEvent<LfoSides, &Lfos::convert, anyLfoBlock, 0x8939b750, 3>,
    setsConvertEvent<LfoSides, &Lfos::convert, anyLfoBlock, 0x8939b750, 3>,
    setsUpdateEvent<LfoSides, &Lfos::update, 0x8939b770, 4>,
    setsCopyEvent<LfoSides, &Lfos::copy, anyLfoBlock, 0x8939b7c0, 5>,
    setsCopyEvent<LfoSides, &Lfos::copy, anyLfoBlock, 0x8939b590, 6>,
};

} // namespace

long vc2SetLfoPart(SetSides& s, const Vc2Run& run) {
    return setsChildPart<LfoSides, lfoDraw, kEvents>(s, run, 3, kRoutines, kReaders);
}
#endif
