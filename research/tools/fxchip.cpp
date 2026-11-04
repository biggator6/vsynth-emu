// fxchip: a throwaway model of the effect chip, to test the decoding in HARDWARE_FIRMWARE.md, "The effects". Not a port: doubles, no word
// widths, no rounding, and it leaves out what is not decoded (FX_UNK=1 counts it). The firmware's own driver, in the interpreter, fills the
// chip's image from the user's rom.2; this runs it, one sample at a time, and reports the gain at each slot.
//   fxchip <rom> <mfx type> <Hz> <slot>   a sine into a frame pair: slot 20 MAIN, 24 the sends, 28 MULTI, 2c DIR
// Env: FX_AMP (0.01); FX_P="i=v,i=v" MFX params; FX_REV=<type>; FX_IMP=<samples>, an impulse at FX_T0 (1000), listing what comes out of slots a0, a2;
//      FX_RMS=1 level of a0, a2 per 100 ms; FX_UNK=1.
// Build: clang++ -std=c++20 -O2 -Iplugin/Source research/tools/fxchip.cpp plugin/Source/Sh4Interp.cpp -o build/tools/fx/fxchip
#include "Sh4Interp.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
uint32_t W[0x802];                                  // program words: A 0x000…0x2ff, B 0x400…0x6ff
double S[256];                                      // slots: the frame, the two programs' mailbox, the way out
double M[1 << 18];                                  // delay memory: one ring, the base falling one per sample
uint32_t memBase = 0; const uint32_t kMask = (1u << 18) - 1;
const double kShift[4] = { 1, 2, 4, 16 };           // code 3 is 16: an MFX's Total Gain +15 dB is 0x2cfc at code 3
std::map<std::string, int> unknown;
double sat(double x) { return x > 1 ? 1 : x < -1 ? -1 : x; }
double limit01(double x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

struct Program {
    double R[256] = {}, Y[16] = {};                 // registers (all rotate: what R[n] holds now is R[n+1] next sample); multiplier latches
    int base = 0;
    double a = 0, b = 0, ha[3] = {}, hb[3] = {};    // the accumulators, and what they held the last three steps
    int pend = 0, pc8 = 0, pDst = 0, pAdd = 0; double pVal = 0;
    int pc = 0, skip = 0, wrAt = 0; uint32_t wrAddr = 0; double mrQ[10] = {}, mr = 0, addrReg = 0;
    double& reg(int r) { static double k; if (r <= 4) { k = r == 4 ? 1.0 : 0.0; return k; } return R[(r + base) & 255]; }   // r4 reads as 1, r0…r3 taken as 0
    void push() { ha[2] = ha[1]; ha[1] = ha[0]; ha[0] = a; hb[2] = hb[1]; hb[1] = hb[0]; hb[0] = b; }
    double la() const { return ha[2]; }             // a store sees the accumulator as of three steps back
    double lb() const { return hb[2]; }
    uint32_t offset(int n) const { auto f = [&](int i) { return (W[i] >> 23) & 31u; }; return f(n + 1) | f(n + 2) << 5 | f(n + 3) << 10 | (f(n + 4) & 7) << 15; }
    void memory(uint32_t x) {                       // five words to an operation; a read's data arrives ten steps on
        mr = mrQ[9]; for (int i = 9; i > 0; i--) mrQ[i] = mrQ[i - 1]; mrQ[0] = 0;
        const int kind = x >> 26 & 3, A = x >> 16 & 63, B = x >> 10 & 63;
        const bool field = skip > 0; if (skip > 0) skip--;
        if (!field && kind == 1) { mrQ[0] = M[(memBase + offset(pc)) & kMask]; skip = 4; }
        else if (!field && kind == 3) { mrQ[0] = M[(memBase + (uint32_t)addrReg + offset(pc) / 2) & kMask]; skip = 4; }
        else if (!field && kind == 2) { wrAddr = offset(pc); wrAt = 4; skip = 4; }
        if (wrAt > 0 && --wrAt == 0) M[(memBase + wrAddr) & kMask] = sat(A == 0x37 && B == 0x2b ? lb() : la());
    }
    void apply(int dst, int add, double v) { if (dst) b = add ? b + v : v; else a = add ? a + v : v; }
    void step(uint32_t x, uint32_t nx) {
        const int X = x >> 22 & 1, f = x >> 18 & 15, r = x >> 10 & 255, sh = x >> 8 & 3, c = x & 255, A = x >> 16 & 63, B = x >> 10 & 63;
        const auto isExt = [](uint32_t w) { return (w >> 16 & 63) == 0x37 && !(w >> 22 & 1) && ((w >> 10 & 63) == 6 || (w >> 10 & 63) == 0x26); };
        if (isExt(x)) {                             // the second word: a 16-bit coefficient's low part, and it finishes the multiply
            if (pend == 2) apply(pDst, pAdd, pVal * kShift[sh]);
            else if (pend) { const int16_t k = (int16_t)(pc8 << 8 | c << 1); apply(pDst, pAdd, pVal * (k / 32768.0) * kShift[sh]); }
            pend = 0; push(); return;
        }
        const bool ext = isExt(nx);
        const auto mac = [&](double v, int dst, int add) {
            if (ext) { pend = 1; pc8 = c; pVal = v; pDst = dst; pAdd = add; }
            else apply(dst, add, v * ((int8_t)c / 128.0) * kShift[sh]);
        };
        const double p = la(), q = lb();
        if (X) switch (f) {
            case 6: reg(r) = sat(p); mac(p, 0, 1); break;   // store A's latch; A += latch × c
            case 7: reg(r) = sat(q); mac(q, 1, 1); break;   // store B's latch; B += latch × c
            case 2: reg(r) = limit01(p); break;             // the ramps' store, read as limited to 0…1
            case 3: reg(r) = limit01(q); break;
            default: if (x & 0x7fffff) unknown["x1 op" + std::to_string(f)]++;
        } else switch (f) {
            case 0: mac(reg(r), 0, 1); break;               // A += R × c
            case 1: mac(reg(r), 0, 0); break;               // A  = R × c
            case 4: mac(reg(r), 1, 1); break;               // B += R × c
            case 5: mac(reg(r), 1, 0); break;               // B  = R × c
            case 2: reg(r) = sat(p); mac(p, 0, 0); break;   // store A's latch; A = latch × c
            case 7: reg(r) = sat(q); mac(q, 1, 0); break;   // store B's latch; B = latch × c
            case 3: reg(r) = sat(q); mac(q, 0, 0); break;   // store B's latch; A = latch × c
            case 6: reg(r) = sat(p); mac(p, 1, 0); break;   // store A's latch; B = latch × c
            case 9: if (sh & 2) b = S[r & 0xfe]; else a = S[r & 0xfe]; break;   // from a slot
            case 12: {                                      // × a latch: c[7:4] which (odd = 1 − the even one), c[2] the accumulator's latch instead of R, c[1] A or B, c[0] add
                const int yi = c >> 4 & 15; const double y = yi & 1 ? 1 - Y[yi - 1] : Y[yi];
                const int dst = c >> 1 & 1, add = c & 1; const double v = (c & 4 ? (dst ? q : p) : reg(r)) * y;
                if (ext) { pend = 2; pVal = v; pDst = dst; pAdd = add; } else apply(dst, add, v * kShift[sh]);
                break; }
            case 13:
                if (A == 0x36) Y[B & 14] = B & 1 ? q : p;
                else if (A == 0x37 && B >= 0xc && B <= 0xf) reg(0xfc + B - 0xc) = mr;   // a read's data to r252…r255
                else if (A == 0x37 && (B == 0x1c || B == 0x3c)) mac(mr, B >> 5, 1);     // … or straight into a multiply
                else if (A == 0x37 && (B == 7 || B == 0x27)) { const double s = (B & 0x20 ? q : p) * 1024.0, fl = std::floor(s); addrReg = fl; Y[12] = s - fl; }   // the address register and its fraction
                else if (A == 0x37 && (B == 0x0b || B == 0x2b)) {}                      // a write's data, taken in memory()
                else unknown["0x37 B" + std::to_string(B)]++;
                break;
            case 14: S[r & 0xfe] = sat(p); break;           // to a slot
            case 15: S[r & 0xfe] = sat(q); break;
            default: unknown["op" + std::to_string(f)]++;
        }
        push();
    }
};
const char* env(const char* n) { return std::getenv(n); }
} // namespace

int main(int argc, char** argv) {
    if (argc < 5) { std::fprintf(stderr, "fxchip <rom> <mfx type> <Hz> <slot hex>\n"); return 2; }
    Sh4 c; if (!c.loadRom(argv[1])) return 1;
    const int mfx = std::atoi(argv[2]); const double freq = std::atof(argv[3]); const int slot = (int)std::strtol(argv[4], nullptr, 16);
    c.mapRam(0x8C000000, 0x02000000); const uint32_t sp = 0x8C0E0000;
    c.hook(0x894af3a0, [](Sh4& m) { m.ret(0); }); c.hook(0x894af3c0, [](Sh4& m) { m.ret(0); }); c.hook(0x894b26a0, [](Sh4& m) { m.ret(0); });
    c.hook(0x894af400, [](Sh4& m) { int i = (int16_t)m.r[4]; if (i >= 0 && i < 0x800) W[i] = m.r[5]; m.ret(0); });
    c.hook(0x894af510, [](Sh4& m) { int i = (int16_t)m.r[4]; m.ret(i >= 0 && i < 0x800 ? W[i] & 0x0fffffff : 0); });
    c.hook(0x894af610, [](Sh4& m) { int i = (int16_t)m.r[4]; uint32_t v = (uint16_t)m.r[5], s = m.r[6] & 3; W[i] = (W[i] & ~0x3ffu) | s << 8 | v >> 8; W[i + 1] = (W[i + 1] & ~0x3ffu) | s << 8 | (v >> 1 & 0x7f); m.ret(0); });
    c.hook(0x894af6a0, [](Sh4& m) { int i = (int16_t)m.r[4]; uint32_t v = m.r[5]; auto put = [&](int k, uint32_t f) { W[i + k] = (W[i + k] & 0x7fffff) | f << 23; }; put(1, v & 31); put(2, v >> 5 & 31); put(3, v >> 10 & 31); put(4, v >> 15 & 7); m.ret(0); });
    // what a patch and the system would have set: Master 127, Output Gain 0, no attenuation, MFX Master 127, sends 0, returns 127, MFX into the mix, EQ off
    c.call(0x894afc50, {}, sp);
    c.call(0x894af1b0, { 127 }, sp); c.call(0x894af2d0, { 0 }, sp); c.call(0x894b0020, { 2 }, sp);
    c.call(0x894ae960, {}, sp); c.call(0x894ae9a0, {}, sp);
    c.call(0x894afdf0, { 0x7fff }, sp); c.call(0x894afef0, { 0 }, sp); c.call(0x894aff00, { 0 }, sp); c.call(0x894aff10, { 0 }, sp);
    c.call(0x894aff20, { 0x7fff }, sp); c.call(0x894aff50, { 0x7fff }, sp); c.call(0x894afd50, { 0 }, sp);
    c.call(0x894d09b0, {}, sp);
    { const int rv = env("FX_REV") ? std::atoi(env("FX_REV")) : 0; c.call(0x894d0a70, { (uint32_t)rv }, sp);
      const int first = (int16_t)c.rd16(0x897a8310 + 4 * rv), last = (int16_t)c.rd16(0x897a8310 + 4 * rv + 2);
      for (int i = 0; i < 24; i++) { const int16_t d = i <= last - first ? (int16_t)c.rd16(0x897a56d8 + 8 * (first + i - 0x55d) + 2) : 0; c.call(0x894d0a80, { (uint32_t)i, (uint32_t)(int32_t)d }, sp); }
      c.call(0x894d0b00, { 0 }, sp); }
    c.call(0x894d3350, {}, sp); c.call(0x894d3420, { 0 }, sp); c.call(0x894d3470, { 0 }, sp);
    { uint32_t obj = 0; c.call(0x894b43a0, { 0x89cef260, (uint32_t)mfx }, sp, &obj);
      const uint32_t vt = c.rd32(obj + 0x54); c.wr32(obj + 0x58, mfx); c.wr32(obj + 0xdc, 12000);
      const auto vcall = [&](uint32_t off) { uint32_t d = c.rd32(vt + off), fn = c.rd32(vt + off + 8), r = 0; c.call(fn, { obj + d }, sp, &r); return r; };
      vcall(0x6c);
      const int first = (int16_t)c.rd16(0x897a8218 + 4 * mfx), last = (int16_t)c.rd16(0x897a8218 + 4 * mfx + 2), np = mfx ? last - first + 1 : 0;
      for (int i = 0; i < np && i < 32; i++) c.wr16(obj + 0x5c + 4 * i, (uint16_t)(int16_t)c.rd16(0x897a56d8 + 8 * (first - 0x55d + i) + 2));
      if (const char* P = env("FX_P")) { std::string q = P; size_t at = 0; for (;;) { const int i = std::atoi(q.c_str() + at); const size_t e = q.find('=', at); c.wr16(obj + 0x5c + 4 * i, (uint16_t)std::atoi(q.c_str() + e + 1)); const size_t cm = q.find(',', e); if (cm == std::string::npos) break; at = cm + 1; } }
      for (int k = 0; k < 100 && !vcall(0x78); k++) {} }
    c.call(0x894aff80, { 1 }, sp); c.call(0x894affa0, { 0 }, sp); c.call(0x894affc0, { 1 }, sp); c.call(0x894affe0, { 1 }, sp);   // the switches on
    c.call(0x894b3020, {}, sp); c.call(0x894b3070, {}, sp);                                                                        // EQ off
    c.call(0x894af610, { 0x27, 0x20, 0 }, sp); c.call(0x894af610, { 0x34, 0x20, 0 }, sp); c.call(0x894aee10, { 1 }, sp);           // the output's mutes open

    static Program A, B;
    const bool imp = env("FX_IMP"); const int t0 = env("FX_T0") ? std::atoi(env("FX_T0")) : 1000, N = imp ? std::atoi(env("FX_IMP")) : 20000;
    const double amp = env("FX_AMP") ? std::atof(env("FX_AMP")) : 0.01; int events = 0; double peakIn = 0; std::map<int, double> peak;
    double e = 0; int k = 0;
    for (int n = 0; n < N; n++) {
        const double x = imp ? (n == t0 ? 0.5 : 0.0) : amp * std::sin(2 * M_PI * freq * n / 44100.0);
        S[slot] = x; S[slot + 2] = x;
        for (int t = 0; t < 0x300; t++) { A.step(W[t], W[t + 1]); B.pc = 0x400 + t; B.memory(W[0x400 + t]); B.step(W[0x400 + t], W[0x400 + t + 1]); }
        A.base = (A.base - 1) & 255; B.base = (B.base - 1) & 255; memBase = (memBase - 1) & kMask;
        if (env("FX_RMS")) { e += S[0xa0] * S[0xa0] + S[0xa2] * S[0xa2]; if (++k == 4410) { std::printf("  t %6.2f s  rms %8.2f dB\n", n / 44100.0, 10 * std::log10(e / (2 * 4410) + 1e-20)); e = 0; k = 0; } }
        if (imp) for (const int s : { 0xa0, 0xa2 }) if (std::fabs(S[s]) > 0.02 && events < 24) { std::printf("  +%6d = %8.2f ms  slot %02x %+.4f\n", n - t0, (n - t0) / 44.1, s, S[s]); events++; }
        if (n > N / 2) { peakIn = std::max(peakIn, std::fabs(x)); for (int s = 0; s < 256; s += 2) peak[s] = std::max(peak[s], std::fabs(S[s])); }
    }
    if (!imp) for (const auto& p : peak) if (p.second > 1e-7) std::printf("  slot %02x: gain %8.4f  %6.2f dB\n", p.first, p.second / peakIn, 20 * std::log10(p.second / peakIn));
    if (env("FX_UNK")) for (const auto& u : unknown) std::printf("  not modelled: %s, %d steps\n", u.first.c_str(), u.second);
    return 0;
}
