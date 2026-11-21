// xt_fx: the unit (XtFxUnit, the frame's way through the chip), the audio thread's allocations and main (XtFxCheck.h;
// moved from XtFxChip.cpp as it stood, v204; the frame's parts by an image's way from v274, XtFxCheckUnit.h).
// XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckUnit.h"

namespace xtfxcheck {
// ------------------------------------------------------------------ the unit
const UnitWay& rom2Way() {
    static const UnitWay way = { rom2Ids(), "the", 0x894db2b0, 0x89d2cb10, 0x89cf7a78, 0.0625f, 0x70, 0x3e, 0.125,
                                 true };
    return way;
}
const UnitWay& vc2Way() {
    static const UnitWay way = { vc2Ids(), "vc2's", 0x89396200, 0x89998e20, 0x8998a080, 1.0f, 0x72, 0x40, 2.0, false };
    return way;
}

Truncation::Truncation(const char* rom, const UnitWay& w) : way(w) {
    if (c.loadRom(rom)) c.mapRam(0x8C000000, 0x100000);
}

bool Truncation::run(const float* in, int32_t* out) {
    for (uint32_t i = 0; i < kWords; i++) {
        uint32_t b;
        std::memcpy(&b, &in[i], 4);
        c.wr32(way.floats + 4 * i, b);
    }
    if (!c.call(way.truncate, {}, kSp)) return false;
    for (uint32_t i = 0; i < kWords; i++) out[i] = (int32_t)c.rd32(way.words + 4 * i);
    return true;
}

// The frame's way in, against the image's own: its routine truncates the bus to int32, and the chip has the top 24 bits.
// The frame's gain isn't in it: the image's bus is its frame as it goes in.
void unitConversion(const char* rom, uint32_t seed, const UnitWay& way) {
    // everything a sum of tones can be: tiny, around a word's step, ordinary, around full scale, and then past it
    static constexpr float kScales[] = { 1e-6f, 1024.0f, 1e6f, 2147483648.0f, 1e12f };
    constexpr int kKinds = (int)std::size(kScales) + 1;
    Truncation fw(rom, way);
    Rng r(seed); long tried = 0, different = 0; float firstX = 0; int32_t firstFw = 0, firstPort = 0;
    for (int round = 0; round < 200; round++) {
        float in[Truncation::kWords];
        int32_t words[Truncation::kWords];
        for (float& x : in) {
            const int k = r.below(kKinds);
            const float u = (float)(int32_t)r.next() / 2147483648.0f;
            const auto past = [&] {
                return (u < 0 ? -2147483648.0f : 2147483648.0f) * (1.0f + (float)r.below(3) * 1e-6f - 1e-6f);
            };
            x = k < kKinds - 1 ? u * kScales[k] : past();
        }
        if (!fw.run(in, words)) {
            std::printf("fault in the truncation: %s\n", fw.c.error().c_str());
            different++;
            break;
        }
        for (int i = 0; i < Truncation::kWords; i++) {
            const int32_t chipWord = words[i] >> 8, port = XtFxUnit::toChip(in[i]);
            tried++;
            if (chipWord == port) continue;
            if (!different) {
                firstX = in[i];
                firstFw = chipWord;
                firstPort = port;
            }
            different++;
        }
    }
    if (different && xtSwitch("XT_FX_FIRST"))
        std::printf("  unit: %.9g to the chip: firmware %d, port %d\n", firstX, firstFw, firstPort);
    std::printf("%s frame into the chip against the firmware's truncation: %ld samples, %ld different\n", way.whose,
                tried, different);
}

// A quarter of full scale at 1 kHz, sample i, in the bus's units
float quarterSine(int i) { return (float)(0.25 * std::sin(2 * std::numbers::pi * 1000 * i / 44100.0)) * 0x1p31f; }

// Through the unit: each word of the frame to its place, at the image's level (rom.2's the level the plugin has without
// the chip, ⅛, −18.06 dB), and how long it takes.
struct UnitLine { double left = 0, right = 0; int delay = -1; };
UnitLine unitLine(const char* rom, int word, bool eq, const UnitWay& way) {
    auto u = std::make_unique<XtFxUnit>(rom, way.fx.image);
    u->control().set(way.eqSwitch, eq ? 1 : 0);
    u->control().update();
    u->settle();
    float frame[8] = {}, lr[2] = {};
    UnitLine out; double in = 0;
    for (int i = 0; i < 20000; i++) {
        const float v = quarterSine(i);
        frame[word] = v; u->process(frame, lr, 1);
        if (i <= 10000) continue;
        in = std::max(in, std::fabs((double)v) * 0x1p-31);
        out.left = std::max(out.left, std::fabs((double)lr[0]));
        out.right = std::max(out.right, std::fabs((double)lr[1]));
    }
    // the delay: a click, in a chip that's settled.  The DC cut's tail is much smaller.
    frame[word] = 0; for (int i = 0; i < 2000; i++) u->process(frame, lr, 1);
    for (int i = 0; i < 64; i++) {
        frame[word] = i == 0 ? 0x1p30f : 0.0f;
        u->process(frame, lr, 1);
        if (out.delay < 0 && (std::fabs(lr[0]) > 0.0125f || std::fabs(lr[1]) > 0.0125f)) out.delay = i;
    }
    out.left /= in; out.right /= in;
    return out;
}
void unitLevels(const char* rom, bool print, const UnitWay& way) {
    int tried = 0, different = 0;
    static const char* const kName[8] = { "MAIN L", "MAIN R", "Reverb send", "Chorus send", "MULTI L", "MULTI R",
                                          "DIR L", "DIR R" };
    const auto near = [](double x, double to, double tol) { return std::fabs(x - to) <= tol; };
    for (int w = 0; w < 8; w++) {
        const UnitLine a = unitLine(rom, w, false, way);
        const bool isLeft = w == 0 || w == 4 || w == 6, silent = w == 2 || w == 3 || (w >= 6 && !way.dirOut);
        const double heard = isLeft ? a.left : a.right, other = isLeft ? a.right : a.left;
        tried++;
        const bool good = silent ? a.left < 1.25e-5 && a.right < 1.25e-5
                                 : near(db(heard), db(way.level), 0.05) && other < 1.25e-5 && a.delay == 1;
        if (!good) {
            different++;
            if (xtSwitch("XT_FX_FIRST"))
                std::printf("  unit: %s: left %.3f dB, right %.3f dB\n", kName[w], db(a.left), db(a.right));
        }
        if (print)
            std::printf("through the unit, %s: left %+.2f dB, right %+.2f dB, %d samples late\n", kName[w],
                        db(a.left), db(a.right), a.delay);
    }
    const UnitLine e = unitLine(rom, 4, true, way);
    if (print) std::printf("through the unit, MULTI L with the system's EQ on at its defaults: %+.2f dB\n", db(e.left));
    {   // With it running, an edit reaches the chip through the queue: Master Level down, and the level follows.
        auto u = std::make_unique<XtFxUnit>(rom, way.fx.image);
        u->settle();
        float frame[8] = {}, lr[2] = {};
        const auto level = [&](int n) {
            double top = 0;
            for (int i = 0; i < n; i++) {
                frame[4] = quarterSine(i);
                u->process(frame, lr, 1);
                if (i > n / 2) top = std::max(top, std::fabs((double)lr[0]));
            }
            return top;
        };
        const double before = level(4000);
        u->control().set(way.masterLevel, 64);
        u->control().update();
        const double after = level(4000);
        tried++;
        if (!(after < before * 0.5)) {
            different++;
            if (xtSwitch("XT_FX_FIRST")) std::printf("  unit: Master Level moved, from %.3f to %.3f\n", before, after);
        }
    }
    if (!print) std::printf("%s frame through the unit: %d samples, %d different\n", way.whose, tried, different);
}

// The outside input through the unit (v204): words 4 and 5 handed back, against External Input Type's law as the manual
// has it (p. 123): STEREO L R, MONO MIX, MONO L, MONO R.  Within a word's last bit, as the chip halves each side on its
// own for MONO MIX, and at a delay the chip gives.  Noise on both sides, a quarter of full scale.  The frames off the law at
// the best delay; 0 when one fits them all.
int inputLaw(const char* rom, int type, int source) {
    const auto u = settledUnit(rom, xtVSynthFx(), { { 0x78, type }, { 0x7c, source } });
    constexpr int kFrames = 2048, kLatest = 4;
    std::vector<int32_t> in(2 * kFrames);
    Rng r((uint32_t)(1 + 2 * type + source));
    for (int32_t& x : in) x = ((int32_t)r.next() >> 10) * 256;                  // whole chip words, the chip's ±2^21
    const std::vector<int32_t> back = handedBack(*u, in);
    const auto law = [&](size_t i, int side) {
        const int64_t left = in[2 * i], right = in[2 * i + 1], mix = (left + right) / 2;
        const int64_t laws[4][2] = { { left, right }, { mix, mix }, { left, left }, { right, right } };
        return laws[type][side];
    };
    const auto near = [&](size_t i, size_t late, int side) {
        return std::llabs(back[2 * i + side] - law(i - late, side)) <= 256;
    };
    int fewest = kFrames;
    for (size_t late = 0; late <= kLatest; ++late) {
        int off = 0;
        for (size_t i = kLatest; i < kFrames; ++i) off += !near(i, late, 0) || !near(i, late, 1);
        fewest = std::min(fewest, off);
    }
    return fewest;
}
void unitInput(const char* rom) {
    int tried = 0, different = 0;
    for (int source = 0; source <= 1; ++source)
        for (int type = 0; type <= 3; ++type) {
            const int off = inputLaw(rom, type, source);
            tried++;
            different += off ? 1 : 0;
            if (off && xtSwitch("XT_FX_FIRST"))
                std::printf("  unit: External Input Type %d, Source %d: %d frames off its law\n", type, source, off);
        }
    std::printf("the input through the unit: %d samples, %d different\n", tried, different);
}
} // namespace xtfxcheck

// The audio thread must not allocate.  Only this harness replaces these.
using xtfxcheck::gAudioAllocations;
using xtfxcheck::tAudioThread;
void* operator new(std::size_t n) { if (tAudioThread) gAudioAllocations++; void* p = std::malloc(n ? n : 1); if (!p) std::abort(); return p; }
void* operator new[](std::size_t n) { if (tAudioThread) gAudioAllocations++; void* p = std::malloc(n ? n : 1); if (!p) std::abort(); return p; }
void operator delete(void* p) noexcept { if (tAudioThread && p) gAudioAllocations++; std::free(p); }
void operator delete[](void* p) noexcept { if (tAudioThread && p) gAudioAllocations++; std::free(p); }
void operator delete(void* p, std::size_t) noexcept { if (tAudioThread && p) gAudioAllocations++; std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { if (tAudioThread && p) gAudioAllocations++; std::free(p); }

// xt_fx <rom> [frames] [seed]: this class against JE-8086's interpreter, unmodified, on the XT's own programs loaded by the firmware's own driver in the interpreter: each MFX, Reverb
//   and Chorus type with its parameters drawn at random and changed as it runs, noise at every level into every input, and then all of them changing type under load.  Every word of
//   both cores' state and all of GRAM, each sample, and the delay memory each 2048 samples.  XT_FX_FIRST=1: say where each case first differs.  XT_FX_STOP=1: stop at the first case that does.
// xt_fx <rom> numbers: the firmware's own numbers on this class: gains, the EQ, the delays, whether every type makes sense.
// xt_fx <rom> speed: how much of a core it takes, and what the control side costs.
// xt_fx <rom> control [events] [seed]: XtFxControl: see "the control side" below.  xt_fx <rom> threads [events] [seed]: only its part with threads.
int main(int argc, char** argv) {
    using namespace xtfxcheck;
    if (argc < 2) { std::fprintf(stderr, "xt_fx <rom> [frames] [seed] | numbers | speed | control [events] [seed]\n"); return 2; }
    const char* rom = argv[1];
    if (argc > 2 && !std::strcmp(argv[2], "numbers")) {
        numbers(rom);
        changeUnderTone(rom);
        unitLevels(rom, true, rom2Way());
        return 0;
    }
    if (argc > 2 && !std::strcmp(argv[2], "threads")) {
        threads(rom, argOr(argc, argv, 3, 200), (uint32_t)argOr(argc, argv, 4, 2024));
        return 0;
    }
    if (argc > 2 && !std::strcmp(argv[2], "control")) {
        const int events = argOr(argc, argv, 3, 200);
        const uint32_t seed = (uint32_t)argOr(argc, argv, 4, 2024);
        rom2Start(rom);
        matrixThroughQueue(rom);
        readers(rom, seed);
        behaviour(rom);
        valueText(rom);
        liveEdits(rom, events, seed);
        threads(rom, events, seed);
        againstReference(rom, events, seed);
        unitConversion(rom, seed, rom2Way());
        unitLevels(rom, false, rom2Way());
        unitAgainstReference(rom, events, seed, rom2Way());
        unitInput(rom);
        copyWithoutChip(rom, seed);
        return 0;
    }
    if (argc > 2 && !std::strcmp(argv[2], "vc2")) {   // rom is rom.4 here (XtFxCheckVc2.cpp)
        vc2Control(rom, argOr(argc, argv, 3, 200), (uint32_t)argOr(argc, argv, 4, 2024));
        return 0;
    }
    if (argc > 2 && !std::strcmp(argv[2], "speed")) { speed(rom); return 0; }
    const int frames = argOr(argc, argv, 2, 8192);
    const uint32_t seed = (uint32_t)argOr(argc, argv, 3, 2024);
    { Sh4 c; c.loadRom(rom); for (int t = 0; t < 42; t++) { char nm[24]; c.read(0x89788c98 + 24 * (uint32_t)t, nm, 20); nm[20] = 0; for (int e = 19; e > 0 && nm[e] == ' '; e--) nm[e] = 0; compare(rom, ("MFX " + std::to_string(t) + " " + nm).c_str(), t, t % 11, t % 9, frames, seed + (uint32_t)t, false); } }
    for (int t = 0; t < 11; t++) compare(rom, (std::string("Reverb ") + kRevName[t]).c_str(), 0, t, 0, frames, seed + 100 + (uint32_t)t, false);
    for (int t = 0; t < 9; t++) compare(rom, (std::string("Chorus ") + kChoName[t]).c_str(), 1, 0, t, frames, seed + 200 + (uint32_t)t, false);
    compare(rom, "every type, changing under load", 0, 0, 0, frames * 8, seed + 300, true);
    return 0;
}
#endif
