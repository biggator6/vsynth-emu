// The XT's COSM TVF — see XtCosm.h.  Single precision as the SH-4 computes
// it (XtFloat.h).
#include "XtCosm.h"
#include "XtFloat.h"
#include "XtOsc.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>

namespace {

constexpr uint32_t kRomBase = 0x89300000;
// the cutoff table (129 floats) and the cap by Q (128 floats), 0x894eecb0
constexpr uint32_t kCutTab = 0x897d1264, kCapTab = 0x897d1468;
// the TB Filter: the exp2 tables (0x894ec090), read as one span from
// 0x897d0228 to the cap table's end (2^(i/256), 2^(i/65536), their inverses,
// 2^i, 2^−i); the gain table (128 shorts, 0x894de110)
constexpr uint32_t kExpTab = 0x897d0228, kGainTab = 0x897cfcb8;
constexpr int kExpN = (int)((kCapTab + 128 * 4 - kExpTab) / 4);
// the Comb Filter's octave factors (5 floats: 4, 2, 1, ½, ¼)
constexpr uint32_t kOctTab = 0x897d1868;
constexpr int kCombLen = 3267;                 // the delay buffer (0x894ecb00)
// the Resonator's coefficients, per Filter Type six (f, d, g) (0x894ffa10);
// the firmware expf's two polynomials (0x89531a28)
constexpr uint32_t kResTab = 0x897d4b80, kExpPTab = 0x897eeebc, kExpQTab = 0x897eeed8;
// the Frequency Shifter's sine table (129 floats, 128 a cycle) and Hilbert
// taps (32 floats), 0x894fda60
constexpr uint32_t kSinTab = 0x897d3dec, kHilbTab = 0x897d45f0;
// Lo-Fi's Balance curve (128 floats), and by Rate Down its hold masks (bytes)
// and lowpass coefficients, by Bit Down its scales 2^−b and 2^b (0x894edd60)
constexpr uint32_t kLofiBalTab = 0x897d1668, kLofiMaskTab = 0x897d187c, kLofiCoefTab = 0x897d1894, kLofiDownTab = 0x897d18ac,
                   kLofiUpTab = 0x897d18ec;

struct Tables {
    float cut[129], cap[128];
    float exp[kExpN];
    uint16_t gain[128];
    float oct[5];
    float keySq[41][128];                       // xtCosmSetKeySquares
    float res[3][18];
    float expP[7], expQ[7];
    float sine[129], hilb[32];
    float lofiBal[128], lofiCoef[6], lofiDown[16], lofiUp[16];
    int8_t lofiMask[6];
    bool ok = false, keySqOk = false;
} T;

using namespace xtf;
inline float fmac(float a, float b, float c) { return add(mul(a, b), c); }   // FMAC: the product, then the sum
inline float sq63(int d) {                                                    // (d/63)·|d/63|
    const float v = div(flt(d), 63.0f);
    return mul(0.0f > v ? -v : v, v);
}
inline int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
constexpr float K(uint32_t u) { return std::bit_cast<float>(u); }               // a constant as rom.2 stores it
// A cutoff with its keyfollow (0x894dc660): (key − 60)·KF·10/100/127 + Cutoff/127, within 0…1
inline float keyCutoff(int cutoff, int kf, int key) {
    const int c = clampi((int16_t)cutoff, 0, 127);
    float f = div(div(mul(flt((int16_t)(key - 60)), flt((int16_t)(kf * 10))), 100.0f), 127.0f);
    f = add(add(f, div(flt(c), 127.0f)), 0.0f);
    return f > 1.0f ? 1.0f : (0.0f > f ? 0.0f : f);
}
inline float qValue(int q) { return add(div(mul(flt(clampi((int16_t)q, 0, 127)), 0.984375f), 127.0f), -1.0f); }
inline float envDepth(float vel, int d) {                                    // vel × (d/63)·|d/63|, d as stored
    const float v = div(flt((int16_t)d), 63.0f);
    return mul(vel, mul(0.0f > v ? -v : v, v));
}

// The kernels (0x89502050…0x89502840): 32 frames, the coefficients stepped
// before every fourth frame; s[0..3] the states at D+0x1c…0x28.
void stage24(float* lr, float* s, float& c1, float& c2, float d1, float d2, int mode) {
    float a = s[0], b = s[1], d = s[2], e = s[3];
    for (int i = 0; i < 32; ++i) {
        if ((i & 3) == 0) { c1 = add(c1, d1); c2 = add(c2, d2); }
        const float x = lr[2 * i];
        b = fmac(a, c2, b);
        float t = mul(-x, c1);
        t = sub(t, b);
        t = fmac(a, c1, t);
        a = fmac(c2, t, a);
        e = fmac(c2, d, e);
        float in2 = t;                                 // HP24
        if (mode == 0) in2 = b;                        // LP24
        else if (mode == 1) in2 = a;                   // BP24
        else if (mode == 3) in2 = add(t, a);           // NOTCH24
        else if (mode == 4) in2 = sub(t, a);           // PEAK24
        float u = sub(in2, e);
        u = fmac(c1, d, u);
        d = fmac(u, c2, d);
        float y = u;                                   // HP24
        if (mode == 0) y = e;
        else if (mode == 1) y = d;
        else if (mode == 3) y = add(u, e);
        else if (mode == 4) y = sub(u, e);
        lr[2 * i] = lr[2 * i + 1] = y;
    }
    s[0] = a; s[1] = b; s[2] = d; s[3] = e;
}

void stage12(float* lr, float* s, float& c1, float& c2, float d1, float d2, int mode) {
    const float k = fmac(-c1, 0.5f, 0.5f);             // the input gain, from c1 at the block's start
    float a = s[0], b = s[1];
    for (int i = 0; i < 32; ++i) {
        if ((i & 3) == 0) { c1 = add(c1, d1); c2 = add(c2, d2); }
        const float x = lr[2 * i];
        b = fmac(a, c2, b);
        float t = mode == 5 ? mul(x, k) : mul(-x, k);  // LP12 keeps the sign
        t = sub(t, b);
        t = fmac(a, c1, t);
        a = fmac(t, c2, a);
        float y = t;                                   // HP12
        if (mode == 5) y = b;
        else if (mode == 6) y = a;
        else if (mode == 8) y = add(t, b);
        else if (mode == 9) y = sub(t, b);
        lr[2 * i] = lr[2 * i + 1] = y;
    }
    s[0] = a; s[1] = b;
}

void stage6(float* lr, float* s, float& c2, float d2, bool hp) {
    float a = s[0];
    for (int i = 0; i < 32; ++i) {
        if ((i & 3) == 0) c2 = add(c2, d2);
        const float y = sub(lr[2 * i], a);
        a = fmac(y, c2, a);
        lr[2 * i] = lr[2 * i + 1] = hp ? y : a;
    }
    s[0] = a;
}

// The Dual Filter's kernels (0x894fcbe0 LPF/HPF, 0x894fcd70 LPF->HPF,
// 0x894fcef0 BPF/BPF): per filter two of the TVF's 24-dB stages, the input
// times Q; c1, c2 the filters' cutoffs, stepped before every fourth frame.
// Filter 1 is LP24 (BP24 for BPF/BPF), Filter 2 HP24 (BP24), on the input or,
// for LPF->HPF, on Filter 1's output; the output their sum (or Filter 2's).
void dual(float* lr, float* s, float& c1, float& c2, float d1, float d2, float q1, float q2, int mode) {
    float s0 = s[0], s1 = s[1], s2 = s[2], s3 = s[3], s4 = s[4], s5 = s[5], s6 = s[6], s7 = s[7];
    for (int i = 0; i < 32; ++i) {
        if ((i & 3) == 0) { c1 = add(c1, d1); c2 = add(c2, d2); }
        const float x = lr[2 * i];
        s1 = fmac(c1, s0, s1);
        float t = sub(mul(x, q1), s1);
        t = fmac(q1, s0, t);
        s0 = fmac(c1, t, s0);
        s3 = fmac(c1, s2, s3);
        float u = sub(mode == 2 ? s0 : s1, s3);
        u = fmac(q1, s2, u);
        s2 = fmac(c1, u, s2);
        s5 = fmac(c2, s4, s5);
        float t2 = sub(mul(mode == 1 ? s3 : x, q2), s5);
        t2 = fmac(q2, s4, t2);
        s4 = fmac(c2, t2, s4);
        s7 = fmac(c2, s6, s7);
        float u2 = sub(mode == 2 ? s4 : t2, s7);
        u2 = fmac(q2, s6, u2);
        s6 = fmac(c2, u2, s6);
        const float y = mode == 0 ? add(u2, s3) : mode == 1 ? u2 : add(s6, s2);
        lr[2 * i] = lr[2 * i + 1] = y;
    }
    s[0] = s0; s[1] = s1; s[2] = s2; s[3] = s3; s[4] = s4; s[5] = s5; s[6] = s6; s[7] = s7;
}

// 2^x through rom.2's tables (0x894ec090): for x ≥ 0, i = trunc(x) and the
// fraction's 16 bits f, 2^(f>>8 /256) · 2^i · 2^((f&255)/65536); for x < 0
// the same of −x through the inverse tables.  An i past the 2^i table reads
// on, as the firmware does.
float exp2t(float x) {
    const bool neg = 0.0f > x;
    if (neg) x = -x;
    const int i = ftrc(x);
    const int f = ftrc(mul(sub(x, flt(i)), 65536.0f));
    const int hi = neg ? 512 : 0, lo = neg ? 768 : 256, in = neg ? 1031 : 1024;
    const float a = T.exp[clampi(hi + (f >> 8), 0, kExpN - 1)], b = T.exp[clampi(in + i, 0, kExpN - 1)];
    return mul(mul(a, b), T.exp[clampi(lo + (f & 255), 0, kExpN - 1)]);
}

// FIPR as the interpreter forms it: the products summed in double, each sum
// rounded toward zero (the FPU mode), then the float toward zero.
inline double addRz(double x, double y) {
    const double s = x + y;
    if (nearest()) return s;
    const double bb = s - x, err = (x - (s - bb)) + (y - bb);
    return err != 0.0 && (s > 0.0 ? err < 0.0 : err > 0.0) ? std::nextafter(s, 0.0) : s;
}
inline float clampf(float x, float lo, float hi) {             // 0x89501900
    if (lo > x) x = lo;
    if (x > hi) x = hi;
    return x;
}
inline float fracOf(float x) { return sub(x, flt(ftrc(x))); }

inline float fipr(float a0, float a1, float a2, float a3, float b0, float b1, float b2, float b3) {
    double s = (double)a0 * b0;
    s = addRz(s, (double)a1 * b1);
    s = addRz(s, (double)a2 * b2);
    s = addRz(s, (double)a3 * b3);
    return rnd(s);
}

} // namespace

bool xtCosmLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    const bool ok = std::fseek(f, (long)(kCutTab - kRomBase), SEEK_SET) == 0 && std::fread(T.cut, 4, 129, f) == 129
                 && std::fseek(f, (long)(kCapTab - kRomBase), SEEK_SET) == 0 && std::fread(T.cap, 4, 128, f) == 128
                 && std::fseek(f, (long)(kExpTab - kRomBase), SEEK_SET) == 0 && std::fread(T.exp, 4, kExpN, f) == (size_t)kExpN
                 && std::fseek(f, (long)(kGainTab - kRomBase), SEEK_SET) == 0 && std::fread(T.gain, 2, 128, f) == 128
                 && std::fseek(f, (long)(kOctTab - kRomBase), SEEK_SET) == 0 && std::fread(T.oct, 4, 5, f) == 5
                 && std::fseek(f, (long)(kResTab - kRomBase), SEEK_SET) == 0 && std::fread(T.res, 4, 54, f) == 54
                 && std::fseek(f, (long)(kExpPTab - kRomBase), SEEK_SET) == 0 && std::fread(T.expP, 4, 7, f) == 7
                 && std::fseek(f, (long)(kExpQTab - kRomBase), SEEK_SET) == 0 && std::fread(T.expQ, 4, 7, f) == 7
                 && std::fseek(f, (long)(kSinTab - kRomBase), SEEK_SET) == 0 && std::fread(T.sine, 4, 129, f) == 129
                 && std::fseek(f, (long)(kHilbTab - kRomBase), SEEK_SET) == 0 && std::fread(T.hilb, 4, 32, f) == 32
                 && std::fseek(f, (long)(kLofiBalTab - kRomBase), SEEK_SET) == 0 && std::fread(T.lofiBal, 4, 128, f) == 128
                 && std::fseek(f, (long)(kLofiMaskTab - kRomBase), SEEK_SET) == 0 && std::fread(T.lofiMask, 1, 6, f) == 6
                 && std::fseek(f, (long)(kLofiCoefTab - kRomBase), SEEK_SET) == 0 && std::fread(T.lofiCoef, 4, 6, f) == 6
                 && std::fseek(f, (long)(kLofiDownTab - kRomBase), SEEK_SET) == 0 && std::fread(T.lofiDown, 4, 16, f) == 16
                 && std::fseek(f, (long)(kLofiUpTab - kRomBase), SEEK_SET) == 0 && std::fread(T.lofiUp, 4, 16, f) == 16;
    std::fclose(f);
    T.ok = ok;
    return ok;
}

bool xtCosmTablesLoaded() { return T.ok; }

XtTvfBlock xtTvfBlock(const XtTvfParams& p, int key, float velA, float velB) {
    XtTvfBlock b;
    b.mode = p.filter;
    b.cutoff = keyCutoff(p.cutoff, p.keyfollow, key);
    b.cutoffLfo = sq63(clampi((int16_t)p.cutoffLfo, -63, 63));
    b.cutoffEnv = envDepth(velA, p.cutoffEnv);
    b.q = qValue(p.q);
    b.qLfo = sq63(clampi((int16_t)p.qLfo, -63, 63));
    b.qEnv = envDepth(velB, p.qEnv);
    return b;
}

XtTvfBlock xtDtvfBlock(const XtTvfParams& p, int key, float velA, float velB) {
    XtTvfBlock b = xtTvfBlock(p, key, velA, velB);   // the same sums at the same places
    b.dynamic = true;
    const float qo = div(flt((int16_t)p.qOwn), 63.0f), co = div(flt((int16_t)p.cutoffOwn), 63.0f);
    b.qOwn = mul(0.0f > qo ? -qo : qo, qo);
    b.cutoffOwn = mul(0.0f > co ? -co : co, co);
    return b;
}

float xtCosmVelocity(int velocity, int patchSens, int patchCurve) {
    return div(flt(xtVelCurveValue(velocity, xtVelSens(patchSens), xtVelCurve(patchCurve))), 32768.0f);
}

float xtTvfQ(float q, float envDepth, float env, float lfoDepth, float lfo) {
    float x = fmac(envDepth, env, q);
    x = fmac(lfoDepth, lfo, x);
    if (-1.0f > x) x = -1.0f;                          // 0x89501900
    if (x > -0.015625f) x = -0.015625f;
    return x;
}

float xtTvfCutoff(float cutoff, float envDepth, float env, float lfoDepth, float lfo, float q) {
    float y = fmac(envDepth, env, cutoff);
    y = fmac(lfoDepth, lfo, y);
    return xtTvfCoef(y, q);
}

float xtTvfCoef(float y, float q) {
    if (0.0f > y) return 0.0f;
    const float z = mul(y, 127.0f);
    int i = ftrc(z);
    const float frac = sub(z, flt(ftrc(z)));
    if (i > 127) i = 127;
    const int j = ftrc(mul(-q, 127.0f));
    const float c = fmac(T.cut[i], sub(1.0f, frac), mul(T.cut[i + 1], frac));
    const float cap = T.cap[127 - j];
    return c > cap ? cap : c;
}

void XtCosmTvf::noteOn(const XtTvfBlock& b, float qEnvStart, float qEnvOut, float qRate, float qLfo,
                       float cutEnvStart, float cutEnvOut, float cutRate, float cutLfo) {
    for (float& x : s) x = 0.0f;
    cur1 = xtTvfQ(b.q, b.qEnv, qEnvStart, b.qLfo, qLfo);
    cur2 = xtTvfCutoff(b.cutoff, b.cutoffEnv, cutEnvStart, b.cutoffLfo, cutLfo, cur1);
    rate1 = mul(qRate, 4.0f);
    rate2 = mul(cutRate, 4.0f);
    setBlock(b, qEnvOut, qLfo, cutEnvOut, cutLfo);
    qOwnS = b.qOwn;                                   // 0x894edf70, type 14: the slews start at the block's depths
    cutOwnS = b.cutoffOwn;
}

void XtCosmTvf::setBlock(const XtTvfBlock& b, float qEnv, float qLfo, float cutEnv, float cutLfo) {
    blk = b;
    targets(qEnv, qLfo, cutEnv, cutLfo);
}

void XtCosmTvf::update(float qEnv, float qRate, float qLfo, float cutEnv, float cutRate, float cutLfo) {
    rate1 = mul(qRate, 4.0f);
    rate2 = mul(cutRate, 4.0f);
    targets(qEnv, qLfo, cutEnv, cutLfo);
}

void XtCosmTvf::targets(float qEnv, float qLfo, float cutEnv, float cutLfo) {
    if (blk.dynamic) {                                // the raw sums (0x894eda10, 0x894eeda0)
        tgt1 = fmac(blk.qLfo, qLfo, fmac(blk.qEnv, qEnv, blk.q));
        tgt2 = fmac(blk.cutoffLfo, cutLfo, fmac(blk.cutoffEnv, cutEnv, blk.cutoff));
        return;
    }
    tgt1 = xtTvfQ(blk.q, blk.qEnv, qEnv, blk.qLfo, qLfo);
    tgt2 = xtTvfCutoff(blk.cutoff, blk.cutoffEnv, cutEnv, blk.cutoffLfo, cutLfo, tgt1);
}

void XtCosmTvf::setState(const float st[4], float c1, float c2, float t1, float t2, float r1, float r2) {
    for (int i = 0; i < 4; ++i) s[i] = st[i];
    cur1 = c1; cur2 = c2; tgt1 = t1; tgt2 = t2; rate1 = r1; rate2 = r2;
}

void XtCosmTvf::process(float* lr, float scale) {
    float t1 = tgt1, t2 = tgt2;
    if (blk.dynamic) {
        // the follower (0x895017e0): Σ|L|, every eighth frame twice
        float sum = 0.0f;
        for (int i = 0; i < 32; ++i) {
            const float a = lr[2 * i] < 0.0f ? -lr[2 * i] : lr[2 * i];
            sum = add(sum, a);
            if ((i & 7) == 7) sum = add(sum, a);
        }
        const float level = mul(mul(sum, scale), 0x1.fffffep-35f);       // 0x2e7fffff
        lev = fmac(0.1f, sub(level, lev), lev);
        constexpr float kSlew = 0x1.0624dcp-10f;       // 0.001 as rom.2 stores it (0x3a83126e, a unit below)
        qOwnS = fmac(kSlew, sub(-blk.qOwn, qOwnS), qOwnS);
        cutOwnS = fmac(kSlew, sub(blk.cutoffOwn, cutOwnS), cutOwnS);
        float q = sub(tgt1, mul(qOwnS, lev));
        if (-1.0f > q) q = -1.0f;
        if (q > -0.015625f) q = -0.015625f;
        t1 = q;
        t2 = xtTvfCoef(fmac(lev, cutOwnS, tgt2), q);
    }
    const float d1 = mul(rate1, sub(t1, cur1)), d2 = mul(rate2, sub(t2, cur2));
    const int m = blk.mode;
    if (m >= 0 && m <= 4) stage24(lr, s, cur1, cur2, d1, d2, m);
    else if (m >= 5 && m <= 9) stage12(lr, s, cur1, cur2, d1, d2, m);
    else if (m == 10 || m == 11) stage6(lr, s, cur2, d2, m == 11);
}

XtDualBlock xtDualBlock(const XtDualParams& p, int key, float velA, float velB) {
    XtDualBlock b;
    b.mode = (int16_t)p.type;
    b.cut1 = keyCutoff(p.freq1, p.kf1, key);
    b.q1 = qValue(p.q1);
    b.lfo1 = sq63(clampi((int16_t)p.lfo1, -63, 63));
    b.env1 = envDepth(velA, p.env1);
    b.cut2 = keyCutoff(p.freq2, p.kf2, key);
    b.q2 = qValue(p.q2);
    b.lfo2 = sq63(clampi((int16_t)p.lfo2, -63, 63));
    b.env2 = envDepth(velB, p.env2);
    return b;
}

void XtCosmDual::noteOn(const XtDualBlock& b, float env1Start, float env1Out, float r1, float lfo1,
                        float env2Start, float env2Out, float r2, float lfo2) {
    for (float& x : s) x = 0.0f;
    cur1 = xtTvfCutoff(b.cut1, b.env1, env1Start, b.lfo1, lfo1, b.q1);
    cur2 = xtTvfCutoff(b.cut2, b.env2, env2Start, b.lfo2, lfo2, b.q2);
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    setBlock(b, env1Out, lfo1, env2Out, lfo2);
}

void XtCosmDual::setBlock(const XtDualBlock& b, float env1, float lfo1, float env2, float lfo2) {
    blk = b;
    targets(env1, lfo1, env2, lfo2);
}

void XtCosmDual::update(float env1, float r1, float lfo1, float env2, float r2, float lfo2) {
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    targets(env1, lfo1, env2, lfo2);
}

void XtCosmDual::targets(float env1, float lfo1, float env2, float lfo2) {
    tgt1 = xtTvfCutoff(blk.cut1, blk.env1, env1, blk.lfo1, lfo1, blk.q1);
    tgt2 = xtTvfCutoff(blk.cut2, blk.env2, env2, blk.lfo2, lfo2, blk.q2);
}

void XtCosmDual::process(float* lr) {
    const float d1 = mul(sub(tgt1, cur1), rate1), d2 = mul(sub(tgt2, cur2), rate2);
    if (blk.mode >= 0 && blk.mode <= 2) dual(lr, s, cur1, cur2, d1, d2, blk.q1, blk.q2, blk.mode);
}

XtTbBlock xtTbBlock(const XtTbParams& p, int key, float velA, float velB) {
    auto cut = [](int c) { return add(div(flt(clampi((int16_t)c, 0, 127)), 127.0f), 0.0f); };
    auto kf = [](int k) { return add(div(mul(flt(clampi((int16_t)(k * 10), -200, 200) + 200), 4.0f), 400.0f), -2.0f); };
    XtTbBlock b;
    b.gain = mul(div(flt(T.gain[clampi((int16_t)p.gain, 0, 127)]), 32768.0f), 2.0f);
    b.lcut = cut(p.lpfCutoff);
    b.lkf = kf(p.lpfKf);
    b.q = cut(p.q);
    b.qf = add(div(mul(flt(clampi((int16_t)p.qFollow, -63, 63) + 63), 2.0f), 126.0f), -1.0f);
    b.hcut = cut(p.hpfCutoff);
    b.hkf = kf(p.hpfKf);
    b.lenv = envDepth(velA, p.lpfEnv);
    b.llfo = sq63(clampi((int16_t)p.lpfLfo, -63, 63));
    b.henv = envDepth(velB, p.hpfEnv);
    b.hlfo = sq63((int16_t)p.hpfLfo);
    b.key = key;
    return b;
}

void XtCosmTb::noteOn(const XtTbBlock& b, float env1, float lfo1, float env2, float lfo2) {
    setBlock(b, env1, lfo1, env2, lfo2);
    for (int i = 0; i < 7; ++i) c[i] = div(t[i], K(0x3c23d709));      // 0x894fbd20: the targets / 0.01
    for (float& x : s) x = 0.0f;
    for (float& x : h) x = 0.0f;
}

void XtCosmTb::setBlock(const XtTbBlock& b, float env1, float lfo1, float env2, float lfo2) {
    blk = b;
    targets(env1, lfo1, env2, lfo2);
}

void XtCosmTb::update(float env1, float lfo1, float env2, float lfo2) { targets(env1, lfo1, env2, lfo2); }

void XtCosmTb::targets(float env1, float lfo1, float env2, float lfo2) {
    constexpr float kG64 = K(0x405121fe), kG68 = K(0xbd7df3b8), kG6c = K(0x40651b9a), kG70 = K(0x3fac1e79);   // set by 0x894fc4c0
    const float k01 = K(0x3c23d709);                   // 0.01
    // the LPF: P(c) = ((−1.161c + 2.1096)c − 0.1344)c − 0.1, the modulation
    // m = LFO·depth + Env·depth (scaled) + 0.647
    const float cc = blk.lcut;
    float p = fmac(cc, K(0xbf949ba5), K(0x400703af));
    p = fmac(p, cc, K(0xbe09a026));
    p = fmac(p, cc, K(0xbdcccccd));
    const float base = fmac(kG64, p, kG68);
    float m = mul(add(mul(lfo1, kG70), 0.0f), mul(blk.llfo, kG6c));
    m = fmac(add(mul(blk.lenv, kG6c), 0.0f), add(mul(env1, kG70), 0.0f), m);
    m = add(m, K(0x3f25acee));
    float e = exp2t(mul(add(base, m), 0.5f));
    float fc = mul(mul(e, e), 278.0f);
    const float oct = div(add(flt(blk.key), -60.0f), 24.0f);
    e = exp2t(mul(oct, blk.lkf));
    fc = mul(fc, mul(e, e));
    bool scale;
    if (fc > 20000.0f) { fc = 20000.0f; scale = true; }
    else if (1.0f > fc) { fc = 1.0f; scale = false; }
    else scale = fc > 3700.0f;
    float q = fmac(mul(blk.qf, m), K(0x3e4ccccd), blk.q);
    q = mul(mul(q, add(div(p, 3.5f), K(0x3f333333))), K(0x40fccccc));
    if (0.0f > q) q = 0.0f;
    else if (q > K(0x40fccccc)) q = K(0x40fccccc);
    if (scale) q = mul(q, fmac(fc, K(0xb7d0359e), K(0x3f8b9e53)));
    const float w = div(mul(fc, K(0x40714637)), 44100.0f);
    const float r = div(add(w, -2.0f), add(w, 2.0f));
    t[0] = mul(div(add(r, 1.0f), K(0x3fa66665)), k01);
    t[1] = mul(t[0], K(0x3e999999));
    t[2] = mul(-r, k01);
    t[3] = mul(q, k01);
    // the HPF: 2^(13·(c + mods)) Hz, keyfollowed
    float y = fmac(blk.henv, env2, blk.hcut);
    y = fmac(blk.hlfo, lfo2, y);
    y = mul(y, 13.0f);
    float hf;
    if (0.0f > y) hf = 1.0f;
    else if (y > 14.0f) hf = 10000.0f;
    else { e = exp2t(mul(y, 0.5f)); hf = mul(e, e); }
    e = exp2t(mul(oct, blk.hkf));
    hf = mul(hf, mul(e, e));
    if (!(10000.0f > hf)) hf = 10000.0f;
    if (hf > 1.0f) {
        const float w2 = div(mul(hf, K(0x40c90fda)), 44100.0f);
        t[4] = mul(div(2.0f, add(w2, 2.0f)), k01);
        t[5] = mul(div(sub(2.0f, w2), add(w2, 2.0f)), k01);
    } else {
        t[4] = t[5] = k01;
    }
    t[6] = mul(blk.gain, k01);
}

void XtCosmTb::process(float* lr) {
    constexpr float k99 = K(0x3f7d70a3);
    float c0 = c[0], c1 = c[1], c2 = c[2], q = c[3], h0c = c[4], h1c = c[5], g = c[6];
    float s0 = s[0], s1 = s[1], s2 = s[2], s3 = s[3], h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3];
    for (int i = 0; i < 32; ++i) {
        const float x = lr[2 * i];
        // the LPF: three bilinear one-poles, y = c0·in + c1·in' + c2·y',
        // the input less Q × the third's last output
        const float u = add(-mul(q, s3), x);
        const float y1 = fipr(q, c1, c2, c0, 0.0f, s0, s1, u);
        const float y2 = fipr(q, c1, c2, c0, 0.0f, s1, s2, y1);
        const float y3 = fipr(q, c1, c2, c0, 0.0f, s2, s3, y2);
        c0 = add(mul(c0, k99), t[0]);
        s1 = y1; s2 = y2; s3 = y3;
        const float qy = mul(y3, q);
        c1 = add(mul(c1, k99), t[1]);
        s0 = sub(x, qy);
        const float lp = add(qy, y3);
        c2 = add(mul(c2, k99), t[2]);
        q = add(mul(q, k99), t[3]);
        // the HPF: two one-poles, y = c0·(in − in') + c1·y'; then the gain
        const float hp1 = fipr(h0c, -h0c, 0.0f, h1c, lp, h0, g, h1);
        h0 = lp; h1 = hp1;
        const float hp2 = fipr(h0c, -h0c, 0.0f, h1c, hp1, h2, 0.0f, h3);
        h0c = add(mul(h0c, k99), t[4]);
        h2 = hp1; h3 = hp2;
        const float out = mul(hp2, g);
        h1c = add(mul(h1c, k99), t[5]);
        lr[2 * i] = lr[2 * i + 1] = out;
        g = add(mul(g, k99), t[6]);
    }
    c[0] = c0; c[1] = c1; c[2] = c2; c[3] = q; c[4] = h0c; c[5] = h1c; c[6] = g;
    s[0] = s0; s[1] = s1; s[2] = s2; s[3] = s3; h[0] = h0; h[1] = h1; h[2] = h2; h[3] = h3;
}

XtCombBlock xtCombBlock(const XtCombParams& p, float velA, float velB) {
    XtCombBlock b;
    b.octave = (int8_t)p.octave;
    b.detune = flt(clampi((int16_t)p.detune, -100, 100));
    b.detuneLfo = sq63(clampi((int16_t)p.detuneLfo, -63, 63));
    b.detuneEnv = envDepth(velB, p.detuneEnv);
    b.tone = add(div(mul(flt(clampi((int16_t)p.tone, 0, 127)), 2.0f), 127.0f), -1.0f);
    b.toneLfo = sq63(clampi((int16_t)p.toneLfo, -63, 63));
    b.toneEnv = envDepth(velA, p.toneEnv);
    return b;
}

float xtCosmRootHz(int originalPitch, bool robot) {
    if (robot) return 440.0f;
    // 0x894f21c0: (P·100 − 0 + 0)/1200 − 5.75 (as stored), exp2, × 44000 × 0.01
    float x = sub(mul(flt(originalPitch), 100.0f), 0.0f);
    x = add(x, 0.0f);
    x = fmac(x, K(0x3a5a740d), K(0xc0b7ffff));
    return mul(mul(exp2t(x), 44000.0f), K(0x3c23d709));
}

XtCosmComb::XtCosmComb() : buf(kCombLen + 1, 0.0f) {}

float XtCosmComb::delayFrom(float d, float env1, float lfo1) const {
    float x = fmac(blk.detuneEnv, env1, d);
    x = fmac(blk.detuneLfo, lfo1, x);
    x = mul(mul(x, T.oct[clampi(blk.octave, 0, 4)]), 44100.0f);
    return clampf(div(x, mul(root, pitch)), 0.0f, 3267.0f);
}

void XtCosmComb::newTarget(float d) {
    if (1.0f > prog || d == nDly) return;              // only after a crossfade, and only a new delay
    nDly = d;
    nFrac = fracOf(d);
    prog = 0.0f;
}

void XtCosmComb::noteOn(const XtCombBlock& b, float p, float r, float env1Start, float env1Out, float r1, float lfo1,
                        float env2Start, float env2Out, float r2, float lfo2) {
    pitch = p; root = r;
    w = kCombLen - 1;                                  // 0xcc2
    dcy = 0.0f;
    for (float& x : buf) x = 0.0f;
    blk = b;
    dly = delayFrom(sub(1.0f, mul(b.detune, K(0x39da7419))), env1Start, lfo1);
    tn = clampf(fmac(b.toneLfo, lfo2, fmac(b.toneEnv, env2Start, b.tone)), -1.0f, 1.0f);
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    setBlock(b, env1Out, lfo1, env2Out, lfo2);
    nDly = dly;                                        // 0x894ee300: settled at the start delay
    frac = nFrac = fracOf(dly);
    prog = 1.0f;
    dcK = K(0x3acb4aec);                               // 0.001551
}

void XtCosmComb::setBlock(const XtCombBlock& b, float env1, float lfo1, float env2, float lfo2) {
    blk = b;
    det = sub(1.0f, mul(b.detune, K(0x39da7419)));
    const float d = delayFrom(det, env1, lfo1);
    tnTgt = clampf(fmac(b.toneLfo, lfo2, fmac(b.toneEnv, env2, b.tone)), -1.0f, 1.0f);
    newTarget(d);
}

void XtCosmComb::update(float p, float env1, float r1, float lfo1, float env2, float r2, float lfo2) {
    pitch = p;
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    tnTgt = clampf(fmac(blk.toneLfo, lfo2, fmac(blk.toneEnv, env2, blk.tone)), -1.0f, 1.0f);
    if (1.0f > prog) return;
    newTarget(delayFrom(sub(1.0f, mul(det, K(0x39da7419))), env1, lfo1));   // +0x2c read as Detune
}

void XtCosmComb::process(float* lr) {
    const float d2 = mul(sub(tnTgt, tn), rate2);
    const int r13 = ftrc(dly), r14 = ftrc(nDly);
    float c = tn, y = dcy, p = prog, f = frac;
    float* const B = buf.data() + 1;                   // B[−1] is the guard word
    for (int i = 0; i < 32; ++i) {
        if (++w >= kCombLen) w = 0;
        if (i == 0) c = add(c, d2);                    // the step meant for every 4th sample runs once
        const float x = lr[2 * i];
        const float e = sub(x, y);
        y = fmac(e, dcK, y);
        int a = w - r13, b = a - 1, n = w - r14, m = n - 1;
        if (a < 0) a += kCombLen;
        if (b < 0) b += kCombLen;                      // once: w = 0 at the longest delay leaves −1
        if (n < 0) n += kCombLen;
        if (m < 0) m += kCombLen;
        const float g = sub(1.0f, f);
        const float ra = add(mul(B[a], g), mul(B[b], f));
        const float rb = add(mul(B[n], g), mul(B[m], f));
        const float s2 = add(mul(ra, sub(1.0f, p)), mul(rb, p));
        B[w] = e;
        lr[2 * i] = lr[2 * i + 1] = add(mul(s2, c), e);
        p = add(p, K(0x3b03126e));                     // 0.002
        if (p > 1.0f) { p = 1.0f; dly = nDly; f = nFrac; }
    }
    tn = c; dcy = y; prog = p;                         // the fraction is not kept
}

// ── The Side Band Filters ──

namespace {

// x² by the firmware's powf (0x894dc540), KF −20…+20
float keySquare(int kf, int key, float x) {
    if (T.keySqOk && kf >= -20 && kf <= 20 && key >= 0 && key < 128) return T.keySq[kf + 20][key];
    return (float)((double)x * (double)x);
}

// The LPF with its keyfollow (0x894dc540): Cutoff/2/127 + ½, ± ln 2 · x²/4
// by the sign of x = (key − 60)·KF·10/100/12, within ½…1
float sbfLpf(int cutoff, int kf, int key) {
    const float x = div(div(mul(flt((int16_t)(key - 60)), flt((int16_t)(kf * 10))), 100.0f), 12.0f);
    float y = add(div(mul(flt(clampi((int16_t)cutoff, 0, 127)), 0.5f), 127.0f), 0.5f);
    const float d = mul(mul(keySquare(kf, key, x), K(0x3f317219)), 0.25f);       // × the firmware's logf(2)
    y = 0.0f > x ? sub(y, d) : add(y, d);
    if (y > 1.0f) return 1.0f;
    return 0.5f > y ? 0.5f : y;
}

// The Width with its keyfollow (0x894dc5f0; the Frequency Shifter's Depth,
// 0x894dc7a0, is the same): (Width/127 + 0)·2^(KF·10·(key − 60)/1200), within 0…1
float sbfWidth(int width, int kf, int key) {
    const float y = mul(xtOscTimeKeyScale((int16_t)(kf * 10), key), add(div(flt(clampi((int16_t)width, 0, 127)), 127.0f), 0.0f));
    return y > 1.0f ? 1.0f : (0.0f > y ? 0.0f : y);
}

// 0x89501910: below 1, 1; otherwise halved until under 3267
float foldDelay(float x) {
    if (1.0f > x) return 1.0f;
    if (!(x < 3.0e38f)) return 1.0f;                   // ∞ or NaN: the XT would halve for ever (never met)
    while (!(3267.0f > x)) x = mul(x, 0.5f);
    return x;
}

constexpr float kSlow = K(0x3a83126e), kSlew = K(0x3dcccccc), kStep = K(0x3c4ccccc), kFade = K(0x3bf83e0f);   // 0.001, 0.1, 0.0125, 1/132

} // namespace

void xtCosmSetKeySquares(const float* t) {
    for (int i = 0; i < 41; ++i)
        for (int k = 0; k < 128; ++k) T.keySq[i][k] = t[i * 128 + k];
    T.keySqOk = true;
}

XtSbfBlock xtSbfBlock(const XtSbfParams& p, int key, float velA, float velB) {
    XtSbfBlock b;
    b.second = p.second;
    b.octave = (int8_t)p.octave;
    if (p.second) {
        b.q = add(div(mul(flt(clampi((int16_t)p.q, 0, 127)), 0.96875f), 127.0f), -1.0f);
    } else {
        b.hpf = add(div(-flt(clampi((int16_t)p.hpf, 0, 127)), 127.0f), 0.0f);
        b.subDetune = flt(clampi((int16_t)p.subDetune, -100, 100));
    }
    b.lpf = sbfLpf(p.lpf, p.lpfKf, key);
    b.detune = flt(clampi((int16_t)p.detune, -100, 100));
    b.detuneLfo = sq63(clampi((int16_t)p.detuneLfo, -63, 63));
    b.detuneOwn = sq63((int16_t)p.detuneOwn);
    b.detuneEnv = envDepth(velB, p.detuneEnv);
    b.width = sbfWidth(p.width, p.widthKf, key);
    b.widthLfo = sq63(clampi((int16_t)p.widthLfo, -63, 63));
    b.widthOwn = sq63((int16_t)p.widthOwn);
    b.widthEnv = envDepth(velA, p.widthEnv);
    return b;
}

XtCosmSbf::XtCosmSbf() : buf1(kCombLen, 0.0f), buf2(kGuard + kCombLen, 0.0f) {}

void XtCosmSbf::noteOn(const XtSbfBlock& b, float p, float r, float env1Start, float env1Out, float r1, float lfo1,
                       float env2Start, float env2Out, float r2, float lfo2) {
    pitch = p; root = r;
    for (float& x : s) x = 0.0f;                       // 0x894edf70 clears D+0x1c…0x88
    dcy = 0.0f;
    w = kCombLen - 1;                                  // 0xcc2
    std::fill(buf1.begin(), buf1.end(), 0.0f);
    std::fill(buf2.begin(), buf2.end(), 0.0f);
    // the start values (0x894ec5f0, 0x894ec6b0)
    float d = fmac(b.detuneEnv, env1Start, sub(1.0f, mul(b.detune, K(0x39da7419))));
    d = mul(mul(fmac(b.detuneLfo, lfo1, d), T.oct[clampi(b.octave, 0, 4)]), 44100.0f);
    dly = clampf(div(d, mul(root, pitch)), 0.0f, 3267.0f);
    wid = clampf(fmac(b.widthLfo, lfo2, fmac(b.widthEnv, env2Start, b.width)), 0.0f, b.second ? K(0x3f733333) : 1.0f);
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    setBlock(b, env1Out, lfo1, env2Out, lfo2);
    // 0x894ee1a0, 0x894ee260
    sl[0] = b.second ? b.q : hpfH;
    sl[1] = b.lpf;
    if (!b.second) sl[2] = subR;
    lev = 0.0f;
    sl[3] = b.detuneOwn;
    sl[4] = b.widthOwn;
    gn = fmac(widT, widT, sub(2.0f, mul(0.0f > widT ? -widT : widT, 2.0f)));   // the unclamped target
    nDly = dly;
    fr[0] = fracOf(dly);
    if (!b.second) fr[1] = fracOf(mul(dly, sl[2]));
    prog = 1.0f;
    dcK = K(0x3acb4aec);                               // 0.001551
}

void XtCosmSbf::setBlock(const XtSbfBlock& b, float env1, float lfo1, float env2, float lfo2) {
    blk = b;
    detR = sub(1.0f, mul(b.detune, K(0x39da7419)));
    if (!b.second) {
        subR = sub(1.0f, mul(b.subDetune, K(0x39da7419)));
        hpfH = mul(b.hpf, 0.5f);
    }
    oct = T.oct[clampi(b.octave, 0, 4)];
    detT = fmac(b.detuneLfo, lfo1, fmac(b.detuneEnv, env1, detR));
    widT = fmac(b.widthLfo, lfo2, fmac(b.widthEnv, env2, b.width));
}

void XtCosmSbf::update(float p, float env1, float r1, float lfo1, float env2, float r2, float lfo2) {
    pitch = p;
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    detT = fmac(blk.detuneLfo, lfo1, fmac(blk.detuneEnv, env1, detR));
    widT = fmac(blk.widthLfo, lfo2, fmac(blk.widthEnv, env2, blk.width));
}

void XtCosmSbf::process(float* lr, float scale) {
    // the follower (0x895017e0): Σ|L|, every eighth frame twice
    float sum = 0.0f;
    for (int i = 0; i < 32; ++i) {
        const float a = lr[2 * i] < 0.0f ? -lr[2 * i] : lr[2 * i];
        sum = add(sum, a);
        if ((i & 7) == 7) sum = add(sum, a);
    }
    lev = fmac(sub(mul(mul(sum, scale), K(0x2e7fffff)), lev), kSlow, lev);
    sl[3] = fmac(kSlew, sub(blk.detuneOwn, sl[3]), sl[3]);
    sl[4] = fmac(kSlew, sub(blk.widthOwn, sl[4]), sl[4]);
    const float wt = clampf(add(widT, mul(sl[4], lev)), 0.0f, blk.second ? K(0x3f733333) : 1.0f);
    const float step = mul(sub(wt, wid), rate2);
    float gt = 0.0f;                                   // × the block's 1.0
    if (wt != 0.0f) {
        const float a = sub(1.0f, 0.0f > wt ? -wt : wt);
        gt = add(add(a, a), mul(wt, wt));
    }
    // a new delay only once the last crossfade has finished
    float cur = dly, nd = nDly, p = prog;
    if (!(1.0f > p)) {
        const float x = div(oct, mul(root, pitch));
        const float d = foldDelay(mul(mul(44100.0f, add(detT, mul(lev, sl[3]))), x));
        if (d != cur) { p = 0.0f; nd = d; }
    }
    nDly = nd;
    sl[0] = fmac(kSlew, sub(blk.second ? blk.q : hpfH, sl[0]), sl[0]);
    sl[1] = fmac(kSlew, sub(blk.lpf, sl[1]), sl[1]);
    if (!blk.second) sl[2] = fmac(kSlew, sub(subR, sl[2]), sl[2]);
    const float hs = sl[0], ls = sl[1], g1 = sub(1.0f, fr[0]), lsI = sub(1.0f, ls);
    float gc = gn, wd = wid, y = dcy;
    float* const B1 = buf1.data();
    float* const B2 = buf2.data() + kGuard;            // line 2 may read up to 136 before it (1st)
    auto back = [&](int d) { const int j = w - d; return j < 0 ? j + kCombLen : j; };
    if (!blk.second) {                                 // 0x89500640
        const float g2 = sub(1.0f, fr[1]), sub2 = sl[2];
        int ra = ftrc(cur), rb = ftrc(mul(cur, sub2)), rc = ftrc(nd), rd = ftrc(mul(nd, sub2));
        float a1 = s[0], h1 = s[1], a2 = s[2], h2 = s[3], l1 = s[4], l2 = s[5];
        for (int i = 0; i < 32; ++i) {
            if ((i & 3) == 0) { wd = add(wd, step); gc = fmac(kStep, sub(gt, gc), gc); }
            if (++w >= kCombLen) w = 0;
            const float e = sub(lr[2 * i], y);
            y = add(y, mul(e, dcK));
            const float q = sub(1.0f, p);
            // line 1: taps → LPF → allpass, then the Width and the HPF
            l1 = add(mul(l1, lsI), mul(add(mul(B1[back(rc)], p), mul(q, B1[back(ra)])), ls));
            float t = sub(l1, mul(g1, a1));
            const float o1 = add(mul(g1, t), a1);
            a1 = t;
            float v = mul(sub(e, o1), wd);
            const float y1 = add(add(mul(h1, hs), mul(add(1.0f, hs), v)), o1);
            h1 = v;
            B1[w] = y1;
            // line 2, at the delay × the Sub Detune factor
            l2 = add(mul(l2, lsI), mul(add(mul(B2[back(rd)], p), mul(q, B2[back(rb)])), ls));
            t = sub(l2, mul(g2, a2));
            const float o2 = add(mul(g2, t), a2);
            a2 = t;
            v = mul(sub(e, o2), wd);
            const float y2 = add(add(mul(h2, hs), mul(add(1.0f, hs), v)), o2);
            h2 = v;
            B2[w] = y2;
            lr[2 * i] = lr[2 * i + 1] = mul(add(y2, y1), gc);
            p = add(p, kFade);
            if (p > 1.0f) { p = 1.0f; cur = nd; ra = rc; rb = rd; }
        }
        s[0] = a1; s[1] = h1; s[2] = a2; s[3] = h2; s[4] = l1; s[5] = l2;
    } else {                                           // 0x89500d40
        int ra = ftrc(cur), rc = ftrc(nd);
        float lb = s[0], ab = s[1], la = s[2], aa = s[3];
        for (int i = 0; i < 32; ++i) {
            if ((i & 3) == 0) { gc = fmac(kStep, sub(gt, gc), gc); wd = add(wd, step); }
            if (++w >= kCombLen) w = 0;
            const float e = sub(lr[2 * i], y);
            y = add(y, mul(e, dcK));
            const int ia = back(ra), ib = back(rc);
            const float q = sub(1.0f, p);
            // A: buffer 2's taps → allpass → LPF; B: buffer 1's
            float t = sub(add(mul(B2[ib], p), mul(B2[ia], q)), mul(g1, aa));
            const float oa = add(mul(g1, t), aa);
            aa = t;
            la = add(mul(la, lsI), mul(oa, ls));
            t = sub(add(mul(B1[ib], p), mul(q, B1[ia])), mul(g1, ab));
            const float ob = add(mul(g1, t), ab);
            ab = t;
            lb = add(mul(lb, lsI), mul(ob, ls));
            const float ya = add(la, mul(lb, wd));
            B2[w] = ya;
            B1[w] = add(mul(add(mul(lb, hs), sub(e, ya)), wd), lb);
            lr[2 * i] = lr[2 * i + 1] = mul(ya, gc);
            p = add(p, kFade);
            if (p > 1.0f) { p = 1.0f; cur = nd; ra = rc; }
        }
        s[0] = lb; s[1] = ab; s[2] = la; s[3] = aa;
    }
    gn = gc; wid = wd; dcy = y; dly = cur; prog = p;
}


// ── The Resonator ──

namespace {

// 0x89532ea8: the exponent field moved by e; 0 below, ∞ above
float fwLdexpf(float x, int e) {
    const uint32_t u = std::bit_cast<uint32_t>(x), sgn = u & 0x80000000u;
    const int ef = (int)((u >> 23) & 0xff);
    if (ef == 255) return (u & 0x7fffffu) ? K(0x7fc00001) : x;
    if (ef == 0) return std::bit_cast<float>(sgn);
    const int ne = ef + e;
    if (ne > 0 && ne < 255) return std::bit_cast<float>(sgn | (uint32_t)ne << 23 | (u & 0x7fffffu));
    return std::bit_cast<float>(e < 0 ? sgn : sgn | 0x7f800000u);
}

// The Size curve (0x894dc700)
float resSize(int size, int kf, int key) {
    const float x = div(div(mul(flt((int16_t)(key - 60)), flt((int16_t)(kf * 10))), 100.0f), 12.0f);
    float y = div(mul(x, K(0x3f317219)), K(0x3fcccccc));                        // × the firmware's logf(2) / 1.6
    y = add(add(y, div(mul(flt(clampi((int16_t)size, 0, 127)), 2.0f), 127.0f)), -1.0f);
    if (y > 1.0f) return 1.0f;
    return -1.0f > y ? -1.0f : y;
}

} // namespace

float xtCosmExpf(float x) {
    const uint32_t u = std::bit_cast<uint32_t>(x);
    if (x != x) return K(0x7fc00001);
    if (x == 0.0f) return 1.0f;
    if ((int)((u >> 23) & 0xff) - 127 >= 15) return (u & 0x80000000u) ? 0.0f : K(0x7f800000);
    const float y = mul(x, K(0x3fb8aa3b));                                        // log2 e
    const uint32_t yu = std::bit_cast<uint32_t>(y);
    float n = (yu & 0x7f800000u) < 0x4b000000u ? flt(ftrc(y)) : y;
    n = std::bit_cast<float>(std::bit_cast<uint32_t>(n) | (yu & 0x80000000u));    // −0 for y in (−1, 0)
    float f = sub(y, n);
    int e = ftrc(n);
    const uint32_t fu = std::bit_cast<uint32_t>(f);
    if ((int)((fu >> 23) & 0xff) - 127 >= -1) {                                  // |f| ≥ ½: one step toward f
        if (fu & 0x80000000u) { e -= 1; f = add(f, 1.0f); }
        else { e += 1; f = add(f, -1.0f); }
    }
    const float t = mul(f, K(0x3f317218));                                        // ln 2
    float p = T.expP[0], q = T.expQ[0];
    for (int i = 1; i < 7; ++i) p = fmac(p, t, T.expP[i]);
    for (int i = 1; i < 7; ++i) q = fmac(q, t, T.expQ[i]);
    return fwLdexpf(div(p, q), e);
}

XtResBlock xtResBlock(const XtResParams& p, int key, float velA, float velB) {
    XtResBlock b;
    b.type = (int8_t)p.type;
    b.size = resSize(p.size, p.sizeKf, key);
    b.sizeEnv = envDepth(velA, p.sizeEnv);
    b.sizeLfo = sq63(clampi((int16_t)p.sizeLfo, -63, 63));
    b.balance = add(div(flt(clampi((int16_t)p.balance, 0, 100)), 100.0f), 0.0f);
    b.balanceEnv = envDepth(velB, p.balanceEnv);
    b.balanceLfo = sq63(clampi((int16_t)p.balanceLfo, -63, 63));
    return b;
}

float XtCosmRes::sizeOf(float env, float lfo) const {
    return xtCosmExpf(mul(clampf(fmac(blk.sizeLfo, lfo, fmac(blk.sizeEnv, env, blk.size)), -1.0f, 1.0f), K(0x3fcccccd)));
}

float XtCosmRes::balanceOf(float env, float lfo) const {
    return clampf(fmac(blk.balanceLfo, lfo, fmac(blk.balanceEnv, env, blk.balance)), 0.0f, 1.0f);
}

void XtCosmRes::noteOn(const XtResBlock& b, float env1Start, float env1Out, float r1, float lfo1,
                       float env2Start, float env2Out, float r2, float lfo2) {
    for (float& x : s) x = 0.0f;                       // 0x894edf70 clears D+0x1c…0x88
    blk = b;
    sz = sizeOf(env1Start, lfo1);                      // 0x894ec580
    bal = balanceOf(env2Start, lfo2);
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    setBlock(b, env1Out, lfo1, env2Out, lfo2);
}

void XtCosmRes::setBlock(const XtResBlock& b, float env1, float lfo1, float env2, float lfo2) {
    blk = b;
    szT = sizeOf(env1, lfo1);
    balT = balanceOf(env2, lfo2);
}

void XtCosmRes::update(float env1, float r1, float lfo1, float env2, float r2, float lfo2) {
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    szT = sizeOf(env1, lfo1);
    balT = balanceOf(env2, lfo2);
}

void XtCosmRes::process(float* lr) {
    const float* const C = T.res[clampi(blk.type, 0, 2)];
    const float d1 = mul(sub(szT, sz), rate1), d2 = mul(sub(balT, bal), rate2);
    float c[6];
    for (int i = 0; i < 32; ++i) {
        if ((i & 3) == 0) {                            // before every fourth sample
            sz = add(sz, d1);
            bal = add(bal, d2);
            for (int j = 0; j < 6; ++j) c[j] = mul(C[3 * j], sz);
        }
        const float x = lr[2 * i];
        for (int j = 0; j < 6; ++j) {
            float& a = s[2 * j];
            float& b = s[2 * j + 1];
            b = fmac(c[j], a, b);
            const float t = sub(fmac(C[3 * j + 1], a, x), b);
            a = fmac(c[j], t, a);
        }
        const float dry = mul(sub(1.0f, bal), x);
        float w = mul(C[2], s[0]);
        for (int j = 1; j < 6; ++j) w = fmac(C[3 * j + 2], s[2 * j], w);
        lr[2 * i] = lr[2 * i + 1] = add(mul(w, bal), dry);
    }
}


// ── The Frequency Shifter ──

XtFsBlock xtFsBlock(const XtFsParams& p, int key, float velA, float velB) {
    XtFsBlock b;
    b.depth = sbfWidth(p.depth, p.depthKf, key);
    b.depthEnv = envDepth(velA, p.depthEnv);
    b.depthLfo = sq63(clampi((int16_t)p.depthLfo, -63, 63));
    b.balance = add(div(flt(clampi((int16_t)p.balance, 0, 100)), 100.0f), 0.0f);
    b.balanceEnv = envDepth(velB, p.balanceEnv);
    b.balanceLfo = sq63(clampi((int16_t)p.balanceLfo, -63, 63));
    return b;
}

XtCosmFs::XtCosmFs() : buf(kLen, 0.0f) {}

float XtCosmFs::depthOf(float env, float lfo) const {
    const float d = clampf(fmac(blk.depthLfo, lfo, fmac(blk.depthEnv, env, blk.depth)), 0.0f, 1.0f);
    return fmac(d, K(0x3c4e703b), K(0x3a83126e));                                 // ·0.0126 + 0.001
}

float XtCosmFs::balanceOf(float env, float lfo) const {
    return clampf(fmac(blk.balanceLfo, lfo, fmac(blk.balanceEnv, env, blk.balance)), 0.0f, 1.0f);
}

void XtCosmFs::noteOn(const XtFsBlock& b, float env1Start, float env1Out, float r1, float lfo1,
                      float env2Start, float env2Out, float r2, float lfo2) {
    w = kLen;                                          // 0xcc2: the first sample goes to 0
    std::fill(buf.begin(), buf.end(), 0.0f);
    blk = b;
    dep = depthOf(env1Start, lfo1);                    // 0x894ec9f0
    bal = balanceOf(env2Start, lfo2);
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    setBlock(b, env1Out, lfo1, env2Out, lfo2);
    ph = 0.0f;                                         // 0x894ee430
}

void XtCosmFs::setBlock(const XtFsBlock& b, float env1, float lfo1, float env2, float lfo2) {
    blk = b;
    depT = depthOf(env1, lfo1);
    balT = balanceOf(env2, lfo2);
}

void XtCosmFs::update(float env1, float r1, float lfo1, float env2, float r2, float lfo2) {
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    depT = depthOf(env1, lfo1);
    balT = balanceOf(env2, lfo2);
}

void XtCosmFs::process(float* lr) {
    const float d1 = mul(sub(depT, dep), rate1), d2 = mul(sub(balT, bal), rate2);
    const float* const C = T.hilb;
    const float* const S = T.sine;
    float* const B = buf.data();
    auto at = [&](int k) { const int j = w + k; return B[j < 0 ? j + kLen : j]; };   // k ≤ 0 samples back
    float acc[4] = {}, inc = 0, sgn = 0;
    for (int i = 0; i < 32; ++i) {
        const int m = i & 3;
        if (m == 0) {                                  // before every fourth sample
            dep = add(dep, d1);
            bal = add(bal, d2);
            if (0.0f > dep) { inc = -dep; sgn = 1.0f; }
            else { inc = dep; sgn = -1.0f; }
        }
        if (++w >= kLen) w = 0;
        const float x = lr[2 * i];
        B[w] = x;
        if (m == 0) {                                  // the four outputs' terms of the samples present
            for (int a = 0; a < 4; ++a) {
                acc[a] = 0.0f;
                for (int j = (a + 1) / 2; j < 32; ++j) acc[a] = fmac(C[j], at(a - 2 * j - m), acc[a]);
            }
        } else {                                       // this sample's terms in the outputs still to come
            for (int a = m; a < 4; a += 2) acc[a] = fmac(C[(a - m) / 2], x, acc[a]);
        }
        int di = w - 31;
        if (di < 0) di += kLen;
        const float d = B[di];
        ph = add(ph, inc);
        if (ph > 1.0f) ph = sub(ph, 1.0f);
        const float y = mul(ph, 127.0f);
        const int k = ftrc(y);
        const float f = sub(y, flt(k)), g = sub(1.0f, f);
        const float sn = fmac(f, S[k + 1], mul(g, S[k]));
        int k2 = k + 32;
        if (k2 > 127) k2 -= 128;
        const float cs = fmac(f, S[k2 + 1], mul(g, S[k2]));
        const float v = mul(fmac(sgn, mul(d, cs), mul(acc[m], mul(2.0f, sn))), bal);
        lr[2 * i] = lr[2 * i + 1] = add(mul(sub(1.0f, bal), x), v);
    }
}


// ── Lo-Fi ──

XtLofiBlock xtLofiBlock(const XtLofiParams& p, int key, float velA, float velB) {
    XtLofiBlock b;
    b.bitDown = (int8_t)p.bitDown;
    b.rateDown = (int8_t)p.rateDown;
    // 0x894dc810: (Filter·0.9/127 + 0.1)·2^(KF·10·(key − 60)/1200), within 0.1…1
    const float y = mul(add(div(mul(flt(clampi((int16_t)p.filter, 0, 127)), K(0x3f666666)), 127.0f), K(0x3dcccccd)),
                        xtOscTimeKeyScale((int16_t)(p.filterKf * 10), key));
    b.filter = y > 1.0f ? 1.0f : (K(0x3dcccccd) > y ? K(0x3dcccccd) : y);
    b.filterEnv = envDepth(velA, p.filterEnv);
    b.filterLfo = sq63(clampi((int16_t)p.filterLfo, -63, 63));
    b.balance = add(div(flt(clampi((int16_t)p.balance, 0, 100)), 100.0f), 0.0f);
    b.balanceEnv = envDepth(velB, p.balanceEnv);
    b.balanceLfo = sq63(clampi((int16_t)p.balanceLfo, -63, 63));
    return b;
}

float XtCosmLofi::filterOf(float env, float lfo) const {
    return clampf(fmac(blk.filterLfo, lfo, fmac(blk.filterEnv, env, blk.filter)), K(0x3dcccccd), 1.0f);
}

float XtCosmLofi::balanceOf(float env, float lfo) const {
    const float b = clampf(fmac(blk.balanceLfo, lfo, fmac(blk.balanceEnv, env, blk.balance)), 0.0f, 1.0f);
    return T.lofiBal[clampi(ftrc(mul(b, 127.0f)), 0, 127)];
}

void XtCosmLofi::noteOn(const XtLofiBlock& b, float env1Start, float env1Out, float r1, float lfo1,
                        float env2Start, float env2Out, float r2, float lfo2) {
    for (float& x : s) x = 0.0f;                       // 0x894edf70 clears D+0x1c…0x88
    blk = b;
    flt_ = filterOf(env1Start, lfo1);                  // 0x894eca70
    bal = balanceOf(env2Start, lfo2);
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    setBlock(b, env1Out, lfo1, env2Out, lfo2);
}

void XtCosmLofi::setBlock(const XtLofiBlock& b, float env1, float lfo1, float env2, float lfo2) {
    blk = b;
    fltT = filterOf(env1, lfo1);
    balT = balanceOf(env2, lfo2);
    const int r = clampi(b.rateDown, 0, 5), bit = clampi(b.bitDown, 0, 15);
    mask = T.lofiMask[r];
    coef = T.lofiCoef[r];
    up = T.lofiUp[bit];
    down = T.lofiDown[bit];
}

void XtCosmLofi::update(float env1, float r1, float lfo1, float env2, float r2, float lfo2) {
    rate1 = mul(r1, 4.0f);
    rate2 = mul(r2, 4.0f);
    fltT = filterOf(env1, lfo1);
    balT = balanceOf(env2, lfo2);
}

void XtCosmLofi::process(float* lr, float scale) {
    const float d1 = mul(sub(fltT, flt_), rate1), d2 = mul(sub(balT, bal), rate2);
    const float unscale = 1.0f / scale;                // a power of 2: exact both ways
    float a1 = s[0], b1 = s[1], a2 = s[2], b2 = s[3], a3 = s[4], b3 = s[5], hold = 0;
    for (int i = 0; i < 32; ++i) {
        if ((i & 3) == 0) {                            // before every fourth sample
            flt_ = add(flt_, d1);
            bal = add(bal, d2);
        }
        const float x = mul(lr[2 * i], scale);
        b1 = fmac(coef, a1, b1);
        a1 = fmac(coef, sub(sub(x, b1), a1), a1);
        b2 = fmac(coef, a2, b2);
        a2 = fmac(coef, sub(sub(b1, b2), a2), a2);
        if (((i & ~3) & mask) == 0) hold = b2;         // the count steps by 4 a group
        const int q = ftrc(mul(mul(hold, down), K(0x38000000)));
        b3 = fmac(flt_, a3, b3);
        a3 = fmac(flt_, sub(sub(mul(mul(flt(q), up), 32768.0f), b3), a3), a3);
        lr[2 * i] = lr[2 * i + 1] = mul(fmac(bal, b3, mul(sub(1.0f, bal), x)), unscale);
    }
    s[0] = a1; s[1] = b1; s[2] = a2; s[3] = b2; s[4] = a3; s[5] = b3;
}


#ifdef XT_COSM_MAIN
// xt_cosm <rom.2.bin> [cases]: the TVF, the Dual, TB, Comb and Side Band Filters, the Resonator, the Frequency Shifter and Lo-Fi against the firmware in the SH-4 interpreter:
// the converter (0x894dd870), the start values (0x894ec340), the block's
// targets (0x894ed8f0), the 10-ms update (0x894ee6a0) and the filter
// (0x89502890 → the twelve kernels) on random states, coefficients and
// signals, run on from block to block.
#include "Sh4Interp.h"
#include <cstdlib>
#include <cstring>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 2 || !xtCosmLoadTables(argv[1]) || !xtOscLoadTables(argv[1])) { std::fprintf(stderr, "usage: xt_cosm rom.2.bin\n"); return 2; }
#if defined(__aarch64__)
    {   // denormals flushed, as the SH-4 does (FPSCR.DN = 1) and the plugin's audio thread does
        uint64_t fpcr;
        __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
        __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
    }
#endif
    Sh4 c;
    if (!c.loadRom(argv[1])) return 1;
    const uint32_t ram = 0x8C000000, sp = ram + 0xF0000;
    c.mapRam(ram, 0x100000);
    auto f32 = [&](uint32_t a) { const uint32_t v = c.rd32(a); float x; std::memcpy(&x, &v, 4); return x; };
    auto wf = [&](uint32_t a, float x) { uint32_t u; std::memcpy(&u, &x, 4); c.wr32(a, u); };
    auto same = [](float a, float b) { return std::memcmp(&a, &b, 4) == 0; };
    uint32_t seed = 12345;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 8) & 0xffff; };
    auto frnd = [&](float lo, float hi) { return lo + (hi - lo) * (float)rnd() / 65535.0f; };

    // the converter
    const uint32_t IN = ram + 0x1000, OUT = ram + 0x1100;
    long cb = 0, cn = 0;
    for (int k = 0; k < 200000; ++k) {
        XtTvfParams p;
        p.filter = (int)(rnd() % 12); p.q = (int)(rnd() % 128); p.qLfo = (int)(rnd() % 127) - 63; p.qEnv = (int)(rnd() % 127) - 63;
        p.cutoff = (int)(rnd() % 128); p.keyfollow = (int)(rnd() % 41) - 20; p.cutoffLfo = (int)(rnd() % 127) - 63;
        p.cutoffEnv = (int)(rnd() % 127) - 63;
        const int key = (int)(rnd() % 128), cosm = (int)(rnd() & 1);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)key);
        const int vals[8] = { p.filter, p.q, p.qLfo, p.qEnv, p.cutoff, p.keyfollow, p.cutoffLfo, p.cutoffEnv };
        for (int i = 0; i < 8; ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)vals[i]);
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(0x894dd870, { IN, OUT }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return 1; }
        const XtTvfBlock b = xtTvfBlock(p, key, va, vb);
        ++cn;
        const bool ok = c.rd32(OUT) == 13 && (int)c.rd32(OUT + 0x34) == b.mode && same(f32(OUT + 4), b.q) && same(f32(OUT + 8), b.qEnv)
                     && same(f32(OUT + 0x10), b.qLfo) && same(f32(OUT + 0x1c), b.cutoff) && same(f32(OUT + 0x20), b.cutoffEnv)
                     && same(f32(OUT + 0x28), b.cutoffLfo) && c.rd32(OUT + 0xc) == (cosm ? 12u : 10u) && c.rd32(OUT + 0x24) == (cosm ? 11u : 9u)
                     && c.rd32(OUT + 0x14) == (cosm ? 4u : 3u) && c.rd32(OUT + 0x2c) == (cosm ? 4u : 3u);
        if (!ok && cb++ < 5)
            std::printf("  converter key %d cutoff %d kf %d q %d: fw q %.9g cut %.9g cenv %.9g, port %.9g %.9g %.9g\n", key, p.cutoff, p.keyfollow, p.q,
                        f32(OUT + 4), f32(OUT + 0x1c), f32(OUT + 0x20), b.q, b.cutoff, b.cutoffEnv);
    }
    std::printf("converter: %ld cases, %ld different\n", cn, cb);
    long cb2 = 0, cn2 = 0;
    for (int k = 0; k < 200000; ++k) {
        XtTvfParams p;
        p.filter = (int)(rnd() % 12); p.q = (int)(rnd() % 128); p.qLfo = (int)(rnd() % 127) - 63; p.qOwn = (int)(rnd() % 127) - 63;
        p.qEnv = (int)(rnd() % 127) - 63; p.cutoff = (int)(rnd() % 128); p.keyfollow = (int)(rnd() % 41) - 20;
        p.cutoffLfo = (int)(rnd() % 127) - 63; p.cutoffOwn = (int)(rnd() % 127) - 63; p.cutoffEnv = (int)(rnd() % 127) - 63;
        const int key = (int)(rnd() % 128), cosm = (int)(rnd() & 1);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)key);
        const int vals[10] = { p.filter, p.q, p.qLfo, p.qOwn, p.qEnv, p.cutoff, p.keyfollow, p.cutoffLfo, p.cutoffOwn, p.cutoffEnv };
        for (int i = 0; i < 10; ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)vals[i]);
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(0x894dda40, { IN, OUT }, sp)) { std::fprintf(stderr, "converter 11: %s\n", c.error().c_str()); return 1; }
        const XtTvfBlock b = xtDtvfBlock(p, key, va, vb);
        ++cn2;
        const bool ok = c.rd32(OUT) == 14 && (int)c.rd32(OUT + 0x34) == b.mode && same(f32(OUT + 4), b.q) && same(f32(OUT + 8), b.qEnv)
                     && same(f32(OUT + 0x10), b.qLfo) && same(f32(OUT + 0x18), b.qOwn) && same(f32(OUT + 0x1c), b.cutoff)
                     && same(f32(OUT + 0x20), b.cutoffEnv) && same(f32(OUT + 0x28), b.cutoffLfo) && same(f32(OUT + 0x30), b.cutoffOwn)
                     && c.rd32(OUT + 0xc) == (cosm ? 12u : 10u) && c.rd32(OUT + 0x24) == (cosm ? 11u : 9u);
        if (!ok && cb2++ < 5)
            std::printf("  dynamic converter key %d: fw qown %.9g cown %.9g cut %.9g, port %.9g %.9g %.9g\n", key, f32(OUT + 0x18), f32(OUT + 0x30),
                        f32(OUT + 0x1c), b.qOwn, b.cutoffOwn, b.cutoff);
    }
    std::printf("dynamic converter: %ld cases, %ld different\n", cn2, cb2);
    cb += cb2;

    // the object: D = obj + 0x58; the envelopes and LFOs it reads
    const uint32_t OBJ = ram + 0x2000, D = OBJ + 0x58, EQ = ram + 0x3000, EC = ram + 0x3100, LQ = ram + 0x3200, LC = ram + 0x3300,
                   BLK = ram + 0x3400, BUF = ram + 0x4000;
    long ub = 0, un = 0, db = 0, dn = 0;
    const int nCases = argc > 2 ? std::atoi(argv[2]) : 3000;
    for (int g = 0; g < 2 * nCases; ++g) {
        const bool dyn = g >= nCases;                // the TVF (code 13), then the Dynamic TVF (code 14)
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200); c.write(BLK, z.data(), 0x80);
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        c.wr8(D, dyn ? 14 : 13);
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);
        XtTvfParams p;
        p.filter = (int)(rnd() % 12); p.q = (int)(rnd() % 128); p.qLfo = (int)(rnd() % 127) - 63; p.qEnv = (int)(rnd() % 127) - 63;
        p.cutoff = (int)(rnd() % 128); p.keyfollow = (int)(rnd() % 41) - 20; p.cutoffLfo = (int)(rnd() % 127) - 63;
        p.cutoffEnv = (int)(rnd() % 127) - 63;
        p.qOwn = (int)(rnd() % 127) - 63; p.cutoffOwn = (int)(rnd() % 127) - 63;
        const int bkey = (int)(rnd() % 128);
        const float bva = frnd(0, 1), bvb = frnd(0, 1);
        const XtTvfBlock b = dyn ? xtDtvfBlock(p, bkey, bva, bvb) : xtTvfBlock(p, bkey, bva, bvb);
        // the block as the converter leaves it
        c.wr32(BLK, dyn ? 14 : 13); wf(BLK + 4, b.q); wf(BLK + 0x18, b.qOwn); wf(BLK + 0x30, b.cutoffOwn); wf(BLK + 8, b.qEnv); c.wr32(BLK + 0xc, 10); wf(BLK + 0x10, b.qLfo); c.wr32(BLK + 0x14, 3);
        wf(BLK + 0x1c, b.cutoff); wf(BLK + 0x20, b.cutoffEnv); c.wr32(BLK + 0x24, 9); wf(BLK + 0x28, b.cutoffLfo); c.wr32(BLK + 0x2c, 3);
        c.wr32(BLK + 0x34, (uint32_t)b.mode);
        // note-on: the start values (0x894ec340, the envelopes' +0x58 and the LFOs' +0x38) …
        const float eqS = frnd(-1, 1), ecS = frnd(-1, 1), lq = frnd(-1, 1), lc = frnd(-1, 1), eqO = frnd(-1, 1), ecO = frnd(-1, 1);
        const float rq = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f, rc = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
        wf(EQ + 0x58, eqS); wf(EC + 0x58, ecS); wf(LQ + 0x38, lq); wf(LC + 0x38, lc);
        const uint32_t s0 = sp - 16;
        c.wr32(s0, EQ); c.wr32(s0 + 4, LQ); c.wr32(s0 + 8, EC); c.wr32(s0 + 12, LC);
        if (!c.call(0x894ec340, { OBJ, BLK, D, OBJ }, s0)) { std::fprintf(stderr, "start: %s\n", c.error().c_str()); return 1; }
        // … and the targets (0x894ed8f0, from the outputs the object holds at +0x104…)
        wf(OBJ + 0x104, eqO); wf(OBJ + 0x108, ecO); wf(OBJ + 0x10c, lq); wf(OBJ + 0x110, lc);
        if (!c.call(dyn ? 0x894eda10 : 0x894ed8f0, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "block: %s\n", c.error().c_str()); return 1; }
        wf(OBJ + 0x11c, 4.0f * rq); wf(OBJ + 0x120, 4.0f * rc);
        XtCosmTvf port;
        port.noteOn(b, eqS, eqO, rq, lq, ecS, ecO, rc, lc);
        if (dyn) {                                   // 0x894edf70, type 14: the slews from the block; the follower carries on
            c.wr32(OBJ + 0xe8, c.rd32(OBJ + 0x28)); c.wr32(OBJ + 0xec, c.rd32(OBJ + 0x40));
            const float lev0 = (rnd() & 1) ? 0.0f : frnd(0, 3);
            wf(OBJ + 0xf0, lev0);
            port.setFollower(port.qOwnSlewed(), port.cutoffOwnSlewed(), lev0);
        }
        ++un;
        if ((!same(f32(D + 0xbc), port.c1()) || !same(f32(D + 0xc0), port.c2()) || !same(f32(OBJ + 0xfc), port.target1())
             || !same(f32(OBJ + 0x100), port.target2()) || (int)c.rd32(OBJ + 0x44) != b.mode) && ub++ < 5)
            std::printf("  note-on %d: fw c %.9g %.9g t %.9g %.9g, port %.9g %.9g %.9g %.9g\n", g, f32(D + 0xbc), f32(D + 0xc0), f32(OBJ + 0xfc),
                        f32(OBJ + 0x100), port.c1(), port.c2(), port.target1(), port.target2());
        for (int blkN = 0; blkN < 40; ++blkN) {
            if (blkN % 14 == 13) {                                   // an update every 10 ms: new envelope and LFO values
                const float eq = frnd(-1, 1), ec = frnd(-1, 1), nlq = frnd(-1, 1), nlc = frnd(-1, 1);
                const float nrq = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f, nrc = 1.0f / 32.0f;
                wf(EQ + 0x4c, eq); wf(EC + 0x4c, ec); wf(LQ + 0x38, nlq); wf(LC + 0x38, nlc); wf(EQ + 0x54, nrq); wf(EC + 0x54, nrc);
                c.fb[0][4] = 0;
                if (!c.call(0x894ee6a0, { OBJ }, sp)) { std::fprintf(stderr, "update: %s\n", c.error().c_str()); return 1; }
                port.update(eq, nrq, nlq, ec, nrc, nlc);
                ++un;
                if ((!same(f32(OBJ + 0xfc), port.target1()) || !same(f32(OBJ + 0x100), port.target2())) && ub++ < 5)
                    std::printf("  update %d.%d: fw %.9g %.9g port %.9g %.9g\n", g, blkN, f32(OBJ + 0xfc), f32(OBJ + 0x100), port.target1(), port.target2());
            }
            float lr[64];
            const int ar = (int)(rnd() % 4);
            const float amp = ar == 0 ? 30000.0f : ar == 1 ? 1.0e9f : ar == 2 ? 4.0e8f : 1.0f;   // the XT's scale is int16 × 65536
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(dyn ? 0x89502920 : 0x89502890, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++dn;
            bool ok = same(f32(D + 0xbc), port.c1()) && same(f32(D + 0xc0), port.c2());
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            const int nst = b.mode <= 4 ? 4 : b.mode <= 9 ? 2 : 1;
            for (int i = 0; i < nst; ++i) ok = ok && same(f32(D + 0x1c + 4 * (uint32_t)i), port.state()[i]);
            if (dyn) ok = ok && same(f32(D + 0x90), port.qOwnSlewed()) && same(f32(D + 0x94), port.cutoffOwnSlewed()) && same(f32(D + 0x98), port.level());
            if (std::getenv("XT_COSM_TRACE") && g == std::atoi(std::getenv("XT_COSM_TRACE")))
                std::printf("    trace %d: fw s %.9g %.9g %.9g %.9g c %.9g %.9g t %.9g %.9g r %.9g %.9g | port s %.9g %.9g %.9g %.9g c %.9g %.9g t %.9g %.9g\n",
                            blkN, f32(D + 0x1c), f32(D + 0x20), f32(D + 0x24), f32(D + 0x28), f32(D + 0xbc), f32(D + 0xc0), f32(OBJ + 0xfc),
                            f32(OBJ + 0x100), f32(OBJ + 0x11c), f32(OBJ + 0x120), port.state()[0], port.state()[1], port.state()[2],
                            port.state()[3], port.c1(), port.c2(), port.target1(), port.target2());
            if (!ok && db++ < 6)
                std::printf("  filter %d (%s) block %d mode %d: fw out %.9g c %.9g %.9g lev %.9g own %.9g %.9g, port %.9g %.9g %.9g %.9g %.9g %.9g\n",
                            g, dyn ? "dynamic" : "tvf", blkN, b.mode, f32(BUF), f32(D + 0xbc), f32(D + 0xc0), f32(D + 0x98), f32(D + 0x90), f32(D + 0x94),
                            lr[0], port.c1(), port.c2(), port.level(), port.qOwnSlewed(), port.cutoffOwnSlewed());
        }
    }
    std::printf("start, block and update: %ld cases, %ld different\n", un, ub);
    std::printf("filter: %ld blocks of 32 frames, %ld different\n", dn, db);

    // The Dual Filter (block code 12): the converter (0x894dd690) …
    auto dualParams = [&]() {
        XtDualParams p;
        p.type = (int)(rnd() % 3);
        p.freq1 = (int)(rnd() % 128); p.kf1 = (int)(rnd() % 41) - 20; p.q1 = (int)(rnd() % 128); p.lfo1 = (int)(rnd() % 127) - 63;
        p.env1 = (int)(rnd() % 127) - 63;
        p.freq2 = (int)(rnd() % 128); p.kf2 = (int)(rnd() % 41) - 20; p.q2 = (int)(rnd() % 128); p.lfo2 = (int)(rnd() % 127) - 63;
        p.env2 = (int)(rnd() % 127) - 63;
        return p;
    };
    long xb = 0, xn = 0;
    for (int k = 0; k < 200000; ++k) {
        const XtDualParams p = dualParams();
        const int key = (int)(rnd() % 128), cosm = (int)(rnd() & 1);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)key);
        const int vals[11] = { p.type, p.freq1, p.kf1, p.q1, p.lfo1, p.env1, p.freq2, p.kf2, p.q2, p.lfo2, p.env2 };
        for (int i = 0; i < 11; ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)vals[i]);
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(0x894dd690, { IN, OUT }, sp)) { std::fprintf(stderr, "dual converter: %s\n", c.error().c_str()); return 1; }
        const XtDualBlock b = xtDualBlock(p, key, va, vb);
        ++xn;
        const bool ok = c.rd32(OUT) == 12 && (int)c.rd32(OUT + 4) == b.mode && same(f32(OUT + 8), b.cut1) && same(f32(OUT + 0xc), b.env1)
                     && same(f32(OUT + 0x10), b.q1) && same(f32(OUT + 0x18), b.lfo1) && same(f32(OUT + 0x20), b.cut2)
                     && same(f32(OUT + 0x24), b.env2) && same(f32(OUT + 0x28), b.q2) && same(f32(OUT + 0x30), b.lfo2)
                     && c.rd32(OUT + 0x14) == (cosm ? 11u : 9u) && c.rd32(OUT + 0x2c) == (cosm ? 12u : 10u)
                     && c.rd32(OUT + 0x1c) == (cosm ? 4u : 3u) && c.rd32(OUT + 0x34) == (cosm ? 4u : 3u);
        if (!ok && xb++ < 5)
            std::printf("  dual converter key %d: fw cut %.9g %.9g q %.9g %.9g env %.9g %.9g, port %.9g %.9g %.9g %.9g %.9g %.9g\n", key,
                        f32(OUT + 8), f32(OUT + 0x20), f32(OUT + 0x10), f32(OUT + 0x28), f32(OUT + 0xc), f32(OUT + 0x24),
                        b.cut1, b.cut2, b.q1, b.q2, b.env1, b.env2);
    }
    std::printf("dual converter: %ld cases, %ld different\n", xn, xb);

    // … the start values (0x894ec340), the block (0x894ed5d0, also while the
    // note sounds), the 10-ms update (0x894ee6a0) and the filter (0x894fd090)
    auto writeDual = [&](const XtDualBlock& b) {
        c.wr32(BLK, 12); c.wr32(BLK + 4, (uint32_t)b.mode); wf(BLK + 8, b.cut1); wf(BLK + 0xc, b.env1); wf(BLK + 0x10, b.q1);
        c.wr32(BLK + 0x14, 9); wf(BLK + 0x18, b.lfo1); c.wr32(BLK + 0x1c, 3); wf(BLK + 0x20, b.cut2); wf(BLK + 0x24, b.env2);
        wf(BLK + 0x28, b.q2); c.wr32(BLK + 0x2c, 10); wf(BLK + 0x30, b.lfo2); c.wr32(BLK + 0x34, 3);
    };
    long yb = 0, yn = 0, zb = 0, zn = 0;
    for (int g = 0; g < nCases; ++g) {
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200); c.write(BLK, z.data(), 0x80);
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        c.wr8(D, 12);                                // slot 1: Filter 1's envelope (EQ) and LFO (LQ); slot 2: Filter 2's
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);
        const int bkey = (int)(rnd() % 128);
        const float bva = frnd(0, 1), bvb = frnd(0, 1);
        const XtDualBlock b = xtDualBlock(dualParams(), bkey, bva, bvb);
        writeDual(b);
        const float e1S = frnd(-1, 1), e2S = frnd(-1, 1), l1 = frnd(-1, 1), l2 = frnd(-1, 1), e1O = frnd(-1, 1), e2O = frnd(-1, 1);
        const float r1 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f, r2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
        wf(EQ + 0x58, e1S); wf(EC + 0x58, e2S); wf(LQ + 0x38, l1); wf(LC + 0x38, l2);
        const uint32_t s0 = sp - 16;
        c.wr32(s0, EQ); c.wr32(s0 + 4, LQ); c.wr32(s0 + 8, EC); c.wr32(s0 + 12, LC);
        if (!c.call(0x894ec340, { OBJ, BLK, D, OBJ }, s0)) { std::fprintf(stderr, "dual start: %s\n", c.error().c_str()); return 1; }
        wf(OBJ + 0x104, e1O); wf(OBJ + 0x108, e2O); wf(OBJ + 0x10c, l1); wf(OBJ + 0x110, l2);
        if (!c.call(0x894ed5d0, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "dual block: %s\n", c.error().c_str()); return 1; }
        wf(OBJ + 0x11c, 4.0f * r1); wf(OBJ + 0x120, 4.0f * r2);
        XtCosmDual port;
        port.noteOn(b, e1S, e1O, r1, l1, e2S, e2O, r2, l2);
        float o1 = e1O, o2 = e2O, ol1 = l1, ol2 = l2;  // the outputs the object holds (+0x104…)
        ++yn;
        if ((!same(f32(D + 0xbc), port.c1()) || !same(f32(D + 0xc0), port.c2()) || !same(f32(OBJ + 0xfc), port.target1())
             || !same(f32(OBJ + 0x100), port.target2()) || (int)c.rd32(OBJ + 0x14) != b.mode) && yb++ < 5)
            std::printf("  dual note-on %d: fw c %.9g %.9g t %.9g %.9g, port %.9g %.9g %.9g %.9g\n", g, f32(D + 0xbc), f32(D + 0xc0),
                        f32(OBJ + 0xfc), f32(OBJ + 0x100), port.c1(), port.c2(), port.target1(), port.target2());
        for (int blkN = 0; blkN < 40; ++blkN) {
            if (blkN % 14 == 13) {                                   // an update every 10 ms
                o1 = frnd(-1, 1); o2 = frnd(-1, 1); ol1 = frnd(-1, 1); ol2 = frnd(-1, 1);
                const float nr1 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f, nr2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
                wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2); wf(LQ + 0x38, ol1); wf(LC + 0x38, ol2); wf(EQ + 0x54, nr1); wf(EC + 0x54, nr2);
                c.fb[0][4] = 0;
                if (!c.call(0x894ee6a0, { OBJ }, sp)) { std::fprintf(stderr, "dual update: %s\n", c.error().c_str()); return 1; }
                port.update(o1, nr1, ol1, o2, nr2, ol2);
                ++yn;
                if ((!same(f32(OBJ + 0xfc), port.target1()) || !same(f32(OBJ + 0x100), port.target2())) && yb++ < 5)
                    std::printf("  dual update %d.%d: fw %.9g %.9g port %.9g %.9g\n", g, blkN, f32(OBJ + 0xfc), f32(OBJ + 0x100), port.target1(), port.target2());
            } else if (blkN % 14 == 6 && (rnd() & 1)) {               // a parameter change while the note sounds: a new block
                const XtDualBlock nb = xtDualBlock(dualParams(), bkey, bva, bvb);
                writeDual(nb);
                if (!c.call(0x894ed5d0, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "dual block: %s\n", c.error().c_str()); return 1; }
                port.setBlock(nb, o1, ol1, o2, ol2);
                ++yn;
                if ((!same(f32(OBJ + 0xfc), port.target1()) || !same(f32(OBJ + 0x100), port.target2())) && yb++ < 5)
                    std::printf("  dual block %d.%d: fw %.9g %.9g port %.9g %.9g\n", g, blkN, f32(OBJ + 0xfc), f32(OBJ + 0x100), port.target1(), port.target2());
            }
            float lr[64];
            const int ar = (int)(rnd() % 4);
            const float amp = ar == 0 ? 30000.0f : ar == 1 ? 1.0e9f : ar == 2 ? 4.0e8f : 1.0f;
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(0x894fd090, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "dual filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++zn;
            bool ok = same(f32(D + 0xbc), port.c1()) && same(f32(D + 0xc0), port.c2());
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            for (int i = 0; i < 8; ++i) ok = ok && same(f32(D + 0x1c + 4 * (uint32_t)i), port.state()[i]);
            if (!ok && zb++ < 6)
                std::printf("  dual filter %d block %d mode %d: fw out %.9g c %.9g %.9g s %.9g %.9g, port %.9g %.9g %.9g %.9g %.9g\n", g, blkN,
                            (int)c.rd32(OBJ + 0x14), f32(BUF), f32(D + 0xbc), f32(D + 0xc0), f32(D + 0x1c), f32(D + 0x38), lr[0], port.c1(),
                            port.c2(), port.state()[0], port.state()[7]);
        }
    }
    std::printf("dual start, block and update: %ld cases, %ld different\n", yn, yb);
    std::printf("dual filter: %ld blocks of 32 frames, %ld different\n", zn, zb);

    // The TB Filter (block code 20): its constants (0x894fc4c0, run at start-up) …
    if (!c.call(0x894fc4c0, {}, sp)) { std::fprintf(stderr, "tb init: %s\n", c.error().c_str()); return 1; }
    auto tbParams = [&]() {
        XtTbParams p;
        p.lpfCutoff = (int)(rnd() % 128); p.lpfKf = (int)(rnd() % 41) - 20; p.lpfLfo = (int)(rnd() % 127) - 63; p.lpfEnv = (int)(rnd() % 127) - 63;
        p.q = (int)(rnd() % 128); p.qFollow = (int)(rnd() % 127) - 63;
        p.hpfCutoff = (int)(rnd() % 128); p.hpfKf = (int)(rnd() % 41) - 20; p.hpfLfo = (int)(rnd() % 127) - 63; p.hpfEnv = (int)(rnd() % 127) - 63;
        p.gain = (int)(rnd() % 128);
        return p;
    };
    // … the converter (0x894de110) …
    long tb = 0, tn = 0;
    for (int k = 0; k < 200000; ++k) {
        const XtTbParams p = tbParams();
        const int key = (int)(rnd() % 128), cosm = (int)(rnd() & 1);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)key);
        const int vals[11] = { p.lpfCutoff, p.lpfKf, p.lpfLfo, p.lpfEnv, p.q, p.qFollow, p.hpfCutoff, p.hpfKf, p.hpfLfo, p.hpfEnv, p.gain };
        for (int i = 0; i < 11; ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)vals[i]);
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(0x894de110, { IN, OUT }, sp)) { std::fprintf(stderr, "tb converter: %s\n", c.error().c_str()); return 1; }
        const XtTbBlock b = xtTbBlock(p, key, va, vb);
        ++tn;
        const float fv[16] = { 0, b.gain, b.lcut, b.lkf, b.q, b.qf, b.hcut, b.hkf, b.lenv, 0, b.llfo, 0, b.henv, 0, b.hlfo, 0 };
        bool ok = c.rd32(OUT) == 20 && c.rd8(OUT + 0x40) == (uint8_t)key && c.rd32(OUT + 0x24) == (cosm ? 11u : 9u)
               && c.rd32(OUT + 0x34) == (cosm ? 12u : 10u) && c.rd32(OUT + 0x2c) == (cosm ? 4u : 3u) && c.rd32(OUT + 0x3c) == (cosm ? 4u : 3u);
        for (int i : { 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14 }) ok = ok && same(f32(OUT + 4 * (uint32_t)i), fv[i]);
        if (!ok && tb++ < 5) {
            std::printf("  tb converter key %d:", key);
            for (int i : { 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14 }) std::printf(" [%d] %.9g/%.9g", i, f32(OUT + 4 * (uint32_t)i), fv[i]);
            std::printf("\n");
        }
    }
    std::printf("tb converter: %ld cases, %ld different\n", tn, tb);

    // … the note-on (0x894fc110, 0x894fbd20), the update (0x894ee6a0), new
    // blocks while the note sounds (0x894fc110) and the filter (0x89501950)
    auto writeTb = [&](const XtTbBlock& b) {
        c.wr32(BLK, 20); wf(BLK + 4, b.gain); wf(BLK + 8, b.lcut); wf(BLK + 0xc, b.lkf); wf(BLK + 0x10, b.q); wf(BLK + 0x14, b.qf);
        wf(BLK + 0x18, b.hcut); wf(BLK + 0x1c, b.hkf); wf(BLK + 0x20, b.lenv); c.wr32(BLK + 0x24, 9); wf(BLK + 0x28, b.llfo); c.wr32(BLK + 0x2c, 3);
        wf(BLK + 0x30, b.henv); c.wr32(BLK + 0x34, 10); wf(BLK + 0x38, b.hlfo); c.wr32(BLK + 0x3c, 3); c.wr32(BLK + 0x40, (uint32_t)b.key);
    };
    auto tbSame = [&](const XtCosmTb& port, bool states) {
        bool ok = true;
        for (int i = 0; i < 4; ++i) ok = ok && same(f32(D + 0x1c + 4 * (uint32_t)i), port.coef()[i]) && same(f32(D + 0x3c + 4 * (uint32_t)i), port.target()[i]);
        for (int i = 0; i < 3; ++i) ok = ok && same(f32(D + 0x5c + 4 * (uint32_t)i), port.coef()[4 + i]) && same(f32(D + 0x78 + 4 * (uint32_t)i), port.target()[4 + i]);
        if (states)
            for (int i = 0; i < 4; ++i) ok = ok && same(f32(D + 0x2c + 4 * (uint32_t)i), port.lpState()[i]) && same(f32(D + 0x68 + 4 * (uint32_t)i), port.hpState()[i]);
        return ok;
    };
    long ob = 0, on = 0, fb = 0, fn = 0;
    for (int g = 0; g < nCases; ++g) {
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200); c.write(BLK, z.data(), 0x80);
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        c.wr8(D, 20);                                // slot 1: the LPF's envelope (EQ) and LFO (LQ); slot 2: the HPF's
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);
        const int bkey = (int)(rnd() % 128);
        const float bva = frnd(0, 1), bvb = frnd(0, 1);
        const XtTbBlock b = xtTbBlock(tbParams(), bkey, bva, bvb);
        writeTb(b);
        float o1 = frnd(-1, 1), o2 = frnd(-1, 1), ol1 = frnd(-1, 1), ol2 = frnd(-1, 1);
        wf(OBJ + 0x104, o1); wf(OBJ + 0x108, o2); wf(OBJ + 0x10c, ol1); wf(OBJ + 0x110, ol2);
        if (!c.call(0x894fc110, { OBJ, BLK + 4 }, sp) || !c.call(0x894fbd20, { OBJ }, sp)) { std::fprintf(stderr, "tb note-on: %s\n", c.error().c_str()); return 1; }
        XtCosmTb port;
        port.noteOn(b, o1, ol1, o2, ol2);
        ++on;
        if (!tbSame(port, true) && ob++ < 5)
            std::printf("  tb note-on %d: fw c %.9g %.9g %.9g %.9g t %.9g %.9g, port c %.9g %.9g %.9g %.9g t %.9g %.9g\n", g, f32(D + 0x1c), f32(D + 0x20),
                        f32(D + 0x24), f32(D + 0x28), f32(D + 0x78), f32(D + 0x7c), port.coef()[0], port.coef()[1], port.coef()[2], port.coef()[3],
                        port.target()[4], port.target()[5]);
        for (int blkN = 0; blkN < 40; ++blkN) {
            if (blkN % 14 == 13) {                                   // an update every 10 ms
                o1 = frnd(-1, 1); o2 = frnd(-1, 1); ol1 = frnd(-1, 1); ol2 = frnd(-1, 1);
                wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2); wf(LQ + 0x38, ol1); wf(LC + 0x38, ol2); wf(EQ + 0x54, 1.0f / 32.0f); wf(EC + 0x54, 1.0f / 32.0f);
                c.fb[0][4] = 0;
                if (!c.call(0x894ee6a0, { OBJ }, sp)) { std::fprintf(stderr, "tb update: %s\n", c.error().c_str()); return 1; }
                port.update(o1, ol1, o2, ol2);
                ++on;
                if (!tbSame(port, false) && ob++ < 5)
                    std::printf("  tb update %d.%d: fw t %.9g %.9g %.9g %.9g port %.9g %.9g %.9g %.9g\n", g, blkN, f32(D + 0x3c), f32(D + 0x40), f32(D + 0x44),
                                f32(D + 0x48), port.target()[0], port.target()[1], port.target()[2], port.target()[3]);
            } else if (blkN % 14 == 6 && (rnd() & 1)) {               // a parameter change while the note sounds
                const XtTbBlock nb = xtTbBlock(tbParams(), bkey, bva, bvb);
                writeTb(nb);
                if (!c.call(0x894fc110, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "tb block: %s\n", c.error().c_str()); return 1; }
                port.setBlock(nb, o1, ol1, o2, ol2);
                ++on;
                if (!tbSame(port, false) && ob++ < 5)
                    std::printf("  tb block %d.%d: fw t %.9g %.9g port %.9g %.9g\n", g, blkN, f32(D + 0x3c), f32(D + 0x48), port.target()[0], port.target()[3]);
            }
            float lr[64];
            const int ar = (int)(rnd() % 4);
            const float amp = ar == 0 ? 30000.0f : ar == 1 ? 1.0e9f : ar == 2 ? 4.0e8f : 1.0f;
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(0x89501950, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "tb filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++fn;
            bool ok = tbSame(port, true);
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            if (!ok && fb++ < 6)
                std::printf("  tb filter %d block %d: fw out %.9g %.9g c %.9g s %.9g h %.9g, port %.9g %.9g %.9g %.9g %.9g\n", g, blkN, f32(BUF), f32(BUF + 248),
                            f32(D + 0x1c), f32(D + 0x2c), f32(D + 0x68), lr[0], lr[62], port.coef()[0], port.lpState()[0], port.hpState()[0]);
        }
    }
    std::printf("tb note-on, block and update: %ld cases, %ld different\n", on, ob);
    std::printf("tb filter: %ld blocks of 32 frames, %ld different\n", fn, fb);

    // The Comb Filter (block code 9): the converter (0x894dd4f0) …
    auto combParams = [&]() {
        XtCombParams p;
        p.octave = (int)(rnd() % 5); p.detune = (int)(rnd() % 201) - 100; p.detuneLfo = (int)(rnd() % 127) - 63;
        p.detuneEnv = (int)(rnd() % 127) - 63; p.tone = (int)(rnd() % 128); p.toneLfo = (int)(rnd() % 127) - 63; p.toneEnv = (int)(rnd() % 127) - 63;
        return p;
    };
    long kb = 0, kn = 0;
    for (int k = 0; k < 200000; ++k) {
        const XtCombParams p = combParams();
        const int cosm = (int)(rnd() & 1);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)(rnd() % 128));
        const int vals[7] = { p.octave, p.detune, p.detuneLfo, p.detuneEnv, p.tone, p.toneLfo, p.toneEnv };
        for (int i = 0; i < 7; ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)vals[i]);
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(0x894dd4f0, { IN, OUT }, sp)) { std::fprintf(stderr, "comb converter: %s\n", c.error().c_str()); return 1; }
        const XtCombBlock b = xtCombBlock(p, va, vb);
        ++kn;
        const bool ok = c.rd32(OUT) == 9 && (int)(int8_t)c.rd8(OUT + 4) == b.octave && same(f32(OUT + 8), b.tone) && same(f32(OUT + 0xc), b.toneEnv)
                     && same(f32(OUT + 0x14), b.toneLfo) && same(f32(OUT + 0x1c), b.detune) && same(f32(OUT + 0x20), b.detuneEnv)
                     && same(f32(OUT + 0x28), b.detuneLfo) && c.rd32(OUT + 0x10) == (cosm ? 11u : 9u) && c.rd32(OUT + 0x24) == (cosm ? 12u : 10u)
                     && c.rd32(OUT + 0x18) == (cosm ? 4u : 3u) && c.rd32(OUT + 0x2c) == (cosm ? 4u : 3u);
        if (!ok && kb++ < 5)
            std::printf("  comb converter: fw tone %.9g %.9g %.9g det %.9g %.9g %.9g, port %.9g %.9g %.9g %.9g %.9g %.9g\n", f32(OUT + 8), f32(OUT + 0xc),
                        f32(OUT + 0x14), f32(OUT + 0x1c), f32(OUT + 0x20), f32(OUT + 0x28), b.tone, b.toneEnv, b.toneLfo, b.detune, b.detuneEnv, b.detuneLfo);
    }
    std::printf("comb converter: %ld cases, %ld different\n", kn, kb);

    // … the note-on itself (0x894edf70; the buffer pool's slot finder
    // 0x894ec2b0 hooked to hand out four buffers, which the allocator clears),
    // the update with a moving pitch (0x894ee6a0), new blocks while the note
    // sounds (0x894ed390) and the filter (0x894fa8d0), its buffer compared
    const uint32_t CB[4] = { ram + 0x20000, ram + 0x24000, ram + 0x28000, ram + 0x2c000 };
    c.hook(0x894ec2b0, [&](Sh4& m) { for (uint32_t i = 0; i < 4; ++i) m.wr32(m.r[5] + 4 * i, CB[i]); m.ret(0); });
    auto writeComb = [&](const XtCombBlock& b) {
        c.wr32(BLK, 9); c.wr32(BLK + 4, 0); c.wr8(BLK + 4, (uint8_t)b.octave); wf(BLK + 8, b.tone); wf(BLK + 0xc, b.toneEnv); c.wr32(BLK + 0x10, 9);
        wf(BLK + 0x14, b.toneLfo); c.wr32(BLK + 0x18, 3); wf(BLK + 0x1c, b.detune); wf(BLK + 0x20, b.detuneEnv); c.wr32(BLK + 0x24, 10);
        wf(BLK + 0x28, b.detuneLfo); c.wr32(BLK + 0x2c, 3);
    };
    auto combSame = [&](const XtCosmComb& port) {
        return same(f32(D + 0xbc), port.delay()) && same(f32(D + 0xc0), port.tone()) && same(f32(OBJ + 0x100), port.toneTarget())
            && same(f32(OBJ + 0x130), port.newDelay()) && same(f32(OBJ + 0x134), port.fraction()) && same(f32(OBJ + 0x138), port.newFraction())
            && same(f32(OBJ + 0x13c), port.progress()) && same(f32(D + 0x34), port.dc()) && (int)c.rd32(D + 4) == port.writeIndex();
    };
    auto pitchOf = [&]() { const int r = (int)(rnd() % 4); return r == 0 ? 1.0f : r == 1 ? frnd(0.5f, 2.0f) : r == 2 ? frnd(0.03f, 0.2f) : frnd(2.0f, 32.0f); };
    long mb = 0, mn = 0, qb = 0, qn = 0;
    for (int g = 0; g < nCases; ++g) {
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200); c.write(BLK, z.data(), 0x80);
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        for (uint32_t bI = 0; bI < 4; ++bI)
            for (uint32_t i = 0; i < 3267; ++i) wf(CB[bI] + 4 * i, frnd(-1, 1));    // the last note's junk
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);   // slot 1 Detune's, slot 2 Tone's
        float pitch = pitchOf();
        const float root = xtCosmRootHz((int)(rnd() % 128), (rnd() % 4) == 0);
        wf(OBJ, pitch); wf(OBJ + 4, root);
        wf(OBJ + 0x13c, (rnd() & 1) ? 0.0f : 1.5f);                              // the last note's crossfade
        const float bva = frnd(0, 1), bvb = frnd(0, 1);
        const XtCombBlock b = xtCombBlock(combParams(), bva, bvb);
        writeComb(b);
        const float e1S = frnd(-1, 1), e2S = frnd(-1, 1), l1 = frnd(-1, 1), l2 = frnd(-1, 1);
        float o1 = frnd(-1, 1), o2 = frnd(-1, 1), ol1 = l1, ol2 = l2;
        const float r1 = frnd(0.2f, 1.0f) / 32.0f, r2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
        wf(EQ + 0x58, e1S); wf(EC + 0x58, e2S); wf(LQ + 0x38, l1); wf(LC + 0x38, l2); wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2);
        wf(EQ + 0x54, r1); wf(EC + 0x54, r2);
        if (!c.call(0x894edf70, { OBJ, BLK, 0 }, sp)) { std::fprintf(stderr, "comb note-on: %s\n", c.error().c_str()); return 1; }
        XtCosmComb port;
        port.noteOn(b, pitch, root, e1S, o1, r1, l1, e2S, o2, r2, l2);
        ++mn;
        bool ok0 = combSame(port);
        for (uint32_t i = 0; i < 3267; ++i) ok0 = ok0 && f32(CB[0] + 4 * i) == 0.0f;
        if (!ok0 && mb++ < 5)
            std::printf("  comb note-on %d: fw delay %.9g tone %.9g t %.9g new %.9g frac %.9g p %.9g w %d, port %.9g %.9g %.9g %.9g %.9g %.9g %d\n", g,
                        f32(D + 0xbc), f32(D + 0xc0), f32(OBJ + 0x100), f32(OBJ + 0x130), f32(OBJ + 0x134), f32(OBJ + 0x13c), (int)c.rd32(D + 4),
                        port.delay(), port.tone(), port.toneTarget(), port.newDelay(), port.fraction(), port.progress(), port.writeIndex());
        for (int blkN = 0; blkN < 40; ++blkN) {
            if (blkN % 14 == 13 || blkN % 14 == 3) {                  // updates, the pitch moving
                if (rnd() % 3) pitch = (rnd() & 1) ? pitchOf() : mul(pitch, frnd(0.98f, 1.02f));
                o1 = frnd(-1, 1); o2 = frnd(-1, 1); ol1 = frnd(-1, 1); ol2 = frnd(-1, 1);
                const float nr1 = frnd(0.2f, 1.0f) / 32.0f, nr2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
                wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2); wf(LQ + 0x38, ol1); wf(LC + 0x38, ol2); wf(EQ + 0x54, nr1); wf(EC + 0x54, nr2);
                uint32_t pb; std::memcpy(&pb, &pitch, 4);
                c.fb[0][4] = pb;                                       // the voice's pitch ratio (+0x144) in fr4
                if (!c.call(0x894ee6a0, { OBJ }, sp)) { std::fprintf(stderr, "comb update: %s\n", c.error().c_str()); return 1; }
                port.update(pitch, o1, nr1, ol1, o2, nr2, ol2);
                ++mn;
                if (!combSame(port) && mb++ < 5)
                    std::printf("  comb update %d.%d: fw t %.9g new %.9g nfrac %.9g p %.9g, port %.9g %.9g %.9g %.9g\n", g, blkN, f32(OBJ + 0x100),
                                f32(OBJ + 0x130), f32(OBJ + 0x138), f32(OBJ + 0x13c), port.toneTarget(), port.newDelay(), port.newFraction(), port.progress());
            } else if (blkN % 14 == 8 && (rnd() & 1)) {               // a parameter change while the note sounds
                const XtCombBlock nb = xtCombBlock(combParams(), bva, bvb);
                writeComb(nb);
                if (!c.call(0x894ed390, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "comb block: %s\n", c.error().c_str()); return 1; }
                port.setBlock(nb, o1, ol1, o2, ol2);
                ++mn;
                if (!combSame(port) && mb++ < 5)
                    std::printf("  comb block %d.%d: fw t %.9g new %.9g p %.9g, port %.9g %.9g %.9g\n", g, blkN, f32(OBJ + 0x100), f32(OBJ + 0x130),
                                f32(OBJ + 0x13c), port.toneTarget(), port.newDelay(), port.progress());
            }
            float lr[64];
            const int ar = (int)(rnd() % 4);
            const float amp = ar == 0 ? 30000.0f : ar == 1 ? 1.0e9f : ar == 2 ? 4.0e8f : 1.0f;
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(0x894fa8d0, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "comb filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++qn;
            bool ok = combSame(port);
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            for (uint32_t i = 0; i < 3267; ++i) ok = ok && same(f32(CB[0] + 4 * i), port.buffer()[i]);
            if (!ok && qb++ < 6)
                std::printf("  comb filter %d block %d: fw out %.9g %.9g delay %.9g tone %.9g p %.9g dc %.9g, port %.9g %.9g %.9g %.9g %.9g %.9g\n", g, blkN,
                            f32(BUF), f32(BUF + 248), f32(D + 0xbc), f32(D + 0xc0), f32(OBJ + 0x13c), f32(D + 0x34), lr[0], lr[62], port.delay(),
                            port.tone(), port.progress(), port.dc());
        }
    }
    std::printf("comb note-on, block and update: %ld cases, %ld different\n", mn, mb);
    std::printf("comb filter: %ld blocks of 32 frames, %ld different\n", qn, qb);

    // The Side Band Filters (block codes 7 and 8): the keyfollow tables by
    // the firmware's powf (0x8952ff9c), the converters (0x894dd0a0,
    // 0x894dd2c0), then as the Comb: the note-on, updates, new blocks
    // (0x894ed1a0, 0x894ed2a0) and the filters (0x89500640, 0x89500d40), both
    // buffers compared; the words before the second buffer stay 0
    {
        std::vector<float> sq(41 * 128), ks(41 * 128);
        for (int i = 0; i < 41; ++i)
            for (int key = 0; key < 128; ++key) {
                const float x = div(div(mul(flt(key - 60), flt((i - 20) * 10)), 100.0f), 12.0f), e = div(flt((i - 20) * 10 * (key - 60)), 1200.0f);
                const float two = 2.0f;
                uint32_t u;
                std::memcpy(&u, &x, 4); c.fb[0][4] = u; std::memcpy(&u, &two, 4); c.fb[0][5] = u;
                if (!c.call(0x8952ff9c, {}, sp)) { std::fprintf(stderr, "powf: %s\n", c.error().c_str()); return 1; }
                std::memcpy(&sq[(size_t)(i * 128 + key)], &c.fb[0][0], 4);
                std::memcpy(&u, &two, 4); c.fb[0][4] = u; std::memcpy(&u, &e, 4); c.fb[0][5] = u;
                if (!c.call(0x8952ff9c, {}, sp)) { std::fprintf(stderr, "powf: %s\n", c.error().c_str()); return 1; }
                std::memcpy(&ks[(size_t)(i * 128 + key)], &c.fb[0][0], 4);
            }
        xtCosmSetKeySquares(sq.data());
        xtOscSetTimeKeyScales(ks.data());
    }
    auto sbfParams = [&](bool second) {
        XtSbfParams p;
        p.second = second;
        p.hpf = (int)(rnd() % 128); p.q = (int)(rnd() % 128); p.lpf = (int)(rnd() % 128); p.lpfKf = (int)(rnd() % 41) - 20;
        p.subDetune = (int)(rnd() % 201) - 100; p.octave = (int)(rnd() % 3); p.detune = (int)(rnd() % 201) - 100;
        p.detuneLfo = (int)(rnd() % 127) - 63; p.detuneOwn = (int)(rnd() % 127) - 63; p.detuneEnv = (int)(rnd() % 127) - 63;
        p.width = (int)(rnd() % 128); p.widthKf = (int)(rnd() % 41) - 20; p.widthLfo = (int)(rnd() % 127) - 63;
        p.widthOwn = (int)(rnd() % 127) - 63; p.widthEnv = (int)(rnd() % 127) - 63;
        if (rnd() % 4 == 0) p.width = (rnd() & 1) ? 0 : 127;
        return p;
    };
    // the block as each converter lays it out
    auto writeSbf = [&](const XtSbfBlock& b, int cosm) {
        std::vector<uint8_t> z(0x48, 0);
        c.write(BLK, z.data(), 0x48);
        const uint32_t o = b.second ? 4 : 0;
        c.wr32(BLK, b.second ? 8 : 7); c.wr8(BLK + 4, (uint8_t)b.octave);
        if (b.second) wf(BLK + 8, b.q);
        wf(BLK + o + 8, b.detune); wf(BLK + o + 0xc, b.detuneEnv); c.wr32(BLK + o + 0x10, cosm ? 12 : 10); wf(BLK + o + 0x14, b.detuneLfo);
        c.wr32(BLK + o + 0x18, cosm ? 4 : 3); wf(BLK + o + 0x1c, b.detuneOwn); wf(BLK + o + 0x20, b.width); wf(BLK + o + 0x24, b.widthEnv);
        c.wr32(BLK + o + 0x28, cosm ? 11 : 9); wf(BLK + o + 0x2c, b.widthLfo); c.wr32(BLK + o + 0x30, cosm ? 4 : 3); wf(BLK + o + 0x34, b.widthOwn);
        if (b.second) { wf(BLK + 0x3c, b.lpf); wf(BLK + 0x40, 1.0f); }
        else { wf(BLK + 0x38, b.hpf); wf(BLK + 0x3c, b.lpf); wf(BLK + 0x40, b.subDetune); wf(BLK + 0x44, 1.0f); }
    };
    long sb[2] = {}, sn[2] = {};
    for (int k = 0; k < 200000; ++k) {
        const bool second = (k & 1) != 0;
        const XtSbfParams p = sbfParams(second);
        const int cosm = (int)(rnd() & 1), key = (int)(rnd() % 128);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)key);
        const int v1[14] = { p.hpf, p.lpf, p.lpfKf, p.subDetune, p.octave, p.detune, p.detuneLfo, p.detuneOwn, p.detuneEnv,
                             p.width, p.widthKf, p.widthLfo, p.widthOwn, p.widthEnv };
        const int v2[13] = { p.q, p.lpf, p.lpfKf, p.octave, p.detune, p.detuneLfo, p.detuneOwn, p.detuneEnv,
                             p.width, p.widthKf, p.widthLfo, p.widthOwn, p.widthEnv };
        for (int i = 0; i < (second ? 13 : 14); ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)(second ? v2[i] : v1[i]));
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(second ? 0x894dd2c0 : 0x894dd0a0, { IN, OUT }, sp)) { std::fprintf(stderr, "sbf converter: %s\n", c.error().c_str()); return 1; }
        const XtSbfBlock b = xtSbfBlock(p, key, va, vb);
        writeSbf(b, cosm);
        ++sn[second];
        std::vector<uint8_t> fw(0x48), pt(0x48);
        c.read(OUT, fw.data(), 0x48); c.read(BLK, pt.data(), 0x48);
        if (fw != pt && sb[second]++ < 5) {
            std::printf("  sbf%d converter key %d:", second ? 2 : 1, key);
            for (uint32_t i = 0; i < 0x48; i += 4)
                if (c.rd32(OUT + i) != c.rd32(BLK + i)) std::printf(" +%02x fw %08x port %08x", i, c.rd32(OUT + i), c.rd32(BLK + i));
            std::printf("\n");
        }
    }
    std::printf("sbf1 converter: %ld cases, %ld different\n", sn[0], sb[0]);
    std::printf("sbf2 converter: %ld cases, %ld different\n", sn[1], sb[1]);

    auto sbfSame = [&](const XtCosmSbf& port, bool second) {
        bool ok = same(f32(D + 0xbc), port.delay()) && same(f32(D + 0xd8), port.newDelay()) && same(f32(D + 0xc0), port.width())
               && same(f32(D + 0x8c), port.gain()) && same(f32(D + 0xe4), port.progress()) && same(f32(D + 0xa4), port.detuneTarget())
               && same(f32(D + 0xa8), port.widthTarget()) && same(f32(D + (second ? 0x98 : 0xa0)), port.level())
               && same(f32(D + 0x90), port.slews()[0]) && same(f32(D + 0x94), port.slews()[1]) && same(f32(D + 0xcc), port.slews()[3])
               && same(f32(D + 0xd0), port.slews()[4]) && same(f32(D + 0xdc), port.fractions()[0]) && same(f32(D + 0x34), port.dc())
               && (int)c.rd32(D + 4) == port.writeIndex();
        if (!second) ok = ok && same(f32(D + 0x98), port.slews()[2]) && same(f32(D + 0xe0), port.fractions()[1]);
        for (uint32_t i = 0; i < (second ? 4u : 6u); ++i) ok = ok && same(f32(D + 0x1c + 4 * i), port.state()[i]);
        return ok;
    };
    long nb[2] = {}, nn[2] = {}, pb[2] = {}, pn[2] = {};
    for (int g = 0; g < nCases; ++g) {
        const bool second = (g & 1) != 0;
        const int cosm = (int)(rnd() & 1);
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200);
        for (uint32_t o = 0x74; o < 0x150; o += 4) wf(OBJ + o, frnd(-1, 1));                        // the last note's state
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        for (uint32_t bI = 0; bI < 4; ++bI)
            for (uint32_t i = 0; i < 3267; ++i) wf(CB[bI] + 4 * i, frnd(-1, 1));                    // the last note's junk
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);   // slot 1 Detune's, slot 2 Width's
        float pitch = pitchOf();
        const float root = xtCosmRootHz((int)(rnd() % 128), (rnd() % 4) == 0);
        wf(OBJ, pitch); wf(OBJ + 4, root);
        const int key = (int)(rnd() % 128);
        const float bva = frnd(0, 1), bvb = frnd(0, 1);
        const XtSbfBlock b = xtSbfBlock(sbfParams(second), key, bva, bvb);
        writeSbf(b, cosm);
        const float e1S = frnd(-1, 1), e2S = frnd(-1, 1), l1 = frnd(-1, 1), l2 = frnd(-1, 1);
        float o1 = frnd(-1, 1), o2 = frnd(-1, 1), ol1 = l1, ol2 = l2;
        const float r1 = frnd(0.2f, 1.0f) / 32.0f, r2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
        wf(EQ + 0x58, e1S); wf(EC + 0x58, e2S); wf(LQ + 0x38, l1); wf(LC + 0x38, l2); wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2);
        wf(EQ + 0x54, r1); wf(EC + 0x54, r2);
        if (!c.call(0x894edf70, { OBJ, BLK, 0 }, sp)) { std::fprintf(stderr, "sbf note-on: %s\n", c.error().c_str()); return 1; }
        XtCosmSbf port;
        port.noteOn(b, pitch, root, e1S, o1, r1, l1, e2S, o2, r2, l2);
        ++nn[second];
        bool ok0 = sbfSame(port, second);
        for (uint32_t i = 0; i < 3267; ++i) ok0 = ok0 && f32(CB[0] + 4 * i) == 0.0f && f32(CB[1] + 4 * i) == 0.0f;
        if (!ok0 && nb[second]++ < 5)
            std::printf("  sbf%d note-on %d: fw delay %.9g w %.9g gain %.9g frac %.9g %.9g wt %.9g, port %.9g %.9g %.9g %.9g %.9g %.9g\n", second ? 2 : 1, g,
                        f32(D + 0xbc), f32(D + 0xc0), f32(D + 0x8c), f32(D + 0xdc), f32(D + 0xe0), f32(D + 0xa8), port.delay(), port.width(), port.gain(),
                        port.fractions()[0], port.fractions()[1], port.widthTarget());
        for (int blkN = 0; blkN < 40; ++blkN) {
            if (blkN % 14 == 13 || blkN % 14 == 3) {                  // updates, the pitch moving
                if (rnd() % 3) pitch = (rnd() & 1) ? pitchOf() : mul(pitch, frnd(0.98f, 1.02f));
                o1 = frnd(-1, 1); o2 = frnd(-1, 1); ol1 = frnd(-1, 1); ol2 = frnd(-1, 1);
                const float nr1 = frnd(0.2f, 1.0f) / 32.0f, nr2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
                wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2); wf(LQ + 0x38, ol1); wf(LC + 0x38, ol2); wf(EQ + 0x54, nr1); wf(EC + 0x54, nr2);
                uint32_t pbits; std::memcpy(&pbits, &pitch, 4);
                c.fb[0][4] = pbits;
                if (!c.call(0x894ee6a0, { OBJ }, sp)) { std::fprintf(stderr, "sbf update: %s\n", c.error().c_str()); return 1; }
                port.update(pitch, o1, nr1, ol1, o2, nr2, ol2);
                ++nn[second];
                if (!sbfSame(port, second) && nb[second]++ < 5)
                    std::printf("  sbf%d update %d.%d: fw dt %.9g wt %.9g, port %.9g %.9g\n", second ? 2 : 1, g, blkN, f32(D + 0xa4), f32(D + 0xa8),
                                port.detuneTarget(), port.widthTarget());
            } else if (blkN % 14 == 8 && (rnd() & 1)) {               // a parameter change while the note sounds
                const XtSbfBlock nb2 = xtSbfBlock(sbfParams(second), key, bva, bvb);
                writeSbf(nb2, cosm);
                if (!c.call(second ? 0x894ed2a0 : 0x894ed1a0, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "sbf block: %s\n", c.error().c_str()); return 1; }
                port.setBlock(nb2, o1, ol1, o2, ol2);
                ++nn[second];
                if (!sbfSame(port, second) && nb[second]++ < 5)
                    std::printf("  sbf%d block %d.%d: fw dt %.9g wt %.9g, port %.9g %.9g\n", second ? 2 : 1, g, blkN, f32(D + 0xa4), f32(D + 0xa8),
                                port.detuneTarget(), port.widthTarget());
            }
            float lr[64];
            const int ar = (int)(rnd() % 4);
            const float amp = ar == 0 ? 30000.0f : ar == 1 ? 1.0e9f : ar == 2 ? 4.0e8f : 1.0f;
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(second ? 0x89500d40 : 0x89500640, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "sbf filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++pn[second];
            bool ok = sbfSame(port, second);
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            for (uint32_t i = 0; i < 3267; ++i) ok = ok && same(f32(CB[0] + 4 * i), port.buffer(0)[i]) && same(f32(CB[1] + 4 * i), port.buffer(1)[i]);
            if (!ok && pb[second]++ < 6)
                std::printf("  sbf%d filter %d block %d: fw out %.9g %.9g delay %.9g new %.9g w %.9g g %.9g p %.9g lev %.9g, port %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",
                            second ? 2 : 1, g, blkN, f32(BUF), f32(BUF + 248), f32(D + 0xbc), f32(D + 0xd8), f32(D + 0xc0), f32(D + 0x8c), f32(D + 0xe4),
                            f32(D + (second ? 0x98 : 0xa0)), lr[0], lr[62], port.delay(), port.newDelay(), port.width(), port.gain(), port.progress(), port.level());
        }
    }
    for (int t = 0; t < 2; ++t) {
        std::printf("sbf%d note-on, block and update: %ld cases, %ld different\n", t + 1, nn[t], nb[t]);
        std::printf("sbf%d filter: %ld blocks of 32 frames, %ld different\n", t + 1, pn[t], pb[t]);
    }

    // The Resonator (block code 6): the firmware's expf (0x8952f6f4) on its
    // range here (±1.6) and on random bit patterns, the converter
    // (0x894dcf00), then the note-on, updates, new blocks (0x894ed0a0) and
    // the filter (0x894ffa10)
    long eb = 0, en = 0;
    for (int k = 0; k < 400000; ++k) {
        float x;
        if (k & 1) { const uint32_t u = (uint32_t)rnd() << 16 | (uint32_t)rnd(); std::memcpy(&x, &u, 4); }
        else x = frnd(-1.6f, 1.6f);
        uint32_t u;
        std::memcpy(&u, &x, 4); c.fb[0][4] = u;
        if (!c.call(0x8952f6f4, {}, sp)) { std::fprintf(stderr, "expf: %s\n", c.error().c_str()); return 1; }
        float fw;
        std::memcpy(&fw, &c.fb[0][0], 4);
        const float pt = xtCosmExpf(x);
        ++en;
        if (!same(fw, pt) && eb++ < 5) std::printf("  expf(%.9g = %08x): fw %.9g port %.9g\n", x, u, fw, pt);
    }
    std::printf("expf: %ld cases, %ld different\n", en, eb);
    auto resParams = [&]() {
        XtResParams p;
        p.type = (int)(rnd() % 3); p.size = (int)(rnd() % 128); p.sizeKf = (int)(rnd() % 41) - 20; p.sizeLfo = (int)(rnd() % 127) - 63;
        p.sizeEnv = (int)(rnd() % 127) - 63; p.balance = (int)(rnd() % 101); p.balanceLfo = (int)(rnd() % 127) - 63;
        p.balanceEnv = (int)(rnd() % 127) - 63;
        return p;
    };
    auto writeRes = [&](const XtResBlock& b, int cosm) {
        std::vector<uint8_t> z(0x30, 0);
        c.write(BLK, z.data(), 0x30);
        c.wr32(BLK, 6); c.wr8(BLK + 4, (uint8_t)b.type); wf(BLK + 8, b.size); wf(BLK + 0xc, b.sizeEnv); c.wr32(BLK + 0x10, cosm ? 11 : 9);
        wf(BLK + 0x14, b.sizeLfo); c.wr32(BLK + 0x18, cosm ? 4 : 3); wf(BLK + 0x1c, b.balance); wf(BLK + 0x20, b.balanceEnv);
        c.wr32(BLK + 0x24, cosm ? 12 : 10); wf(BLK + 0x28, b.balanceLfo); c.wr32(BLK + 0x2c, cosm ? 4 : 3);
    };
    long rb = 0, rn = 0;
    for (int k = 0; k < 200000; ++k) {
        const XtResParams p = resParams();
        const int cosm = (int)(rnd() & 1), key = (int)(rnd() % 128);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)key);
        const int vals[8] = { p.type, p.size, p.sizeKf, p.sizeLfo, p.sizeEnv, p.balance, p.balanceLfo, p.balanceEnv };
        for (int i = 0; i < 8; ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)vals[i]);
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(0x894dcf00, { IN, OUT }, sp)) { std::fprintf(stderr, "res converter: %s\n", c.error().c_str()); return 1; }
        writeRes(xtResBlock(p, key, va, vb), cosm);
        ++rn;
        std::vector<uint8_t> fw(0x30), pt(0x30);
        c.read(OUT, fw.data(), 0x30); c.read(BLK, pt.data(), 0x30);
        if (fw != pt && rb++ < 5) {
            std::printf("  res converter key %d:", key);
            for (uint32_t i = 0; i < 0x30; i += 4)
                if (c.rd32(OUT + i) != c.rd32(BLK + i)) std::printf(" +%02x fw %08x port %08x", i, c.rd32(OUT + i), c.rd32(BLK + i));
            std::printf("\n");
        }
    }
    std::printf("res converter: %ld cases, %ld different\n", rn, rb);
    auto resSame = [&](const XtCosmRes& port) {
        bool ok = same(f32(D + 0xbc), port.size()) && same(f32(D + 0xc0), port.balance()) && same(f32(D + 0xa4), port.sizeTarget())
               && same(f32(D + 0xa8), port.balanceTarget());
        for (uint32_t i = 0; i < 12; ++i) ok = ok && same(f32(D + 0x1c + 4 * i), port.state()[i]);
        return ok;
    };
    long ab = 0, an = 0, gb = 0, gn = 0;
    for (int g = 0; g < nCases; ++g) {
        const int cosm = (int)(rnd() & 1);
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200);
        for (uint32_t o = 0x74; o < 0x150; o += 4) wf(OBJ + o, frnd(-1, 1));                        // the last note's state
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);   // slot 1 Size's, slot 2 Balance's
        const int key = (int)(rnd() % 128);
        const float bva = frnd(0, 1), bvb = frnd(0, 1);
        const XtResBlock b = xtResBlock(resParams(), key, bva, bvb);
        writeRes(b, cosm);
        const float e1S = frnd(-1, 1), e2S = frnd(-1, 1), l1 = frnd(-1, 1), l2 = frnd(-1, 1);
        float o1 = frnd(-1, 1), o2 = frnd(-1, 1), ol1 = l1, ol2 = l2;
        const float r1 = frnd(0.2f, 1.0f) / 32.0f, r2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
        wf(EQ + 0x58, e1S); wf(EC + 0x58, e2S); wf(LQ + 0x38, l1); wf(LC + 0x38, l2); wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2);
        wf(EQ + 0x54, r1); wf(EC + 0x54, r2);
        if (!c.call(0x894edf70, { OBJ, BLK, 0 }, sp)) { std::fprintf(stderr, "res note-on: %s\n", c.error().c_str()); return 1; }
        XtCosmRes port;
        port.noteOn(b, e1S, o1, r1, l1, e2S, o2, r2, l2);
        ++an;
        if (!resSame(port) && ab++ < 5)
            std::printf("  res note-on %d: fw size %.9g bal %.9g t %.9g %.9g, port %.9g %.9g %.9g %.9g\n", g, f32(D + 0xbc), f32(D + 0xc0),
                        f32(D + 0xa4), f32(D + 0xa8), port.size(), port.balance(), port.sizeTarget(), port.balanceTarget());
        for (int blkN = 0; blkN < 40; ++blkN) {
            if (blkN % 14 == 13 || blkN % 14 == 3) {                  // updates
                o1 = frnd(-1, 1); o2 = frnd(-1, 1); ol1 = frnd(-1, 1); ol2 = frnd(-1, 1);
                const float nr1 = frnd(0.2f, 1.0f) / 32.0f, nr2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
                wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2); wf(LQ + 0x38, ol1); wf(LC + 0x38, ol2); wf(EQ + 0x54, nr1); wf(EC + 0x54, nr2);
                c.fb[0][4] = 0x3f800000;
                if (!c.call(0x894ee6a0, { OBJ }, sp)) { std::fprintf(stderr, "res update: %s\n", c.error().c_str()); return 1; }
                port.update(o1, nr1, ol1, o2, nr2, ol2);
                ++an;
                if (!resSame(port) && ab++ < 5)
                    std::printf("  res update %d.%d: fw t %.9g %.9g, port %.9g %.9g\n", g, blkN, f32(D + 0xa4), f32(D + 0xa8), port.sizeTarget(),
                                port.balanceTarget());
            } else if (blkN % 14 == 8 && (rnd() & 1)) {               // a parameter change while the note sounds
                const XtResBlock nb2 = xtResBlock(resParams(), key, bva, bvb);
                writeRes(nb2, cosm);
                if (!c.call(0x894ed0a0, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "res block: %s\n", c.error().c_str()); return 1; }
                port.setBlock(nb2, o1, ol1, o2, ol2);
                ++an;
                if (!resSame(port) && ab++ < 5)
                    std::printf("  res block %d.%d: fw t %.9g %.9g, port %.9g %.9g\n", g, blkN, f32(D + 0xa4), f32(D + 0xa8), port.sizeTarget(),
                                port.balanceTarget());
            }
            float lr[64];
            const int ar = (int)(rnd() % 4);
            const float amp = ar == 0 ? 30000.0f : ar == 1 ? 1.0e9f : ar == 2 ? 4.0e8f : 1.0f;
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(0x894ffa10, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "res filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++gn;
            bool ok = resSame(port);
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            if (!ok && gb++ < 6)
                std::printf("  res filter %d block %d: fw out %.9g %.9g size %.9g bal %.9g a0 %.9g, port %.9g %.9g %.9g %.9g %.9g\n", g, blkN,
                            f32(BUF), f32(BUF + 248), f32(D + 0xbc), f32(D + 0xc0), f32(D + 0x1c), lr[0], lr[62], port.size(), port.balance(),
                            port.state()[0]);
        }
    }
    std::printf("res note-on, block and update: %ld cases, %ld different\n", an, ab);
    std::printf("res filter: %ld blocks of 32 frames, %ld different\n", gn, gb);

    // The Frequency Shifter (block code 18): the converter (0x894ddde0), then
    // the note-on, updates, new blocks (0x894edc80) and the filter
    // (0x894fda60), its ring buffer compared
    auto fsParams = [&]() {
        XtFsParams p;
        p.depth = (int)(rnd() % 128); p.depthKf = (int)(rnd() % 41) - 20; p.depthLfo = (int)(rnd() % 127) - 63;
        p.depthEnv = (int)(rnd() % 127) - 63; p.balance = (int)(rnd() % 101); p.balanceLfo = (int)(rnd() % 127) - 63;
        p.balanceEnv = (int)(rnd() % 127) - 63;
        return p;
    };
    auto writeFs = [&](const XtFsBlock& b, int cosm) {
        std::vector<uint8_t> z(0x2c, 0);
        c.write(BLK, z.data(), 0x2c);
        c.wr32(BLK, 18); wf(BLK + 4, b.depth); wf(BLK + 8, b.depthEnv); c.wr32(BLK + 0xc, cosm ? 11 : 9); wf(BLK + 0x10, b.depthLfo);
        c.wr32(BLK + 0x14, cosm ? 4 : 3); wf(BLK + 0x18, b.balance); wf(BLK + 0x1c, b.balanceEnv); c.wr32(BLK + 0x20, cosm ? 12 : 10);
        wf(BLK + 0x24, b.balanceLfo); c.wr32(BLK + 0x28, cosm ? 4 : 3);
    };
    long hb = 0, hn = 0;
    for (int k = 0; k < 200000; ++k) {
        const XtFsParams p = fsParams();
        const int cosm = (int)(rnd() & 1), key = (int)(rnd() % 128);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)key);
        const int vals[7] = { p.depth, p.depthKf, p.depthLfo, p.depthEnv, p.balance, p.balanceLfo, p.balanceEnv };
        for (int i = 0; i < 7; ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)vals[i]);
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(0x894ddde0, { IN, OUT }, sp)) { std::fprintf(stderr, "fs converter: %s\n", c.error().c_str()); return 1; }
        writeFs(xtFsBlock(p, key, va, vb), cosm);
        ++hn;
        std::vector<uint8_t> fw(0x2c), pt(0x2c);
        c.read(OUT, fw.data(), 0x2c); c.read(BLK, pt.data(), 0x2c);
        if (fw != pt && hb++ < 5) {
            std::printf("  fs converter key %d:", key);
            for (uint32_t i = 0; i < 0x2c; i += 4)
                if (c.rd32(OUT + i) != c.rd32(BLK + i)) std::printf(" +%02x fw %08x port %08x", i, c.rd32(OUT + i), c.rd32(BLK + i));
            std::printf("\n");
        }
    }
    std::printf("fs converter: %ld cases, %ld different\n", hn, hb);
    auto fsSame = [&](const XtCosmFs& port) {
        return same(f32(D + 0xbc), port.depth()) && same(f32(D + 0xc0), port.balance()) && same(f32(D + 0xa4), port.depthTarget())
            && same(f32(D + 0xa8), port.balanceTarget()) && same(f32(D + 0x94), port.phase()) && (int)c.rd32(D + 4) == port.writeIndex();
    };
    long vb2 = 0, vn = 0, wb = 0, wn = 0;
    for (int g = 0; g < nCases; ++g) {
        const int cosm = (int)(rnd() & 1);
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200);
        for (uint32_t o = 0x74; o < 0x150; o += 4) wf(OBJ + o, frnd(-1, 1));                        // the last note's state
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        for (uint32_t bI = 0; bI < 4; ++bI)
            for (uint32_t i = 0; i < 3267; ++i) wf(CB[bI] + 4 * i, frnd(-1, 1));                    // the last note's junk
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);   // slot 1 Depth's, slot 2 Balance's
        const int key = (int)(rnd() % 128);
        const float bva = frnd(0, 1), bvb = frnd(0, 1);
        const XtFsBlock b = xtFsBlock(fsParams(), key, bva, bvb);
        writeFs(b, cosm);
        const float e1S = frnd(-1, 1), e2S = frnd(-1, 1), l1 = frnd(-1, 1), l2 = frnd(-1, 1);
        float o1 = frnd(-1, 1), o2 = frnd(-1, 1), ol1 = l1, ol2 = l2;
        const float r1 = frnd(0.2f, 1.0f) / 32.0f, r2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
        wf(EQ + 0x58, e1S); wf(EC + 0x58, e2S); wf(LQ + 0x38, l1); wf(LC + 0x38, l2); wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2);
        wf(EQ + 0x54, r1); wf(EC + 0x54, r2);
        if (!c.call(0x894edf70, { OBJ, BLK, 0 }, sp)) { std::fprintf(stderr, "fs note-on: %s\n", c.error().c_str()); return 1; }
        XtCosmFs port;
        port.noteOn(b, e1S, o1, r1, l1, e2S, o2, r2, l2);
        ++vn;
        bool ok0 = fsSame(port);
        for (uint32_t i = 0; i < 3266; ++i) ok0 = ok0 && f32(CB[0] + 4 * i) == 0.0f;
        if (!ok0 && vb2++ < 5)
            std::printf("  fs note-on %d: fw depth %.9g bal %.9g t %.9g %.9g ph %.9g w %d, port %.9g %.9g %.9g %.9g %.9g %d\n", g, f32(D + 0xbc),
                        f32(D + 0xc0), f32(D + 0xa4), f32(D + 0xa8), f32(D + 0x94), (int)c.rd32(D + 4), port.depth(), port.balance(),
                        port.depthTarget(), port.balanceTarget(), port.phase(), port.writeIndex());
        for (int blkN = 0; blkN < 40; ++blkN) {
            if (blkN % 14 == 13 || blkN % 14 == 3) {                  // updates
                o1 = frnd(-1, 1); o2 = frnd(-1, 1); ol1 = frnd(-1, 1); ol2 = frnd(-1, 1);
                const float nr1 = frnd(0.2f, 1.0f) / 32.0f, nr2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
                wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2); wf(LQ + 0x38, ol1); wf(LC + 0x38, ol2); wf(EQ + 0x54, nr1); wf(EC + 0x54, nr2);
                c.fb[0][4] = 0x3f800000;
                if (!c.call(0x894ee6a0, { OBJ }, sp)) { std::fprintf(stderr, "fs update: %s\n", c.error().c_str()); return 1; }
                port.update(o1, nr1, ol1, o2, nr2, ol2);
                ++vn;
                if (!fsSame(port) && vb2++ < 5)
                    std::printf("  fs update %d.%d: fw t %.9g %.9g, port %.9g %.9g\n", g, blkN, f32(D + 0xa4), f32(D + 0xa8), port.depthTarget(),
                                port.balanceTarget());
            } else if (blkN % 14 == 8 && (rnd() & 1)) {               // a parameter change while the note sounds
                const XtFsBlock nb2 = xtFsBlock(fsParams(), key, bva, bvb);
                writeFs(nb2, cosm);
                if (!c.call(0x894edc80, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "fs block: %s\n", c.error().c_str()); return 1; }
                port.setBlock(nb2, o1, ol1, o2, ol2);
                ++vn;
                if (!fsSame(port) && vb2++ < 5)
                    std::printf("  fs block %d.%d: fw t %.9g %.9g, port %.9g %.9g\n", g, blkN, f32(D + 0xa4), f32(D + 0xa8), port.depthTarget(),
                                port.balanceTarget());
            }
            float lr[64];
            const int ar = (int)(rnd() % 4);
            const float amp = ar == 0 ? 30000.0f : ar == 1 ? 1.0e9f : ar == 2 ? 4.0e8f : 1.0f;
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(0x894fda60, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "fs filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++wn;
            bool ok = fsSame(port);
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            for (uint32_t i = 0; i < 3266; ++i) ok = ok && same(f32(CB[0] + 4 * i), port.buffer()[i]);
            if (!ok && wb++ < 6)
                std::printf("  fs filter %d block %d: fw out %.9g %.9g depth %.9g ph %.9g, port %.9g %.9g %.9g %.9g\n", g, blkN, f32(BUF),
                            f32(BUF + 248), f32(D + 0xbc), f32(D + 0x94), lr[0], lr[62], port.depth(), port.phase());
        }
    }
    std::printf("fs note-on, block and update: %ld cases, %ld different\n", vn, vb2);
    std::printf("fs filter: %ld blocks of 32 frames, %ld different\n", wn, wb);

    // Lo-Fi (block code 19): the converter (0x894ddf80), then the note-on,
    // updates, new blocks (0x894edd60) and the filter (0x894fe9c0)
    auto lofiParams = [&]() {
        XtLofiParams p;
        p.bitDown = (int)(rnd() % 16); p.rateDown = (int)(rnd() % 6); p.filter = (int)(rnd() % 128); p.filterKf = (int)(rnd() % 41) - 20;
        p.filterLfo = (int)(rnd() % 127) - 63; p.filterEnv = (int)(rnd() % 127) - 63; p.balance = (int)(rnd() % 101);
        p.balanceLfo = (int)(rnd() % 127) - 63; p.balanceEnv = (int)(rnd() % 127) - 63;
        return p;
    };
    auto writeLofi = [&](const XtLofiBlock& b, int cosm) {
        std::vector<uint8_t> z(0x30, 0);
        c.write(BLK, z.data(), 0x30);
        c.wr32(BLK, 19); c.wr8(BLK + 4, (uint8_t)b.bitDown); c.wr8(BLK + 5, (uint8_t)b.rateDown); wf(BLK + 8, b.filter);
        wf(BLK + 0xc, b.filterEnv); c.wr32(BLK + 0x10, cosm ? 11 : 9); wf(BLK + 0x14, b.filterLfo); c.wr32(BLK + 0x18, cosm ? 4 : 3);
        wf(BLK + 0x1c, b.balance); wf(BLK + 0x20, b.balanceEnv); c.wr32(BLK + 0x24, cosm ? 12 : 10); wf(BLK + 0x28, b.balanceLfo);
        c.wr32(BLK + 0x2c, cosm ? 4 : 3);
    };
    long ib = 0, in_ = 0;
    for (int k = 0; k < 200000; ++k) {
        const XtLofiParams p = lofiParams();
        const int cosm = (int)(rnd() & 1), key = (int)(rnd() % 128);
        const float va = (float)(rnd() % 32769) / 32768.0f, vb = (float)(rnd() % 32769) / 32768.0f;
        std::vector<uint8_t> z(0x100, 0);
        c.write(IN, z.data(), 0x100); c.write(OUT, z.data(), 0x100);
        c.wr8(IN + 2, (uint8_t)cosm); c.wr8(IN + 3, (uint8_t)key);
        const int vals[9] = { p.bitDown, p.rateDown, p.filter, p.filterKf, p.filterLfo, p.filterEnv, p.balance, p.balanceLfo, p.balanceEnv };
        for (int i = 0; i < 9; ++i) c.wr16(IN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)vals[i]);
        wf(IN + 0x34, va); wf(IN + 0x38, vb);
        if (!c.call(0x894ddf80, { IN, OUT }, sp)) { std::fprintf(stderr, "lofi converter: %s\n", c.error().c_str()); return 1; }
        writeLofi(xtLofiBlock(p, key, va, vb), cosm);
        ++in_;
        std::vector<uint8_t> fw(0x30), pt(0x30);
        c.read(OUT, fw.data(), 0x30); c.read(BLK, pt.data(), 0x30);
        if (fw != pt && ib++ < 5) {
            std::printf("  lofi converter key %d:", key);
            for (uint32_t i = 0; i < 0x30; i += 4)
                if (c.rd32(OUT + i) != c.rd32(BLK + i)) std::printf(" +%02x fw %08x port %08x", i, c.rd32(OUT + i), c.rd32(BLK + i));
            std::printf("\n");
        }
    }
    std::printf("lofi converter: %ld cases, %ld different\n", in_, ib);
    auto lofiSame = [&](const XtCosmLofi& port) {
        bool ok = same(f32(D + 0xbc), port.filter()) && same(f32(D + 0xc0), port.balance()) && same(f32(D + 0xa4), port.filterTarget())
               && same(f32(D + 0xa8), port.balanceTarget());
        for (uint32_t i = 0; i < 6; ++i) ok = ok && same(f32(D + 0x1c + 4 * i), port.state()[i]);
        return ok;
    };
    long jb = 0, jn = 0, lb = 0, ln = 0;
    for (int g = 0; g < nCases; ++g) {
        const int cosm = (int)(rnd() & 1);
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200);
        for (uint32_t o = 0x74; o < 0x150; o += 4) wf(OBJ + o, frnd(-1, 1));                        // the last note's state
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);   // slot 1 Filter's, slot 2 Balance's
        const int key = (int)(rnd() % 128);
        const float bva = frnd(0, 1), bvb = frnd(0, 1);
        const XtLofiBlock b = xtLofiBlock(lofiParams(), key, bva, bvb);
        writeLofi(b, cosm);
        const float e1S = frnd(-1, 1), e2S = frnd(-1, 1), l1 = frnd(-1, 1), l2 = frnd(-1, 1);
        float o1 = frnd(-1, 1), o2 = frnd(-1, 1), ol1 = l1, ol2 = l2;
        const float r1 = frnd(0.2f, 1.0f) / 32.0f, r2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
        wf(EQ + 0x58, e1S); wf(EC + 0x58, e2S); wf(LQ + 0x38, l1); wf(LC + 0x38, l2); wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2);
        wf(EQ + 0x54, r1); wf(EC + 0x54, r2);
        if (!c.call(0x894edf70, { OBJ, BLK, 0 }, sp)) { std::fprintf(stderr, "lofi note-on: %s\n", c.error().c_str()); return 1; }
        XtCosmLofi port;
        port.noteOn(b, e1S, o1, r1, l1, e2S, o2, r2, l2);
        ++jn;
        if (!lofiSame(port) && jb++ < 5)
            std::printf("  lofi note-on %d: fw filter %.9g bal %.9g t %.9g %.9g, port %.9g %.9g %.9g %.9g\n", g, f32(D + 0xbc), f32(D + 0xc0),
                        f32(D + 0xa4), f32(D + 0xa8), port.filter(), port.balance(), port.filterTarget(), port.balanceTarget());
        for (int blkN = 0; blkN < 40; ++blkN) {
            if (blkN % 14 == 13 || blkN % 14 == 3) {                  // updates
                o1 = frnd(-1, 1); o2 = frnd(-1, 1); ol1 = frnd(-1, 1); ol2 = frnd(-1, 1);
                const float nr1 = frnd(0.2f, 1.0f) / 32.0f, nr2 = (rnd() & 1) ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f;
                wf(EQ + 0x4c, o1); wf(EC + 0x4c, o2); wf(LQ + 0x38, ol1); wf(LC + 0x38, ol2); wf(EQ + 0x54, nr1); wf(EC + 0x54, nr2);
                c.fb[0][4] = 0x3f800000;
                if (!c.call(0x894ee6a0, { OBJ }, sp)) { std::fprintf(stderr, "lofi update: %s\n", c.error().c_str()); return 1; }
                port.update(o1, nr1, ol1, o2, nr2, ol2);
                ++jn;
                if (!lofiSame(port) && jb++ < 5)
                    std::printf("  lofi update %d.%d: fw t %.9g %.9g, port %.9g %.9g\n", g, blkN, f32(D + 0xa4), f32(D + 0xa8), port.filterTarget(),
                                port.balanceTarget());
            } else if (blkN % 14 == 8 && (rnd() & 1)) {               // a parameter change while the note sounds
                const XtLofiBlock nb2 = xtLofiBlock(lofiParams(), key, bva, bvb);
                writeLofi(nb2, cosm);
                if (!c.call(0x894edd60, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "lofi block: %s\n", c.error().c_str()); return 1; }
                port.setBlock(nb2, o1, ol1, o2, ol2);
                ++jn;
                if (!lofiSame(port) && jb++ < 5)
                    std::printf("  lofi block %d.%d: fw t %.9g %.9g, port %.9g %.9g\n", g, blkN, f32(D + 0xa4), f32(D + 0xa8), port.filterTarget(),
                                port.balanceTarget());
            }
            float lr[64];
            const int ar = (int)(rnd() % 4);
            const float amp = ar == 0 ? 30000.0f : ar == 1 ? 1.0e9f : ar == 2 ? 4.0e8f : 1.0f;
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(0x894fe9c0, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "lofi filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++ln;
            bool ok = lofiSame(port);
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            if (!ok && lb++ < 6)
                std::printf("  lofi filter %d block %d: fw out %.9g %.9g filter %.9g bal %.9g b2 %.9g, port %.9g %.9g %.9g %.9g %.9g\n", g, blkN,
                            f32(BUF), f32(BUF + 248), f32(D + 0xbc), f32(D + 0xc0), f32(D + 0x28), lr[0], lr[62], port.filter(), port.balance(),
                            port.state()[3]);
        }
    }
    std::printf("lofi note-on, block and update: %ld cases, %ld different\n", jn, jb);
    std::printf("lofi filter: %ld blocks of 32 frames, %ld different\n", ln, lb);
    return cb || ub || db || xb || yb || zb || tb || ob || fb || kb || mb || qb || sb[0] || sb[1] || nb[0] || nb[1] || pb[0] || pb[1]
        || eb || rb || ab || gb || hb || vb2 || wb || ib || jb || lb ? 3 : 0;
}
#endif
