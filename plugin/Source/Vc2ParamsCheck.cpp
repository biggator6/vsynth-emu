// xt_vc2params <rom.4> [cases] [seed] (v199): Vc2Params, rom.4's parameter tables, against rom.4's own routines in the
// interpreter: each name against the names' getter; each first-bank descriptor by writing a value into a store of
// random bits where it says, read back by the store's own reader; each second-bank one through the typed reader, from
// its slot; each value's text against the routine's wrapper that adds the unit, as the screens call it.  Every ID's
// lowest, highest and default value, and `cases` more drawn in its range (default 2, seed 2024); a store's values
// also one past each end.  A line a section, with its first differences under it.  XT_VC2PARAMS_MAIN only.
#ifdef XT_VC2PARAMS_MAIN
#include "Vc2Params.h"
#include "Sh4Interp.h"
#include "XtFxValueText.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kNameOf = 0x894f9fc0;         // [FW] (id) → its name, from the names table, both banks
constexpr uint32_t kReadValue = 0x8932b050;      // [FW] (a section's place, id) → the value in its bits
constexpr uint32_t kReadTyped = 0x8932b460;      // [FW] (a section's place, slot base, id) → a typed ID's, from its slot
constexpr uint32_t kTextWithUnit = 0x894fa730;   // [FW] (id, value) → the text and its unit, as 0x89505b70 calls it
constexpr uint32_t kText = 0x894fa070;           // [FW] (id, value, 0) → the text alone, to know where it faults
constexpr uint32_t kSp = 0x8C0E0000, kSection = 0x8C020000, kSectionBytes = 0x40000;
constexpr uint64_t kSteps = 2000000;
constexpr long kMostShown = 5;
constexpr int kLongestText = 64, kWidest = 24;
constexpr uint32_t kRound = 8;   // bytes each side of a field made random again before it's written

// [FW] the blocks the typed IDs are in, each its first typed ID and its slot base (VC2_VOCAL_DESIGNER.md, "The second
// descriptor bank, read"); an ID in no block's type table is read as the block before it reads its slots
struct Block {
    int first, slotBase;
};
constexpr Block kBlocks[] = { { 0x56a, 0x1bb }, { 0x7ce, 0x23f }, { 0x819, 0x272 }, { 0x926, 0x464 } };

struct Tally {
    const char* what;
    long ids = 0, values = 0, different = 0;
    void differs(const std::string& line) {
        if (different++ < kMostShown) std::printf("  %s\n", line.c_str());
    }
    long say(const std::string& how) const {
        std::printf("%s: %ld IDs, %ld values %s, %ld different\n", what, ids, values, how.c_str(), different);
        return different;
    }
};

struct Fw {
    Sh4 cpu;
    std::mt19937 rng;
    bool call(uint32_t f, std::initializer_list<uint32_t> args, uint32_t& r0) {
        return cpu.call(f, args, kSp, &r0, kSteps);
    }
    std::string at(uint32_t a) {
        std::string t;
        for (char ch = (char)cpu.rd8(a); ch && (int)t.size() < kLongestText; ch = (char)cpu.rd8(++a)) t += ch;
        return t;
    }
    // A store section of random bits, so a field read from the wrong place reads something else: the whole of it once,
    // then the bytes round each field before it's written
    void scramble(uint32_t from = 0, uint32_t bytes = kSectionBytes) {
        std::vector<uint8_t> random(bytes);
        for (uint8_t& b : random) b = (uint8_t)rng();
        cpu.write(kSection + from, random.data(), random.size());
    }
    void scrambleRound(const XtParamDesc& d) {
        const uint32_t at = (uint32_t)d.offset, from = at > kRound ? at - kRound : 0;
        scramble(from, 2 * kRound);
    }
    // `raw` into its field as the descriptor places it: big-endian bits from `bit` on, the rest kept
    void put(const XtParamDesc& d, uint32_t raw) {
        const int width = std::abs(d.width), total = width + d.bit, bytes = (total + 7) / 8;
        const uint32_t at = kSection + (uint32_t)d.offset, shift = (uint32_t)(bytes * 8 - total);
        uint8_t b[4] = {};
        cpu.read(at, b, (size_t)bytes);
        uint32_t v = 0;
        for (int k = 0; k < bytes; ++k) v = v << 8 | b[k];
        const uint32_t mask = ((1u << width) - 1) << shift;
        v = (v & ~mask) | ((raw << shift) & mask);
        for (int k = bytes - 1; k >= 0; --k, v >>= 8) b[k] = (uint8_t)v;
        cpu.write(at, b, (size_t)bytes);
    }
};

std::string trimmed(const std::string& t) {
    const size_t from = t.find_first_not_of(' '), to = t.find_last_not_of(' ');
    return from == std::string::npos ? std::string() : t.substr(from, to - from + 1);
}

bool bareNumber(const std::string& t) {
    const size_t from = !t.empty() && (t[0] == '-' || t[0] == '+') ? 1 : 0;
    return t.size() > from && t.find_first_not_of("0123456789.", from) == std::string::npos;
}

// The argument at `at` as a number; `otherwise` when it's missing or isn't one
int argument(int argc, char** argv, int at, int otherwise) {
    int x = 0;
    if (argc <= at) return otherwise;
    const bool number = std::from_chars(argv[at], argv[at] + std::strlen(argv[at]), x).ec == std::errc();
    return number ? x : otherwise;
}

// What the store's reader makes of a stored value: the default if it's outside the range, but for a range of one value
// (a dummy's, 442 of them), which reads as it is stored: rom.2's reader's rule too (xtPatchValue, XtPatchStore.cpp)
int inRange(const XtParamDesc& d, int raw) {
    return d.min != d.max && (raw < d.min || raw > d.max) ? d.def : raw;
}

// What to try in an ID's range: its ends, its default and `cases` drawn between
std::vector<int> valuesOf(Fw& fw, int lo, int hi, int def, int cases) {
    std::vector<int> v { lo, hi, def };
    std::uniform_int_distribution<int> any(lo, hi);
    for (int k = 0; k < cases; ++k) v.push_back(any(fw.rng));
    return v;
}

long names(Fw& fw) {
    Tally t { "names" };
    for (int id = 0; id < kVc2Ids; ++id, ++t.ids, ++t.values) {
        uint32_t p = 0;
        std::string want = fw.call(kNameOf, { (uint32_t)id }, p) ? fw.at(p) : "?";
        want = trimmed(want.substr(0, want.find(" *")));   // up to a footnote's mark
        if (vc2ParamName(id) != want) t.differs(std::to_string(id) + " reads " + vc2ParamName(id) + ", not " + want);
    }
    if (!vc2ParamName(kVc2Ids).empty()) t.differs("an ID past the table has a name");
    return t.say("against the getter 0x894f9fc0");
}

long firstBank(Fw& fw, int cases) {
    Tally t { "first bank" };
    for (int id = 0; id < kVc2FirstTyped; ++id) {
        const XtParamDesc* d = vc2ParamDesc(id);
        if (!d || vc2TypedDesc(id)) t.differs(std::to_string(id) + " should be in the first bank alone");
        if (!d || d->width == 0 || std::abs(d->width) > kWidest) continue;
        ++t.ids;
        std::vector<int> raws = valuesOf(fw, d->min, d->max, d->def, cases);
        raws.push_back(d->min - 1);
        raws.push_back(d->max + 1);
        for (const int raw : raws) {
            if (raw < 0 || raw >= 1 << std::abs(d->width)) continue;
            ++t.values;
            fw.scrambleRound(*d);
            fw.put(*d, (uint32_t)raw);
            uint32_t got = 0;
            const int want = inRange(*d, raw) - d->centre;
            if (!fw.call(kReadValue, { kSection, (uint32_t)id }, got) || (int)got != want)
                t.differs(std::to_string(id) + " stored " + std::to_string(raw) + " reads " + std::to_string((int)got)
                          + ", the descriptor says " + std::to_string(want));
        }
    }
    return t.say("through the reader 0x8932b050");
}

long secondBank(Fw& fw, int cases) {
    Tally t { "second bank" };
    for (int id = kVc2FirstTyped; id < kVc2Ids; ++id) {
        const XtTypedDesc* d = vc2TypedDesc(id);
        if (!d || vc2ParamDesc(id)) t.differs(std::to_string(id) + " should be in the second bank alone");
        const auto block = std::find_if(std::rbegin(kBlocks), std::rend(kBlocks), [id](const Block& b) {
            return b.first <= id;
        });
        const XtParamDesc* slot = d ? vc2ParamDesc(block->slotBase + d->slot) : nullptr;
        if (!d || d->min > d->max || !slot || slot->width == 0) continue;
        ++t.ids;
        std::vector<int> values = valuesOf(fw, d->min, d->max, d->def, cases);
        values.push_back(d->min - 1);
        values.push_back(d->max + 1);
        for (const int v : values) {
            const int raw = v + slot->centre;
            if (raw < 0 || raw >= 1 << std::abs(slot->width)) continue;
            ++t.values;
            fw.scrambleRound(*slot);
            fw.put(*slot, (uint32_t)raw);
            const int inSlot = inRange(*slot, raw) - slot->centre;
            const int want = inSlot < d->min || inSlot > d->max ? d->def : inSlot;
            uint32_t got = 0;
            if (!fw.call(kReadTyped, { kSection, (uint32_t)block->slotBase, (uint32_t)id }, got) || (int)got != want)
                t.differs(std::to_string(id) + " slot value " + std::to_string(v) + " reads " + std::to_string((int)got)
                          + ", the descriptors say " + std::to_string(want));
        }
    }
    return t.say("through the typed reader 0x8932b460");
}

// What the text's check counted besides: the values the routine faults on, and the note values
struct TextKinds {
    long faults = 0, notes = 0;
};

// The port's text for a value, by the wrapper alone: its text, and the unit it adds (in brackets) only after a bare
// number, as the XT has the unit in a label beside the box; a note value by its LCD glyph, as rom.2's (xtNoteText); a
// value the routine faults on reads as its number
std::string wantedText(Fw& fw, int id, int v, TextKinds& kinds) {
    uint32_t p = 0;
    if (!fw.call(kText, { (uint32_t)id, (uint32_t)v, 0 }, p)) {
        ++kinds.faults;
        return std::to_string(v);
    }
    if (!fw.call(kTextWithUnit, { (uint32_t)id, (uint32_t)v }, p)) return "(the wrapper faults)";
    const std::string with = fw.at(p);
    if (const std::string note = xtNoteText(with); !note.empty()) {
        ++kinds.notes;
        return note;
    }
    const size_t open = with.rfind('[');
    if (with.empty() || with.back() != ']' || open == std::string::npos) return trimmed(with);
    const std::string head = trimmed(with.substr(0, open)), unit = with.substr(open + 1, with.size() - open - 2);
    return bareNumber(head) && !unit.empty() ? head + " " + unit : head;
}

long texts(Fw& fw, int cases) {
    Tally t { "text" };
    TextKinds kinds;
    for (int id = 0; id < kVc2Ids; ++id) {
        const XtParamDesc* d = vc2ParamDesc(id);
        const XtTypedDesc* typed = vc2TypedDesc(id);
        const bool has = d ? d->width != 0 && d->max > d->min : typed && typed->max > typed->min;
        if (!has) continue;
        ++t.ids;
        const int centre = d ? d->centre : 0;
        const std::vector<int> values = d ? valuesOf(fw, d->min - centre, d->max - centre, d->def - centre, cases)
                                          : valuesOf(fw, typed->min, typed->max, typed->def, cases);
        for (const int v : values) {
            ++t.values;
            const std::string want = wantedText(fw, id, v, kinds), got = vc2ValueText(id, v);
            if (got != want)
                t.differs(std::to_string(id) + " at " + std::to_string(v) + " reads " + got + ", not " + want);
        }
    }
    return t.say("against the wrapper 0x894fa730 (" + std::to_string(kinds.faults) + " the routine faults on, "
          + std::to_string(kinds.notes) + " note values)");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "xt_vc2params <rom.4> [cases] [seed]\n");
        return 2;
    }
    const int cases = argument(argc, argv, 2, 2);
    const bool none = vc2ParamName(0).empty() && !vc2ParamDesc(0) && !vc2TypedDesc(kVc2FirstTyped)
                      && vc2ValueText(0, 0).empty();
    std::printf("before rom.4's tables are in: %s\n", none ? "nothing answered" : "ANSWERED");
    Fw fw;
    fw.rng.seed((unsigned)argument(argc, argv, 3, 2024));
    if (!vc2LoadParams(argv[1]) || !fw.cpu.loadRom(argv[1])) {
        std::printf("xt_vc2params: rom.4's tables won't load from %s\n", argv[1]);
        return 1;
    }
    fw.cpu.mapRam(0x8C000000, 0x00100000);
    fw.scramble();
    const long different = names(fw) + firstBank(fw, cases) + secondBank(fw, cases) + texts(fw, cases) + (none ? 0 : 1);
    std::printf("xt_vc2params: %ld different\n", different);
    return different ? 1 : 0;
}
#endif
