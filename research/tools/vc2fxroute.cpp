// vc2run <rom.4> fxroute <rom.2>: where the effect chip's program A, its routing and mix, takes each input, on both
// images (research/VC2_VOCAL_DESIGNER.md, "Program A's routing").  rom.4's own effect driver on an XtFxChip
// (vc2fxdriver.h), rom.2's through XtFxUnit itself.  Then, on copies of each settled chip, an impulse into one input word
// against silence: every GRAM word that differs, the first sample it does and its peak against the impulse, marked =
// where the two images' word is the same sample for sample; and the mix out, rom.4's against rom.2's.  Over kVariants'
// settings: the defaults (MFX, Chorus and Reverb off), the system's EQ, each effect alone, the three on.
// vc2run <rom.4> fxroute <rom.2> scan: every GRAM word as the input, with the three on.
// vc2run <rom.4> fxroute <rom.2> flags: each bit of the driver's message alone, the chip's words it changes.  Fails on a
// fault.
#include "XtFxChip.h"
#include "XtFxControl.h"
#include "vc2fxdriver.h"
#include "vc2patches.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <numeric>
#include <string_view>
#include <vector>

namespace vc2 {

namespace {

// Each image's settings over its defaults, by its own IDs [FW]: the system's EQ (rom.2's 0x65…0x70, rom.4's 0x67…0x72);
// the switches (rom.2's MFX, Chorus, Reverb 0x120…0x122, rom.4's 0x13d…0x13f); the types (rom.2's MFX 0x177, Chorus
// 0x1fc, Reverb 0x230; rom.4's 0x1b7, 0x23c, 0x270) and the MFX's Chorus and Reverb sends (rom.2's 0x179, 0x17a; rom.4's
// 0x1b9, 0x1ba).  With an effect on, rom.4's EQ Hi Gain (0x70) is at rom.2's default, 13 (−2 dB, its own 15), so the
// two EQs are alike.  `scanned`: the one the scan runs.
struct Variant {
    const char* what;
    FxSettings rom2, rom4;
    bool scanned;
};
const Variant kVariants[] = {
    {"at the defaults: MFX, Chorus and Reverb off", {}, {}, false},
    {"EQ Switch off", {{0x70, 0}}, {{0x72, 0}}, false},
    {"EQ Low Gain +15 dB", {{0x66, 30}}, {{0x68, 30}}, false},
    {"rom.4's EQ Hi Gain at rom.2's default, -2 dB", {}, {{0x70, 13}}, false},
    {"rom.4's External In Type STEREO (its default MONO-L+R)", {}, {{0x7a, 0}, {0x70, 13}}, false},
    {"rom.4's External In Type MONO-R", {}, {{0x7a, 3}, {0x70, 13}}, false},
    {"rom.4's Direct Sound Level 127 (its default 0)", {}, {{0x195, 127}, {0x70, 13}}, false},
    {"rom.4's OSC 1 Type EXTIN (its default ANALOG)", {}, {{0x2e5, 2}, {0x70, 13}}, false},
    {"rom.4's OSC 1 Type VOCODER", {}, {{0x2e5, 3}, {0x70, 13}}, false},
    {"rom.4's USB External In Type STEREO (its default OFF)", {}, {{0x7f, 1}, {0x70, 13}}, false},
    {"rom.4's patch EQ 1 Gain +15 dB (its default 0)", {}, {{0x1b1, 30}, {0x70, 13}}, false},
    {"rom.4's tone's MFX Send 0, Reverb Send 127, the three on", {},
     {{0x13d, 1}, {0x13f, 1}, {0x270, 8}, {0x2e0, 0}, {0x2e2, 127}, {0x70, 13}}, false},
    {"MFX Thru alone", {{0x120, 1}, {0x177, 0}}, {{0x13d, 1}, {0x1b7, 0}, {0x70, 13}}, false},
    {"Chorus 1 alone", {{0x121, 1}, {0x1fc, 1}}, {{0x13e, 1}, {0x23c, 1}, {0x70, 13}}, false},
    {"Reverb Room 1 alone", {{0x122, 1}, {0x230, 1}}, {{0x13f, 1}, {0x270, 8}, {0x70, 13}}, false},
    {"Reverb Plate (rom.2), Stereo Plate (rom.4) alone", {{0x122, 1}, {0x230, 8}}, {{0x13f, 1}, {0x270, 1}, {0x70, 13}},
     false},
    {"MFX Thru, Chorus 1 and Reverb Room 1 on, the MFX's sends full",
     {{0x120, 1}, {0x121, 1}, {0x122, 1}, {0x177, 0}, {0x179, 127}, {0x17a, 127}, {0x1fc, 1}, {0x230, 1}},
     {{0x13d, 1}, {0x13e, 1}, {0x13f, 1}, {0x1b7, 0}, {0x1b9, 127}, {0x1ba, 127}, {0x23c, 1}, {0x270, 8}, {0x70, 13}},
     true},
};

constexpr unsigned kGramWords = 256;
constexpr int kFrames = 8192, kScanFrames = 2048;

// What an impulse does to one GRAM word: the first sample it differs, its largest difference, and a hash of every
// difference and its sample, to tell two images' words alike
struct GramReach {
    int first = -1;
    int32_t peak = 0;
    uint64_t hash = 0xcbf29ce484222325ull;
};
constexpr uint64_t kHashPrime = 0x100000001b3ull;
using Reaches = std::array<GramReach, kGramWords>;

// What an impulse into `in` changes, word by word, on a copy of `chip` against a copy given silence.  A word is read as
// written that sample (one on after the sample); the impulse's own word, as it rotates through GRAM, is left out.
Reaches reachOf(const XtFxChip& chip, unsigned in, int frames) {
    auto a = std::make_unique<XtFxChip>(chip), b = std::make_unique<XtFxChip>(chip);
    Reaches r{};
    for (int t = 0; t < frames; ++t) {
        silence(*a);
        silence(*b);
        a->setGram(in, t == 0 ? kImpulse : 0);
        a->runFrame();
        b->runFrame();
        for (unsigned w = 0; w < kGramWords; ++w) {
            const int32_t d = a->gram(w + 1) - b->gram(w + 1);
            if (d == 0 || (t < (int)kGramWords && ((w - in) & (kGramWords - 1)) == (unsigned)t)) continue;
            if (r[w].first < 0) r[w].first = t;
            if (std::abs(d) > std::abs(r[w].peak)) r[w].peak = d;
            r[w].hash = (r[w].hash ^ ((uint64_t)t << 32 | (uint32_t)d)) * kHashPrime;
        }
    }
    return r;
}

// [FW] the mix out, left and right (HARDWARE_FIRMWARE.md, "The chip in the sound")
constexpr unsigned kMixL = 0xa0, kMixR = 0xa2;

// The two images' mix out against each other: how many samples differ, and the largest difference against the impulse
void sayMixAgainst(const XtFxChip& rom2, const XtFxChip& rom4, unsigned in, int frames) {
    std::printf("    the mix, rom.4 against rom.2:");
    const auto a = tracesOf(rom2, in, {kMixL, kMixR}, frames), b = tracesOf(rom4, in, {kMixL, kMixR}, frames);
    for (size_t side = 0; side < a.size(); ++side) {
        long differ = 0;
        int32_t most = 0;
        for (size_t t = 0; t < a[side].size(); ++t) {
            differ += a[side][t] != b[side][t] ? 1 : 0;
            most = std::max(most, std::abs(a[side][t] - b[side][t]));
        }
        const double db = most ? 20 * std::log10((double)most / kImpulse) : 0;
        std::printf(" %02x: %ld samples differ, the most %+.1f dB;", side ? kMixR : kMixL, differ, db);
    }
    std::printf("\n");
}

// One image's reach; a word marked = is, sample for sample, `other`'s same word
void sayReaches(const char* image, const Reaches& r, const Reaches& other) {
    std::printf("    %s:", image);
    for (unsigned w = 0; w < kGramWords; ++w) {
        if (r[w].first < 0) continue;
        const double db = 20 * std::log10(std::abs((double)r[w].peak) / kImpulse);
        std::printf(" %02x@%d %+.1f%s", w, r[w].first, db, r[w].hash == other[w].hash ? "=" : "");
    }
    std::printf("\n");
}

// Each input word's reach on both images; an input that reaches nothing on either is left out
void sayRoutes(const char* what, const XtFxChip& rom2, const XtFxChip& rom4, const std::vector<unsigned>& inputs,
               int frames) {
    std::printf("%s (word@first sample, peak dB against the impulse; %d samples)\n", what, frames);
    const auto quiet2 = settled(rom2), quiet4 = settled(rom4);
    for (const unsigned in : inputs) {
        const Reaches r2 = reachOf(*quiet2, in, frames), r4 = reachOf(*quiet4, in, frames);
        const auto none = [](const Reaches& r) { return std::ranges::all_of(r, [](auto x) { return x.first < 0; }); };
        if (none(r2) && none(r4)) continue;
        std::printf("  in %02x\n", in);
        sayReaches("rom.2", r2, r4);
        sayReaches("rom.4", r4, r2);
        sayMixAgainst(*quiet2, *quiet4, in, frames);
    }
}

// rom.2's effects with `s` over its defaults, through XtFxUnit, settled; null on a fault, said
std::unique_ptr<XtFxUnit> rom2With(const char* path, const FxSettings& s) {
    auto u = std::make_unique<XtFxUnit>(path);
    for (const auto& [id, v] : s) u->control().set(id, v);
    u->control().loadPatch();
    u->settle();
    if (u->ok()) return u;
    std::printf("fxroute: rom.2's driver: %s\n", u->control().error().c_str());
    return nullptr;
}

// The IDs rom.4's driver has read, by name; the typed ones (the second bank's) only counted
void sayHeard(Machine& m, const std::set<int>& heard) {
    std::printf("rom.4's driver read, at its start:");
    long typed = 0;
    for (const int id : heard) {
        if (id >= (int)kFirstTyped) ++typed;
        if (id < (int)kFirstTyped) std::printf(" %03x%s,", id, paramName(m, (uint32_t)id).c_str());
    }
    std::printf(" and %ld typed IDs\n", typed);
}

// Each of the message's 32 bits alone, from the start on, with the chip's words cleared before it: the words it writes
// (left not 0), first…last and how many
void sayFlags(Machine& m, XtFxChip& chip) {
    constexpr unsigned kChipWords = 2048, kBits = 32;
    std::printf("each message bit alone, the chip's words cleared first: the words it writes (core 1's from 400)\n");
    for (unsigned bit = kBits; bit-- > 0;) {
        for (unsigned i = 0; i < kChipWords; ++i) chip.hostWord(0x34, i, 0);
        const bool ok = fxMessage(m, 1u << bit);
        std::vector<unsigned> changed;
        for (unsigned i = 0; i < kChipWords; ++i)
            if ((chip.word(i) & 0x0fffffff) != 0) changed.push_back(i);
        std::printf("  byte %u, %02x: %zu words", 3 - bit / 8, 1u << (bit % 8), changed.size());
        if (!changed.empty()) std::printf(", %03x…%03x", changed.front(), changed.back());
        std::printf("%s\n", ok ? "" : (", a fault: " + m.error()).c_str());
    }
}

} // namespace

int fxroute(Machine& m, int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "%s <rom.2> [scan|flags]\n", argv[0]);
        return 2;
    }
    const bool scan = argc > 2 && std::string_view(argv[2]) == "scan";
    std::vector<unsigned> inputs(std::begin(kOutsideWords), std::end(kOutsideWords));
    if (scan) inputs.resize(kGramWords);
    if (scan) std::iota(inputs.begin(), inputs.end(), 0u);
    FxSettings answers;
    std::set<int> heard;
    auto chip = std::make_unique<XtFxChip>();
    hookFxDriver(m, *chip, answers, heard);
    bool ok = startFxDriver(m);
    sayHeard(m, heard);
    if (ok && argc > 2 && std::string_view(argv[2]) == "flags") {
        sayFlags(m, *chip);
        return 0;
    }
    for (const Variant& v : kVariants) {
        if (!ok || (scan && !v.scanned)) continue;
        const auto rom2 = rom2With(argv[1], v.rom2);
        answers = v.rom4;
        ok = rom2 && reloadFx(m);
        if (ok) sayRoutes(v.what, rom2->chip(), *chip, inputs, scan ? kScanFrames : kFrames);
    }
    if (ok) return 0;
    std::printf("fxroute: a fault: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
