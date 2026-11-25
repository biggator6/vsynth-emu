// xt_vc2sets' sixth part (v285): C's TVA, Vc2SetTvas, against rom.4's own routines on its own TVAs (0x89997c04), notes
// W and its global step (0x8993eb08), after the mix's part on the seed after its; the group reader its sums come from
// (0x893a2bf0) hooked to log the M it's given and answer the drawn sums, the velocity curve and powf rom.4's own (the
// port's curve through RomShared, its powf the port's).  The voices, the parts, the parts' TVA words (0x89e4987c), the
// part's controllers' flag (0x89997fcc), the part table's trailer (0x89e4b07c) and four Ms' controllers (in Vc2Fw's
// RAM) are drawn in rom.4's memory, which the port reads too.  Each case noise in the TVAs, then 400 events: the
// placement, the constructor, the end, the conversion, the update, the copy and the block writer alone; after each the
// TVAs, the step, W, the calls and the answer.  Not drawn: a controller past 127 (MIDI's), an M but the four, a rate of
// 0 (1 / 0).  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetTva.h"
#include "Vc2SetsChildCheck.h"

// rom.4's curve in the interpreter, its registers kept (SetKeptShared, v390)
int SetPortParts::velocityCurve(int velocity, int sens, int curve) { return kept_.velocity(velocity, sens, curve); }

namespace {
using Tvas = Vc2SetTvas;
constexpr uint32_t kMatrices = 4, kControls = 4;
// [FW] the TVA's routines, which the sets' part hooks; the reader its sums come from, which this part hooks
constexpr uint32_t kRoutines[] = { 0x893a67d0, 0x893a67f0, 0x893a68b0, 0x893a68c0, 0x893a6940, 0x893a69a0 };
constexpr uint32_t kReaders[] = { 0x893a2bf0 };

struct TvaCounts : SetChildCounts<7> {
    static constexpr const char* kPool = "the TVAs";
    static constexpr const char* kSaid = "TVA: %ld events: %ld placements, %ld constructors, %ld ends, %ld "
                                         "conversions, %ld updates, %ld copies, %ld block writes (%ld Matrix sums)\n";
};

using TvaSides = SetChildSides<Tvas, TvaCounts>;
using TvaEvent = std::string (*)(TvaSides&, std::mt19937&);

uint32_t anyTvaBlock(std::mt19937& rng) { return setsNote(rng) + Vc2ToneInput::kTva; }

// One of the four Ms whose controllers are drawn
uint32_t tvaMatrix(std::mt19937& rng) {
    return kSetsTvaMatricesAt + kSetsTvaMatrixBytes * (uint32_t)(rng() % kMatrices);
}

// [FW] a level's byte at its edges; a depth's either side of ±63; the curve's depth either side of ±200 (0x893a7fb0's)
constexpr auto tvaByte = setsByteDrawn<0, 1, 63, 64, 126, 127, 128, 255>;
constexpr auto tvaDepth = setsEdgeDrawn<int8_t, 0, 1, -1, 62, 63, 64, -63, -64, -128>;
constexpr auto tvaSens = setsEdgeDrawn<int16_t, 0, 1, -1, 63, -63, 199, 200, -199, -200, 32767, -32768>;
// A controller's value, MIDI's 0…127: at the pan's centre and ends, or any of them
uint8_t tvaController(std::mt19937& rng) { return setsByteDrawn<0, 1, 63, 64, 65, 126, 127>(rng) & 0x7f; }
constexpr auto tvaGain = setsEdgeDrawn<int8_t, 0, 6, -6, 12, -12, 127, -128>;
constexpr auto tvaRate = setsEdgeDrawn<int16_t, 100, 50, 200, 1, -1, 32767, -32768>;

void tvaWordsOf(Vc2SetTvaWords& w, std::mt19937& rng) {
    setsNoise(w, rng);
    w.curve = (uint8_t)(rng() % 4 ? rng() % 9 : rng());
    w.sens = tvaSens(rng);
    w.on = rng() % 4 ? 1 : rng() % 2 ? 0 : (uint8_t)rng();
    w.level = tvaByte(rng);
    w.levelFollow = setsFollowDrawn(rng);
    w.depthA = tvaDepth(rng);
    w.pan = tvaByte(rng);
    w.panFollow = setsFollowDrawn(rng);
    w.depthB = tvaDepth(rng);
    w.output = tvaByte(rng);
    w.sendB = tvaByte(rng);
    w.sendA = tvaByte(rng);
    w.route = (uint8_t)(rng() % 4 ? rng() % 4 : rng());
    w.gain = tvaByte(rng);
    w.velocityLevel = (float)(rng() % 65537) / 65536.0f;
    w.matrix = tvaMatrix(rng);
}

// rom.4's memory the writer reads past the words: the flag, the trailer (its rate never 0), the Ms' controllers
void tvaAround(Vc2Fw& fw, std::mt19937& rng, bool all) {
    if (all || rng() % 8 == 0) fw.cpu.wr8(kSetsTvaFlagAt, rng() % 2 ? 0 : rng() % 2 ? 1 : (uint8_t)rng());
    if (all || rng() % 8 == 0) {
        fw.cpu.wr8(kSetsTrailerAt, (uint8_t)tvaGain(rng));
        const int16_t rate = tvaRate(rng);
        fw.cpu.wr16(kSetsTrailerAt + 2, (uint16_t)(rate == 0 ? 100 : rate));
        fw.cpu.wr8(kSetsTrailerAt + 4, rng() % 2 ? 0 : (uint8_t)rng());
    }
    for (uint32_t m = 0; m < kMatrices; ++m)
        for (uint32_t n = 0; n < kControls; ++n)
            if (all || rng() % 8 == 0)
                fw.cpu.wr8(kSetsTvaMatricesAt + kSetsTvaMatrixBytes * m + kSetsControlsAt + n, tvaController(rng));
}

void tvaDraw(TvaSides& x, std::mt19937& rng, bool all) {
    setsChildDraw<TvaSides, kSetsTvaTable, 12, tvaWordsOf, true>(x, rng, all);
    tvaAround(x.s.fw, rng, all);
}

// [FW] the events by routine; the copy and the block writer 0x893a6260 alone, the same work
const TvaEvent kEvents[] = {
    setsPlaceEvent<TvaSides, 0x893a67d0, 40, 0>,
    setsMakeEvent<TvaSides, anyTvaBlock, 0x893a67f0, 1, Tvas::kPerVoice, tvaMatrix>,
    setsMakeEvent<TvaSides, anyTvaBlock, 0x893a67f0, 1, Tvas::kPerVoice, tvaMatrix>,
    setsEndEvent<TvaSides, 0x893a68b0, 2>,
    setsConvertEvent<TvaSides, &Tvas::convert, anyTvaBlock, 0x893a68c0, 3>,
    setsConvertEvent<TvaSides, &Tvas::convert, anyTvaBlock, 0x893a68c0, 3>,
    setsUpdateEvent<TvaSides, &Tvas::update, 0x893a6940, 4>,
    setsCopyEvent<TvaSides, &Tvas::copy, anyTvaBlock, 0x893a69a0, 5>,
    setsCopyEvent<TvaSides, &Tvas::copy, anyTvaBlock, 0x893a6260, 6>,
};
} // namespace

long vc2SetTvaPart(SetSides& s, const Vc2Run& run) {
    return setsChildPart<TvaSides, tvaDraw, kEvents>(s, run, 5, kRoutines, kReaders);
}
#endif
