// vc2run <rom.4> voices [events] [seed] [mixed|full|leak|taken]: rom.4's own note handlers under random events, with the
// COSM buffer pools counted after each (research/VC2_VOCAL_DESIGNER.md, "The part's voices and the COSM pools").  The part
// comes up by its own routines: the init table, the part's init (0x89392ab0), rom.4's default patch as the store, three
// part nodes on channel 0.  The events are what the notes' task would call: note-on (0x89392da0), note-off (0x89392e70),
// controllers (0x89392f90: the pedal, 120, 121, 123), a tone's end (0x89390ee0), stop all (0x89390f40), the voices' reserve
// (0x893911b0), the CPU budget, and Mono/Poly, Legato, Modulator Key Mode and Auto Note in the store.  Answered here: the
// wait for a truncated fade (the fade taken to 0, as the block task's render would), the tone's silent test (drawn), and
// each COSM's block code at its note-on (drawn, most of them types with buffers, so every tone wants both pools).
// After each event: no take found the pool empty, and each slot marked used is held by exactly one live tone, and each
// tone's slot is marked used.  `full` keeps the voices full (no stops, a budget nothing reaches).  Two controls show the
// check can fail, and must: `leak` loses every free, `taken` starts with every slot marked used.  Exit 0 when the run is
// clean, or when a control was caught.
#include "vc2patches.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <random>
#include <string_view>
#include <vector>

namespace vc2 {

namespace {

// [FW] the part's routines
constexpr uint32_t kPartInit = 0x89392ab0, kToneEndHook = 0x89396960, kToneEnded = 0x8998a514, kPostEnded = 0x893901a0;
constexpr uint32_t kNoteOn = 0x89392da0, kNoteOff = 0x89392e70, kController = 0x89392f90;
constexpr uint32_t kEnded = 0x89390ee0, kStopAll = 0x89390f40, kReserve = 0x893911b0;
constexpr uint32_t kEnableNode = 0x893932c0, kLinkNode = 0x89393100, kPartSettings = 0x89396e20;
constexpr uint32_t kNodes = 0x899888d0, kNodeStride = 0x100, kSettingsRows = 0x89995390, kBudget = 0x899826d8;
constexpr uint32_t kPartEntry = 0x8950d8e0;
// [FW] the init table's entries (12 bytes, the routine first); these four call the kernel's table directly
constexpr uint32_t kInitFirst = 0x898c59c4, kInitLast = 0x898c6ba0, kInitStride = 12;
constexpr std::array<uint32_t, 4> kInitSkipped = {0x8931a940, 0x8933a3f0, 0x8933ac10, 0x89340c30};
// [FW] what's hooked
constexpr uint32_t kWait = 0x893017f0, kFadeEvent = 0x1000, kFadeOfTone = 0x1a54, kSilent = 0x893b25c0;
constexpr uint32_t kCosmNoteOn = 0x893aa0e0, kCosmFree = 0x893aa060;
constexpr std::array<uint32_t, 2> kTaken = {0x893a8cea, 0x893a8dbc};   // just after each pool's search returns
constexpr uint32_t kStealing = 0x893905c0, kOverBudget = 0x89390abc, kLegatoPartner = 0x89390960;
// [FW] the voices, a note's objects and the pools
constexpr uint32_t kVoiceTable = 0x895354b0, kVoiceNote = 0x4c, kNoteTone = 0x738;
constexpr int kVoices = 24, kSlots = 24;
constexpr std::array<uint32_t, 2> kPools = {0x89999294, 0x89999474}, kCosms = {0x1728, 0x188c};
constexpr uint32_t kSlotStride = 20, kCosmSlot = 0x144, kCosmPool = 0x140;
constexpr uint32_t kFreeTones = 0x8993fb48, kToneEnd = 0x8993fb44, kToneLink = 0x2618;
// [FW] the patch's IDs the mode events write
constexpr uint32_t kMonoPoly = 0x130, kLegato = 0x131, kModulatorKeys = 0x1a2, kAutoNote = 0x19c;
// Block codes drawn at a COSM's note-on: 7, 8, 15, 16, 18 have buffers, 12 and 13 none
constexpr std::array<uint32_t, 12> kCodes = {7, 8, 15, 16, 18, 15, 16, 7, 8, 18, 12, 13};
constexpr uint64_t kSteps = 400'000'000;

enum class Mode { mixed, full, leak, taken };
struct ModeName {
    std::string_view name;
    Mode mode;
};
constexpr std::array<ModeName, 4> kModes = {
    ModeName{"mixed", Mode::mixed}, ModeName{"full", Mode::full}, ModeName{"leak", Mode::leak},
    ModeName{"taken", Mode::taken}};

struct Tally {
    long events = 0, ons = 0, offs = 0, takes = 0, frees = 0, empty = 0;
    long steals = 0, overBudget = 0, partners = 0, fades = 0, firstEmpty = -1, errors = 0;
    int mostTones = 0, mostSlots = 0;
};

using SlotCounts = std::array<std::array<int, kSlots>, 2>;

// The live tones and which slots they hold; how many tones
int holders(Machine& m, SlotCounts& held, long& errors) {
    for (auto& pool : held) pool.fill(0);
    int tones = 0;
    for (int v = 0; v < kVoices; ++v) {
        const uint32_t note = m.cpu.rd32(m.cpu.rd32(kVoiceTable + 4 * (uint32_t)v) + kVoiceNote);
        if (!note) continue;
        const uint32_t tone = m.cpu.rd32(note + kNoteTone);
        if (!tone) {
            ++errors;
            continue;
        }
        ++tones;
        for (size_t k = 0; k < kCosms.size(); ++k) {
            const auto slot = (int32_t)m.cpu.rd32(tone + kCosms[k] + kCosmSlot);
            if (slot < 0) continue;
            if (slot >= kSlots || m.cpu.rd32(tone + kCosms[k] + kCosmPool) != k) ++errors;
            else ++held[k][(size_t)slot];
        }
    }
    return tones;
}

void check(Machine& m, Tally& t, const char* what) {
    SlotCounts held;
    const int tones = holders(m, held, t.errors);
    int used = 0;
    for (size_t k = 0; k < kPools.size(); ++k)
        for (size_t s = 0; s < kSlots; ++s) {
            const bool marked = m.cpu.rd32(kPools[k] + kSlotStride * (uint32_t)s) != 0;
            used += marked;
            if (marked == (held[k][s] == 1)) continue;
            if (++t.errors <= 5)
                std::printf("  after %s, event %ld: pool %zu slot %zu marked %d, held by %d tones\n", what, t.events,
                            k, s, marked, held[k][s]);
        }
    t.mostTones = std::max(t.mostTones, tones);
    t.mostSlots = std::max(t.mostSlots, used);
}

bool bringUp(Machine& m) {
    answerKernel(m);
    for (uint32_t e = kInitFirst; e <= kInitLast; e += kInitStride) {
        const uint32_t f = m.cpu.rd32(e);
        if (std::ranges::find(kInitSkipped, f) == kInitSkipped.end() && !m.run(f, {}, nullptr, kSteps)) return false;
    }
    if (!m.run(kToneEndHook, {})) return false;
    m.cpu.wr32(kToneEnded, kPostEnded);
    if (!m.run(kPartInit, {}, nullptr, kSteps) || !loadStore(m, {})) return false;
    const uint32_t entry = m.cpu.rd32(kPartEntry);
    m.cpu.wr8(entry, 1);
    m.cpu.wr32(entry + 8, kPatchStore);
    for (uint32_t k = 0; k < 3; ++k) {
        const uint32_t node = kNodes + kNodeStride * k;
        if (!m.run(kEnableNode, {node}) || !m.run(kLinkNode, {node, k, 0})) return false;
    }
    return m.run(kPartSettings, {kSettingsRows, 0});
}

void setBudget(Machine& m, float seconds) {
    uint32_t w = 0;
    std::memcpy(&w, &seconds, sizeof w);
    m.cpu.wr32(kBudget, w);
}

struct Events {
    Machine& m;
    Tally& t;
    std::mt19937& rng;
    Mode mode;
    int& silent;
    std::vector<uint32_t> keys {};

    bool call(uint32_t f, std::initializer_list<uint32_t> args) { return m.run(f, args, nullptr, kSteps); }
    uint32_t draw(uint32_t n) { return rng() % n; }

    bool noteOn(uint32_t source) {
        const uint32_t key = draw(2) ? 48 + draw(12) : draw(128);
        keys.push_back(key);
        ++t.ons;
        return call(kNoteOn, {source, 0, key, 1 + draw(127)});
    }
    bool noteOff(uint32_t source) {
        if (keys.empty()) return true;
        const size_t i = draw((uint32_t)keys.size());
        const uint32_t key = keys[i];
        keys.erase(keys.begin() + (long)i);
        ++t.offs;
        return call(kNoteOff, {source, 0, key, 64});
    }
    bool modes() {
        const bool set = setStoreValue(m, kPatchStore, kMonoPoly, (int)draw(2)) &&
                         setStoreValue(m, kPatchStore, kLegato, (int)draw(2)) &&
                         setStoreValue(m, kPatchStore, kModulatorKeys, (int)draw(2)) &&
                         setStoreValue(m, kPatchStore, kAutoNote, (int)draw(2));
        return set && call(kPartSettings, {kSettingsRows, 0});
    }
    // One event by its share of a hundred; `what` says which
    bool one(const char*& what) {
        static constexpr std::array<uint32_t, 3> kModeMessages = {120, 121, 123};
        static constexpr std::array<float, 5> kBudgets = {1e-5f, 1e-4f, 4e-4f, 1e-3f, 1.0f};
        uint32_t r = draw(100);
        if (mode == Mode::full && r >= 76) r %= 42;
        const uint32_t source = 1 + draw(4);
        silent = draw(4) == 0;
        if (r < 42) return what = "a note-on", noteOn(source);
        if (r < 70) return what = "a note-off", noteOff(source);
        if (r < 76) return what = "the pedal", call(kController, {1 + draw(3), 0, 64, draw(2) ? 127u : 0u});
        if (r < 79) {
            keys.clear();
            return what = "a mode message", call(kController, {1 + draw(3), 0, kModeMessages[draw(3)], 0});
        }
        if (r < 90) return what = "a tone's end", call(kEnded, {});
        if (r < 92) return what = "stop all", call(kStopAll, {});
        if (r < 95) {
            const uint32_t offset = draw(24);
            return what = "the reserve", call(kReserve, {draw(25 - offset), offset});
        }
        if (r < 97) return what = "the budget", setBudget(m, kBudgets[draw(5)]), true;
        return what = "the modes", modes();
    }
};

void hookAll(Machine& m, Tally& t, std::mt19937& rng, Mode mode, const int& silent) {
    m.cpu.hook(kWait, [&t](Sh4& c) {
        if (c.r[4] == kFadeEvent) {
            ++t.fades;
            c.wr32(c.r[12] + kFadeOfTone, 0);   // r12 is the tone in 0x893b3bc0's wait
        }
        c.ret(kFadeEvent);
    });
    m.cpu.hook(kSilent, [&silent](Sh4& c) { c.ret((uint32_t)silent); });
    for (const uint32_t at : kTaken)
        m.cpu.hook(at, [&t](Sh4& c) {
            ++t.takes;
            if ((int32_t)c.r[0] >= 0) return;
            ++t.empty;
            if (t.firstEmpty < 0) t.firstEmpty = t.events;
        });
    m.cpu.hook(kCosmFree, [&t, mode](Sh4& c) {
        if (!c.r[4] || (int32_t)c.rd32(c.r[4] + kCosmSlot) < 0) return;
        ++t.frees;
        if (mode == Mode::leak) c.wr32(c.r[4] + kCosmSlot, 0xffffffffu);
    });
    m.cpu.hook(kStealing, [&t](Sh4&) { ++t.steals; });
    m.cpu.hook(kOverBudget, [&t](Sh4&) { ++t.overBudget; });
    m.cpu.hook(kLegatoPartner, [&t](Sh4&) { ++t.partners; });
    m.cpu.hook(kCosmNoteOn, [&rng](Sh4& c) { c.wr32(c.r[5], kCodes[rng() % kCodes.size()]); });
}

int freeTones(Machine& m) {
    int n = 0;
    const uint32_t end = m.cpu.rd32(kToneEnd);
    for (uint32_t tone = m.cpu.rd32(kFreeTones); tone && tone != end && n <= kVoices;
         tone = m.cpu.rd32(tone + kToneLink))
        ++n;
    return n;
}

void report(Machine& m, const Tally& t, std::string_view name, uint32_t seed) {
    std::printf("voices %.*s, seed %u: %ld events (%ld note-ons, %ld note-offs)\n", (int)name.size(), name.data(), seed,
                t.events, t.ons, t.offs);
    std::printf("  slots taken %ld, freed %ld, a pool found empty %ld times (first at event %ld)\n", t.takes, t.frees,
                t.empty, t.firstEmpty);
    std::printf("  voices stolen %ld, over the budget %ld, legato partners %ld, fades waited for %ld\n", t.steals,
                t.overBudget, t.partners, t.fades);
    std::printf("  most tones live %d of %d, most slots used %d of %d, tones on the free list %d, wrong counts %ld\n",
                t.mostTones, kVoices, t.mostSlots, 2 * kSlots, freeTones(m), t.errors);
}

} // namespace

int voices(Machine& m, int argc, char** argv) {
    const long events = argc > 1 ? intArg(argv[1]).value_or(20000) : 20000;
    const auto seed = (uint32_t)(argc > 2 ? intArg(argv[2]).value_or(2024) : 2024);
    const std::string_view name = argc > 3 ? argv[3] : "mixed";
    const auto* chosen = std::ranges::find(kModes, name, &ModeName::name);
    if (chosen == kModes.end()) {
        std::printf("voices: no mode %.*s\n", (int)name.size(), name.data());
        return 2;
    }
    const Mode mode = chosen->mode;
    const bool control = mode == Mode::leak || mode == Mode::taken;

    Tally t;
    std::mt19937 rng(seed);
    int silent = 0;
    if (!bringUp(m)) {
        std::printf("voices: the part didn't come up: %s\n", m.error().c_str());
        return 1;
    }
    hookAll(m, t, rng, mode, silent);
    if (mode == Mode::full) setBudget(m, 1.0f);
    if (mode == Mode::taken)
        for (const uint32_t pool : kPools)
            for (uint32_t s = 0; s < kSlots; ++s) m.cpu.wr32(pool + kSlotStride * s, 1);

    Events e {m, t, rng, mode, silent};
    bool ran = true;
    const char* what = "";
    for (; t.events < events && ran && !t.empty; ++t.events) {
        ran = e.one(what);
        if (ran) check(m, t, what);
    }
    report(m, t, name, seed);
    if (!ran) std::printf("  stopped by a fault at %s: %s\n", what, m.error().c_str());
    if (control) {
        const bool caught = t.empty > 0;
        std::printf("  control %s\n", caught ? "caught: the check can fail" : "NOT caught: the check proves nothing");
        return caught ? 0 : 1;
    }
    return ran && !t.empty && !t.errors ? 0 : 1;
}

} // namespace vc2
