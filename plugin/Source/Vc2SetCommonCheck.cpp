// xt_vc2sets' second part (v281): C's common, Vc2SetCommons, against rom.4's own routines on its own commons
// (0x89996f68) and notes W, after the sets' part on the seed after its, nothing hooked (the sets' part's hooks on the
// common's routines taken off: they call nothing).  The voices, the parts and the parts' three words the common takes
// (0x89997090) are drawn in rom.4's memory, which the port reads too.  Each case noise in the commons, then 400 events:
// the placement, the constructor, the end, the conversion, the update, the copy and its twin 0x893a1870; before each
// the sets' draws, the parts' words (the level and the depth at their edges half the time) and now and then the
// commons' words; after each the commons, W and the answer.  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetCommon.h"
#include "Vc2SetsCheck.h"

namespace {
using Commons = Vc2SetCommons;
constexpr int kSteps = 400, kParts = 16;
constexpr uint32_t kEnded = 3;   // [FW] what an end is given beside its object
// [FW] the common's routines, which the sets' part hooks
constexpr uint32_t kRoutines[] = { 0x893a1940, 0x893a1960, 0x893a19b0, 0x893a19c0, 0x893a19e0, 0x893a1a10 };

struct CommonCounts {
    long events = 0;
    std::array<long, 7> byEvent {};
};

struct CommonSides {
    explicit CommonSides(SetSides& sides) : s(sides) {}
    SetSides& s;
    Commons port {};
    CommonCounts n {};
};

using CommonEvent = std::string (*)(CommonSides&, std::mt19937&);

uint32_t anyCommon(std::mt19937& rng) { return Commons::kAt + Commons::kBytes * (uint32_t)(rng() % Commons::kCount); }

uint32_t anyBlock(std::mt19937& rng) { return setsNote(rng) + Vc2ToneInput::kOscSet; }

// Where a block of rom.4's memory is in the port's notes
uint32_t* portBlock(CommonSides& c, uint32_t block) {
    auto* notes = reinterpret_cast<uint8_t*>(c.s.notes.notes.data());
    return reinterpret_cast<uint32_t*>(notes + (block - Vc2Notes::kNotesAt));
}

// A level as the square meets it (0, 1 alone, the top, past it) or any; a depth at its edges or any
uint8_t commonLevel(std::mt19937& rng) {
    const uint8_t levels[9] = { 0, 1, 2, 64, 126, 127, 128, 255, (uint8_t)rng() };
    return levels[rng() % 9];
}

int16_t commonSens(std::mt19937& rng) {
    const int16_t depths[11] = { 0, 1, -1, 63, -63, 127, -127, 32767, -32768, -32767, (int16_t)rng() };
    return depths[rng() % 11];
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

std::string commonOnBoth(CommonSides& c, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                         const std::function<uint32_t()>& mine) {
    Vc2Fw& fw = c.s.fw;
    fw.cpu.write(Commons::kAt, &c.port.items, sizeof c.port.items);
    fw.cpu.write(Vc2Notes::kNotesAt, &c.s.notes.notes, sizeof c.s.notes.notes);
    const std::string what = vc2BothAnswer(fw, r, args, mine, [&] {
        return vc2RegionsDifference(fw, { { Commons::kAt, &c.port.items, sizeof c.port.items, "the commons" },
                                          { Vc2Notes::kNotesAt, &c.s.notes.notes, sizeof c.s.notes.notes, "W" } });
    });
    return what.empty() ? what : std::string(r.name) + ": " + what;
}

// The call struct as rom.4 is given it and the port
Vc2SetCall commonCall(CommonSides& c, std::mt19937& rng, uint32_t& at) {
    at = setsCall(c.s, rng);
    Vc2SetCall call;
    c.s.fw.cpu.read(at, &call, sizeof call);
    return call;
}

std::string commonPlaceEvent(CommonSides& c, std::mt19937& rng) {
    const uint32_t n = rng() % 256;
    ++c.n.byEvent[0];
    return commonOnBoth(c, { 0x893a1940, "the common's placement", vc2R0 }, { 10, n },
                        [&] { return c.port.addressOf(n); });
}

std::string commonMakeEvent(CommonSides& c, std::mt19937& rng) {
    uint32_t at = 0;
    const Vc2SetCall call = commonCall(c, rng, at);
    const uint32_t common = anyCommon(rng), block = anyBlock(rng);
    ++c.n.byEvent[1];
    return commonOnBoth(c, { 0x893a1960, "the common's constructor", vc2R0 }, { common, at, block }, [&] {
        c.port.construct(common, call, portBlock(c, block), c.s.parts);
        return common;
    });
}

std::string commonEndEvent(CommonSides& c, std::mt19937& rng) {
    const uint32_t common = anyCommon(rng);
    ++c.n.byEvent[2];
    return commonOnBoth(c, { 0x893a19b0, "the common's end" }, { common, kEnded }, [] { return 0u; });
}

std::string commonConvertEvent(CommonSides& c, std::mt19937& rng) {
    uint32_t at = 0;
    const Vc2SetCall call = commonCall(c, rng, at);
    const uint32_t common = anyCommon(rng), block = anyBlock(rng);
    ++c.n.byEvent[3];
    return commonOnBoth(c, { 0x893a19c0, "the common's conversion" }, { common, at, block }, [&] {
        c.port.convert(common, call, portBlock(c, block), c.s.parts);
        return 0u;
    });
}

std::string commonUpdateEvent(CommonSides& c, std::mt19937& rng) {
    uint32_t at = 0;
    const Vc2SetCall call = commonCall(c, rng, at);
    const uint32_t common = anyCommon(rng);
    ++c.n.byEvent[4];
    return commonOnBoth(c, { 0x893a19e0, "the common's update" }, { common, at }, [&] {
        c.port.update(common, call, c.s.parts);
        return 0u;
    });
}

// The copy and its twin, the same code
template <size_t I>
std::string commonCopyEvent(CommonSides& c, std::mt19937& rng) {
    const uint32_t common = anyCommon(rng), block = anyBlock(rng);
    ++c.n.byEvent[5 + I];
    const Vc2CheckRoutine r { I == 0 ? 0x893a1a10u : 0x893a1870u, I == 0 ? "the common's copy" : "its twin" };
    return commonOnBoth(c, r, { common, block }, [&] {
        c.port.copy(common, portBlock(c, block));
        return 0u;
    });
}

const CommonEvent kEvents[] = {
    commonPlaceEvent, commonMakeEvent, commonMakeEvent, commonEndEvent, commonConvertEvent, commonConvertEvent,
    commonUpdateEvent, commonCopyEvent<0>, commonCopyEvent<1>,
};

std::string commonCase(CommonSides& c, std::mt19937& rng) {
    setsNoise(c.port.items, rng);
    std::string what;
    for (int k = 0; k < kSteps && what.empty(); ++k, ++c.n.events) {
        commonDraw(c, rng, k == 0);
        what = kEvents[rng() % std::size(kEvents)](c, rng);
    }
    return what;
}

void sayCommonCounts(const CommonCounts& n) {
    const auto& e = n.byEvent;
    std::printf("common: %ld events: %ld placements, %ld constructors, %ld ends, %ld conversions, %ld updates, %ld "
                "copies and %ld of the twin\n", n.events, e[0], e[1], e[2], e[3], e[4], e[5], e[6]);
}
} // namespace

long vc2SetCommonPart(SetSides& s, const Vc2Run& run) {
    for (const uint32_t at : kRoutines) s.fw.cpu.unhook(at);
    const auto c = std::make_unique<CommonSides>(s);
    const long different = vc2CheckPart(run, 1, { "XT_VC2SETS_FIRST", "XT_VC2SETS_STOP" }, c->n.events,
                                        [&](std::mt19937& rng) { return commonCase(*c, rng); });
    sayCommonCounts(c->n);
    return different;
}
#endif
