// xt_fx: XtFxChip against JE-8086's interpreter, whole state each sample (XtFxCheck.h; moved from XtFxChip.cpp as it
// stood, v204).  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"
#include "XtFxCheckNoise.h"

namespace xtfxcheck {
const char* kRevName[] = { "Off", "Room 1", "Room 2", "Room 3", "Hall 1", "Hall 2", "Hall 3", "Garage", "Plate", "Non-Linear", "Delay" };
const char* kChoName[] = { "Off", "Chorus 1", "Chorus 2", "Chorus 3", "Chorus 4", "Feedback Chorus", "Flanger", "Short Delay", "Fbk Short Delay" };

// Every word of the two states, after the rotation.  Returns the first that differs.
std::string differs(const XtFxChip& x, const Reference& ref, bool memory) {
    const auto& r = ref.runtime(); char b[120];
    if (x.position() != r.core[0].iramPos) return "position";
    for (unsigned i = 0; i < 256; i++) if (x.gram(i) != r.readGram(i)) { std::snprintf(b, sizeof b, "GRAM %02x: %d, against %d", i, x.gram(i), r.readGram(i)); return b; }
    for (unsigned k = 0; k < 2; k++) {
        const auto& c = r.core[k];
        for (unsigned i = 0; i < 256; i++) if (x.iram(k, i) != c.readIram(i)) { std::snprintf(b, sizeof b, "IRAM %u %02x: %d, against %d", k, i, x.iram(k, i), c.readIram(i)); return b; }
        if (x.accA(k) != c.accA.acc || x.accB(k) != c.accB.acc) { std::snprintf(b, sizeof b, "core %u accumulators: %d %d, against %d %d", k, x.accA(k), x.accB(k), c.accA.acc, c.accB.acc); return b; }
        for (unsigned age = 1; age <= 3; age++) {                                // [head] is the oldest, three clocks back
            const unsigned i = ((unsigned)c.accA.head + 3 - age) % 3;
            if (x.historyA(k, age) != c.accA.hist[i] || x.historyB(k, age) != c.accB.hist[i]) { std::snprintf(b, sizeof b, "core %u pipeline, %u clocks back", k, age); return b; }
        }
    }
    for (unsigned i = 0; i < 8; i++) if (x.coefficient(i) != r.mulcoeffs[i]) { std::snprintf(b, sizeof b, "coefficient %u: %d, against %d", i, x.coefficient(i), r.mulcoeffs[i]); return b; }
    if (x.readLatch() != r.eram.readLatch) { std::snprintf(b, sizeof b, "read latch: %d, against %d", x.readLatch(), r.eram.readLatch); return b; }
    if (x.writeLatch() != r.eram.writeLatch) return "write latch";
    if (x.varOffset() != r.eram.varOffset) return "address register";
    if (((x.memoryPosition() ^ r.eram.pos) & 0x3ffff) != 0) return "memory's position";
    if (memory) for (uint32_t a = 0; a < (1u << 18); a++) if (x.memoryWord(a) != r.eram.readRaw(a)) { std::snprintf(b, sizeof b, "memory %05x: %d, against %d", a, x.memoryWord(a), r.eram.readRaw(a)); return b; }
    return "";
}

// A run: noise into every input at levels that change, the parameters changing.  Prints a line.
void compare(const char* rom, const char* label, int mfxType, int revType, int choType, int frames, uint32_t seed, bool roam) {
    Rig g(rom); Rng r(seed);
    g.boot(mfxType, revType, choType, &r); g.levels(&r);
    InputNoise noise(seed + 1);                                                  // its own stream, the analog pair too (v204)
    int different = 0, firstAt = -1; std::string what;
    const bool first = xtSwitch("XT_FX_FIRST") != nullptr;
    for (int n = 0; n < frames; n++) {
        if (n % 1024 == 512) { const int k = r.below(4);
            if (k == 0) g.mfxParams(&r); else if (k == 1) g.setReverb(revType, &r); else if (k == 2) g.setChorus(choType, &r); else g.levels(&r); }
        if (roam && n % 2048 == 1024) { const int k = r.below(3);
            if (k == 0) g.setMfx(r.below(42), &r); else if (k == 1) { revType = r.below(11); g.setReverb(revType, &r); } else { choType = r.below(9); g.setChorus(choType, &r); } }
        noise.into(g.chip, *g.ref, n);
        g.frame();
        const std::string d = differs(g.chip, *g.ref, n % 2048 == 2047 || n == frames - 1);
        if (!d.empty()) { if (!different) { firstAt = n; what = d; } different++; }
    }
    if (different && first) std::printf("  %s: first at sample %d, %s\n", label, firstAt, what.c_str());
    std::printf("%s: %d samples, %d different%s\n", label, frames, different + (g.readsDiffer ? 1 : 0), g.readsDiffer ? ", and reads back" : "");
    if ((different || g.readsDiffer) && xtSwitch("XT_FX_STOP")) {
        std::fflush(stdout);
        std::exit(0);
    }
}
} // namespace xtfxcheck
#endif
