// The XT's VariPhrase OSC converter — see XtOsc.h.  Single precision as the
// SH-4 computes it (XtFloat.h).
#include "XtOsc.h"
#include "XtEnvelope.h"
#include "XtFloat.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

constexpr uint32_t kRomBase = 0x89300000;
constexpr uint32_t kLevelTab = 0x897cfcb8;   // u16 ×128: 32768·(k/127)², truncated (Energy, OSC levels)
// exp2 (0x894ec090): 2^(hi/256) and 2^(lo/65536) for the bytes of a 16-bit
// fraction, 2^n for the integer part; then the same for negative arguments.
constexpr uint32_t kExpHi = 0x897d0228, kExpLo = 0x897d0628, kExpInt = 0x897d1228;
constexpr uint32_t kExpHiNeg = 0x897d0a28, kExpLoNeg = 0x897d0e28, kExpIntNeg = 0x897d1244;
// velocity (0x894ebd60): the sens table (u16 ×200, 65536·s/200) and the
// pointers to the seven 128-byte velocity curves
constexpr uint32_t kVelSens = 0x897cfeda, kVelCurves = 0x897cfb9c;

struct Tables {
    uint16_t level[128];
    float hi[256], lo[256], in[7], hiN[256], loN[256], inN[7];
    float keyScale[41][128];                  // xtOscSetTimeKeyScales
    uint16_t velSens[200];
    uint8_t velCurve[7][128];
    float waveGain[25];                       // xtOscSetWaveGains, −12…+12 dB
    float pitchRatio[127];                    // xtOscSetPitchRatios, Pitch −63…+63
    bool keyScales = false, waveGains = false, pitchRatios = false, ok = false;
} T;

template <class V> bool readAt(FILE* f, uint32_t addr, V* v, size_t n) {
    return std::fseek(f, (long)(addr - kRomBase), SEEK_SET) == 0 && std::fread(v, sizeof(V), n, f) == n;
}

// 2^(kf·(key − 60)/1200): the exponent as 0x894e0da0 forms it, then powf
float timeKeyScale(int kf, int key) {
    if (kf == 0) return 1.0f;
    if (T.keyScales && kf >= -200 && kf <= 200 && kf % 10 == 0 && key >= 0 && key < 128)
        return T.keyScale[(kf + 200) / 10][key];
    const float e = xtf::div(xtf::flt((int16_t)kf * (int16_t)(key - 60)), 1200.0f);
    return (float)std::pow(2.0, (double)e);
}

// The depth curve the time envelope and LFO depths share: d/20 within ±60,
// then on to ±5 at ±63 (2d − 120 is formed with FMAC).
float timeDepth(int d) {
    using namespace xtf;
    const float x = flt(d);
    if (x > 60.0f) return add(3.0f, div(add(mul(x, 2.0f), -120.0f), 3.0f));
    if (-60.0f > x) return add(-3.0f, div(add(mul(x, -2.0f), -120.0f), -3.0f));
    return add(div(x, 20.0f), 0.0f);
}

int clamp63(int d) { return d > 63 ? 63 : d < -63 ? -63 : d; }

} // namespace

bool xtOscLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    uint32_t curvePtr[7];
    bool ok = readAt(f, kLevelTab, T.level, 128)
           && readAt(f, kExpHi, T.hi, 256) && readAt(f, kExpLo, T.lo, 256) && readAt(f, kExpInt, T.in, 7)
           && readAt(f, kExpHiNeg, T.hiN, 256) && readAt(f, kExpLoNeg, T.loN, 256) && readAt(f, kExpIntNeg, T.inN, 7)
           && readAt(f, kVelSens, T.velSens, 200) && readAt(f, kVelCurves, curvePtr, 7);
    for (int c = 0; ok && c < 7; ++c)
        ok = curvePtr[c] >= kRomBase && curvePtr[c] < 0x89900000u && readAt(f, curvePtr[c], T.velCurve[c], 128);
    std::fclose(f);
    if (!ok) return false;
    // the sens table rises from 0; each curve runs from about 255 down to 0
    if (T.velSens[0] != 0 || !(T.velSens[199] > T.velSens[100]) || T.velSens[100] != 32768) return false;
    for (int c = 0; c < 7; ++c)
        if (T.velCurve[c][127] != 0 || T.velCurve[c][0] < 200) return false;
    // Sanity: the level curve runs 0…32768 and rises; the exp2 tables start
    // at 1 and the integer ones double and halve.
    if (T.level[0] != 0 || T.level[127] != 32768) return false;
    for (int i = 1; i < 128; ++i)
        if (!(T.level[i] > T.level[i - 1])) return false;
    if (T.hi[0] != 1.0f || T.lo[0] != 1.0f || T.hiN[0] != 1.0f || T.loN[0] != 1.0f) return false;
    for (int i = 0; i < 7; ++i)
        if (T.in[i] != std::ldexp(1.0f, i) || T.inN[i] != std::ldexp(1.0f, -i)) return false;
    T.ok = true;
    return true;
}

bool xtOscTablesLoaded() { return T.ok; }

void xtOscSetTimeKeyScales(const float* t) {
    for (int i = 0; i < 41; ++i)
        for (int k = 0; k < 128; ++k) T.keyScale[i][k] = t[i * 128 + k];
    T.keyScales = true;
}

float xtOscEnergy(int k) {
    if (!T.ok || k <= 0) return 0.0f;
    return xtf::div((float)T.level[k > 127 ? 127 : k], 32768.0f);
}

float xtOscTime(int t, int offset, int kf, int key) {
    using namespace xtf;
    // Time Offset (+0x61, 0x894e31d6): BWD −1, ZERO 0, FWD and above +1
    const float base = offset == 0 ? -1.0f : offset == 1 ? 0.0f : 1.0f;
    const float s = timeKeyScale(kf, key);
    // x = t + controller·63/12000, the controller 0
    float x = flt((int16_t)t), y;
    bool up = false, down = false;
    if (x > 63.0f) { x = 63.0f; up = true; }
    else if (-63.0f > x) { x = -63.0f; down = true; }
    else if (x > 60.0f) up = true;
    else if (-60.0f > x) down = true;
    if (up) {
        const float u = add(base, 3.0f);
        y = add(div(mul(sub(5.0f, u), add(x, -60.0f)), 3.0f), u);
    } else if (down) {
        const float u = add(base, -3.0f);
        y = add(div(mul(sub(-5.0f, u), add(x, 60.0f)), -3.0f), u);
    } else {
        y = add(div(x, 20.0f), base);
    }
    // + (b1 − 3·b0)/127 from the part's bytes at +0x180 (0 with no controller)
    y = mul(y, s);
    return y > 5.0f ? 5.0f : (-5.0f > y ? -5.0f : y);
}

float xtOscTimeEnvDepth(int d) { return timeDepth((int16_t)d); }          // × the velocity factor (sens 0: 1)
float xtOscTimeLfoDepth(int d) { return timeDepth(clamp63((int16_t)d)); }

float xtOscFormant(int f, int kf, int key) {
    using namespace xtf;
    const float k = div(flt((int16_t)(key - 60) * (int16_t)kf), 1200.0f);
    // f·12000/63 + the controller (0), within ±12000, back to /63
    float v = div(mul(flt((int16_t)f), 12000.0f), 63.0f);
    if (v > 12000.0f) v = 12000.0f;
    else if (-12000.0f > v) v = -12000.0f;
    const float r = add(div(mul(div(v, 12000.0f), 63.0f), 63.0f), k);
    return r > 1.0f ? 1.0f : (-1.0f > r ? -1.0f : r);
}

float xtOscFormantEnvDepth(int d) { return xtf::div(xtf::flt((int16_t)d), 63.0f); }   // × the velocity factor (1)
float xtOscFormantLfoDepth(int d) { return xtf::div(xtf::flt(clamp63((int16_t)d)), 63.0f); }

float xtOscExp2(float x) {
    using namespace xtf;
    const bool neg = 0.0f > x;
    if (neg) x = -x;
    int n = ftrc(x);
    const int q = ftrc(mul(sub(x, flt(n)), 65536.0f));    // the fraction, 16 bits
    if (n > 6) n = 6;                                      // the integer tables end at 2^±6
    const float* hi = neg ? T.hiN : T.hi;
    const float* lo = neg ? T.loN : T.lo;
    const float* in = neg ? T.inN : T.in;
    return mul(mul(hi[(q >> 8) & 0xff], in[n]), lo[q & 0xff]);
}

float xtOscWalk(float time, float env, float envDepth, float lfo, float lfoDepth) {
    using namespace xtf;
    // (time + (env·depth + lfo·depth)) × the Time Trip factor (0x894f4c00: 1
    // without Time Trip), clamped
    const float w = add(time, add(mul(env, envDepth), mul(lfo, lfoDepth)));
    return w > 5.0f ? 5.0f : (-5.0f > w ? -5.0f : w);
}

float xtOscFormantRatio(float formant, float env, float envDepth, float lfo, float lfoDepth) {
    using namespace xtf;
    const float r = xtOscExp2(add(formant, add(mul(env, envDepth), mul(lfo, lfoDepth))));
    return r > 2.0f ? 2.0f : (0.5f > r ? 0.5f : r);
}

int xtPitch10(int key, int coarse, int kf, int fine) {
    const int k = (uint8_t)key, c = (int16_t)coarse, f = (int16_t)kf;
    if (f == 0) return (60 + c) * 1000 + fine;
    if (f == 1000) return (k + c) * 1000 + fine;
    return (60 + c) * 1000 + (k - 60) * f + fine;
}

float xtPitchCents(int key, int coarse, int kf, int fine, int origKey, bool robot) {
    using namespace xtf;
    // the Original Key less the wave's shift (+0x88, +0x85: 0 here), ×1000
    const int ref = robot ? 69000 : (int16_t)origKey * 1000;
    const float c = div(flt(xtPitch10(key, coarse, kf, fine) - ref), 10.0f);
    return c > 6000.0f ? 6000.0f : (-6000.0f > c ? -6000.0f : c);
}

void xtOscSetPitchRatios(const float* r) {
    for (int i = 0; i < 127; ++i) T.pitchRatio[i] = r[i];
    T.pitchRatios = true;
}

float xtPitchRatio(int pitch) {
    using namespace xtf;
    // knob + modulation + controller·63/12000 (no controller), within ±63
    const int p = clamp63((int16_t)pitch);
    if (T.pitchRatios) return T.pitchRatio[p + 63];
    // + (2·b183 − b182)/127 from the part's bytes at +0x180 (0 with no controller)
    const float e = add(div(0.0f, 127.0f), div(xtPitchDepthCents(flt(p)), 1200.0f));
    return (float)std::pow(2.0, (double)e);
}

float xtPitchTarget(float cents, float env, float envDepth, float lfo, float lfoDepth, float ratio,
                    float ramp, float offset, float rate, float detune) {
    using namespace xtf;
    constexpr float kPerCent = 0x1.b4e81ap-11f;           // 1/1200 as rom.2 stores it (0x3a5a740d)
    float c = add(mul(env, envDepth), cents);              // fmac: product, then sum
    c = add(mul(lfo, lfoDepth), c);
    c = add(add(c, ramp), offset);
    c = c > 6000.0f ? 6000.0f : (-6000.0f > c ? -6000.0f : c);
    float r = mul(mul(xtOscExp2(mul(c, kPerCent)), ratio), rate);
    r = r > 32.0f ? 32.0f : (-32.0f > r ? -32.0f : r);
    return mul(r, xtOscExp2(mul(-detune, kPerCent)));
}

float xtOscSmooth(float cur, float target) {
    using namespace xtf;
    return add(mul(sub(target, cur), 0.699999988f), cur);  // 0x3f333333
}

int xtVelSens(int s) {
    if (-32 > s) return -(100 + (-32 - s) * 100 / 31);
    if (0 > s) return -(-s * 100 / 32);
    if (32 >= s) return s * 100 / 32;
    return 100 + (s - 32) * 100 / 31;
}

int xtVelCurve(int c) { return c >= 1 && c <= 7 ? c - 1 : 7; }

int xtVelCurveValue(int velocity, int sens, int curve) {
    if (sens == 0 || curve >= 7 || curve < 0 || !T.ok) return 0x8000;
    int s;
    if (sens > 0) s = sens >= 200 ? 0x10000 : T.velSens[sens];
    else s = -200 < sens ? -(int)T.velSens[-sens] : -0x10000;
    const int v = velocity < 0 ? 0 : velocity > 127 ? 127 : velocity;
    int r = (s * (256 - T.velCurve[curve][v])) >> 8;
    r += 0x8000;
    if (s > 0) r -= s;
    return r > 0x8000 ? 0x8000 : r < 0 ? 0 : r;
}

float xtVelGain(int velocity, int patchSens, int patchCurve) {
    using namespace xtf;
    const float c = div(flt(xtVelCurveValue(velocity, xtVelSens(patchSens), xtVelCurve(patchCurve))), 32768.0f);
    return mul(c, c);
}

void xtOscSetWaveGains(const float* g) {
    for (int i = 0; i < 25; ++i) T.waveGain[i] = g[i];
    T.waveGains = true;
}

float xtOscLevel(int level, int kf, int key, float velGain) {
    using namespace xtf;
    int x = level > 127 ? 127 : level < 0 ? 0 : level;
    // 0x894e0c90: 127·kf·(key − 60)/6000 by a reciprocal multiply, toward zero
    const int32_t p = 127 * (int32_t)(int16_t)kf * (int32_t)(int16_t)(key - 60);
    int32_t q = (int32_t)(((int64_t)p * 0x057619f1) >> 32) >> 7;
    q += (int32_t)((uint32_t)q >> 31);
    x = (int16_t)(x + q);
    x = x > 127 ? 127 : x < 0 ? 0 : x;
    return mul(div((float)T.level[x], 32768.0f), velGain);
}

float xtOscLevelLfoDepth(int d) {
    using namespace xtf;
    const int x = clamp63((int16_t)d);
    const float v = div(flt(x), 63.0f);
    return mul(x < 0 ? -v : v, v);
}

float xtOscGain(int db) {
    using namespace xtf;
    db = db > 12 ? 12 : db < -12 ? -12 : db;
    const float g = T.waveGains ? T.waveGain[db + 12] : (float)std::pow(2.0, (double)div(flt(db), 6.0f));
    // × level curve(+0x84)/32768 × level curve(+0x86)/32768, both full
    return mul(mul(div((float)T.level[127], 32768.0f), g), div((float)T.level[127], 32768.0f));
}

float xtOscGainTarget(float gain, float level, float env, float lfo, float lfoDepth) {
    using namespace xtf;
    float a = add(mul(lfo, lfoDepth), 1.0f);                         // fmac: product, then sum
    a = mul(a, mul(env, level));
    a = a > 1.0f ? 1.0f : (-1.0f > a ? -1.0f : a);
    return mul(gain, a);
}

#ifdef XT_OSC_MAIN
// xt_osc <rom.2.bin> : the firmware's VariPhrase converter (0x894e2f20) run
// in the SH-4 interpreter beside the port, over knob + matrix sums −100…+100,
// the three Time Offsets, Time and Formant KF and every key; and the
// firmware's exp2 (0x894ec090) on a grid.  The matrix fetch (0x894d96d0)
// is hooked to return the case's modulation; the controller getters return
// a zeroed block, as with no controller assigned.  Run it without XT_ROUND:
// this interpreter rounds toward zero, as the SH-4 does.
#include "Sh4Interp.h"
#include "XtFirmware.h"
#include <cstring>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 2 || !xtOscLoadTables(argv[1]) || !xtEnvLoadTables(argv[1]) || !xtFwOpen(argv[1])) { std::fprintf(stderr, "usage: xt_osc rom.2.bin\n"); return 2; }
    std::vector<float> ks;
    if (!xtFwOscTimeKeyScales(ks)) { std::fprintf(stderr, "time key scales failed\n"); return 1; }
    xtOscSetTimeKeyScales(ks.data());
    if (!xtFwOscWaveGains(ks)) { std::fprintf(stderr, "wave gains failed\n"); return 1; }
    xtOscSetWaveGains(ks.data());
    {   // the Pitch ratios: std::pow before the firmware's powf is loaded
        std::vector<float> pr;
        if (!xtFwOscPitchRatios(pr)) { std::fprintf(stderr, "pitch ratios failed\n"); return 1; }
        int d = 0;
        for (int p = -63; p <= 63; ++p) d += xtPitchRatio(p) != pr[(size_t)(p + 63)];
        xtOscSetPitchRatios(pr.data());
        std::printf("pitch ratios: std::pow differs from the firmware's powf on %d of 127\n", d);
    }
    Sh4 c;
    if (!c.loadRom(argv[1])) return 1;
    const uint32_t ram = 0x8C000000, P = ram, out = ram + 0x400, part = ram + 0x800, zero = ram + 0xC00, sp = ram + 0xF0000;
    c.mapRam(ram, 0x100000);
    int mtx = 0;
    c.hook(0x894d96d0, [&](Sh4& m) { m.ret((uint32_t)mtx); });
    for (uint32_t a : { 0x894e7010u, 0x894e70b0u, 0x894e7320u, 0x894e7520u }) c.hook(a, [&](Sh4& m) { m.ret(zero); });
    auto f32 = [&](uint32_t a) { const uint32_t v = c.rd32(a); float x; std::memcpy(&x, &v, 4); return x; };
    auto bitsEq = [](float a, float b) { return std::memcmp(&a, &b, 4) == 0; };
    long cases = 0, bad = 0;
    float lo[6], hi[6];
    for (int i = 0; i < 6; ++i) lo[i] = hi[i] = 0.0f;
    const int kfs[] = { -200, -150, -100, -30, 0, 10, 50, 100, 170, 200 };
    for (int sum = -100; sum <= 100; ++sum) {
        const int knob = sum > 63 ? 63 : sum < -63 ? -63 : sum;
        mtx = sum - knob;
        for (int off = 0; off < 3; ++off)
            for (int kf : kfs)
                for (int key = 0; key < 128; key += (kf == 0 ? 1 : 3)) {
                    std::vector<uint8_t> z(0x400, 0);
                    c.write(P, z.data(), z.size()); c.write(out, z.data(), 0x100); c.write(part, z.data(), 0x200);
                    c.wr8(P + 3, (uint8_t)key); c.wr8(P + 4, 100);
                    for (uint32_t o : { 0x48u, 0x4cu, 0x4du, 0x52u, 0x56u, 0x57u }) c.wr8(P + o, (uint8_t)(int8_t)knob);
                    c.wr16(P + 0x4a, (uint16_t)(int16_t)kf); c.wr16(P + 0x54, (uint16_t)(int16_t)kf);
                    c.wr8(P + 0x61, (uint8_t)off);
                    c.wr8(P + 0x87, 6);                                  // no encode-type tail
                    c.wr32(P + 0xdc, part);
                    if (!c.call(0x894e2f20, { P, out }, sp)) { std::fprintf(stderr, "convert: %s\n", c.error().c_str()); return 1; }
                    const float fw[6] = { f32(out + 0x10), f32(out + 0x14), f32(out + 0x24), f32(out + 0x2c), f32(out + 0x3c), f32(out + 0x44) };
                    const float po[6] = { xtOscTime(sum, off, kf, key), xtOscFormant(sum, kf, key), xtOscTimeEnvDepth(sum),
                                          xtOscFormantEnvDepth(sum), xtOscTimeLfoDepth(sum), xtOscFormantLfoDepth(sum) };
                    static const char* name[6] = { "time", "formant", "time env depth", "formant env depth", "time LFO depth", "formant LFO depth" };
                    ++cases;
                    for (int i = 0; i < 6; ++i) { lo[i] = std::fmin(lo[i], fw[i]); hi[i] = std::fmax(hi[i], fw[i]); }
                    for (int i = 0; i < 6; ++i)
                        if (!bitsEq(fw[i], po[i]) && bad++ < 12)
                            std::printf("  %s: sum %d offset %d kf %d key %d: fw %.9g port %.9g\n", name[i], sum, off, kf, key, fw[i], po[i]);
                }
    }
    std::printf("converter: %ld cases × 6 values, %ld different\n", cases, bad);
    std::printf("  firmware ranges: time %g…%g, formant %g…%g, time env depth %g…%g, formant env depth %g…%g,\n"
                "  time LFO depth %g…%g, formant LFO depth %g…%g\n", lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], lo[3], hi[3],
                lo[4], hi[4], lo[5], hi[5]);
    std::printf("  e.g. FWD at key 60: Time −20 → %g, +20 → %g, +63 → %g; Formant +21 → %g octave\n",
                xtOscTime(-20, 2, 0, 60), xtOscTime(20, 2, 0, 60), xtOscTime(63, 2, 0, 60), xtOscFormant(21, 0, 60));
    long eb = 0, en = 0;
    for (int i = -60000; i <= 60000; i += 7) {
        const float x = (float)i / 10007.0f;
        uint32_t xb; std::memcpy(&xb, &x, 4); c.fb[0][4] = xb;
        if (!c.call(0x894ec090, {}, sp)) { std::fprintf(stderr, "exp2: %s\n", c.error().c_str()); return 1; }
        float fw; std::memcpy(&fw, &c.fb[0][0], 4);
        ++en;
        if (!bitsEq(fw, xtOscExp2(x)) && eb++ < 5) std::printf("  exp2(%.9g): fw %.9g port %.9g\n", x, fw, xtOscExp2(x));
    }
    std::printf("exp2: %ld arguments, %ld different\n", en, eb);

    // Velocity: the curve function (0x894ebd60) over every velocity, sens
    // −210…+210 and curve 0…8; the patch converters 0x894ebe20 / 0x894ebed0.
    long vb = 0, vn = 0;
    for (int curve = 0; curve <= 8; ++curve)
        for (int sens = -210; sens <= 210; ++sens)
            for (int vel = 0; vel < 128; ++vel) {
                uint32_t r0 = 0;
                if (!c.call(0x894ebd60, { (uint32_t)vel, (uint32_t)sens, (uint32_t)curve }, sp, &r0)) { std::fprintf(stderr, "vel: %s\n", c.error().c_str()); return 1; }
                ++vn;
                if ((int)(r0 & 0xffff) != xtVelCurveValue(vel, sens, curve) && vb++ < 5)
                    std::printf("  velocity %d sens %d curve %d: fw %u port %d\n", vel, sens, curve, r0 & 0xffff, xtVelCurveValue(vel, sens, curve));
            }
    for (int s = -100; s <= 100; ++s) {
        uint32_t r0 = 0;
        c.call(0x894ebe20, { (uint32_t)s }, sp, &r0);
        ++vn;
        if ((int16_t)r0 != xtVelSens(s) && vb++ < 10) std::printf("  sens %d: fw %d port %d\n", s, (int16_t)r0, xtVelSens(s));
    }
    for (int k = -3; k <= 12; ++k) {
        uint32_t r0 = 0;
        c.call(0x894ebed0, { (uint32_t)k }, sp, &r0);
        ++vn;
        if ((int)r0 != xtVelCurve(k) && vb++ < 15) std::printf("  curve %d: fw %d port %d\n", k, (int)r0, xtVelCurve(k));
    }
    std::printf("velocity: %ld cases, %ld different\n", vn, vb);

    // The level slots of the converter (+0x54 level, +0x58 LFO depth, +0x5c gain)
    long lb = 0, ln = 0;
    mtx = 0;
    const int lkfs[] = { -200, -110, -30, 0, 50, 200 };
    const int sens[][2] = { { 0, 1 }, { 32, 1 }, { 63, 2 }, { -40, 3 }, { 7, 4 }, { 33, 5 }, { -63, 6 }, { 50, 7 }, { 20, 0 } };
    for (int level = 0; level < 128; ++level)
        for (int kf : lkfs)
            for (int key = 0; key < 128; key += 5)
                for (const auto& sv : sens) {
                    std::vector<uint8_t> z(0x400, 0);
                    c.write(P, z.data(), z.size()); c.write(out, z.data(), 0x100); c.write(part, z.data(), 0x200);
                    const int vel = (level * 7 + key) & 127, gainDb = (level % 25) - 12, lfo = ((level * 3 + kf) % 200 + 200) % 200 - 100;
                    c.wr8(P + 3, (uint8_t)key); c.wr8(P + 4, (uint8_t)vel);
                    c.wr8(P + 0xa, (uint8_t)(int8_t)gainDb);
                    c.wr8(P + 0x1c, (uint8_t)level); c.wr16(P + 0x1e, (uint16_t)(int16_t)kf); c.wr8(P + 0x20, (uint8_t)(int8_t)lfo);
                    c.wr8(P + 0x21, (uint8_t)xtVelCurve(sv[1])); c.wr16(P + 0x22, (uint16_t)(int16_t)xtVelSens(sv[0]));
                    c.wr8(P + 0x84, 127); c.wr8(P + 0x86, 127);
                    c.wr8(P + 0x61, 2); c.wr8(P + 0x87, 6);
                    c.wr32(P + 0xdc, part);
                    if (!c.call(0x894e2f20, { P, out }, sp)) { std::fprintf(stderr, "convert: %s\n", c.error().c_str()); return 1; }
                    const float fw[3] = { f32(out + 0x54), f32(out + 0x58), f32(out + 0x5c) };
                    const float po[3] = { xtOscLevel(level, kf, key, xtVelGain(vel, sv[0], sv[1])), xtOscLevelLfoDepth(lfo), xtOscGain(gainDb) };
                    static const char* name[3] = { "level", "level LFO depth", "gain" };
                    ++ln;
                    for (int i = 0; i < 3; ++i)
                        if (!bitsEq(fw[i], po[i]) && lb++ < 8)
                            std::printf("  %s: level %d kf %d key %d vel %d sens %d curve %d gain %d: fw %.9g port %.9g\n", name[i], level, kf, key, vel,
                                        sv[0], sv[1], gainDb, fw[i], po[i]);
                }
    std::printf("level slots: %ld cases × 3 values, %ld different\n", ln, lb);

    // The gain target (0x894f3dc0) and its note-on form (0x894f3e20) on a
    // voice whose OSC amp envelope and LFO are given objects.
    const uint32_t V = ram + 0x10000, ENV = ram + 0x18000, LFO = ram + 0x18200;
    auto wf = [&](uint32_t a, float x) { uint32_t u; std::memcpy(&u, &x, 4); c.wr32(a, u); };
    long gb = 0, gn = 0;
    const float envs[] = { 0.0f, 0.13f, 0.5f, 0.99999f, 1.0f }, lfos[] = { -1.0f, -0.37f, 0.0f, 0.62f, 1.0f };
    const float depths[] = { -1.0f, -0.25f, 0.0f, 0.3f, 1.0f }, levels[] = { 0.0f, 0.2f, 0.75f, 1.0f }, gains[] = { 0.25f, 1.0f, 4.0f };
    for (float e : envs) for (float l : lfos) for (float d : depths) for (float lv : levels) for (float g : gains) for (int atk = 0; atk < 2; ++atk) {
        std::vector<uint8_t> z(0x1000, 0);
        c.write(V, z.data(), z.size());
        c.write(ENV, z.data(), 0x100); c.write(LFO, z.data(), 0x100);
        c.wr32(V + 0x97c, ENV); c.wr32(V + 0x98c, LFO);
        wf(ENV + 0x4c, e); wf(ENV + 0x1c, atk ? 25.0f : 0.0f); wf(ENV + 0x2c, 0.0f);
        wf(LFO + 0x38, l);
        wf(V + 0x54, lv); wf(V + 0x58, d); wf(V + 0x5c, g);
        for (int on = 0; on < 2; ++on) {
            if (!c.call(on ? 0x894f3e20 : 0x894f3dc0, { V }, sp)) { std::fprintf(stderr, "gain: %s\n", c.error().c_str()); return 1; }
            float fw; std::memcpy(&fw, &c.fb[0][0], 4);
            const float po = xtOscGainTarget(g, lv, on && atk ? 0.0f : e, l, d);
            ++gn;
            if (!bitsEq(fw, po) && gb++ < 5) std::printf("  gain target%s: env %g lfo %g depth %g level %g gain %g: fw %.9g port %.9g\n",
                                                          on ? " (note-on)" : "", e, l, d, lv, g, fw, po);
        }
    }
    std::printf("gain target: %ld cases, %ld different\n", gn, gb);

    // Pitch.  0x894e1250 (its fifth argument, the centre key, on the stack)
    // over keys, Coarse, Pitch KF and Fine.
    long pb = 0, pn = 0;
    const int pkfs[] = { -2000, -1500, -1000, -990, -10, 0, 10, 500, 990, 1000, 1010, 2000 };
    for (int key = 0; key < 128; ++key)
        for (int coarse = -60; coarse <= 60; coarse += 4)
            for (int kf : pkfs)
                for (int fine : { -500, -37, 0, 1, 499 }) {
                    c.wr32(sp, 60);
                    uint32_t r0 = 0;
                    if (!c.call(0x894e1250, { (uint32_t)key, (uint32_t)(int32_t)coarse, (uint32_t)(int32_t)kf, (uint32_t)(int32_t)fine }, sp, &r0)) {
                        std::fprintf(stderr, "pitch10: %s\n", c.error().c_str()); return 1;
                    }
                    ++pn;
                    if ((int32_t)r0 != xtPitch10(key, coarse, kf, fine) && pb++ < 5)
                        std::printf("  pitch10 key %d coarse %d kf %d fine %d: fw %d port %d\n", key, coarse, kf, fine, (int32_t)r0, xtPitch10(key, coarse, kf, fine));
                }
    std::printf("pitch10: %ld cases, %ld different\n", pn, pb);

    // The converter's pitch slots with the SOLO tail (+0x87 = 1, playback
    // mode RETRIGGER): +8 and +0x68 (the cents at the key and at +0x7c's key),
    // +0xc (the Pitch ratio), +0x1c and +0x34 (the envelope and LFO depths),
    // over knob + matrix sums, keys, Coarse, KF, Fine, Original Key, the
    // wave's shift (+0x85) and Robot.
    long cb = 0, cn = 0;
    const int ckfs[] = { -2000, -730, 0, 500, 1000, 2000 };
    for (int sum = -100; sum <= 100; ++sum) {
        const int knob = sum > 63 ? 63 : sum < -63 ? -63 : sum;
        mtx = sum - knob;
        for (int kf : ckfs)
            for (int key = (sum + 100) % 9; key < 128; key += 9) {
                const int coarse = ((sum * 7 + key) % 97) - 48, fine = ((key * 13 + sum) % 1001) - 500;
                const int orig = (key * 5 + 3 * sum + 300) % 128, shift = ((sum + key) % 25) - 12, key2 = (key * 3 + 11) % 128;
                const bool robot = ((key + sum) & 3) == 1;
                std::vector<uint8_t> z(0x400, 0);
                c.write(P, z.data(), z.size()); c.write(out, z.data(), 0x100); c.write(part, z.data(), 0x200);
                c.wr8(P + 3, (uint8_t)key); c.wr8(P + 4, 100);
                for (uint32_t o : { 0x0cu, 0x10u, 0x11u }) c.wr8(P + o, (uint8_t)(int8_t)knob);
                c.wr16(P + 0xe, (uint16_t)(int16_t)kf); c.wr16(P + 0x68, (uint16_t)(int16_t)coarse); c.wr32(P + 0x70, (uint32_t)(int32_t)fine);
                c.wr8(P + 0x7c, (uint8_t)key2); c.wr8(P + 0x85, (uint8_t)(int8_t)shift); c.wr8(P + 0x88, (uint8_t)orig);
                c.wr8(P + 0x5d, robot ? 1 : 0); c.wr8(P + 0x60, robot ? 1 : 0);
                c.wr8(P + 0x61, 2); c.wr8(P + 0x87, 1);
                c.wr32(P + 0xdc, part);
                if (!c.call(0x894e2f20, { P, out }, sp)) { std::fprintf(stderr, "convert: %s\n", c.error().c_str()); return 1; }
                const float fw[5] = { f32(out + 8), f32(out + 0x68), f32(out + 0xc), f32(out + 0x1c), f32(out + 0x34) };
                const float dc = xtPitchDepthCents((float)knob);
                const float po[5] = { xtPitchCents(key, coarse, kf, fine, orig - shift, robot), xtPitchCents(key2, coarse, kf, fine, orig - shift, robot),
                                      xtPitchRatio(sum), dc, dc };
                static const char* name[5] = { "cents", "cents (+0x7c)", "pitch ratio", "env depth", "LFO depth" };
                ++cn;
                for (int i = 0; i < 5; ++i)
                    if (!bitsEq(fw[i], po[i]) && cb++ < 10)
                        std::printf("  %s: sum %d kf %d key %d coarse %d fine %d orig %d shift %d robot %d: fw %.9g port %.9g\n", name[i], sum, kf, key,
                                    coarse, fine, orig, shift, (int)robot, fw[i], po[i]);
                if (((c.rd8(out + 0xb4) & 0x10) != 0) != robot && cb++ < 10) std::printf("  robot flag: key %d sum %d\n", key, sum);
            }
    }
    std::printf("pitch slots: %ld cases × 5 values, %ld different\n", cn, cb);

    // The pitch target (0x894f3a00 note-on, 0x894f3ba0 update) on a voice
    // with given envelope and LFO objects, the portamento ramp (+0x880)
    // inactive or running, and the wave's +0x990/+0x994/+0x998; and the voice
    // update's 70% step (0x8950c1b0, its pitch slot +0x144).
    const uint32_t RAMP = V + 0x880;
    long tb = 0, tn = 0, sb = 0, sn = 0;
    const float tcents[] = { -9000.0f, -6000.0f, -1234.5f, -100.0f, -0.3f, 0.0f, 0.1f, 37.0f, 700.0f, 1200.0f, 5999.9f, 6000.0f, 7000.0f };
    const float tenv[] = { 0.0f, 0.25f, 1.0f }, tedep[] = { -6000.0f, -100.0f, 0.0f, 40.0f, 1200.0f };
    const float tlfo[] = { -1.0f, -0.4f, 0.0f, 0.7f, 1.0f }, tldep[] = { -1200.0f, 0.0f, 13.0f, 6000.0f };
    const float tratio[] = { 0.03125f, 0.5f, 1.0f, 1.0594631f, 32.0f };
    for (float ce : tcents) for (float e : tenv) for (float ed : tedep) for (float l : tlfo) for (float ld : tldep) for (float ra : tratio)
        for (int variant = 0; variant < 3; ++variant) {
            std::vector<uint8_t> z(0x1000, 0);
            c.write(V, z.data(), z.size());
            c.write(ENV, z.data(), 0x100); c.write(LFO, z.data(), 0x100);
            for (uint32_t o = 0; o < 16; o += 4) { c.wr32(V + 0x970 + o, ENV); c.wr32(V + 0x980 + o, LFO); }
            wf(ENV + 0x4c, e); wf(LFO + 0x38, l);
            wf(V + 8, ce); wf(V + 0xc, ra); wf(V + 0x1c, ed); wf(V + 0x34, ld);
            float ramp = 0.0f, rate = 1.0f, offset = 0.0f, detune = 0.0f;
            if (variant == 1) { ramp = -250.0f; c.wr32(RAMP + 4, 1); wf(RAMP + 0x3c, ramp); }
            if (variant == 2) { rate = 44000.0f / 44100.0f; offset = 12.5f; detune = -7.25f; }
            wf(V + 0x990, rate); wf(V + 0x994, offset); wf(V + 0x998, detune);
            const float po = xtPitchTarget(ce, e, ed, l, ld, ra, ramp, offset, rate, detune);
            for (int on = 0; on < 2; ++on) {
                if (!c.call(on ? 0x894f3a00 : 0x894f3ba0, { V }, sp)) { std::fprintf(stderr, "pitch target: %s\n", c.error().c_str()); return 1; }
                float fw; std::memcpy(&fw, &c.fb[0][0], 4);
                ++tn;
                if (!bitsEq(fw, po) && tb++ < 8)
                    std::printf("  pitch target%s: cents %g env %g×%g lfo %g×%g ratio %g variant %d: fw %.9g port %.9g\n", on ? " (note-on)" : "",
                                ce, e, ed, l, ld, ra, variant, fw, po);
            }
            if (variant == 0 && e != 1.0f) {
                const float cur = 0.37f * ra + 0.001f * ce;
                wf(V + 0x144, cur);
                if (!c.call(0x8950c1b0, { V }, sp)) { std::fprintf(stderr, "voice update: %s\n", c.error().c_str()); return 1; }
                ++sn;
                if (!bitsEq(f32(V + 0x144), xtOscSmooth(cur, po)) && sb++ < 5)
                    std::printf("  70%% step from %.9g to %.9g: fw %.9g port %.9g\n", cur, po, f32(V + 0x144), xtOscSmooth(cur, po));
            }
        }
    std::printf("pitch target: %ld cases, %ld different; voice update step: %ld cases, %ld different\n", tn, tb, sn, sb);
    return bad || eb || vb || lb || gb || pb || cb || tb || sb ? 3 : 0;
}
#endif
