// The V-Synth XT's SOLO encoder — the grain-record ("vpdt") analysis the XT
// runs on Sample → Encode → SOLO.  Ported literally from V-Trainer's
// vpenc.dll (x86, Ghidra project Vsynth_software/ghidra_vt), whose source the
// XT shares (rom.2 0x8951dd70 setup / 0x8951e120 encode), with the XT's own
// differences applied:
//   - decimation filter: a 127-tap float FIR designed at run time (rom.2
//     0x8951f380: the 19-entry low-pass table at 0x897ee1e4 convolved with a
//     Kaiser-windowed high-pass at (pmin + 307.95)/44100); each decimated
//     value is truncated to an integer (ftrc) before the integer sums;
//   - parameters (0x8951dc40 defaults): onset hop 200, energy gate
//     (10·32)²>>8, octave threshold 0.2, onset threshold 11469, voicing
//     threshold 0x4000, 22…2000 Hz;
//   - energy gate tests ≤ (vpenc: <).
// Function names give the vpenc address they port.  Positions are 1/16
// sample; periods 16.16 samples; qualities Q15 normalised distances.
// Reference: research/HARDWARE_FIRMWARE.md ("The XT's own SOLO encoder,
// emulated"; validation against research/ghidra_scripts/EmuEnc.java).

#include "XtSoloEncoder.h"

// Bit-exactness against the XT needs plain IEEE single-precision steps: no
// fused multiply-adds, and the dynamic rounding mode honoured (the SH-4 runs
// round-to-zero; see xtSoloEncode).
#if defined(__clang__)
#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#pragma STDC FENV_ACCESS ON
#endif
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cfenv>
#include <deque>

namespace {

float g_lp[19] = {};
bool g_lpLoaded = false;

struct P48 { uint32_t lo = 0, hi = 0; };   // value = hi·65536 + lo (lo < 65536)
inline bool lt(const P48& a, const P48& b) { return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo); }
inline bool le(const P48& a, const P48& b) { return !lt(b, a); }

struct Node { int pos = 0; Node* prev = nullptr; Node* next = nullptr; uint16_t v = 0, low = 0; };

constexpr int kBuf = 0x3000;

class Enc {
public:
    Enc(const int16_t* x, int n) : x_(x), n_(n) {}
    std::vector<XtRecord> run();

private:
    // ── fixed-point helpers ────────────────────────────────────────────
    // 0x1000b060 / rom.2 0x89522940: divide by a Newton–Raphson reciprocal
    // (12-bit mantissas, two iterations), then an upward-only correction —
    // so the quotient can come out one above floor(a/b).
    static int b060(int a, int b) {
        if (a == 0) return 0;
        int sgn = 1, m = a, rem = a;
        if (a < 0) { sgn = -1; m = -a; rem = m; }
        int e = 0;
        for (; m < 0x40000000; m *= 2) --e;
        int d = b;
        if (d <= 0) return 0;
        for (; d < 0x40000000; d *= 2) ++e;
        d >>= 0x13;
        int r = d * -2 + 0x3000;
        r = (0x2000 - ((r * d) >> 0xc)) * r >> 0xc;
        int q = 0;
        if (0x18 - e < 0x20)
            q = (int)((unsigned)(((0x2000 - ((r * d) >> 0xc)) * r >> 0xc) * (m >> 0x13)) >> ((0x18 - e) & 0x1f));
        rem -= (q + 1) * b;
        while (rem >= 0) { ++q; rem -= b; }
        return q * sgn;
    }
    static P48 ae80(const P48& a, const P48& b) {       // a + b
        P48 r; const uint32_t s = a.lo + b.lo;
        r.hi = a.hi + b.hi + (s >> 16); r.lo = s & 0xffff; return r;
    }
    static P48 aec0(const P48& a, const P48& b) {       // a − b
        P48 r; r.hi = a.hi - b.hi;
        if (b.lo <= a.lo) r.lo = a.lo - b.lo; else { r.lo = a.lo - b.lo + 0x10000; r.hi -= 1; }
        return r;
    }
    static int af00(const P48& a, const P48& b, int c) { // a·c / b, normalised
        int e = 0;
        for (uint32_t u = a.hi; u > 0x7fff; u >>= 1) ++e;
        int na = (int)((a.hi << (16 - e)) + (a.lo >> e));
        int eb = 0;
        for (uint32_t u = b.hi; u > 0x7fff; u >>= 1) ++eb;
        e -= eb;
        int nb = (int)((b.hi << (16 - eb)) + (b.lo >> eb));
        for (; c > 1; c >>= 1) ++e;
        if (na == 0 && nb == 0) return 0;
        for (; nb < 0x40000000 && na < 0x40000000; na <<= 1) nb <<= 1;
        if (e > 0) { const int u = nb >> (e - 1); return b060(na, (u & 1) + (u >> 1)); }
        if (e < 0) { const int u = na >> (-e - 1); na = (u & 1) + (u >> 1); }
        return b060(na, nb);
    }
    static P48 afe0(const P48& in, int mul, unsigned div) {   // in·mul / div (div power of 2)
        int b = 0; for (; div > 1; div >>= 1) ++b;
        const uint32_t m = (uint32_t)mul;
        const uint32_t u1 = (in.hi & 0xffff) * m + ((in.lo * m) >> 16);
        const uint32_t u2 = ((u1 & 0xffff) << (16 - b)) + (((in.lo * m) & 0xffff) >> b);
        P48 r; r.hi = (((in.hi >> 16) * m + (u1 >> 16)) << (16 - b)) + (u2 >> 16); r.lo = u2 & 0xffff;
        return r;
    }
    static int a350(int s) { return (0x8000 - s) >> 1; }
    static int a360(int q) { return 0x8000 - 2 * q; }
    int fb0(int s) const {                               // 0x10009fb0: similarity → v
        if (s <= 0) return 0;
        if (s <= 0x1999) return lo_;
        return lo_ + b060((hi_ - lo_) * (s - 0x1999), 0x6667);
    }
    int ff0(int v) const {                               // 0x10009ff0: v → similarity
        if (v <= 0) return 0;
        if (v <= lo_) return 0x1999;
        return std::min(0x8000, b060((v - lo_) * 0x6667, hi_ - lo_) + 0x1999);
    }
    static int ad60(const P48& d, const P48& e) { const int r = af00(d, e, 0x8000); return r > 0x7fff ? 0x8000 : r; }
    int abs16(int b) const { return off_ * 16 + b; }     // 0x100081e0
    int buf16(int a) const { return a - off_ * 16; }     // 0x100081f0

    // ── signal ─────────────────────────────────────────────────────────
    float fetch(int i) const { return (i >= 0 && i < n_) ? (float)x_[i] : 0.0f; }
    void designFir();                                     // rom.2 0x8951f380
    float decAtF(int s) const {                           // rom.2 0x8951f680 (float)
        float acc = c_[0] * fetch(s);
        for (int k = 1; k < 64; ++k) acc += c_[k] * (fetch(s - k) + fetch(s + k));
        return acc;
    }
    int decAt(int s) const { return (int)decAtF(s); }     // ftrc, as the integer sums read it
    void setDec(int j, int s) { decF_[j] = decAtF(s); dec_[j] = (int)decF_[j]; }
    void fill(int b, int to, bool zeroTail);              // 0x10008a50 / rom.2 0x8951f790
    int shift(int pos, int p3);                           // 0x1000a5b0
    int nextEvent();                                      // 0x10008210

    // D/E sums (0x1000a050 / a0b0 / a1c0) on int buffers
    static P48 sumD(const int* p, int lag, int len) {
        P48 r; for (int i = 0; i < len; ++i) { const uint32_t d = (uint32_t)(p[i - lag] - p[i]);
            const uint32_t u = r.lo + d * d; r.lo = u & 0xffff; r.hi += u >> 16; } return r;
    }
    static P48 sumE(const int* p, int len) {
        P48 r; for (int i = 0; i < len; ++i) { const uint32_t v = (uint32_t)p[i], u = r.lo + v * v;
            r.lo = u & 0xffff; r.hi += u >> 16; } return r;
    }
    static void sumSD(const int* p, int lag, int len, P48& S, P48& D) {
        S = P48(); D = P48();
        for (int i = 0; i < len; ++i) {
            const uint32_t a = (uint32_t)p[i - lag], b = (uint32_t)p[i], s = a + b, d = a - b;
            const uint32_t u5 = S.lo + s * s;
            const uint32_t u7 = D.lo + d * d;
            D.lo = u7 & 0xffff; D.hi += u7 >> 16;
            S.hi += u5 >> 16; S.lo = u5 & 0xffff;
        }
    }
    static int normDiff(const int* p, int lag, int len, bool uns, bool zeroGuard) {   // a100 / a260
        P48 S, D; sumSD(p, lag, len, S, D);
        int sign = 1; P48 num;
        if (!uns) { if (lt(S, D)) { num = aec0(D, S); sign = -1; } else num = aec0(S, D); }
        else num = D;
        const P48 den = ae80(S, D);
        if (zeroGuard && den.lo == 0 && den.hi == 0) return (uns ? 0x10000 : 0) - 0x8000;
        return af00(num, den, 0x8000) * sign;
    }
    int a100(int fp, int lag, int len) const { return normDiff(&full_[fp], lag, len, true, false); }
    // rom.2 0x89521bb0 / 0x89521970: on the FLOAT decimated values, squares
    // in float (wrapped at 2^32 like a 32-bit product, then truncated).
    int a260(int dp, int lag, int len) const {
        const float* p = &decF_[dp];
        const int lg = lag >> 2, n = len >> 2;
        P48 S, D;
        for (int i = 0; i < n; ++i) {
            float s2 = p[i - lg] + p[i]; s2 = s2 * s2;
            float d2 = p[i - lg] - p[i];
            if (2.1474836e+09f <= s2) s2 = s2 - 4.2949673e+09f;
            d2 = d2 * d2;
            if (2.1474836e+09f <= d2) d2 = d2 - 4.2949673e+09f;
            const uint32_t id = (uint32_t)(int)d2, is = (uint32_t)(int)s2;
            D.hi += (D.lo + id) >> 16; D.lo = (D.lo + id) & 0xffff;
            S.hi += (S.lo + is) >> 16; S.lo = (S.lo + is) & 0xffff;
        }
        const P48 den = ae80(S, D);
        if (den.lo == 0 && den.hi == 0) return 0x8000;
        return af00(D, den, 0x8000);
    }
    static int a370(int p1, int p2, int p3, int* out);
    static int a3f0(P48& p1, P48& p2, P48& p3);
    int s94a0(const int* buf, int p, int len, P48& D, P48& E, bool withE) const;

    // ── tracker / onset / octave ──────────────────────────────────────
    int tracker(int pos, int* period);                    // 0x1000aac0
    bool silent();                                         // 0x10008540
    int firstMark(int P) const;                            // 0x10008480
    int candidates(int* q);                                // 0x100085d0
    int refine(int* q, int L);                             // 0x10008730
    int onset(int chan, int* q);                           // 0x10008240
    int backExtend(int q0, Node** first);                  // 0x10008830
    bool keepExtending(int q, Node* n) const;              // 0x100089f0
    int octave(Node* first, Node* last, int chan, int hint);   // 0x10008da0
    void scratchFill(int pos, int span);                   // 0x10009660
    int gapFill(Node* p);                                  // 0x10009970
    float drift(const Node* a, const Node* b) const;       // rom.2 0x89520d30 (XT only)
    void spreadDrift(Node* seg);                           // rom.2 0x89520f30 (XT only)
    void lagTable();                                       // 0x10007be0

    // ── unvoiced fills ─────────────────────────────────────────────────
    int f9c20(int a, int b, int step, int low);
    int f9cf0(int a, int b, int pa, int pb);
    int f9e10(int a, int b, int pa, int pb);
    static int f9de0(int p, int ref) { const int d = p * 2; if (ref <= d) { if (ref * 2 < p) return p >> 1; return 0; } return d; }

    // ── mark list ──────────────────────────────────────────────────────
    Node* newNode(int pos, int v, int low) { pool_.emplace_back(); Node* n = &pool_.back(); n->pos = pos; n->v = (uint16_t)v; n->low = (uint16_t)low; return n; }
    Node* insert(int pos, int v, int low);                 // 0x1000a9b0
    Node* insertAfter(int pos, int v, int low, Node* after);   // 0x1000ae10
    void unlink(Node* n);                                  // 0x1000ada0
    Node* trimFrom(Node* n, int pos) { while (n && pos <= n->pos) { Node* p = n->prev; unlink(n); n = p; } return n; }   // 0x1000ade0
    Node* markAt(int bufPos, int q) { return insert(abs16(bufPos), fb0(a360(q)), 7); }   // 0x1000aa70
    void insertSplit(int a, int b, int k, int v, int low, Node* after) {   // 0x100098d0
        int st = b - a;
        if (k == 2) st >>= 1; else if (k == 4) st = (st + 2) >> 2; else st = b060((k >> 1) + st, k);
        int at = (k - 1) * st + a;
        for (int i = k - 1; i >= 1; --i, at -= st)
            if (at >= 0 && at < n_ * 16) insertAfter(at, (int16_t)v, low, after);
    }

    // ── state (vpenc globals) ──────────────────────────────────────────
    const int16_t* x_; int n_;
    int rd_ = 0;                         // DAT_1002b800 input read index
    std::vector<int> ev_; int evi_ = 0;  // events, DAT_100227c4
    int minLag_ = 0, maxLag_ = 0;        // 227d4 / 227d8
    int onsetThr_ = 0, voiceThr_ = 0, oruThr_ = 0, gate_ = 0, hop_ = 0;
    int unvRec_ = 0, unvStep_ = 0, unvLow_ = 0, lo_ = 0x5999, hi_ = 0xa666;   // b850, 227a0, b854
    std::vector<int> lags_;              // 226a0
    float c_[64] = {};
    int full_[kBuf] = {}, dec_[kBuf / 4 + 4] = {};
    float decF_[kBuf / 4 + 4] = {};     // rom.2 0x8a00f0f4: the XT keeps the decimated signal in float
    std::vector<int> scrBuf_ = std::vector<int>(8192, 0);
    int* scr_ = scrBuf_.data() + 4096;  // DAT_100233f8 (indices ±)
    int off_ = 0;                        // 227ec
    int onsetPos_ = 0, cur_ = 0, back_ = 0, evPos_ = 0, state_ = 0;   // 227e0 227e8 227e4 227dc 227c8
    int fp_ = 0, dp_ = 0;                // 227f4 227f0
    int runEnd_ = 0, runStart_ = 0;      // 2269c 22698
    int period_ = 0;                     // 227c0
    std::deque<Node> pool_;
    int shifts_ = 0;
    FILE* trace_ = std::getenv("XT_TRACE") ? std::fopen(std::getenv("XT_TRACE"), "w") : nullptr;
    Node* head_ = nullptr; Node* tail_ = nullptr;
};

// rom.2 0x8951f380: low-pass table ⊛ Kaiser-windowed high-pass, 64 taps (half).
void Enc::designFir() {
    // The 19-entry low-pass table at rom.2 0x897ee1e4, read from the user's
    // own firmware image (xtLoadTables) — not copied into this source.
    const float* h = g_lp;
    const float fc = ((float)22 + 307.94998f) / 44100.0f;      // pmin = 22 Hz
    float hp[46];
    hp[0] = 1.0f - fc * 2.0f;
    float k = 1.0f; int k2 = 1, odd = 1;
    for (int i = 1; i <= 45; ++i) {
        float t = (std::sqrt((float)(0x7e9 - k2) / 2025.0f) * 1.5064099f) / 3.75f;
        t = t * t;
        float u = k * fc, sg = 1.0f;
        if (u < 0.0f) { u = -u; sg = -1.0f; }
        u = u - (float)(int)u;
        if (0.5f < u) { u = 1.0f - u; sg = -sg; }
        if (0.25f < u) u = 0.5f - u;
        odd += 2; k2 += odd;
        const float i0 = (((((t * 0.0045812996f + 0.0360768f) * t + 0.26597318f) * t + 1.2067491f) * t
                           + 3.0899422f) * t + 3.5156229f) * t * 0.60494757f + 0.60494757f;
        hp[i] = ((6.281732f - (41.131832f - u * u * 74.14366f) * u * u) * u * sg * -i0) / (k * 3.1415925f);
        k += 1.0f;
    }
    for (int j = 0; j < 64; ++j) {
        float s = 0.0f;
        for (int m = 18; m >= 1; --m) { const int a = std::abs(j + m); s = (m < 19 ? h[m] : 0.0f) * (a < 46 ? hp[a] : 0.0f) + s; }
        for (int m = 0; m < 19; ++m) { const int a = std::abs(j - m); s = h[m] * (a < 46 ? hp[a] : 0.0f) + s; }
        c_[j] = s;
    }
}

// 0x10008a50 / rom.2 0x8951f790 (mono): full-rate copy + decimated every 4th.
void Enc::fill(int b, int to, bool zeroTail) {
    int u = b & 3, j = b >> 2;
    if (u == 0) u = 4;
    for (; b < to; ++b, ++u, ++rd_) {
        full_[b] = (rd_ >= 0 && rd_ < n_) ? x_[rd_] : 0;
        if (u > 3) { u = 0; setDec(j++, b + off_); }
    }
    if (zeroTail && b < kBuf) {
        full_[b] = 0;
        if (u > 3) setDec(j, b + off_);
        for (++b, ++rd_; b < kBuf; ++b, ++rd_) full_[b] = 0;
    }
}

int Enc::nextEvent() {                                     // 0x10008210
    const int last = (int)ev_.size() - 1;
    if (evi_ > last) evi_ = last;
    while (evi_ < last && ev_[evi_] < 0) ++evi_;
    return buf16(ev_[evi_]);
}

int Enc::shift(int pos, int p3) {                          // 0x1000a5b0
    if (const char* dd = std::getenv("XT_DUMPDEC"); dd && shifts_ < 3) {   // diagnostic
        char fn[512]; std::snprintf(fn, sizeof fn, "%s.dec%d", dd, shifts_);
        if (FILE* f = std::fopen(fn, "wb")) { std::fwrite(decF_, 4, 3072, f); std::fclose(f); }
    }
    ++shifts_;
    if (state_ == 1) { state_ = 2; return -1; }
    int u6 = (kBuf - maxLag_ < (pos >> 4)) ? kBuf - 3 * maxLag_ : (pos - maxLag_ * 32) >> 4;
    const int u7 = u6 & ~3;
    const int cnt = kBuf - u7;
    for (int i = 0, k = 0, c = 4; i < cnt; ++i, ++c) {
        full_[i] = full_[i + u7];
        if (c > 3) { dec_[k] = dec_[k + (u6 >> 2)]; decF_[k] = decF_[k + (u6 >> 2)]; ++k; c = 0; }
    }
    if (p3 == 0 && cur_ < u7) {
        const int b = (maxLag_ + u7) * 16;
        if (f9cf0(abs16(unvStep_ + cur_), abs16(b), 0, 0) != 0) return 0x4a8;
        cur_ = unvStep_ + b;
    }
    cur_ -= u7 * 16; onsetPos_ -= u7 * 16;
    const int i2 = pos - u7 * 16;
    off_ += u7;
    (void)i2;
    // rom.2 0x89521fc0 end handling (vpenc differs): the zero tail starts
    // only past n + maxLag (samples up to there are read, zero past the
    // wave), and the event pointer is not advanced past the position.
    const unsigned end = (unsigned)(off_ + kBuf);
    if ((unsigned)(n_ - maxLag_) < end) {
        if ((unsigned)(n_ + maxLag_) < end) {
            fill(cnt, kBuf - (int)(end - (unsigned)(n_ + maxLag_)), true);
            evPos_ = buf16(ev_.back() & 0x7fffffff); state_ = 1; return 2;
        }
        fill(cnt, kBuf, false);
    } else fill(cnt, kBuf, false);
    evPos_ = nextEvent();
    return 0;
}

// rom.2 (setup, 0x8a015268 table; vpenc 0x10007be0 steps L/8 and stops at
// 64): candidate lags from minLag in steps of L/12 (multiple of 4), the step
// re-derived each time L doubles; up to 128 entries below maxLag.
void Enc::lagTable() {
    int L = ((minLag_ - 1) >> 2) * 4 + 4, lim = L * 2;
    int step = (b060(L, 12) >> 2) << 2;
    if (step == 0) step = 4;
    lags_.clear();
    do {
        lags_.push_back(L);
        if (lags_.size() > 127) return;
        L += step;
        if (lim < L) { lim = L * 2; step = (b060(L, 12) >> 2) << 2; if (step == 0) step = 4; }
    } while (L < maxLag_);
}

int Enc::a370(int p1, int p2, int p3, int* out) {          // 0x1000a370 / rom.2 0x89521d00
    if (p1 <= p2) { *out = p1; return -0x10000; }
    if (p3 <= p2) { *out = p3; return 0x10000; }
    const int den = p3 + p1 + p2 * -2;
    if (den == 0) return 0;
    const int o = b060((p1 - p3) * 0x10000, den * 2);
    const int v = p2 - ((o * (p1 - p3)) >> 0x12);
    *out = v < 0 ? 0 : v;
    return o;
}

int Enc::a3f0(P48& p1, P48& p2, P48& p3) {                 // 0x1000a3f0 (writes min into p2)
    if (lt(p1, p2)) { p2 = p1; return -0x10000; }
    if (!le(p2, p3)) { p2 = p3; return 0x10000; }
    const P48 s13 = ae80(p3, p1), s22 = ae80(p2, p2);
    const P48 den = aec0(s13, s22);
    if (den.hi == 0 && den.lo == 0) return 0;
    P48 num; int sg;
    if (lt(p3, p1)) { num = aec0(p1, p3); sg = 1; } else { num = aec0(p3, p1); sg = -1; }
    const P48 den2 = ae80(den, den);
    const int r = af00(num, den2, 0x10000);
    const uint32_t m1 = num.hi * (uint32_t)r, m0 = (num.lo * (uint32_t)r) >> 16;
    const uint32_t u2 = (m1 & 0xffff) + m0, u3 = (m1 >> 16) + (u2 >> 16);
    P48 c; c.hi = u3 >> 2; c.lo = ((u2 & 0xffff) + (u3 & 3) * 0x10000) >> 2;
    if (le(c, p2)) { p2 = aec0(p2, c); } else { p2 = P48(); }
    return r * sg;
}

// 0x100094a0: lag search on a decimated buffer in steps of 4 samples.
int Enc::s94a0(const int* buf, int p, int len, P48& D, P48& E, bool withE) const {
    int u5 = (p + 2) & ~3;
    const int lim15 = u5 * 15;
    const int wl = (len + 2) >> 2;
    D = sumD(buf, (p + 2) >> 2, wl);
    if (withE) E = sumE(buf, (p + 2) >> 2);
    int u1 = u5;
    P48 T;
    bool down = false;
    while (!down) {
        while (true) {
            u1 += 4;
            if ((u5 * 17) >> 4 <= u1) { down = true; break; }
            T = sumD(buf, u1 >> 2, wl);
            if (le(D, T)) break;
            D = T; p = u1;
        }
        if (down) break;
        if (!(T.hi * 2 <= E.hi)) break;
    }
    while (true) {
        u5 -= 4;
        if (u5 < (lim15 >> 4) || u5 < minLag_) break;
        T = sumD(buf, u5 >> 2, wl);
        if (le(D, T)) { if (E.hi < T.hi * 2) break; continue; }
        D = T; p = u5;
    }
    if (p < minLag_) p = minLag_;
    P48 Dm = sumD(buf, (p - 4) >> 2, wl), Dp = sumD(buf, (p + 4) >> 2, wl);
    const int o = a3f0(Dm, D, Dp);
    return (o + p * 0x4000) * 4;
}

int Enc::tracker(int pos, int* period) {                   // 0x1000aac0
    if (trace_) std::fprintf(trace_, "tracker %d %d onset=%d cur=%d back=%d off=%d\n", pos, *period, onsetPos_, cur_, back_, off_);
    const int u6 = (*period + 0x8000) >> 16;
    fp_ = (pos + 8) >> 4;
    const int L = (*period + 0x8000) >> 0x12;
    dp_ = (pos + 8) >> 6;
    const int* d = &dec_[dp_];
    P48 D0 = sumD(d, L, L), D1 = sumD(d, L + 1, L), E = sumE(d, L), best;
    if (E.hi == 0 || E.lo == 0) return 0x8000;
    const P48 T = afe0(E, 0x2000, 0x8000);
    if (lt(D0, D1)) {
        if (lt(T, D0) || L * 4 <= minLag_) {
            *period = s94a0(d, u6, u6, D0, E, true);
        } else {
            int c = L - 1;
            P48 Dm = sumD(d, c, L);
            while (lt(Dm, D0)) {
                D1 = D0; D0 = Dm; --c;
                if (!(c * 4 > minLag_)) { ++c; break; }
                Dm = sumD(d, c, L);
            }
            const int o = a3f0(Dm, D0, D1);
            *period = (o + (c + 1) * 0x10000) * 4;
        }
        best = D0;
    } else {
        if (lt(T, D1)) {
            *period = s94a0(d, u6, u6, D1, E, true);
        } else {
            int l = L + 2;
            P48 Dp = sumD(d, l, L);
            if (lt(Dp, D1)) { ++l; D0 = D1; D1 = Dp; }   // single step (see 0x1000abcd)
            const int o = a3f0(D0, D1, Dp);
            *period = (o - 0x10000 + l * 0x10000) * 4;
        }
        best = D1;
    }
    if (maxLag_ * 0x10000 < *period) *period = maxLag_ * 0x10000;
    return ad60(best, E);
}

bool Enc::silent() {                                       // 0x10008540 (XT: ≤)
    const int* p = &full_[fp_ - maxLag_];
    if (evPos_ + (maxLag_ + 1) * -0x10 < onsetPos_) return false;
    const int len = maxLag_ * 2;
    int s = 0;
    for (int i = 0; i < len; ++i) {
        const int v = p[i] * p[i];
        s += ((v >> 7) & 1) + (v >> 8);
        if (s < 0) { s = 0x7fffffff; break; }
    }
    return s <= gate_ * len;
}

// rom.2 0x8951e900 (the XT's rule; vpenc 0x10008480 used the strongest P/4
// energy window instead): the largest |sample| of the band-passed decimated
// signal over one period, then the nearer zero crossing (ties forward); the
// mark is the first decimated sample past it, ×4, wrapped into [0, P).
int Enc::firstMark(int P) const {
    const float* d = &decF_[dp_];
    const int m = P >> 2;
    float best = -1e10f; int i5 = -1;
    for (int i = 0; i < m; ++i) { const float a = std::fabs(d[i]); if (best < a) { best = a; i5 = i; } }
    if (i5 < 0) return 0;
    const float sg = (0.0f < d[i5]) ? 1.0f : -1.0f;
    int fwd = 0, bwd = 0;
    if (0.0f < sg * d[i5]) {
        do { ++fwd; } while (0.0f < d[i5 + fwd] * sg);
        do { --bwd; } while (0.0f < d[i5 + bwd] * sg);
    }
    if (fwd + bwd < 1) bwd = fwd;
    int off = (i5 + bwd) * 4;
    while (P <= off) off -= P;
    while (off < 0) off += P;
    return off;
}

int Enc::candidates(int* q) {                              // 0x100085d0
    bool armed = false;
    *q = onsetThr_;
    int found = -1;
    for (const int L : lags_) {
        int d = a260(dp_, L, L);
        if (d * 2 < 0x8001) { if (!armed) continue; }
        else armed = true;
        if (d < *q) {
            const int s = std::min(L >> 2, 0x40);
            for (int k = 1; k <= 3; ++k) d = std::max(d, a260(dp_ + k * s, L, L));
            if (d < *q) {
                const int d2 = a260(dp_, L, L * 2), d3 = a260(dp_, L, L * 3);
                if (d2 < d * 2 && d3 < d * 2 && a100(fp_, L, L) <= onsetThr_) { *q = d; found = L; }
            }
        }
    }
    return found;
}

int Enc::refine(int* q, int L) {                           // 0x10008730
    int hi = std::min((L * 3) >> 1, maxLag_ - 2), lo = std::max((L * 5) >> 3, minLag_ + 1);
    int best = L, l = L, d;
    do { if (++l >= hi) break; d = a100(fp_, l, l); if (d < *q) { *q = d; best = l; } } while (d * 2 < 0x8001);
    l = L;
    do { if (--l < lo) break; d = a100(fp_, l, l); if (d < *q) { *q = d; best = l; } } while (d * 2 < 0x8001);
    const int dp = a100(fp_, best + 1, best + 1), q0 = *q, dm = a100(fp_, best - 1, best - 1);
    return a370(dm, q0, dp, q) + best * 0x10000;
}

int Enc::onset(int chan, int* q) {                         // 0x10008240
    (void)chan;
    bool first = abs16(onsetPos_) < maxLag_ * 16;
    while (true) {
        onsetPos_ += (first ? maxLag_ + 1 : hop_) * 16;
        const int a = abs16(onsetPos_);
        if ((kBuf - maxLag_) * 16 <= onsetPos_ || n_ * 16 <= a) {
            if (shift(onsetPos_, 0) == 0x4a8) return -0xdd;
            if (state_ == 2) return -1;
        }
        fp_ = (onsetPos_ + 8) >> 4;
        dp_ = ((onsetPos_ >> 2) + 8) >> 4;
        if (!silent()) {
            first = false;
            const int L = candidates(q);
            if (L >= 1) {
                const int fm = firstMark(L);
                if (trace_) std::fprintf(trace_, "firstMark P=%d -> %d onset=%d\n", L, fm, onsetPos_);
                onsetPos_ += fm * 16;
                dp_ = onsetPos_ >> 6;
                if (abs16(onsetPos_) <= n_ * 16) return refine(q, L);
            }
        } else {
            if (first) insert(a, 0x8000, unvLow_);
            else {
                if (!tail_ || !tail_->prev || tail_->low != 7) f9c20(abs16(cur_), a, unvStep_, unvLow_);
                else f9cf0(abs16(cur_), a, (tail_->pos - tail_->prev->pos) * 0x1000, 0);
                first = true;
            }
            cur_ = hop_ * 16 + onsetPos_;
        }
    }
}

bool Enc::keepExtending(int q, Node* nd) const {          // 0x100089f0
    const bool before = cur_ < back_;
    if (!before && nd) {
        int t = a350(ff0(nd->v));
        if (t < 0x2000) t = (t << 7) >> 8;
        if (q < t) return true;
    }
    return before;
}

int Enc::backExtend(int q0, Node** first) {                // 0x10008830
    (void)q0;
    int per = period_;
    back_ -= period_ >> 12;
    int q = tracker(back_, &per);
    Node* nd = (*first)->prev;
    bool go = keepExtending(q, nd);
    while (go && q < voiceThr_) {
        nd = trimFrom(nd, abs16(back_));
        if (back_ < maxLag_ * 16 + (per >> 12)) break;
        Node* m = markAt(back_, q);
        *first = m;
        nd = m->prev;
        back_ -= per >> 12;
        q = tracker(back_, &per);
        go = keepExtending(q, nd);
    }
    if (back_ <= cur_) {
        nd = trimFrom(nd, abs16(back_));
        if (!nd) cur_ = buf16(0);
        else { cur_ = buf16(nd->pos); unlink(nd->prev ? nd->prev->next : head_); }
    }
    runEnd_ = abs16(cur_);
    runStart_ = abs16(back_);
    return 0;
}

void Enc::scratchFill(int pos, int span) {                 // 0x10009660 (mono, step 4)
    const int u6 = ((span * 9) >> 3) & ~3, h = (span * 9) >> 5;
    for (int j = 0; j < 2 * h; ++j) scr_[-h + j] = decAt(pos - u6 + 4 * j);
}

int Enc::octave(Node* first, Node* last, int chan, int hint) {   // 0x10008da0
    int i2 = first->pos, maxv = 0;
    const int s0 = i2 >> 4;
    int merges = 0, checks = 0, votes[6] = {};
    if ((last->pos >> 4) <= hop_ + s0 || first == last || first->next == last) return 0;
    const int runLen = (last->pos >> 4) - s0;
    int pt = (runLen >> 6) + s0;
    Node* p3 = first;
    while (i2 < s0 * 16 && p3 != last) { p3 = p3->next; i2 = p3->pos; }
    Node* ptNode[32]; int ptPos[32];
    for (int i = 0; i < 32; ++i) {
        i2 = p3->pos;
        while (i2 < pt * 16 && p3 != last) { p3 = p3->next; i2 = p3->pos; }
        ptNode[i] = (p3 == first) ? p3 : p3->prev;
        if (maxv < p3->v) maxv = p3->v;
        ptPos[i] = pt;
        pt += runLen >> 5;
    }
    const int thr = a350(ff0((((maxv - lo_) * 3) >> 2) + lo_));
    const int hint1 = hint + 1;
    int kmax = std::min(6, hint1);
    for (int i = 0; i < 32; ++i) {
        Node* nd = ptNode[i];
        const int iv = nd->next->pos - nd->pos;
        const int P = (((iv >> 4) - 1) & ~3) + 4, P2 = (((iv * 2 >> 4) - 1) & ~3) + 4;
        scratchFill(ptPos[i], P2);
        P48 E = sumE(scr_, P >> 2), D2, Dk[6], DP;
        s94a0(scr_, P2, P, D2, E, false);
        for (int k = 2; k <= kmax; ++k) {
            const int u = ((b060(iv, k) >> 4) + 2) & ~3;
            if (u < minLag_) { kmax = k - 1; break; }
            s94a0(scr_, u, P, Dk[k - 2], E, false);
        }
        const int q1 = a350(ff0(nd->next->v)), q0 = a350(ff0(nd->v));
        if (q1 < thr || q0 < thr) {
            ++checks;
            s94a0(scr_, P, P, DP, E, false);
            if (lt(D2, DP) && P * 2 < maxLag_) { if (oruThr_ < af00(aec0(DP, D2), E, 0x8000)) ++merges; }
            for (int k = 2; k <= kmax; ++k)
                if (le(Dk[k - 2], DP) || af00(aec0(Dk[k - 2], DP), E, 0x8000) * 2 < oruThr_) ++votes[k - 2];
        }
    }
    int k = 1;
    if (kmax > 1) {
        for (int j = 2; j <= kmax; ++j) if (checks * 9 <= votes[j - 2] * 32) k = j;
        if (k > 1) {
            Node* a = last; Node* b = last->prev; Node* c;
            do { c = b; insertSplit(c->pos, a->pos, k, (c->v + a->v) >> 1, a->low, c); a = c; b = c->prev; } while (c != first);
            insertSplit(runStart_, first->pos, k, first->v, c->low, first->prev);
            insertSplit(last->pos, abs16(cur_), k, last->v, c->low, last);
            for (int j = k - 1; j > 0; --j) { if (first->prev) first = first->prev; if (last->next) last = last->next; }
            if (6 < hint1) octave(first, last, chan, b060(hint1, k));
            return (first->next->pos - first->pos) * 0x1000;
        }
    }
    if (merges < 9) return (first->next->pos - first->pos) * 0x1000;
    // octave down: drop every other mark
    Node* p4 = first->next;
    int ret = p4->pos - first->pos;
    bool viaGoto = false;
    while (p4 != last) {
        Node* nx = p4->next;
        unlink(p4);
        if (nx == last) {
            cur_ = buf16(last->pos * 2 - last->prev->pos);
            if (p4 != last) { ret = (first->next->pos - first->pos) * 0x1000; viaGoto = true; }
            break;
        }
        p4 = nx->next;
        ret = p4->pos - nx->pos;
    }
    if (!viaGoto) { unlink(p4); ret <<= 12; }
    if (first == tail_) return ret;
    int pr = first->next->pos - first->pos, at = first->pos - pr;
    bool skipTail = false;
    while (runEnd_ < at) {
        const int ps = pr >> 4, as = at >> 4;
        if (as <= ps * 3 + 4) { if (runEnd_ < at && ps * 3 + 4 < as) skipTail = true; break; }
        scratchFill(as, ((ps * 2 - 1) & ~3) + 4);
        P48 D, E;
        ret = s94a0(scr_, ps, ps, D, E, true);
        const int q = af00(D, E, 0x8000);
        if (voiceThr_ <= q) { if (runEnd_ < at && ps * 3 + 4 < as) skipTail = true; break; }
        first = insert(at, fb0(a360(q)), 7);
        pr = ret >> 12; at -= pr;
    }
    if (!skipTail) {
        at += pr;
        first = first->next;
        unlink(first->prev);
    }
    if (first != tail_) ret = (first->next->pos - first->pos) * 0x1000;
    runStart_ = at;
    back_ = buf16(at);
    return ret;
}

// rom.2 0x89520d30 (XT only): phase drift between two marks, measured on
// the band-passed signal at full rate.  Around mark A, the zero crossing
// nearest the mark (linear interpolation; its direction is remembered);
// around mark B, the crossing of the same direction (forward if B's centre
// sample has the opposite sign, else backward).  Returns
// (crossing_B − B) − (crossing_A − A) in samples.
float Enc::drift(const Node* A, const Node* B) const {
    const int P = (B->prev ? B->pos - B->prev->pos : B->next->pos - B->pos) >> 4;
    const int ia = A->pos >> 4, ib = B->pos >> 4;
    const float fa = (float)(A->pos - ia * 16) * 0.0625f, fb = (float)(B->pos - ib * 16) * 0.0625f;
    std::vector<float> ba(2 * P + 2), bb(2 * P + 2);
    float* a = ba.data() + P; float* b = bb.data() + P;
    for (int k = -P; k < P; ++k) { a[k] = decAtF(ia + k); b[k] = decAtF(ib + k); }
    auto at = [P](const float* v, int k) { return (k >= -P && k < P) ? v[k] : 0.0f; };
    const float sg = (0.0f < a[0]) ? 1.0f : -1.0f;
    int r = 0;
    if (0.0f < sg * a[0]) { do { ++r; } while (0.0f < at(a, r) * sg); }
    float cA = at(a, r - 1) / (at(a, r - 1) - at(a, r));
    cA = cA + (float)r; cA = cA + -1.0f;
    int rb = 0;
    if (0.0f < sg * a[0]) { do { --rb; } while (0.0f < at(a, rb) * sg); }
    float cAb = (float)rb - at(a, rb + 1) / (at(a, rb + 1) - at(a, rb));
    cAb = cAb + 1.0f;
    float dir;
    if (0.0f < cA + cAb) { cA = cAb; dir = sg; } else dir = -sg;
    const float sgB = (0.0f < b[0]) ? 1.0f : -1.0f;
    const float prod = dir * sgB, s8 = sgB * b[0];
    int q = 0; float cB;
    if (prod < 0.0f) {
        if (0.0f < s8) { do { ++q; } while (0.0f < at(b, q) * sgB); }
        cB = at(b, q - 1) / (at(b, q - 1) - at(b, q)); cB = cB + (float)q; cB = cB + -1.0f;
    } else {
        if (0.0f < s8) { do { --q; } while (0.0f < at(b, q) * sgB); }
        cB = (float)q - at(b, q + 1) / (at(b, q + 1) - at(b, q)); cB = cB + 1.0f;
    }
    return (cB - fb) - (cA - fa);
}

// rom.2 0x89520f30 (XT only): spread the drift linearly over the marks after
// seg (the k-th of N moves by round(16·k·drift/N)/16; the newest by the whole
// drift), so marks stay locked to the waveform's zero crossings.
void Enc::spreadDrift(Node* seg) {
    int count = 0; Node* last = seg;
    for (Node* n = seg->next; n; n = n->next) { ++count; last = n; }
    if (count == 0) return;
    const float d = drift(seg, last);
    float k = 1.0f;
    for (Node* n = seg->next; n; n = n->next, k = k + 1.0f) {
        const float x = (k * d) / (float)count;
        int r;
        if (0.0f < x) r = (int)(x * 16.0f + 0.5f); else r = -(int)(-x * 16.0f + 0.5f);
        n->pos += r;
    }
}

int Enc::gapFill(Node* p) {                                // 0x10009970
    if (!p || p->low != 7 || !p->next || p->next->low != 7) return 0;
    Node* q = p->next;
    int a = q->pos - runStart_, b = runEnd_ - p->pos;
    if (!(a * 2 < b * 3 && b * 2 < a * 3)) return 0;
    const int g = (a + b) >> 1;
    if (!(runStart_ - runEnd_ <= g * 4 && runStart_ + g < tail_->pos)) return 0;
    Node* n3 = insert(runEnd_, p->v, 7);
    Node* n4 = insert(runStart_, q->v, 7);
    int at = n3->pos + g;
    while (at < n4->pos) { insert(at, n3->v, 7); at += g; }
    Node* nx = n4->next;
    int nxp = nx->pos;
    while (nxp <= at) { unlink(nx->prev); n4 = nx; nx = nx->next; nxp = nx->pos; }
    const int f = b060((at - n4->pos) * 0x8000, n4->next->pos - n4->pos);
    n4->pos = at;
    Node* r = n4->next;
    while (r->next) { Node* s = r->next; r->pos = ((s->pos - r->pos) * f >> 15) + r->pos; r = s; }
    const int d = abs16(cur_) - r->pos;
    r->pos = r->pos + (f * d >> 15);
    if (maxLag_ * 32 <= buf16(r->pos)) { cur_ = buf16(r->pos); unlink(r); return 1; }
    cur_ = buf16(r->pos + d);
    return 1;
}

int Enc::f9c20(int a, int b, int step, int low) {          // 0x10009c20
    const int span = b - a;
    if (step < span) {
        int k = b060((step >> 1) + span, step), s = b060(span, k);
        while (maxLag_ * 16 <= s) { ++k; s = b060(span, k); }
        for (int i = 0; i < k; ++i) { insert(a, 0x8000, low); a += s; }
    } else if (step < span * 2) insert(a, 0x8000, low);
    insert(b, 0x8000, low);
    return 0;
}

int Enc::f9cf0(int a, int b, int pa, int pb) {             // 0x10009cf0
    while (true) {
        const int span = b - a;
        if (span < 16000 || span < (int)(((pa + pb) >> 10) & ~3u)) return f9e10(a, b, pa, pb);
        if (pa > 0) { insert(a, lo_, 7); a += pa >> 12; pa = f9de0(pa, unvRec_); }
        if (pb > 0) { insert(b, lo_, 7); b -= pb >> 12; pb = f9de0(pb, unvRec_); }
        if (pa == 0 && pb == 0) break;
    }
    return f9c20(a, b, unvStep_, unvLow_);
}

int Enc::f9e10(int a, int b, int pa, int pb) {             // 0x10009e10
    int g = (pa + pb) >> 0xd, A = pa, B = pb;
    if (g * 4 <= b - a) {
        do {
            pa = A;
            if (A > 0) { insert(a, lo_, 7); a += A >> 12; pa = f9de0(A, B); }
            pb = B;
            if (B > 0) { insert(b, lo_, 7); b -= B >> 12; pb = f9de0(B, A); }
            if (pa == 0 || pb == 0) {
                int s = (A + B) >> 1;
                if (A == 0) s = B;
                if (B == 0) s = A;
                return f9c20(a, b, s >> 12, 7);
            }
            g = (pb + pa) >> 0xd; B = pb; A = pa;
        } while (g * 4 <= b - a);
    }
    if (pa == 0 || pb == 0) g <<= 1;
    if (g != 0) return f9c20(a, b, g, 7);
    return f9c20(a, b, unvStep_, unvLow_);
}

Node* Enc::insert(int pos, int v, int low) {               // 0x1000a9b0
    if (trace_) std::fprintf(trace_, "insert %d %d %d onset=%d cur=%d back=%d off=%d\n", pos, v, low, onsetPos_, cur_, back_, off_);
    Node* nd = newNode(pos, v, low);
    Node* p = tail_;
    while (p && pos < p->pos) p = p->prev;
    Node* nx = p ? p->next : head_;
    nd->prev = p; nd->next = nx;
    if (p) p->next = nd; else head_ = nd;
    if (nx) nx->prev = nd; else tail_ = nd;
    return nd;
}

Node* Enc::insertAfter(int pos, int v, int low, Node* after) {   // 0x1000ae10
    Node* nd = newNode(pos, v, low);
    Node* nx = after ? after->next : head_;
    nd->prev = after; nd->next = nx;
    if (after) after->next = nd; else head_ = nd;
    if (nx) nx->prev = nd; else tail_ = nd;
    return nd;
}

void Enc::unlink(Node* nd) {                               // 0x1000ada0
    Node* p = nd->prev; Node* nx = nd->next;
    if (p) p->next = nx; else head_ = nx;
    if (nx) nx->prev = p; else tail_ = p;
}

std::vector<XtRecord> Enc::run() {
    // setup (0x100079d0 with rom.2 0x8951dc40 defaults)
    ev_.assign(1, n_ * 16);
    minLag_ = b060(44100, 2000);
    maxLag_ = std::min(0x7ff, b060(44100, 22));
    onsetThr_ = (0x8000 - b060(3 << 15, 10)) >> 1;
    voiceThr_ = (0x8000 - b060(0, 10)) >> 1;
    oruThr_ = b060(2000 << 15, 10000);
    gate_ = (10 * 0x20 * 10 * 0x20) >> 8;
    hop_ = 200;
    unvRec_ = b060(44100, 1000) << 16;
    unvStep_ = unvRec_ >> 12;
    unvLow_ = 0;
    lagTable();
    designFir();
    if (const char* fd = std::getenv("XT_DUMPFIR")) if (FILE* f = std::fopen(fd, "wb")) { std::fwrite(c_, 4, 64, f); std::fclose(f); }

    // main loop (0x10007c70), mono
    const int chan = 2;
    off_ = -maxLag_;
    fill(maxLag_, kBuf - ((unsigned)(n_ + maxLag_) < (unsigned)kBuf ? kBuf - n_ - maxLag_ : 0), false);
    state_ = (unsigned)(n_ + maxLag_) < (unsigned)kBuf ? 1 : 0;
    cur_ = maxLag_ << 4;
    int nextOnset = cur_;
    while (true) {
        onsetPos_ = nextOnset;
        if (state_ > 1) break;
        if (state_ < 2) evPos_ = nextEvent();
        int q = 0;
        period_ = onset(chan, &q);
        if (period_ == -0xdd) break;
        // zero crossings over the first period of the decimated signal
        const int span = ((((period_ >> 16) - 1) & ~3) + 4) >> 2;
        int zc = 0, sg = dec_[dp_] >= 0 ? 1 : -1;
        for (int k = 1; k < span; ++k) if (dec_[dp_ + k] * sg < 0) { ++zc; sg = -sg; }
        if (state_ == 2) break;
        back_ = onsetPos_;
        Node* m = markAt(back_, q);
        Node* first = m;
        Node* seg = m;                       // rom.2 0x8951e120: drift segment start
        backExtend(q, &first);
        int per = period_;
        cur_ = (period_ >> 12) + onsetPos_;
        if (cur_ < evPos_ || state_ != 1) {
            bool done = false;
            if ((kBuf - maxLag_) * 16 <= cur_ || maxLag_ * 32 + evPos_ <= cur_) {
                shift(cur_, 1);
                if (state_ == 2 || evPos_ <= cur_) done = true;
            }
            if (!done) {
                int qq = tracker(cur_, &per);
                if (qq < voiceThr_) {
                    while (true) {
                        if ((per >> 16) < minLag_ || maxLag_ < (per >> 16)) { done = true; break; }
                        m = markAt(cur_, qq);
                        if (!first) first = m;
                        if (seg->pos + 0x1b000 < m->pos) {     // XT: every 6912 samples
                            spreadDrift(seg);
                            cur_ = buf16(m->pos);
                            seg = m;
                        }
                        cur_ += per >> 12;
                        if (evPos_ <= cur_ && state_ == 1) break;
                        if ((kBuf - maxLag_) * 16 <= cur_ || maxLag_ * 32 + evPos_ <= cur_) {
                            shift(cur_, 1);
                            if (evPos_ <= cur_ || state_ == 2) { done = true; break; }
                        }
                        qq = tracker(cur_, &per);
                        if (voiceThr_ <= qq) { done = true; break; }
                    }
                    if (!done) state_ = 2;
                }
            }
        } else state_ = 2;
        // 0x1000800d
        if (!first) first = m;
        Node* before = first->prev;
        const int r = octave(first, m, chan, ((zc - 1) >> 1) + 1);
        if (r < 0) break;
        if (r == 0) {
            trimFrom(tail_, first->pos);
            const int c = cur_;
            cur_ = buf16(runEnd_);
            nextOnset = c;
        } else {
            const int g = gapFill(before);
            nextOnset = cur_;
            if (g == 0) {
                int pa;
                if (!before || !before->prev || (pa = (before->pos - before->prev->pos) * 0x1000, maxLag_ * 0x10000 < pa)) pa = 0;
                f9cf0(runEnd_, runStart_, pa, r);
                nextOnset = cur_;
            }
        }
    }
    // finalize (0x100080fe)
    const int endBuf = (n_ - off_) * 16;
    if (cur_ < endBuf) {
        int pa = unvRec_, pb = unvRec_;
        if (tail_ && tail_->prev && tail_->low == 7) { pa = (tail_->pos - tail_->prev->pos) * 0x1000; pb = 0; }
        f9cf0(abs16(cur_), abs16(endBuf), pa, pb);
    }
    if (head_ && 0 < head_->pos) insert(0, 0, head_->low);
    const int last = ev_.back() & 0x7fffffff;
    if (tail_ && tail_->pos < last) insert(last, 0, unvLow_);

    std::vector<XtRecord> out;
    for (Node* a = head_; a && a->next; a = a->next) {
        const Node* b = a->next;
        out.push_back({ a->pos, b->pos - a->pos, ((b->v >> 8) & 0xf8) | b->low });
    }
    return out;
}

}  // namespace

bool xtLoadTables(const char* romPath) {
    if (g_lpLoaded) return true;
    if (!romPath) return false;
    FILE* f = std::fopen(romPath, "rb");
    if (!f) return false;
    // rom.2 is loaded at 0x89300000 (VAPP1); the table is at 0x897ee1e4.
    const long off = 0x897ee1e4L - 0x89300000L;
    float t[19];
    const bool ok = std::fseek(f, off, SEEK_SET) == 0 && std::fread(t, 4, 19, f) == 19;
    std::fclose(f);
    if (!ok) return false;
    // Sanity: a low-pass half-kernel — centre tap 0.1…0.2, taps decreasing, ≥ 0.
    if (!(t[0] > 0.1f && t[0] < 0.2f)) return false;
    for (int i = 1; i < 19; ++i) if (!(t[i] >= 0.0f && t[i] <= t[i - 1])) return false;
    std::memcpy(g_lp, t, sizeof t);
    g_lpLoaded = true;
    return true;
}

bool xtTablesLoaded() { return g_lpLoaded; }

std::vector<XtRecord> xtSoloEncode(const int16_t* x, int n) {
    if (!x || n <= 0 || !g_lpLoaded) return {};
    // The SH-4 runs with FPSCR RM = 1 (round to zero).  XT_ROUND=nearest
    // gives the P-code emulator's rounding instead (diagnostic).
    const char* rm = std::getenv("XT_ROUND");
    const bool rtz = !(rm && std::string(rm) == "nearest");
    const int saved = std::fegetround();
    if (rtz) std::fesetround(FE_TOWARDZERO);
    Enc e(x, n);
    std::vector<XtRecord> out = e.run();
    std::fesetround(saved);
    return out;
}

#ifdef XT_ENCODER_MAIN
#include <cstdio>
// xt_encode <rom.2.bin> <samp.bin (int16 LE)> [stereo] : prints "start len info"
// (1/16 sample).  "stereo": interleaved L/R, encoded from (L+R) >> 1 as the
// XT's stereo reader (0x8951f6f0) does.
int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s V-SynthXT.rom.2.bin samp.bin [stereo]\n", argv[0]); return 1; }
    if (!xtLoadTables(argv[1])) { std::fprintf(stderr, "cannot read the low-pass table from %s\n", argv[1]); return 1; }
    const bool stereo = argc > 3 && std::string(argv[3]) == "stereo";
    ++argv;
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) return 1;
    std::vector<int16_t> x;
    int16_t s[2];
    while (std::fread(s, 2, stereo ? 2 : 1, f) == (stereo ? 2u : 1u))
        x.push_back(stereo ? (int16_t)((s[0] + s[1]) >> 1) : s[0]);
    std::fclose(f);
    for (const XtRecord& r : xtSoloEncode(x.data(), (int)x.size()))
        std::printf("%d\t%d\t%d\n", r.start16, r.len16, r.info);
    return 0;
}
#endif
