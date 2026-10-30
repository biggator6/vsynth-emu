// The XT's LFO — see XtLfo.h.  Single precision as the SH-4 computes it
// (XtFloat.h).
#include "XtLfo.h"
#include "XtFloat.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr uint32_t kRomBase = 0x89300000;
constexpr uint32_t kRateTab = 0x897ced34;    // float ×128: rate knob → Hz (0x894e0290)
constexpr uint32_t kOffsetTab = 0x897d1c38;  // float ×5: −1, −0.5, 0, +0.5, +1 (0x894f12e4)
// The sine table lives in .data (RAM 0x8987af24, 257 floats); its image in
// rom.2 starts at the same offset into .data.
constexpr uint32_t kDataRom = 0x89834018, kDataRam = 0x8985858c;
constexpr uint32_t kSineRam = 0x8987af24;

struct Tables {
    float rate[128], offset[5], sine[257];
    bool ok = false;
} T;

template <class V> bool readAt(FILE* f, uint32_t addr, V* v, size_t n) {
    return std::fseek(f, (long)(addr - kRomBase), SEEK_SET) == 0 && std::fread(v, sizeof(V), n, f) == n;
}

constexpr float kFull = 131072.0f;

} // namespace

bool xtLfoLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    const bool ok = readAt(f, kRateTab, T.rate, 128) && readAt(f, kOffsetTab, T.offset, 5)
                 && readAt(f, kSineRam - kDataRam + kDataRom, T.sine, 257);
    std::fclose(f);
    if (!ok) return false;
    // Sanity: rates rise; the offsets are −1…+1; the sine table is a cycle.
    for (int i = 1; i < 128; ++i)
        if (!(T.rate[i] > T.rate[i - 1])) return false;
    if (T.offset[0] != -1.0f || T.offset[2] != 0.0f || T.offset[4] != 1.0f) return false;
    if (T.sine[0] != 0.0f || T.sine[64] != 1.0f || T.sine[192] != -1.0f) return false;
    T.ok = true;
    return true;
}

bool xtLfoTablesLoaded() { return T.ok; }

XtLfoParams xtLfoConvert(int wave, int rate, int offset, int delay, int fadeMode, int fade, bool keySync) {
    using namespace xtf;
    static const int waveMap[8] = { 1, 0, 2, 3, 6, 4, 5, 7 };     // 0x894e0010
    auto c127 = [](int v) { return v < 0 ? 0 : v > 127 ? 127 : v; };
    auto ms = [&](int k) { const float u = div(flt(c127(k)), 127.0f); return mul(mul(u, u), 10000.0f); };
    XtLfoParams p;
    p.wave = waveMap[wave & 7];
    p.rate = T.rate[c127(rate)];
    p.delayMs = ms(delay);
    p.fadeMs = ms(fade);
    p.keySync = keySync ? 1 : 0;
    p.offset = offset >= 0 && offset <= 4 ? offset : 2;                 // 0x894e0210
    p.fadeMode = fadeMode >= 1 && fadeMode <= 3 ? fadeMode : 0;
    return p;
}

// ── the delay/fade base ──────────────────────────────────────────────────────

// 0x894f0520: fade in from 0 (ON <<, OFF <<) or out from full (>>)
void XtLfo::setDir(int down) {
    if (!down) { dir = 1; rest = 0.0f; target = kFull; }
    else { dir = -1; rest = kFull; target = 0.0f; }
}

// 0x894f04d0
void XtLfo::fadeStart() {
    amp = rest;
    state = delay == 0.0f ? (tIn == 0.0f ? 3 : 2) : 1;
}

// 0x894f05d0
void XtLfo::fadeTick(int n) {
    using namespace xtf;
    float a = amp;
    switch (state) {
    case 0: a = rest; break;
    case 1:
        count = (int16_t)(count + (int16_t)(n * 10));
        if (!(delay > flt(count))) state = 2;
        break;
    case 2:
        a = add(mul(flt(n), rIn), a);                              // fmac
        if (0.0f > rIn) { if (target > a) { a = target; state = 3; } }
        else if (a > target) { a = target; state = 3; }
        break;
    case 3: a = target; state = 3; break;
    case 4:
        a = add(mul(flt(n), rOut), a);
        if (0.0f > rOut) { if (rest > a) { a = rest; state = 5; } }
        else if (a > rest) { a = rest; state = 5; }
        break;
    default: a = rest; break;
    }
    amp = a > kFull ? kFull : (0.0f > a ? 0.0f : a);
}

// ── the LFO ──────────────────────────────────────────────────────────────────

// 0x894f1320 (no tempo sync): rate·65535/100 phase per tick, 1…65535
float XtLfo::rate() const {
    const float v = xtf::div(xtf::mul(p.rate, 65535.0f), 100.0f);
    if (v > 65535.0f) return 65535.0f;
    if (1.0f > v) return 1.0f;
    return v;
}

// 0x894f4cd0: the logistic map, x ← 3.997·x·(1 − x); returns 2x − 1
float XtLfo::chaos() {
    using namespace xtf;
    const float nx = mul(mul(sub(1.0f, x), x), 0x1.ff9db0p+1f);   // 3.997 as rom.2 stores it (0x407fced8)
    x = nx;
    return add(mul(nx, 2.0f), -1.0f);
}

// 0x894f0cf0 → 0x894f0bf0
void XtLfo::noteOn(const XtLfoParams& prm, float seed) {
    using namespace xtf;
    p = prm;
    x = seed;
    ph = 0.0f;
    prev = hold = 0.0f;                                            // as the constructor leaves them
    // the base as constructed (0x894f04a0)
    state = 0; tIn = tOut = 0.0f; amp = kFull; count = 0; delay = 0.0f; dir = 1; rest = 0.0f; target = kFull;
    // trigger: direction by fade mode, then the times
    setDir(p.fadeMode == 1 || p.fadeMode == 3);
    setTimes();
    // ON modes start the delay/fade now, OFF modes at note-off (0x894f0860)
    if (p.fadeMode <= 1) fadeStart(); else state = 0;
    active = true;
    tick(0);
}

// 0x894f05a0, 0x894f0570, 0x894f0560: the fade rates per tick for the
// current direction, and the delay
void XtLfo::setTimes() {
    using namespace xtf;
    auto perTick = [](float t) { return t == 0.0f ? kFull : div(kFull, div(t, 10.0f)); };
    tIn = p.fadeMs;  rIn = mul(perTick(p.fadeMs), flt(dir));
    tOut = p.fadeMs; rOut = mul(-perTick(p.fadeMs), flt(dir));
    delay = p.delayMs;
}

// 0x894f0f40
void XtLfo::update(const XtLfoParams& prm) {
    const int keep = p.keySync;
    p = prm;
    p.keySync = keep;
    setTimes();
}

// 0x894f0810
void XtLfo::noteOff() {
    if (!active) return;
    active = false;
    if (p.fadeMode >= 2) fadeStart();
}

// 0x894f1050
void XtLfo::tick(int n) {
    using namespace xtf;
    const float old = ph;
    float pn = add(mul(flt(n), rate()), old);                       // fmac
    if (pn > 65535.0f) pn = add(pn, -65535.0f);
    ph = pn;
    const int u = ftrc(ph) & 0xffff;
    float v;
    switch (p.wave) {
    case 0:   // triangle
        if (u < 0x4000) v = flt(u);
        else if (u < 0xc000) v = sub(32768.0f, flt(u));
        else v = add(flt(u), -65535.0f);
        v = div(v, 16384.0f);
        break;
    case 1: { // sine, from the table
        const int i = u >> 8;
        const float a = T.sine[i], b = T.sine[i + 1];
        v = add(mul(mul(sub(b, a), flt(u & 0xff)), 0.00390625f), a);
        break;
    }
    case 2: v = div(flt(u - 0x8000), 32768.0f); break;            // saw
    case 3: v = u >= 0x8000 ? -1.0f : 1.0f; break;                 // square
    case 4:   // trapezoid
        if (u <= 0x2000) v = div(flt(u), 8192.0f);
        else if (u <= 0x6000) v = 1.0f;
        else if (u < 0xa000) v = div(sub(32768.0f, flt(u)), 8192.0f);
        else if (u < 0xe000) v = -1.0f;
        else v = div(add(flt(u), -65535.0f), 8192.0f);
        break;
    case 5:   // sample and hold: a new value each cycle
        if (old > flt(u)) hold = chaos();
        v = hold;
        break;
    case 6:   // random: to a new value each cycle, linearly
        if (old > flt(u)) { prev = hold; hold = chaos(); }
        v = add(div(mul(sub(hold, prev), flt(u)), 65536.0f), prev);
        break;
    case 7: v = chaos(); break;                                    // chaos
    default: v = 0.0f; break;
    }
    v = add(v, T.offset[p.offset]);
    fadeTick(n);
    o = mul(div(amp, kFull), v);
}

#ifdef XT_LFO_MAIN
// xt_lfo <rom.2.bin> : the firmware's LFO (constructor 0x894f0b40, note-on
// 0x894f0cf0, tick 0x894f1050, note-off 0x894f0810) run in the SH-4
// interpreter beside the port, for a set of settings, tick by tick.
#include "Sh4Interp.h"
#include <cstring>
#include <string>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 2 || !xtLfoLoadTables(argv[1])) { std::fprintf(stderr, "usage: xt_lfo rom.2.bin\n"); return 2; }
    struct Case { int wave, rate, offset, delay, mode, fade, off, ticks; };
    const Case cases[] = {
        { 0, 64, 2, 0, 0, 0, -1, 400 }, { 1, 64, 2, 0, 0, 0, -1, 400 }, { 2, 80, 2, 0, 0, 0, -1, 400 },
        { 3, 70, 2, 0, 0, 0, -1, 400 }, { 4, 90, 2, 0, 0, 0, -1, 600 }, { 5, 100, 2, 0, 0, 0, -1, 600 },
        { 6, 100, 2, 0, 0, 0, -1, 600 }, { 7, 64, 2, 0, 0, 0, -1, 300 }, { 1, 127, 2, 0, 0, 0, -1, 300 },
        { 1, 0, 2, 0, 0, 0, -1, 200 }, { 1, 64, 0, 0, 0, 0, -1, 200 }, { 1, 64, 4, 0, 0, 0, -1, 200 },
        { 1, 64, 3, 40, 0, 50, -1, 900 }, { 1, 64, 1, 30, 1, 60, -1, 900 }, { 0, 64, 2, 20, 2, 40, 300, 900 },
        { 2, 64, 2, 0, 3, 70, 200, 900 }, { 1, 50, 2, 127, 0, 127, -1, 1500 }, { 3, 64, 2, 0, 2, 0, 100, 300 },
    };
    Sh4 c;
    if (!c.loadRom(argv[1])) return 1;
    const uint32_t ram = 0x8C000000, lfo = ram, prm = ram + 0x200, sp = ram + 0xF0000;
    c.mapRam(ram, 0x100000);
    int bad = 0;
    for (const Case& k : cases) {
        const XtLfoParams P = xtLfoConvert(k.wave, k.rate, k.offset, k.delay, k.mode, k.fade, true);
        // a seed for the global chaos generator the constructor draws from
        const float g = 0.3141f; uint32_t gw; std::memcpy(&gw, &g, 4); c.wr32(0x89f91c48, gw);
        std::vector<uint8_t> z(0x100, 0); c.write(lfo, z.data(), z.size());
        if (!c.call(0x894f0b40, { lfo }, sp)) { std::fprintf(stderr, "ctor: %s\n", c.error().c_str()); return 1; }
        float seed; { uint32_t v = c.rd32(lfo + 0x6c); std::memcpy(&seed, &v, 4); }   // drawn by the constructor
        uint32_t w[8];
        w[0] = (uint32_t)P.wave; std::memcpy(&w[1], &P.rate, 4); std::memcpy(&w[2], &P.delayMs, 4); std::memcpy(&w[3], &P.fadeMs, 4);
        w[4] = (uint32_t)P.keySync; w[5] = 0; w[6] = (uint32_t)P.offset; w[7] = (uint32_t)P.fadeMode;
        c.write(prm, w, sizeof w);
        const float tempo = 120.0f; std::memcpy(&c.fb[0][4], &tempo, 4);
        if (!c.call(0x894f0cf0, { lfo, prm }, sp)) { std::fprintf(stderr, "note-on: %s\n", c.error().c_str()); return 1; }
        XtLfo L;
        L.noteOn(P, seed);
        auto f32 = [&](uint32_t a) { uint32_t v = c.rd32(a); float x; std::memcpy(&x, &v, 4); return x; };
        int caseBad = 0;
        // note-on itself ticked once with n = 0: compare that state first
        for (int t = 0; t <= k.ticks && caseBad < 3; ++t) {
            if (t > 0) {
                if (t == k.off) { c.call(0x894f0810, { lfo }, sp); L.noteOff(); }
                if (!c.call(0x894f1050, { lfo, 1 }, sp)) { std::fprintf(stderr, "tick: %s\n", c.error().c_str()); return 1; }
                L.tick(1);
            }
            const float fo = f32(lfo + 0x38), fp = f32(lfo + 0x40), fa = f32(lfo + 0x20);
            if (fo != L.out() || fp != L.phase() || fa != L.amplitude()) {
                if (caseBad++ < 3)
                    std::printf("  wave %d rate %d t %d: fw out %.8g ph %.8g amp %.8g | port %.8g %.8g %.8g\n", k.wave, k.rate, t,
                                fo, fp, fa, L.out(), L.phase(), L.amplitude());
            }
        }
        std::printf("wave %d rate %3d offset %d delay %3d mode %d fade %3d off %4d: %s\n", k.wave, k.rate, k.offset, k.delay, k.mode, k.fade,
                    k.off, caseBad ? "DIFFERENT" : "identical");
        bad += caseBad != 0;
    }
    return bad ? 3 : 0;
}
#endif
