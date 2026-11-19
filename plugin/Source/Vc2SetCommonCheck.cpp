// xt_vc2sets' second part (v281): C's common, Vc2SetCommons, against rom.4's own routines on its own commons
// (0x89996f68) and notes W, after the sets' part on the seed after its, nothing hooked (the sets' part's hooks on the
// common's routines taken off: they call nothing).  The voices, the parts and the parts' three words the common takes
// (0x89997090) are drawn in rom.4's memory, which the port reads too.  Each case noise in the commons, then 400 events:
// the placement, the constructor, the end, the conversion, the update, the copy and its twin 0x893a1870; before each
// the sets' draws, the parts' words (the level and the depth at their edges half the time) and now and then the
// commons' words; after each the commons, W and the answer.  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetCommon.h"
#include "Vc2SetsChildCheck.h"

namespace {
using Commons = Vc2SetCommons;
constexpr int kParts = 16;
// [FW] the common's routines, which the sets' part hooks
constexpr uint32_t kRoutines[] = { 0x893a1940, 0x893a1960, 0x893a19b0, 0x893a19c0, 0x893a19e0, 0x893a1a10 };

struct CommonCounts : SetChildCounts<7> {
    static constexpr const char* kPool = "the commons";
};

using CommonSides = SetChildSides<Commons, CommonCounts>;
using CommonEvent = std::string (*)(CommonSides&, std::mt19937&);

uint32_t anyBlock(std::mt19937& rng) { return setsNote(rng) + Vc2ToneInput::kOscSet; }

// A level as the square meets it (0, 1 alone, the top, past it) or any; a depth at its edges or any
uint8_t commonLevel(std::mt19937& rng) {
    return setsOneOf(std::to_array<uint8_t>({ 0, 1, 2, 64, 126, 127, 128, 255, (uint8_t)rng() }), rng);
}

int16_t commonSens(std::mt19937& rng) {
    const auto depths = std::to_array<int16_t>({ 0, 1, -1, 63, -63, 127, -127, 32767, -32768, -32767, (int16_t)rng() });
    return setsOneOf(depths, rng);
}

uint8_t commonSwitch(std::mt19937& rng) { return rng() % 3 ? (uint8_t)(rng() % 2) : (uint8_t)rng(); }

void commonDraw(CommonSides& c, std::mt19937& rng, bool all) {
    setsDraw(c.s, rng, all);
    for (uint32_t p = 0; p < (uint32_t)kParts; ++p) {
        if (!all && rng() % 4) continue;
        const uint16_t words[3] = { (uint16_t)(commonSwitch(rng) | commonSwitch(rng) << 8),
                                    (uint16_t)(commonLevel(rng) | (rng() % 2 ? 0 : rng() << 8)),
                                    (uint16_t)commonSens(rng) };
        c.s.fw.cpu.write(kSetsCommonsAt + kSetsCommonBytes * p, words, sizeof words);
    }
    for (Vc2CommonWords& w : c.port.items) {
        if (!all && rng() % 4) continue;
        w.velocity = rng() % 2 ? (uint8_t)(rng() % 128) : (uint8_t)rng();
        w.level = commonLevel(rng);
        w.sens = commonSens(rng);
        w.firstSwitch = commonSwitch(rng);
        w.secondSwitch = commonSwitch(rng);
    }
}

std::string commonPlaceEvent(CommonSides& c, std::mt19937& rng) {
    const uint32_t n = rng() % 256;
    ++c.n.byEvent[0];
    return setsChildOnBoth(c.s, c.region(CommonCounts::kPool), { 0x893a1940, "the common's placement", vc2R0 },
                           { 10, n }, [&] { return c.port.addressOf(n); });
}

std::string commonMakeEvent(CommonSides& c, std::mt19937& rng) {
    const uint32_t at = setsCall(c.s, rng);
    const Vc2SetCall call = setsCallAt(c.s, at);
    const uint32_t common = c.any(rng), block = anyBlock(rng);
    ++c.n.byEvent[1];
    const Vc2CheckRoutine r { 0x893a1960, "the common's constructor", vc2R0 };
    return setsChildOnBoth(c.s, c.region(CommonCounts::kPool), r, { common, at, block }, [&] {
        c.port.construct(common, call, setsPortWords(c.s, block), c.s.parts);
        return common;
    });
}

// [FW] the events by routine; the copy and its twin 0x893a1870, the same code
const CommonEvent kEvents[] = {
    commonPlaceEvent,
    commonMakeEvent,
    commonMakeEvent,
    setsEndEvent<CommonSides, 0x893a19b0, 2>,
    setsConvertEvent<CommonSides, &Commons::convert, anyBlock, 0x893a19c0, 3>,
    setsConvertEvent<CommonSides, &Commons::convert, anyBlock, 0x893a19c0, 3>,
    setsUpdateEvent<CommonSides, &Commons::update, 0x893a19e0, 4>,
    setsCopyEvent<CommonSides, &Commons::copy, anyBlock, 0x893a1a10, 5>,
    setsCopyEvent<CommonSides, &Commons::copy, anyBlock, 0x893a1870, 6>,
};

std::span<const CommonEvent> commonEvents() { return kEvents; }

void sayCommonCounts(const CommonCounts& n) {
    const auto& e = n.byEvent;
    std::printf("common: %ld events: %ld placements, %ld constructors, %ld ends, %ld conversions, %ld updates, %ld "
                "copies and %ld of the twin\n", n.events, e[0], e[1], e[2], e[3], e[4], e[5], e[6]);
}
} // namespace

long vc2SetCommonPart(SetSides& s, const Vc2Run& run) {
    return setsChildPart<CommonSides, commonDraw, commonEvents, sayCommonCounts>(s, run, 1, kRoutines);
}
#endif
