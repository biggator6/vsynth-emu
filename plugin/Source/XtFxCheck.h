#pragma once
// xt_fx's check of XtFxChip and the control side (XT_FX_MAIN only): what its parts share.  Moved from XtFxChip.cpp as it
// stood (v204), its parts by what they check: XtFxCheckRig.cpp the chip against JE-8086's, XtFxCheckNumbers.cpp the
// firmware's numbers and the speed, XtFxCheckControl.cpp the driver against the reference, its readers and live edits,
// XtFxCheckThreads.cpp the threads and a step under a tone, XtFxCheckBehaviour.cpp what a change costs on the bus,
// XtFxCheckText.cpp the values' text, XtFxCheckUnit.cpp the unit and main.  Build: see research/tools/build.sh.
#ifdef XT_FX_MAIN
#include "Sh4Interp.h"
#include "XtArgs.h"
#include "XtFxChip.h"
#include "XtFxControl.h"
#include "XtSwitches.h"   // XT_FX_FIRST, XT_FX_STOP, XT_FX_COUNTS, XT_FX_LIVE: read through it, never getenv
#include "esp_interpreter.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <span>
#include <string>
#include <thread>

namespace xtfxcheck {
using Reference = esp::Interpreter<18>;
constexpr uint32_t kSp = 0x8C0E0000, kMfxObj = 0x89cef260, kMfxIds = 0x897a8218, kRevIds = 0x897a8310, kChoIds = 0x897a82e4, kDesc = 0x897a56d8;
extern const char* kRevName[];
extern const char* kChoName[];

struct Rng { uint32_t s; explicit Rng(uint32_t seed) : s(seed * 2654435761u + 1) {} uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; } int below(int n) { return (int)(next() % (uint32_t)n); } };

// Argument k as a whole number, `otherwise` when there's none; one that doesn't read as a number stops the run, saying so
inline int argOr(int argc, char** argv, int k, int otherwise) {
    if (argc <= k) return otherwise;
    if (const std::optional<int> v = intArg(argv[k])) return *v;
    std::fprintf(stderr, "xt_fx: \"%s\" isn't a number\n", argv[k]);
    std::exit(2);
}
// A real number at the start of a value's text (the catalog's, the firmware's strings), NaN when there's none there, so
// the check it's in fails
inline double realOf(const char* s) {
    char* end = nullptr;
    const double x = std::strtod(s, &end);
    return end != s ? x : std::nan("");
}

// The firmware's driver, with the chip's bus at its four primitives.  Each sends what the routine sends, byte for byte (read off 0x894af400, 0x894af510, 0x894af610, 0x894af6a0).
struct Rig {
    Sh4 c; XtFxChip chip; std::unique_ptr<Reference> ref; uint32_t obj = 0, vt = 0; int mfx = -1; uint64_t readsDiffer = 0; const std::map<int, int>* forced = nullptr;
    explicit Rig(const char* rom, bool withReference = true, const XtFxImage& image = xtVSynthFx()) {
        if (withReference) ref = std::make_unique<Reference>();
        if (!c.loadRom(rom)) { std::printf("cannot load %s\n", rom); std::exit(1); }
        c.mapRam(0x8C000000, 0x02000000);
        c.hook(image.answered[0], [](Sh4& m) { m.ret(0); });
        c.hook(image.answered[1], [](Sh4& m) { m.ret(0); });
        // one-off: the check's own copy of the bus's hooks: sharing XtFxControl's would hide an error in its encodings
        c.hook(image.sleep, [](Sh4& m) { m.ret(0); });
        c.hook(image.putWord, [this](Sh4& m) { put(0x34, (int16_t)m.r[4], m.r[5]); m.ret(0); });
        c.hook(image.getWord, [this](Sh4& m) { m.ret(get((int16_t)m.r[4])); });
        c.hook(image.putCoefficient, [this](Sh4& m) {
            const uint32_t v = (uint16_t)m.r[5], s = m.r[6] & 0xff;
            const uint32_t mid = (v << 1 & 0xfc) | (s & 3), top = (v >> 7 & 1) | (s << 2 & 12);
            put(0x35, (int16_t)m.r[4], (v >> 8 & 0xff) | mid << 8 | top << 16);
            m.ret(0);
        });
        c.hook(image.putOffset, [this](Sh4& m) {
            const int i = (int16_t)m.r[4];
            const uint32_t v = m.r[5], a = get(i), b = get(i + 4);
            const uint32_t top = (v >> 16 & 3) | (b >> 24 & 0xc0);
            put(0x36, i, (a >> 20 & 0xf8) | (v & 0xff) << 8 | (v >> 8 & 0xff) << 16 | top << 24);
            m.ret(0);
        });
    }
    void bus(uint32_t a, uint8_t v) { chip.hostWrite(a, v); if (ref) ref->hostWrite(a, v); }
    void put(uint8_t cmd, int i, uint32_t w) { bus(0x2003, cmd); const uint32_t a = (uint32_t)i * 4; for (uint32_t k = 0; k < 4; k++) bus(a + k, (uint8_t)(w >> (8 * k))); }
    uint32_t get(int i) {
        put(0x37, i, 0);
        uint32_t v = 0; for (uint32_t k = 0; k < 4; k++) v |= (uint32_t)chip.hostRead(k) << (8 * k);
        if (ref) { uint32_t u = 0; for (uint32_t k = 0; k < 4; k++) u |= (uint32_t)ref->hostRead(k) << (8 * k); if (u != v) readsDiffer++; }
        return v & 0x0fffffff;
    }
    uint32_t call(uint32_t fn, std::initializer_list<uint32_t> a = {}) { uint32_t r = 0; if (!c.call(fn, a, kSp, &r)) { std::printf("fault in %08x: %s\n", fn, c.error().c_str()); std::exit(1); } return r; }
    int s16(uint32_t a) { return (int16_t)c.rd16(a); }
    int pick(int id, Rng* r) {                                                  // a parameter: its default, or anything in range
        if (forced) { const auto it = forced->find(id); if (it != forced->end()) return it->second; }
        const uint32_t d = kDesc + 8 * (uint32_t)(id - 0x55d); const int def = s16(d + 2), lo = s16(d + 4), hi = s16(d + 6);
        return r && r->below(2) ? lo + r->below(hi - lo + 1) : def;
    }
    uint32_t vcall(uint32_t off) { const uint32_t d = c.rd32(vt + off), fn = c.rd32(vt + off + 8); return call(fn, { obj + d }); }
    void setMfx(int t, Rng* r, const char* over = nullptr) {
        if (mfx >= 0) vcall(0x84);
        obj = call(0x894b43a0, { kMfxObj, (uint32_t)t }); vt = c.rd32(obj + 0x54); c.wr32(obj + 0x58, (uint32_t)t); c.wr32(obj + 0xdc, 12000); mfx = t;
        vcall(0x6c);
        mfxParams(r, over);
    }
    void mfxParams(Rng* r, const char* over = nullptr) {
        const int first = s16(kMfxIds + 4 * (uint32_t)mfx), last = s16(kMfxIds + 4 * (uint32_t)mfx + 2), n = mfx ? last - first + 1 : 0;
        for (int i = 0; i < n && i < 32; i++) c.wr16(obj + 0x5c + 4 * (uint32_t)i, (uint16_t)pick(first + i, r));
        if (over) overrides(over);
        for (int k = 0; k < 100 && !vcall(0x78); k++) {}
    }
    // "i=v,i=v…": the MFX's parameter i at v, over what was drawn (the numbers' cases); a list that doesn't read stops the run
    void overrides(const char* over) {
        const char* const end = over + std::strlen(over);
        for (const char* at = over; at < end;) {
            int i = 0, v = 0;
            const auto a = std::from_chars(at, end, i);
            const auto b = a.ptr < end ? std::from_chars(a.ptr + 1, end, v) : a;
            if (a.ec != std::errc() || b.ec != std::errc() || *a.ptr != '=') {
                std::printf("xt_fx: \"%s\" isn't a list of i=v\n", over);
                std::exit(2);
            }
            c.wr16(obj + 0x5c + 4 * (uint32_t)i, (uint16_t)v);
            at = b.ptr < end && *b.ptr == ',' ? b.ptr + 1 : end;
        }
    }
    void setReverb(int t, Rng* r) {
        call(0x894d0a70, { (uint32_t)t });
        const int first = s16(kRevIds + 4 * (uint32_t)t), last = s16(kRevIds + 4 * (uint32_t)t + 2);
        for (int i = 0; i < 24; i++) call(0x894d0a80, { (uint32_t)i, (uint32_t)(i <= last - first ? pick(first + i, r) : 0) });
        call(0x894d0b00, { 0 });
    }
    void setChorus(int t, Rng* r) {
        call(0x894d3420, { (uint32_t)t });
        const int first = s16(kChoIds + 4 * (uint32_t)t);
        for (int i = 0; i < 10; i++) call(0x894d3430, { (uint32_t)i, (uint32_t)pick(first + i, r) });
        call(0x894d3470, { 0 });
    }
    void levels(Rng* r) {                                                       // what a patch and the system set: Master, Output Gain, the MFX's and returns' levels and sends
        const auto v = [&](uint32_t def) { return r ? (uint32_t)r->below(0x8000) : def; };
        call(0x894af1b0, { r ? (uint32_t)r->below(128) : 127u }); call(0x894af2d0, { r ? (uint32_t)r->below(25) - 12 : 0u });
        call(0x894afdf0, { v(0x7fff) }); call(0x894afef0, { v(0) }); call(0x894aff00, { v(0) }); call(0x894aff10, { v(0) }); call(0x894aff20, { v(0x7fff) }); call(0x894aff50, { v(0x7fff) });
        call(0x894afd50, { r ? (uint32_t)r->below(2) : 0u });
    }
    void boot(int mfxType, int revType, int choType, Rng* r, const char* over = nullptr) {
        call(0x894afc50);
        call(0x894af1b0, { 127 }); call(0x894af2d0, { 0 }); call(0x894b0020, { 2 }); call(0x894ae960); call(0x894ae9a0);
        levels(nullptr);
        call(0x894d09b0); setReverb(revType, r);
        call(0x894d3350); setChorus(choType, r);
        setMfx(mfxType, r, over);
        call(0x894aff80, { 1 }); call(0x894affa0, { 0 }); call(0x894affc0, { 1 }); call(0x894affe0, { 1 });
        call(0x894b3020); call(0x894b3070);
        call(0x894af610, { 0x27, 0x20, 0 }); call(0x894af610, { 0x34, 0x20, 0 }); call(0x894aee10, { 1 });
        // The input's routing as XtFxControl's start asks for it (v204): the EXT IN request's setters in its order (bytes
        // 2, 1, 6, 7, 5), at External Input Type's and Source's defaults (1 MONO MIX, 0 Analog), then its event's routine
        static constexpr std::pair<uint32_t, uint32_t> kRequest[] = {
            { 0x894af290, 0 }, { 0x894af1e0, 0 }, { 0x894af220, 1 }, { 0x894af240, 0 }, { 0x894af2b0, 0 } };
        for (const auto& [setter, value] : kRequest) call(setter, { value });
        call(0x894aee80);
    }
    void input(unsigned m, int32_t v) { chip.setGram(m, v); if (ref) ref->runtime().writeGram(v, (uint8_t)m); }
    void frame() { chip.runFrame(); if (ref) { ref->runProgram(768); ref->sync(); } }
};

// Noise into every input of both chips, a sample at a time: the frame's eight, the USB pair and (v204) the analog pair the
// input's routing reads, each at silence, low, mid or enough to saturate, a level held 256 samples
struct InputNoise {
    static constexpr unsigned kInputs[] = { 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x18, 0x1a, 0x10, 0x12 };
    static constexpr double kLevels[] = { 0.0, 0.002, 0.25, 2.0 };
    static constexpr int kCount = (int)(sizeof kInputs / sizeof *kInputs);
    Rng r;
    double level[kCount] = {};
    explicit InputNoise(uint32_t seed) : r(seed) {}
    void into(XtFxChip& chip, Reference& ref, long sample) {
        if (sample % 256 == 0) {
            const int w = r.below(kCount);
            level[w] = kLevels[r.below(4)];
        }
        for (int i = 0; i < kCount; i++) {
            const double v = level[i] * ((int32_t)r.next() / 2147483648.0) * 8388608.0;
            const auto x = (int32_t)std::clamp(v, -8388608.0, 8388607.0);
            chip.setGram(kInputs[i], x);
            ref.runtime().writeGram(x, (uint8_t)kInputs[i]);
        }
    }
};

// Every word of the two states, after the rotation.  Returns the first that differs.
std::string differs(const XtFxChip& x, const Reference& ref, bool memory);
inline double db(double x) { return x > 0 ? 20 * std::log10(x) : -999.0; }

// ------------------------------------------------------------------ the control side's edits
struct Edit { std::vector<std::pair<int, int>> sets; bool patch = false, matrix = false; int prm[3] = {}, clockMs = 0; };

// What the edits go by in one image: its driver, and the IDs of what they move there.
struct FxIds {
    const XtFxImage& image;
    const char* label;                // what the results' lines call this image's control side
    int mfxType, choType, revType;    // the three Type parameters
    int clockSource, tempo;
    std::span<const int> levels;      // the levels, switches and system settings an edit draws from
    std::span<const int> alone;       // what the firmware's update doesn't take alone: a new patch has them, an edit not
};
const FxIds& rom2Ids();   // XtFxCheckControl.cpp
const FxIds& vc2Ids();    // XtFxCheckVc2.cpp

// Draws edits, from the firmware's own ranges.
struct Editor {
    Sh4 rom; XtFxControl& ranges; Rng r; const FxIds& ids; int mfx = 0, cho = 0, rev = 0;
    Editor(const char* path, XtFxControl& c, uint32_t seed, const FxIds& fx = rom2Ids()) : ranges(c), r(seed), ids(fx) {
        rom.loadRom(path);
    }
    int s16(uint32_t a) { return (int16_t)rom.rd16(a); }
    int draw(int id) { const int lo = ranges.lowest(id), hi = ranges.highest(id); return lo + r.below(hi - lo + 1); }
    void params(Edit& e, uint32_t table, int type, int n) {
        const int first = s16(table + 4 * (uint32_t)type), last = s16(table + 4 * (uint32_t)type + 2);
        for (int i = 0; i < n; i++) {
            const int id = first + r.below(last - first + 1);
            e.sets.push_back({ id, draw(id) });
        }
    }
    // A change of type: the Type parameter, then three of the new type's own; `current` remembers it for later edits
    Edit typeEdit(int& current, int typeId, uint32_t table, int t) {
        current = t;
        Edit e;
        e.sets.push_back({ typeId, t });
        params(e, table, t, 3);
        return e;
    }
    Edit mfxType(int t) { return typeEdit(mfx, ids.mfxType, ids.image.mfxIds, t); }
    Edit choType(int t) { return typeEdit(cho, ids.choType, ids.image.choIds, t); }
    Edit revType(int t) { return typeEdit(rev, ids.revType, ids.image.revIds, t); }
    Edit any() {
        Edit e;
        const XtFxImage& im = ids.image;
        switch (r.below(10)) {
            case 9:
                e.matrix = true;
                for (int& v : e.prm) {
                    const int k = r.below(4);
                    v = k == 0 ? 0 : k == 1 ? r.below(128) - 64 : (int)(r.next() % 65535) - 32767;
                }
                break;
            case 0: return mfxType(r.below(im.mfxTypes));
            case 1: return choType(r.below(im.choTypes));
            case 2: return revType(r.below(im.revTypes));
            case 3: case 4: params(e, im.mfxIds, mfx, 1 + r.below(4)); break;
            case 5: params(e, im.choIds, cho, 1 + r.below(3)); break;
            case 6: params(e, im.revIds, rev, 1 + r.below(3)); break;
            case 7: {
                const int id = ids.levels[(size_t)r.below((int)ids.levels.size())];
                e.sets.push_back({ id, draw(id) });
                break;
            }
            default:
                e = mfxType(r.below(im.mfxTypes));
                e.patch = true;
                break;
        }
        return e;
    }
};
void apply(XtFxControl& c, const Edit& e);

// The audio thread must not allocate (XtFxCheckUnit.cpp counts it; XtFxCheckThreads.cpp holds the counters).
extern thread_local bool tAudioThread;
extern std::atomic<uint64_t> gAudioAllocations;

// The stream as it should be, one thread and no queue: a checksum of what's sent, and where each wait falls in it.
struct Recorder : XtFxControl::Bus {
    XtFxChip chip; uint32_t crc = 0; uint64_t n = 0; std::vector<std::pair<uint64_t, uint32_t>> waits;
    static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
    void write(uint8_t c, uint16_t i, uint32_t w) override { chip.hostWord(c, i, w); crc = mix(mix(mix(crc, c), i), w); n++; }
    uint32_t read(uint16_t i) override { write(0x37, i, 0); uint32_t v = 0; for (uint32_t k = 0; k < 4; k++) v |= (uint32_t)chip.hostRead(k) << (8 * k); return v & 0x0fffffff; }
    void wait(uint32_t s) override { waits.push_back({ n, s }); }
};

// ------------------------------------------------------------------ each part's entry, by its file
void compare(const char* rom, const char* label, int mfxType, int revType, int choType, int frames, uint32_t seed,
             bool roam);                                                                // XtFxCheckRig.cpp
void numbers(const char* rom);                                                          // XtFxCheckNumbers.cpp
void speed(const char* rom);
void againstReference(const char* rom, int randomEvents, uint32_t seed, const FxIds& fx = rom2Ids());   // XtFxCheckControl.cpp
void readers(const char* rom, uint32_t seed);
void liveEdits(const char* rom, int events, uint32_t seed, const FxIds& fx = rom2Ids());
void vc2Control(const char* rom, int events, uint32_t seed);                            // XtFxCheckVc2.cpp
void threads(const char* rom, int events, uint32_t seed);                               // XtFxCheckThreads.cpp
double stepUnderTone(const char* rom, bool honoured, bool patch);
void changeUnderTone(const char* rom);
void behaviour(const char* rom);                                                        // XtFxCheckBehaviour.cpp
void valueText(const char* rom);                                                        // XtFxCheckText.cpp
void unitConversion(const char* rom, uint32_t seed);                                    // XtFxCheckUnit.cpp
void unitLevels(const char* rom, bool print);
void unitInput(const char* rom);
void copyWithoutChip(const char* rom, uint32_t seed);                                  // XtFxCheckCopy.cpp, v205
} // namespace xtfxcheck
#endif
