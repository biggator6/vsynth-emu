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
    const int c = clampi((int16_t)p.cutoff, 0, 127);
    float f = div(div(mul(flt((int16_t)(key - 60)), flt((int16_t)(p.keyfollow * 10))), 100.0f), 127.0f);
    f = add(add(f, div(flt(c), 127.0f)), 0.0f);
    b.cutoff = f > 1.0f ? 1.0f : (0.0f > f ? 0.0f : f);
    b.cutoffLfo = sq63(clampi((int16_t)p.cutoffLfo, -63, 63));
    {
        const float v = div(flt((int16_t)p.cutoffEnv), 63.0f);
        b.cutoffEnv = mul(velA, mul(0.0f > v ? -v : v, v));
    }
    const int q = clampi((int16_t)p.q, 0, 127);
    b.q = add(div(mul(flt(q), 0.984375f), 127.0f), -1.0f);
    b.qLfo = sq63(clampi((int16_t)p.qLfo, -63, 63));
    {
        const float v = div(flt((int16_t)p.qEnv), 63.0f);
        b.qEnv = mul(velB, mul(0.0f > v ? -v : v, v));
    }
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
}

void XtCosmTvf::setBlock(const XtTvfBlock& b, float qEnv, float qLfo, float cutEnv, float cutLfo) {
    blk = b;
    tgt1 = xtTvfQ(b.q, b.qEnv, qEnv, b.qLfo, qLfo);
    tgt2 = xtTvfCutoff(b.cutoff, b.cutoffEnv, cutEnv, b.cutoffLfo, cutLfo, tgt1);
}

void XtCosmTvf::update(float qEnv, float qRate, float qLfo, float cutEnv, float cutRate, float cutLfo) {
    rate1 = mul(qRate, 4.0f);
    rate2 = mul(cutRate, 4.0f);
    tgt1 = xtTvfQ(blk.q, blk.qEnv, qEnv, blk.qLfo, qLfo);
    tgt2 = xtTvfCutoff(blk.cutoff, blk.cutoffEnv, cutEnv, blk.cutoffLfo, cutLfo, tgt1);
}

void XtCosmTvf::setState(const float st[4], float c1, float c2, float t1, float t2, float r1, float r2) {
    for (int i = 0; i < 4; ++i) s[i] = st[i];
    cur1 = c1; cur2 = c2; tgt1 = t1; tgt2 = t2; rate1 = r1; rate2 = r2;
}

void XtCosmTvf::process(float* lr) {
    const float d1 = mul(rate1, sub(tgt1, cur1)), d2 = mul(rate2, sub(tgt2, cur2));
    const int m = blk.mode;
    if (m >= 0 && m <= 4) stage24(lr, s, cur1, cur2, d1, d2, m);
    else if (m >= 5 && m <= 9) stage12(lr, s, cur1, cur2, d1, d2, m);
    else if (m == 10 || m == 11) stage6(lr, s, cur2, d2, m == 11);
}

#ifdef XT_COSM_MAIN
// xt_cosm <rom.2.bin> [cases]: the TVF against the firmware in the SH-4 interpreter:
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

    // the object: D = obj + 0x58; the envelopes and LFOs it reads
    const uint32_t OBJ = ram + 0x2000, D = OBJ + 0x58, EQ = ram + 0x3000, EC = ram + 0x3100, LQ = ram + 0x3200, LC = ram + 0x3300,
                   BLK = ram + 0x3400, BUF = ram + 0x4000;
    long ub = 0, un = 0, db = 0, dn = 0;
    const int nCases = argc > 2 ? std::atoi(argv[2]) : 3000;
    for (int g = 0; g < nCases; ++g) {
        std::vector<uint8_t> z(0x200, 0);
        c.write(OBJ, z.data(), 0x200); c.write(BLK, z.data(), 0x80);
        for (uint32_t o : { EQ, EC, LQ, LC }) c.write(o, z.data(), 0x100);
        c.wr8(D, 13);
        c.wr32(OBJ + 0x150, EQ); c.wr32(OBJ + 0x154, EC); c.wr32(OBJ + 0x158, LQ); c.wr32(OBJ + 0x15c, LC);
        XtTvfParams p;
        p.filter = (int)(rnd() % 12); p.q = (int)(rnd() % 128); p.qLfo = (int)(rnd() % 127) - 63; p.qEnv = (int)(rnd() % 127) - 63;
        p.cutoff = (int)(rnd() % 128); p.keyfollow = (int)(rnd() % 41) - 20; p.cutoffLfo = (int)(rnd() % 127) - 63;
        p.cutoffEnv = (int)(rnd() % 127) - 63;
        const XtTvfBlock b = xtTvfBlock(p, (int)(rnd() % 128), frnd(0, 1), frnd(0, 1));
        // the block as the converter leaves it
        c.wr32(BLK, 13); wf(BLK + 4, b.q); wf(BLK + 8, b.qEnv); c.wr32(BLK + 0xc, 10); wf(BLK + 0x10, b.qLfo); c.wr32(BLK + 0x14, 3);
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
        if (!c.call(0x894ed8f0, { OBJ, BLK + 4 }, sp)) { std::fprintf(stderr, "block: %s\n", c.error().c_str()); return 1; }
        wf(OBJ + 0x11c, 4.0f * rq); wf(OBJ + 0x120, 4.0f * rc);
        XtCosmTvf port;
        port.noteOn(b, eqS, eqO, rq, lq, ecS, ecO, rc, lc);
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
            const float amp = (rnd() % 3 == 0) ? 30000.0f : 1.0f;
            for (int i = 0; i < 32; ++i) { lr[2 * i] = frnd(-amp, amp); lr[2 * i + 1] = frnd(-amp, amp); }
            for (int i = 0; i < 64; ++i) wf(BUF + 4 * (uint32_t)i, lr[i]);
            if (!c.call(0x89502890, { D, OBJ + 0x10, OBJ, BUF }, sp)) { std::fprintf(stderr, "filter: %s\n", c.error().c_str()); return 1; }
            port.process(lr);
            ++dn;
            bool ok = same(f32(D + 0xbc), port.c1()) && same(f32(D + 0xc0), port.c2());
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            const int nst = b.mode <= 4 ? 4 : b.mode <= 9 ? 2 : 1;
            for (int i = 0; i < nst; ++i) ok = ok && same(f32(D + 0x1c + 4 * (uint32_t)i), port.state()[i]);
            if (std::getenv("XT_COSM_TRACE") && g == std::atoi(std::getenv("XT_COSM_TRACE")))
                std::printf("    trace %d: fw s %.9g %.9g %.9g %.9g c %.9g %.9g t %.9g %.9g r %.9g %.9g | port s %.9g %.9g %.9g %.9g c %.9g %.9g t %.9g %.9g\n",
                            blkN, f32(D + 0x1c), f32(D + 0x20), f32(D + 0x24), f32(D + 0x28), f32(D + 0xbc), f32(D + 0xc0), f32(OBJ + 0xfc),
                            f32(OBJ + 0x100), f32(OBJ + 0x11c), f32(OBJ + 0x120), port.state()[0], port.state()[1], port.state()[2],
                            port.state()[3], port.c1(), port.c2(), port.target1(), port.target2());
            if (!ok && db++ < 6)
                std::printf("  filter %d block %d mode %d: fw out %.9g c %.9g %.9g, port %.9g %.9g %.9g\n", g, blkN, b.mode, f32(BUF), f32(D + 0xbc),
                            f32(D + 0xc0), lr[0], port.c1(), port.c2());
        }
    }
    std::printf("start, block and update: %ld cases, %ld different\n", un, ub);
    std::printf("filter: %ld blocks of 32 frames, %ld different\n", dn, db);
    return cb || ub || db ? 3 : 0;
}
#endif
