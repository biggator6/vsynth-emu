// The XT's analog oscillator — see XtAnalog.h.  Single precision as the SH-4
// computes it (XtFloat.h).
#include "XtAnalog.h"
#include "XtCosm.h"
#include "XtFloat.h"
#include "XtOsc.h"
#include <bit>
#include <cstdio>

namespace {

constexpr uint32_t kRomBase = 0x89300000;
// the parameter descriptors (12 bytes per ID: storage, then int16 centre,
// default, minimum, maximum) and the level curve (128 shorts)
constexpr uint32_t kDescTab = 0x897a167c, kLevelTab = 0x897cfcb8;
// the analog calibration's system parameters
constexpr int kSys0 = 0x528, kSysN = 0x55b - 0x528 + 1;
enum : int {
    kAliasCancel = 0x52c, kSawSlope = 0x52d, kSquSlope = 0x52e,
    kBias = 0x530, kLowerKf = 0x53b, kUpperKf = 0x546, kLevel = 0x551,   // + the waveform, SAW…LA-SQUARE
};

struct Tables {
    uint16_t level[128];
    int sys[kSysN];                 // default − centre, as the parameter getter returns them
    bool ok = false;
} T;

using namespace xtf;
inline float fmac(float a, float b, float c) { return add(mul(a, b), c); }   // FMAC: the product, then the sum
inline int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
inline float clamp01(float x) { return x > 1.0f ? 1.0f : (0.0f > x ? 0.0f : x); }
constexpr float K(uint32_t u) { return std::bit_cast<float>(u); }               // a constant as rom.2 stores it
inline int sys(int id) { return T.sys[id - kSys0]; }

// (d/63)·|d/63|
inline float sq63(int d) {
    const float x = div(flt((int16_t)d), 63.0f);
    return mul(d < 0 ? -x : x, x);
}

// Fat (0x894e1010): F/127 within 0…1, + (key − 60)·KF/1200 within 0…1
float fatCurve(int f, int kf10, int dk) {
    const float x = clamp01(div(flt((int16_t)f), 127.0f));
    return clamp01(add(div(flt((int16_t)kf10 * (int16_t)dk), 1200.0f), x));
}

// Pulse Width (0x894e0ea0): PW within ±63, + (key − 60)·KF·126/1200, + 63;
// the integer part (toward zero) wrapped into 0…126, the fraction kept; /126
float pwCurve(int pw, int kf10, int dk) {
    float x = flt((int16_t)pw);
    x = x > 63.0f ? 63.0f : (-63.0f > x ? -63.0f : x);
    const float y = add(add(div(mul(flt((int16_t)dk * (int16_t)kf10), 126.0f), 1200.0f), x), 63.0f);
    int i = ftrc(y);
    const float frac = sub(y, flt(i));
    if (i < 0) i = i % 126 + 126;
    else if (i > 126) i = i % 126;
    return div(add(flt(i), frac), 126.0f);
}

// The waveform's level at the note's pitch (0x894e1350), for the patch's
// waveform; SUPER-SAW, FEEDBACK-OSC and X-MOD take the Saw's
float waveLevel(int wave, int pitch10) {
    const int w = wave >= 0 && wave <= 10 ? wave : 0;
    const int bias = (int8_t)sys(kBias + w), upper = (int8_t)sys(kUpperKf + w), lower = (int8_t)sys(kLowerKf + w);
    const int level = (int8_t)sys(kLevel + w);
    const int d = pitch10 - bias * 1000;
    const int m = d >= 0 ? d * upper : -d * lower;
    float f = add(div(flt(m), 6.0e6f), 1.0f);
    if (0.0f > f) f = 0.0f;
    return mul(div(flt(T.level[level & 0x7f]), 32768.0f), f);
}

constexpr float kHalf = 0.5f;
constexpr float kPwLo = K(0x3b820880), kPwSpan = K(0x3f7df7de);   // 0.0039683, 0.992063
constexpr float kFatMin = K(0x3b7f9723), kFatFloor = K(0x3b03126f); // 0.0039, 0.002
constexpr float kImpactLog = K(0xc1265ad5);    // the firmware's logf(3.05185e-5) (0x89531b78 on 0x38000100)
constexpr float kImpactPeriod = K(0x42c8745c); // 100.227 = 44100/440
constexpr float kPhaseInc = K(0x3ba377ee);     // 220/44100 per sample at ratio 1: two cycles a phase
constexpr float kFull = K(0x4effffff);         // 2^31 − 128

} // namespace

bool xtAnalogLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    bool ok = std::fseek(f, (long)(kLevelTab - kRomBase), SEEK_SET) == 0 && std::fread(T.level, 2, 128, f) == 128;
    for (int i = 0; ok && i < kSysN; ++i) {
        int16_t d[2];
        ok = std::fseek(f, (long)(kDescTab + 12u * (uint32_t)(kSys0 + i) + 4 - kRomBase), SEEK_SET) == 0 && std::fread(d, 2, 2, f) == 2;
        T.sys[i] = d[1] - d[0];
    }
    std::fclose(f);
    T.ok = ok && T.level[127] == 32768 && sys(kSawSlope) == 100;
    return T.ok;
}

bool xtAnalogTablesLoaded() { return T.ok; }

int xtAnalogCode(int wave) {
    static const int code[14] = { 2, 3, 4, 6, 5, 7, 0, 1, 8, 10, 9, 11, 12, 13 };
    return wave >= 1 && wave <= 13 ? code[wave] : 2;      // the converter's default case: SAW
}

bool xtAnalogPlays(int wave) { const int c = xtAnalogCode(wave); return c >= 2 && c <= 8; }

XtAnalogBlock xtAnalogBlock(const XtAnalogParams& p, int key, int pitch10, int waveGainDb, float velFat, float velPw) {
    XtAnalogBlock b;
    const int dk = key - 60;
    b.code = xtAnalogCode(p.wave);
    b.impact = div(mul(flt(p.impact & 0xff), 4.0f), 40.0f);
    if (sys(kAliasCancel) && (p.wave == 0 || p.wave == 1))
        b.slope = div(flt((uint16_t)sys(p.wave == 0 ? kSawSlope : kSquSlope)), 100.0f);
    b.pw = pwCurve(p.pw, p.pwKf * 10, dk);
    b.pwEnv = mul(sq63(p.pwEnv), velPw);
    b.pwLfo = sq63(clampi((int16_t)p.pwLfo, -63, 63));
    b.fat = fatCurve(p.fat, p.fatKf * 10, dk);
    b.fatEnv = mul(sq63(p.fatEnv), velFat);
    b.fatLfo = sq63(clampi((int16_t)p.fatLfo, -63, 63));
    b.gain = mul(waveLevel(p.wave, pitch10), xtOscWaveGainFactor(waveGainDb));
    return b;
}

// Fat's and the Pulse Width's targets, from the block and the envelope and
// LFO outputs (the note-on's default case 0x89504e80, the update's 0x89505590)
void XtAnalogOsc::shape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo, bool on) {
    const float fat = clamp01(fmac(lfo, b.fatLfo, fmac(fatEnv, b.fatEnv, b.fat)));
    const float half = sub(kHalf, mul(fat, kHalf));
    if (on) {
        fatT = fatC = half;
        if (!(fatT > kFatMin)) fatT = fatC = kFatFloor;
    } else {
        fatT = fmac(sub(half, fatT), K(0x3f333333), fatT);                      // 70% of the way
        if (!(fatT > kFatMin)) fatT = kFatFloor;
    }
    const float pw = clamp01(fmac(lfo, b.pwLfo, fmac(pwEnv, b.pwEnv, b.pw)));
    float x = fmac(pw, kPwSpan, kPwLo);
    switch (code) {
    case 2: {   // SAW: folded to the upper half, widened by the Slope at the pitch
        if (kHalf > x) x = sub(1.0f, x);
        const float s = mul(mul(ratio_, b.slope), K(0x3c23d709));             // 0.01, a unit below
        x = add(x, s);
        const float top = sub(1.0f, s);
        if (x > top) x = top;
        break;
    }
    case 3: {   // SQUARE: the edge's half-width, by the Slope and a step of the pitch
        const float r = ratio_;
        float f = 0.0f;
        if (!(K(0x3f6411ef) > r)) {                                            // 2^(−2/12)
            if (1.0f > r) f = K(0x3d4ccccd);
            else if (K(0x3f879c7c) > r) f = K(0x3dcccccd);                     // 2^(1/12)
            else if (K(0x3fa14518) > r) f = K(0x3e19999a);                     // 2^(4/12)
            else if (K(0x3fd744fc) > r) f = K(0x3e4ccccd);                     // 2^(9/12)
            else f = 0.25f;
        }
        edge_ = mul(mul(b.slope, mul(r, f)), K(0x3ba377ed));                   // 220/44100, a unit below the phase's
        break;
    }
    case 4: case 6:   // TRIANGLE, SINE: halved
        x = mul(x, kHalf);
        break;
    case 8: {   // NOISE: the hold, 1 + 30·|1 − 2x| samples, as a byte
        const float d = sub(1.0f, mul(x, 2.0f));
        int n = kHalf > x ? ftrc(fmac(d, 30.0f, 1.0f)) : ftrc(sub(1.0f, mul(d, 30.0f)));
        n = (int8_t)n;
        if (on && n == 0) n = 1;                                               // the note-on's, not the update's
        hold = n;
        break;
    }
    default:    // RAMP, JUNO: as is
        break;
    }
    pwT = x;
    if (on) pwC = x;
}

void XtAnalogOsc::noteOn(const XtAnalogBlock& b, float ratio, float gain, float gainStart, float fatEnv, float pwEnv, float lfo) {
    code = b.code;
    ratio_ = ratio;
    gainT = gain;
    gainC = gainStart;
    fl = 0;
    imp = b.impact;
    dec = xtCosmExpf(mul(kImpactLog, div(ratio_, kImpactPeriod)));            // powf(3.05185e-5, ratio/100.227)
    if (1.0f > imp) ph = mul(ph, imp);
    if (ph > 1.0f || 0.0f > ph) ph = 0.0f;
    shape(b, fatEnv, pwEnv, lfo, true);
}

void XtAnalogOsc::update(const XtAnalogBlock& b, float ratio, float gain, float fatEnv, float pwEnv, float lfo) {
    ratio_ = ratio;
    gainT = gain;
    shape(b, fatEnv, pwEnv, lfo, false);
}

int XtAnalogOsc::breakpoints(float* p) const {
    const float a = fatC, w = pwC;
    switch (code) {
    case 2: {
        float d = sub(kHalf, w);
        if (0.0f > d) d = add(d, 1.0f);
        const float ad = mul(a, d);
        p[0] = ad; p[1] = a; p[2] = sub(add(a, d), ad);
        return 3;
    }
    case 3: {
        const float s = edge_, pa = mul(w, a), bb = sub(add(a, w), pa);
        p[0] = s; p[1] = sub(pa, s); p[2] = add(pa, s); p[3] = sub(a, s); p[4] = add(a, s); p[5] = sub(bb, s); p[6] = add(bb, s);
        p[7] = sub(1.0f, s);
        return 8;
    }
    case 4: {
        const float ap = mul(a, w), e9 = sub(a, ap);
        p[0] = ap; p[1] = e9; p[2] = a; p[3] = add(w, e9); p[4] = add(sub(1.0f, w), ap);
        return 5;
    }
    case 5: {
        const float ap = mul(w, a);
        p[0] = ap; p[1] = a; p[2] = sub(add(a, w), ap);
        return 3;
    }
    case 6: {
        const float ap = mul(a, w), a2 = mul(kHalf, a), d4 = sub(w, ap);
        p[0] = ap; p[1] = a2; p[2] = sub(a, ap); p[3] = a; p[4] = add(d4, a); p[5] = add(kHalf, a2); p[6] = sub(1.0f, d4);
        return 7;
    }
    case 7: {
        const float h1 = mul(add(1.0f, a), kHalf), a2 = mul(a, kHalf), q0 = mul(a2, w), q1 = mul(sub(h1, a), w);
        p[0] = sub(a2, q0); p[1] = a2; p[2] = sub(a, q0); p[3] = a; p[4] = sub(h1, q1); p[5] = h1; p[6] = sub(1.0f, q1);
        return 7;
    }
    default:
        return 0;
    }
}

uint32_t XtAnalogOsc::process(float* lr, uint32_t sync) {
    switch (code) {
    case 3: return square(lr, sync);
    case 4: return triangle(lr, sync);
    case 5: return ramp(lr, sync);
    case 6: return sine(lr, sync);
    case 7: return juno(lr, sync);
    case 8: return noise(lr);
    default: return saw(lr, sync);
    }
}

// The routines share their frame: a = the Fat value (+0x1c4, ½ at Fat 0),
// p = the Pulse Width (+0x1cc), and a phase of 0…1 holding two cycles, the
// first over 0…a.  Each sample: a set sync bit resets the phase; the wave
// y, then ((y + Impact)·gain)·(2^31 − 128) on L and R; the phase steps by
// ratio·220/44100 and wraps; the gain moves 1/32 toward its target, Impact
// by its decay; the half-cycle flags (1 in the first cycle, 2 in the
// second) collect, and both seen set the sample's bit in the returned mask.
// Once a block Fat and PW move 1/32 toward their targets.
constexpr float kStep = K(0x3d000000);     // 1/32

inline void XtAnalogOsc::moveShape() {
    fatC = fmac(kStep, sub(fatT, fatC), fatC);
    pwC = fmac(kStep, sub(pwT, pwC), pwC);
}

// SAW (0x89508c20).  d = ½ − p (+1 and the wave inverted when negative); the
// breakpoints a·d, a and b = a + d − a·d; the wave rises over 0…a·d and
// a…b and falls over a·d…a and b…1, each segment −1…+1.
uint32_t XtAnalogOsc::saw(float* lr, uint32_t sync) {
    float d = sub(kHalf, pwC), sign = 1.0f;
    if (0.0f > d) { d = add(d, 1.0f); sign = -1.0f; }
    const float a = fatC, ad = mul(a, d), bb = sub(add(a, d), ad);
    const float kb1 = div(2.0f, sub(bb, 1.0f)), kba = div(2.0f, sub(bb, a)), kad = div(2.0f, sub(ad, a)), k0 = div(2.0f, ad);
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(bb > ph)) { y = mul(sub(ph, 1.0f), kb1); fl |= 2; }
        else if (!(a > ph)) { y = mul(sub(ph, a), kba); fl |= 2; }
        else {
            fl |= 1;
            y = ad > ph ? mul(ph, k0) : mul(sub(ph, a), kad);
        }
        y = sub(y, 1.0f);
        ph = add(ph, inc);
        if (fl == 3) { fl = 0; wrap |= bit; }
        y = mul(y, sign);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
        y = mul(mul(add(y, imp), gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        imp = mul(imp, dec);
        bit <<= 1;
    }
    moveShape();
    return wrap;
}

// SQUARE (0x89508aa0).  Breakpoints p·a, a and b = a + p − p·a: −1 up to
// p·a, +1 to a, −1 to b, +1 to 1; each edge a ramp over ±s around it (s the
// edge half-width, +0x1e0), the one at 0/1 split across the wrap.
uint32_t XtAnalogOsc::square(float* lr, uint32_t sync) {
    const float s = edge_, ks = div(1.0f, s);
    const float a = fatC, p = pwC, pa = mul(p, a), bb = sub(add(a, p), pa);
    const float e0 = sub(pa, s), e1 = add(pa, s), e2 = sub(a, s), e3 = add(a, s), e4 = sub(bb, s), e5 = add(bb, s), e6 = sub(1.0f, s);
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (s > ph) { y = mul(-ph, ks); fl |= 1; }
        else if (e0 > ph) { y = -1.0f; fl |= 1; }
        else if (e1 > ph) { y = mul(sub(ph, pa), ks); fl |= 1; }
        else if (e2 > ph) { y = 1.0f; fl |= 1; }
        else if (e3 > ph) { y = mul(sub(a, ph), ks); fl |= 2; }
        else if (e4 > ph) { y = -1.0f; fl |= 2; }
        else if (e5 > ph) { y = mul(sub(ph, bb), ks); fl |= 2; }
        else if (e6 > ph) { y = 1.0f; fl |= 2; }
        else { y = mul(sub(1.0f, ph), ks); fl |= 2; }
        y = mul(mul(add(y, imp), gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        imp = mul(imp, dec);
        gainC = fmac(sub(gainT, gainC), kStep, gainC);
        ph = add(ph, inc);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
        if (fl == 3) { fl = 0; wrap |= bit; }
        bit <<= 1;
    }
    moveShape();
    return wrap;
}

// TRIANGLE (0x895094c0), p halved.  Each cycle rises 0…1, falls 1…−1 and
// rises −1…0: the first over 0…a·p, …a − a·p, …a; the second over
// a…p + a − a·p, …1 − p + a·p, …1.
uint32_t XtAnalogOsc::triangle(float* lr, uint32_t sync) {
    const float a = fatC, p = pwC, ap = mul(a, p);
    const float k8 = div(1.0f, ap), k4 = div(1.0f, sub(p, ap));
    const float e9 = sub(a, ap), e10 = add(p, e9), e11 = add(sub(1.0f, p), ap);
    const float k12 = div(2.0f, sub(e10, e11)), k16 = div(2.0f, sub(ap, e9));
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(e11 > ph)) { y = mul(sub(ph, 1.0f), k4); fl |= 2; }
        else if (!(e10 > ph)) { y = sub(mul(sub(ph, e11), k12), 1.0f); fl |= 2; }
        else if (!(a > ph)) { y = mul(sub(ph, a), k4); fl |= 2; }
        else if (!(e9 > ph)) { y = mul(sub(ph, a), k8); fl |= 1; }
        else if (!(ap > ph)) { y = sub(mul(sub(ph, e9), k16), 1.0f); fl |= 1; }
        else { y = mul(ph, k8); fl |= 1; }
        y = add(y, imp);
        if (fl == 3) { fl = 0; wrap |= bit; }
        y = mul(mul(y, gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        ph = add(ph, inc);
        bit <<= 1;
        imp = mul(imp, dec);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
    }
    moveShape();
    return wrap;
}

// SINE (0x89509030), p halved: TRIANGLE's frame with each quarter a
// parabola, x·(2 − x) toward a peak and x·(x + 2) toward a trough (x the
// quarter's −1…1), the peaks at a·p and p + a − a·p, the zero crossings at
// a/2 and ½ + a/2.
uint32_t XtAnalogOsc::sine(float* lr, uint32_t sync) {
    const float a = fatC, p = pwC, ap = mul(a, p);
    const float k8 = div(1.0f, ap), a2 = mul(kHalf, a), e8 = sub(a, ap), d4 = sub(p, ap);
    const float e9 = add(d4, a), e10 = add(kHalf, a2), e11 = sub(1.0f, d4), k4 = div(1.0f, d4);
    const float d12 = sub(ap, a2), k12 = div(1.0f, d12);
    const float d16 = sub(p, add(kHalf, d12)), k16 = div(1.0f, d16);
    const float k20 = div(1.0f, sub(sub(add(d16, a), a2), a2));               // d16 again, by another sum
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    auto up = [](float x) { return mul(add(x, 2.0f), x); };
    auto down = [](float x) { return mul(add(-x, 2.0f), x); };
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(e11 > ph)) { y = up(mul(sub(ph, 1.0f), k4)); fl |= 2; }
        else if (!(e10 > ph)) { y = up(mul(sub(ph, e10), k16)); fl |= 2; }
        else if (!(e9 > ph)) { y = down(mul(sub(ph, e10), k20)); fl |= 2; }
        else if (!(a > ph)) { y = down(mul(sub(ph, a), k4)); fl |= 2; }
        else if (!(e8 > ph)) { y = up(mul(sub(ph, a), k8)); fl |= 1; }
        else if (!(a2 > ph)) { y = up(mul(sub(ph, a2), k12)); fl |= 1; }
        else if (!(ap > ph)) { y = down(mul(sub(ph, a2), k12)); fl |= 1; }
        else { y = down(mul(ph, k8)); fl |= 1; }
        y = mul(mul(add(y, imp), gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        ph = add(ph, inc);
        if (fl == 3) { fl = 0; wrap |= bit; }
        bit <<= 1;
        imp = mul(imp, dec);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
    }
    moveShape();
    return wrap;
}

// RAMP (0x89508800).  SAW's breakpoints with d = p and no fold: each cycle
// rises 0…1 (over 0…a·p, a…b) and falls 1…−1 (over a·p…a, b…1).
uint32_t XtAnalogOsc::ramp(float* lr, uint32_t sync) {
    const float a = fatC, p = pwC, ap = mul(p, a), bb = sub(add(a, p), ap);
    const float k0 = div(1.0f, ap), kad = div(2.0f, sub(ap, a)), kba = div(1.0f, sub(bb, a)), kb1 = div(2.0f, sub(bb, 1.0f));
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(bb > ph)) { y = sub(mul(sub(ph, 1.0f), kb1), 1.0f); fl |= 2; }
        else if (!(a > ph)) { y = mul(sub(ph, a), kba); fl |= 2; }
        else if (!(ap > ph)) { y = sub(mul(sub(ph, a), kad), 1.0f); fl |= 1; }
        else { y = mul(ph, k0); fl |= 1; }
        y = mul(add(y, imp), gainC);
        if (fl == 3) { fl = 0; wrap |= bit; }
        ph = add(ph, inc);
        bit <<= 1;
        y = mul(y, kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        imp = mul(imp, dec);
    }
    moveShape();
    return wrap;
}

// JUNO (0x89508db0): a rising saw each cycle (−1…1 over 0…a and a…1),
// held at −1 − Impact by a pulse at twice its rate: low over each half
// cycle's first (1 − p) part, so p is the share it is heard.
uint32_t XtAnalogOsc::juno(float* lr, uint32_t sync) {
    const float a = fatC, p = pwC;
    const float k1 = div(2.0f, add(a, -1.0f)), k0 = div(2.0f, a);
    const float h1 = mul(add(1.0f, a), kHalf), a2 = mul(a, kHalf);
    const float q0 = mul(a2, p), e6 = sub(a2, q0), e8 = sub(a, q0);
    const float q1 = mul(sub(h1, a), p), e9 = sub(h1, q1), e11 = sub(1.0f, q1);
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (a > ph) { y = mul(k0, ph); fl |= 1; }
        else { y = mul(k1, sub(a, ph)); fl |= 2; }
        y = add(add(y, -1.0f), imp);
        const bool high = ph > e11 || (!(ph > h1) && (ph > e9 || (!(ph > a) && (ph > e8 || (!(ph > a2) && ph > e6)))));
        if (!high) y = sub(-1.0f, imp);
        y = mul(mul(y, gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        ph = add(ph, inc);
        if (fl == 3) { fl = 0; wrap |= bit; }
        if (!(1.0f > ph)) ph = add(ph, -1.0f);
        bit <<= 1;
        imp = mul(imp, dec);
    }
    moveShape();
    return wrap;
}

// NOISE (0x89504a40): integer random values (two states: +0x1dc, cleared at
// note-on, and +0x244), each held for the hold's samples, the hold starting
// again every block; (Impact + value)·gain, the value already ±2^31.  No
// phase, sync, Fat or PW; returns 0.
uint32_t XtAnalogOsc::noise(float* lr) {
    const int n0 = hold > 0 ? hold : 1;        // the firmware never ends a block with a hold of 0; it never gets one
    int left = 32;
    while (left > 0) {
        fl = fl * 0xffffff06u + s2 * 127u + 30u;     // −250·s1 + 127·s2 + 30
        s2 = s2 * 252u + fl;
        const float v = flt((int32_t)fl);
        for (int n = n0; n > 0 && left > 0; --n, --left) {
            const float y = mul(add(imp, v), gainC);
            imp = mul(imp, dec);
            lr[0] = lr[1] = y;
            lr += 2;
            gainC = fmac(sub(gainT, gainC), kStep, gainC);
        }
    }
    return 0;
}

#ifdef XT_ANALOG_MAIN
// xt_analog <rom.2.bin> [cases]: the analog converter (0x894e1920), the
// note-on (0x89504af0), the 10-ms update (0x89505400) and SAW (0x89508c20)
// against the firmware in the SH-4 interpreter.  The system's analog
// calibration (0x894e5f80) is set from the descriptors' defaults; the matrix
// fetch (0x894d96d0) returns the case's modulation, the controller getters a
// zeroed block.
#include "Sh4Interp.h"
#include "XtFirmware.h"
#include <cstdlib>
#include <cstring>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 2 || !xtAnalogLoadTables(argv[1]) || !xtOscLoadTables(argv[1]) || !xtCosmLoadTables(argv[1]) || !xtFwOpen(argv[1])) {
        std::fprintf(stderr, "usage: xt_analog rom.2.bin [cases]\n");
        return 2;
    }
#if defined(__aarch64__)
    {   // denormals flushed, as the SH-4 does (FPSCR.DN = 1) and the plugin's audio thread does
        uint64_t fpcr;
        __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
        __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
    }
#endif
    {
        std::vector<float> v;
        if (!xtFwOscWaveGains(v)) { std::fprintf(stderr, "wave gains failed\n"); return 1; }
        xtOscSetWaveGains(v.data());
    }
    Sh4 c;
    if (!c.loadRom(argv[1])) return 1;
    const uint32_t ram = 0x8C000000, sp = ram + 0xF0000, P = ram, OUT = ram + 0x400, part = ram + 0x800, zero = ram + 0xC00;
    const uint32_t V = ram + 0x10000, EP = ram + 0x18000, EF = ram + 0x18100, EW = ram + 0x18200, EA = ram + 0x18300, LF = ram + 0x18400,
                   BUF = ram + 0x19000;
    c.mapRam(ram, 0x100000);
    auto f32 = [&](uint32_t a) { const uint32_t v = c.rd32(a); float x; std::memcpy(&x, &v, 4); return x; };
    auto wf = [&](uint32_t a, float x) { uint32_t u; std::memcpy(&u, &x, 4); c.wr32(a, u); };
    auto same = [](float a, float b) { return std::memcmp(&a, &b, 4) == 0; };
    uint32_t seed = 12345;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 8) & 0xffff; };
    auto frnd = [&](float lo, float hi) { return lo + (hi - lo) * (float)rnd() / 65535.0f; };
    const int nCases = argc > 2 ? std::atoi(argv[2]) : 3000;

    // the analog calibration, as the system updater writes it from the parameters
    std::vector<uint8_t> z(0x1000, 0);
    c.hook(0x894796d0, [](Sh4& m) { m.ret(0); });
    c.hook(0x8947d310, [](Sh4& m) {
        const int id = (int16_t)m.r[5];
        m.ret(id >= kSys0 && id < kSys0 + kSysN ? (uint32_t)T.sys[id - kSys0] : 0u);
    });
    if (!c.call(0x894e5f80, { 0x89d1b070u, 0, 0, 0 }, sp)) { std::fprintf(stderr, "system: %s\n", c.error().c_str()); return 1; }
    int mtx = 0;
    c.hook(0x894d96d0, [&](Sh4& m) { m.ret((uint32_t)mtx); });
    for (uint32_t a : { 0x894e7010u, 0x894e70b0u, 0x894e7320u, 0x894e7520u }) c.hook(a, [&](Sh4& m) { m.ret(zero); });

    struct Case {
        XtAnalogParams p;
        int key, key2, vel, coarse, kf, fine, gainDb, level, levelKf, fatCurve, fatSens, pwCurve, pwSens;
    };
    auto newCase = [&](bool wide) {
        Case k;
        k.p.wave = (int)(rnd() % 11);
        k.p.impact = (int)(rnd() % 41);
        k.p.pw = (int)(rnd() % 127) - 63; k.p.pwKf = (int)(rnd() % 41) - 20; k.p.pwLfo = (int)(rnd() % 127) - 63; k.p.pwEnv = (int)(rnd() % 127) - 63;
        k.p.fat = (int)(rnd() % 128); k.p.fatKf = (int)(rnd() % 41) - 20; k.p.fatLfo = (int)(rnd() % 127) - 63; k.p.fatEnv = (int)(rnd() % 127) - 63;
        if (rnd() % 4 == 0) k.p.pwKf = k.p.fatKf = 0;
        k.key = (int)(rnd() % 128); k.key2 = (int)(rnd() % 128); k.vel = 1 + (int)(rnd() % 127);
        k.coarse = (int)(rnd() % 97) - 48; k.kf = (rnd() & 1) ? 1000 : (int)(rnd() % 4001) - 2000; k.fine = (int)(rnd() % 1001) - 500;
        if (!wide) k.coarse = (int)(rnd() % 25) - 12;
        k.gainDb = (int)(rnd() % 25) - 12; k.level = (int)(rnd() % 128); k.levelKf = (int)(rnd() % 41) * 10 - 200;
        k.fatCurve = (int)(rnd() % 8); k.fatSens = (int)(rnd() % 127) - 63; k.pwCurve = (int)(rnd() % 8); k.pwSens = (int)(rnd() % 127) - 63;
        return k;
    };
    // the converter's input, as the OSC table updater (0x894e55d0) lays it out
    auto writeInput = [&](const Case& k) {
        c.write(P, z.data(), 0x100); c.write(part, z.data(), 0x200);
        c.wr8(P + 3, (uint8_t)k.key); c.wr8(P + 4, (uint8_t)k.vel); c.wr8(P + 0xa, (uint8_t)(int8_t)k.gainDb);
        c.wr16(P + 0xe, (uint16_t)(int16_t)k.kf); c.wr16(P + 0x68, (uint16_t)(int16_t)k.coarse); c.wr32(P + 0x70, (uint32_t)(int32_t)k.fine);
        c.wr8(P + 0x7c, (uint8_t)k.key2);
        c.wr8(P + 0x1c, (uint8_t)k.level); c.wr16(P + 0x1e, (uint16_t)(int16_t)k.levelKf);
        c.wr8(P + 0x21, (uint8_t)xtVelCurve(1)); c.wr16(P + 0x22, (uint16_t)(int16_t)xtVelSens(0));
        c.wr8(P + 0x24, (uint8_t)k.p.wave); c.wr8(P + 0x25, (uint8_t)k.p.impact); c.wr8(P + 0x26, (uint8_t)(int8_t)k.p.pw);
        c.wr16(P + 0x28, (uint16_t)(int16_t)(k.p.pwKf * 10)); c.wr8(P + 0x2a, (uint8_t)(int8_t)k.p.pwLfo); c.wr8(P + 0x2b, (uint8_t)(int8_t)k.p.pwEnv);
        c.wr8(P + 0x2c, (uint8_t)xtVelCurve(k.pwCurve)); c.wr16(P + 0x2e, (uint16_t)(int16_t)xtVelSens(k.pwSens));
        c.wr8(P + 0x30, (uint8_t)k.p.fat); c.wr16(P + 0x32, (uint16_t)(int16_t)(k.p.fatKf * 10));
        c.wr8(P + 0x34, (uint8_t)(int8_t)k.p.fatLfo); c.wr8(P + 0x35, (uint8_t)(int8_t)k.p.fatEnv);
        c.wr8(P + 0x36, (uint8_t)xtVelCurve(k.fatCurve)); c.wr16(P + 0x38, (uint16_t)(int16_t)xtVelSens(k.fatSens));
        c.wr32(P + 0xdc, part);
    };
    auto velFactor = [&](int vel, int curve, int sens) { return div(flt(xtVelCurveValue(vel, xtVelSens(sens), xtVelCurve(curve))), 32768.0f); };
    auto portBlock = [&](const Case& k) {
        XtAnalogParams q = k.p;
        q.pw += mtx; q.pwLfo += mtx; q.pwEnv += mtx; q.fat += mtx; q.fatLfo += mtx; q.fatEnv += mtx;
        return xtAnalogBlock(q, k.key, xtPitch10(k.key, k.coarse, k.kf, k.fine), k.gainDb, velFactor(k.vel, k.fatCurve, k.fatSens),
                             velFactor(k.vel, k.pwCurve, k.pwSens));
    };

    // The converter
    long cb = 0, cn = 0;
    for (int g = 0; g < 200000; ++g) {
        const Case k = newCase(true);
        mtx = (g % 3 == 0) ? (int)(rnd() % 75) - 37 : 0;
        writeInput(k);
        c.write(OUT, z.data(), 0x100);
        if (!c.call(0x894e1920, { P, OUT }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return 1; }
        const XtAnalogBlock b = portBlock(k);
        ++cn;
        const float fw[10] = { f32(OUT + 0x84), f32(OUT + 0x88), f32(OUT + 0x90), f32(OUT + 0x98), f32(OUT + 0x9c), f32(OUT + 0xa0),
                               f32(OUT + 0xac), f32(OUT + 0xb0), f32(OUT + 0x5c), f32(OUT + 8) };
        const float po[10] = { b.fat, b.fatEnv, b.fatLfo, b.pw, b.pwEnv, b.pwLfo, b.impact, b.slope, b.gain,
                               xtPitchCents(k.key, k.coarse, k.kf, k.fine, 69, false) };
        static const char* name[10] = { "fat", "fat env", "fat lfo", "pw", "pw env", "pw lfo", "impact", "slope", "gain", "cents" };
        bool ok = (int)c.rd32(OUT + 0x80) == b.code && f32(OUT + 0xb4) == 0.0f && c.rd32(OUT + 0x8c) == 3 && c.rd32(OUT + 0x94) == 1
                  && c.rd32(OUT + 0xa4) == 2 && c.rd32(OUT + 0xa8) == 1;
        for (int i = 0; i < 10; ++i) ok = ok && same(fw[i], po[i]);
        if (!ok && cb++ < 8) {
            std::printf("  converter wave %d key %d mtx %d pw %d kf %d fat %d kf %d: code %u/%d", k.p.wave, k.key, mtx, k.p.pw, k.p.pwKf, k.p.fat,
                        k.p.fatKf, c.rd32(OUT + 0x80), b.code);
            for (int i = 0; i < 10; ++i)
                if (!same(fw[i], po[i])) std::printf(" %s %.9g/%.9g", name[i], fw[i], po[i]);
            std::printf("\n");
        }
    }
    std::printf("converter: %ld cases, %ld different\n", cn, cb);

    // The voice: the converter's block, the note-on, the install, then blocks
    // of the installed routine with an update every few blocks, the state,
    // the output and the returned mask compared after each.  Each case
    // takes one of the waveforms the port plays, in turn.
    c.write(V, z.data(), 0x1000);
    c.wr32(V + 0x970, EP); c.wr32(V + 0x974, EF); c.wr32(V + 0x978, EW); c.wr32(V + 0x97c, EA);
    for (uint32_t o = 0x980; o < 0x990; o += 4) c.wr32(V + o, LF);
    static const int waves[7] = { 0, 1, 2, 3, 4, 5, 8 };
    static const char* waveName[7] = { "saw", "square", "triangle", "sine", "ramp", "juno", "noise" };
    static const uint32_t routine[9] = { 0, 0, 0x89508c20, 0x89508aa0, 0x895094c0, 0x89508800, 0x89509030, 0x89508db0, 0x89504a40 };
    XtAnalogOsc osc;
    long nb = 0, nn = 0, wb = 0, db[7] = {}, dn[7] = {}, mb = 0;
    float peak[7] = {};
    int code = 2;
    auto stateSame = [&](bool full) {
        bool ok = same(f32(V + 0x144), osc.ratio()) && same(f32(V + 0x154), osc.gainTarget()) && same(f32(V + 0x1c0), osc.fatTarget())
               && same(f32(V + 0x1c8), osc.pwTarget());
        if (code == 3) ok = ok && same(f32(V + 0x1e0), osc.edge());
        if (code == 8) ok = ok && (int)(int8_t)c.rd8(V + 0x248) == osc.noiseHold();
        if (full)
            ok = ok && same(f32(V + 0x158), osc.gain()) && same(f32(V + 0x1c4), osc.fat()) && same(f32(V + 0x1cc), osc.pw())
                 && same(f32(V + 0x1d0), osc.decay()) && same(f32(V + 0x1d4), osc.phase()) && same(f32(V + 0x1d8), osc.impact())
                 && c.rd32(V + 0x1dc) == osc.flags() && c.rd32(V + 0x244) == osc.noiseState();
        return ok;
    };
    auto dump = [&](const char* what, int g, int blk) {
        std::printf("  %s %d.%d code %d: ratio %.9g/%.9g gain %.9g/%.9g cur %.9g/%.9g fat %.9g/%.9g %.9g/%.9g pw %.9g/%.9g %.9g/%.9g dec %.9g/%.9g "
                    "phase %.9g/%.9g imp %.9g/%.9g flags %u/%u edge %.9g/%.9g noise %u/%u hold %d/%d\n", what, g, blk, code, f32(V + 0x144),
                    osc.ratio(), f32(V + 0x154), osc.gainTarget(), f32(V + 0x158), osc.gain(), f32(V + 0x1c0), osc.fatTarget(), f32(V + 0x1c4),
                    osc.fat(), f32(V + 0x1c8), osc.pwTarget(), f32(V + 0x1cc), osc.pw(), f32(V + 0x1d0), osc.decay(), f32(V + 0x1d4), osc.phase(),
                    f32(V + 0x1d8), osc.impact(), c.rd32(V + 0x1dc), osc.flags(), f32(V + 0x1e0), osc.edge(), c.rd32(V + 0x244), osc.noiseState(),
                    (int)(int8_t)c.rd8(V + 0x248), osc.noiseHold());
    };
    for (int g = 0; g < nCases; ++g) {
        Case k = newCase(false);
        const int wi = g % 7;
        k.p.wave = waves[wi];
        if (g % 8 == 1) {                          // SQUARE's pitch steps: G4, A4, A#4, C#5, F#5 untuned
            static const int steps[5] = { 67, 69, 70, 73, 78 };
            k.key = steps[rnd() % 5]; k.coarse = 0; k.fine = 0; k.kf = 1000;
        }
        code = xtAnalogCode(k.p.wave);
        mtx = 0;
        writeInput(k);
        if (!c.call(0x894e1920, { P, V }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return 1; }
        wf(V + 0x990, 1.0f); wf(V + 0x994, 0.0f);
        const XtAnalogBlock b = portBlock(k);
        // the envelopes' and the LFO's outputs; the amp envelope's attack time for the note-on gain
        float eP = frnd(-1, 1), eF = frnd(0, 1), eW = frnd(0, 1), eA = frnd(0, 1), lfo = frnd(-1, 1);
        if (rnd() % 4 == 0) eF = eW = 0;
        const bool atk = rnd() & 1;
        wf(EP + 0x4c, eP); wf(EF + 0x4c, eF); wf(EW + 0x4c, eW); wf(EA + 0x4c, eA); wf(LF + 0x38, lfo);
        wf(EA + 0x1c, atk ? 25.0f : 0.0f); wf(EA + 0x2c, 0.0f);
        auto ratioOf = [&]() { return xtPitchTarget(f32(V + 8), eP, f32(V + 0x1c), lfo, f32(V + 0x34), f32(V + 0xc)); };
        auto gainOf = [&](bool on) { return xtOscGainTarget(f32(V + 0x5c), f32(V + 0x54), on && atk ? 0.0f : eA, lfo, f32(V + 0x58)); };
        if (!c.call(0x89504af0, { V }, sp)) { std::fprintf(stderr, "note-on: %s\n", c.error().c_str()); return 1; }
        osc.noteOn(b, ratioOf(), gainOf(false), gainOf(true), eF, eW, lfo);
        ++nn;
        if (!stateSame(true) && nb++ < 6) dump("note-on", g, 0);
        if (!c.call(0x89505170, { V }, sp)) { std::fprintf(stderr, "install: %s\n", c.error().c_str()); return 1; }
        const uint32_t fn = c.rd32(V + 0x968);
        if (fn != routine[code] && wb++ < 4) std::printf("  install code %d: routine %08x, the map has %08x\n", code, fn, routine[code]);
        for (int blk = 0; blk < 40; ++blk) {
            if (blk % 5 == 4) {
                eP = frnd(-1, 1); eF = frnd(0, 1); eW = frnd(0, 1); eA = frnd(0, 1); lfo = frnd(-1, 1);
                wf(EP + 0x4c, eP); wf(EF + 0x4c, eF); wf(EW + 0x4c, eW); wf(EA + 0x4c, eA); wf(LF + 0x38, lfo);
                if (!c.call(0x89505400, { V }, sp)) { std::fprintf(stderr, "update: %s\n", c.error().c_str()); return 1; }
                osc.update(b, ratioOf(), gainOf(false), eF, eW, lfo);
                ++nn;
                if (!stateSame(false) && nb++ < 6) dump("update", g, blk);
            }
            if (blk % 6 == 2) {                    // the phase exactly on a breakpoint (as the routine forms it), or 0
                float pts[9];
                const int n = osc.breakpoints(pts);
                pts[n] = 0.0f;
                const float p = pts[rnd() % (uint32_t)(n + 1)];
                if (code != 8 && p >= 0.0f && 1.0f > p) { wf(V + 0x1d4, p); osc.setPhase(p); }
            }
            const uint32_t sync = rnd() % 8 == 0 ? ((uint32_t)rnd() << 16 | rnd()) & ((uint32_t)rnd() << 16 | rnd()) : 0u;
            uint32_t mask = 0;
            if (!c.call(fn, { BUF, V + 0xd4, sync }, sp, &mask)) { std::fprintf(stderr, "%s: %s\n", waveName[wi], c.error().c_str()); return 1; }
            float lr[64];
            const uint32_t pm = osc.process(lr, sync);
            ++dn[wi];
            for (int i = 0; i < 64; i += 2) peak[wi] = std::fmax(peak[wi], std::fabs(lr[i]));
            bool ok = stateSame(true);
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            if (mask != pm && mb++ < 4) std::printf("  %s %d block %d: mask %08x, port %08x\n", waveName[wi], g, blk, mask, pm);
            if (!ok && db[wi]++ < 4) {
                int i = 0;
                while (i < 64 && same(f32(BUF + 4 * (uint32_t)i), lr[i])) ++i;
                std::printf("  %s %d block %d: first difference at %d: fw %.9g port %.9g\n", waveName[wi], g, blk, i,
                            i < 64 ? f32(BUF + 4 * (uint32_t)i) : 0.0f, i < 64 ? lr[i] : 0.0f);
                dump(waveName[wi], g, blk);
            }
        }
    }
    std::printf("note-on and update: %ld cases, %ld different\n", nn, nb);
    std::printf("install: %d cases, %ld routines not as the map\n", nCases, wb);
    long dbad = mb;
    for (int w = 0; w < 7; ++w) {
        std::printf("%s: %ld blocks of 32 frames, %ld different (peak %.4g of 2^31)\n", waveName[w], dn[w], db[w], peak[w] / 2147483648.0f);
        dbad += db[w];
    }
    std::printf("wrap masks: %ld different\n", mb);
    return cb || nb || wb || dbad ? 3 : 0;
}
#endif
