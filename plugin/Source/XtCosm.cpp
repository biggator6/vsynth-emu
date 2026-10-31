// The XT's COSM TVF — see XtCosm.h.  Single precision as the SH-4 computes
// it (XtFloat.h).
#include "XtCosm.h"
#include "XtFloat.h"
#include "XtOsc.h"
#include <cstdio>

namespace {

constexpr uint32_t kRomBase = 0x89300000;
// the cutoff table (129 floats) and the cap by Q (128 floats), 0x894eecb0
constexpr uint32_t kCutTab = 0x897d1264, kCapTab = 0x897d1468;

struct Tables {
    float cut[129], cap[128];
    bool ok = false;
} T;

using namespace xtf;
inline float fmac(float a, float b, float c) { return add(mul(a, b), c); }   // FMAC: the product, then the sum
inline float sq63(int d) {                                                    // (d/63)·|d/63|
    const float v = div(flt(d), 63.0f);
    return mul(0.0f > v ? -v : v, v);
}
inline int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
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

} // namespace

bool xtCosmLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    const bool ok = std::fseek(f, (long)(kCutTab - kRomBase), SEEK_SET) == 0 && std::fread(T.cut, 4, 129, f) == 129
                 && std::fseek(f, (long)(kCapTab - kRomBase), SEEK_SET) == 0 && std::fread(T.cap, 4, 128, f) == 128;
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

#ifdef XT_COSM_MAIN
// xt_cosm <rom.2.bin> [cases]: the TVF and the Dual Filter against the firmware in the SH-4 interpreter:
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
    return cb || ub || db || xb || yb || zb ? 3 : 0;
}
#endif
