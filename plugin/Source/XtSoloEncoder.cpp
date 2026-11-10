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
//   - energy gate tests ≤ (vpenc: <);
//   - unvoiced fills step along the wave's own zero crossings (0x895214b0);
//   - the lag search is wider for the tracker than for the octave votes;
//   - a stereo wave has its own mode, 0x8951e120(3): see "the reader" below.
// Function names give the vpenc address they port.  Positions are 1/16
// sample; periods 16.16 samples; qualities Q15 normalised distances.
// Reference: research/HARDWARE_FIRMWARE.md ("The XT's own SOLO encoder,
// emulated"; "The SOLO encoder against the firmware's", v112; "The SOLO encoder's
// stereo mode", v113).  Its check is
// XT_ENCODER_MAIN below (xt_enc in fcheck).

#include "XtSoloEncoder.h"
#include "XtSwitches.h"

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

// SH-4 integer steps: everything wraps at 32 bits, and a loud wave does reach it.
inline int wadd(int a, int b) { return (int)((uint32_t)a + (uint32_t)b); }
inline int wsub(int a, int b) { return (int)((uint32_t)a - (uint32_t)b); }
inline int wmul(int a, int b) { return (int)((uint32_t)a * (uint32_t)b); }
inline int wshl(int a, int n) { return (int)((uint32_t)a << n); }
inline int shad(int x, int n) {                            // SHAD: n < 0 is an arithmetic right shift
    if (n >= 0) return wshl(x, n & 31);
    return (n & 31) ? x >> ((-n) & 31) : (x < 0 ? -1 : 0);
}
inline uint32_t shld(uint32_t x, int n) {                  // SHLD: n < 0 is a logical right shift
    if (n >= 0) return x << (n & 31);
    return (n & 31) ? x >> ((-n) & 31) : 0;
}
inline int ftrc(float x) {                                 // FTRC: truncates and saturates
    if (x != x) return (int)0x80000000;
    if (x >= 2147483648.0f) return 0x7fffffff;
    if (x < -2147483648.0f) return (int)0x80000000;
    return (int)x;
}

class Enc {
public:
    Enc(const int16_t* x, int n, int channels, std::vector<uint64_t>* trace) : x_(x), n_(n), stereo_(channels == 2), memTrace_(trace) {}
    std::vector<XtRecord> run();

private:
    // ── fixed-point helpers ────────────────────────────────────────────
    // 0x1000b060 / rom.2 0x89522940: divide by a Newton–Raphson reciprocal
    // (12-bit mantissas, two iterations), then an upward-only correction —
    // so the quotient can come out one above floor(a/b).  A quotient over 2^24
    // shifts left (SHAD).  The firmware never returns from a zero divisor: 0 here.
    static int b060(int a, int b) {
        if (a == 0) return 0;
        int sgn = 1, m = a, rem = a;
        if (a < 0) { sgn = -1; m = wsub(0, a); rem = m; }
        int e = 0;
        for (; m < 0x40000000; m = wshl(m, 1)) { --e; if (m == 0) return 0; }
        int d = b;
        for (; d < 0x40000000; d = wshl(d, 1)) { ++e; if (d == 0) return 0; }
        d >>= 0x13;
        int r = d * -2 + 0x3000;
        r = (0x2000 - ((r * d) >> 0xc)) * r >> 0xc;
        int q = 0;
        if (0x18 - e < 0x20)
            q = shad(wmul((0x2000 - ((r * d) >> 0xc)) * r >> 0xc, m >> 0x13), e - 0x18);
        rem = wsub(rem, b);
        if (wmul(q, b) <= rem) {
            int left = wsub(rem, wmul(q, b));
            do { left = wsub(left, b); ++q; } while (left >= 0);
        }
        return wmul(q, sgn);
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
        int na = (int)(shld(a.hi, 16 - e) + shld(a.lo, -e));
        int eb = 0;
        for (uint32_t u = b.hi; u > 0x7fff; u >>= 1) ++eb;
        e -= eb;
        int nb = (int)(shld(b.hi, 16 - eb) + shld(b.lo, -eb));
        for (; c > 1; c >>= 1) ++e;
        if (na == 0 && nb == 0) return 0;
        for (; nb < 0x40000000 && na < 0x40000000; na = wshl(na, 1)) nb = wshl(nb, 1);
        if (e > 0) { const int u = shad(nb, -(e - 1)); return b060(na, (u & 1) + (u >> 1)); }
        if (e < 0) { const int u = shad(na, e + 1); na = (u & 1) + (u >> 1); }
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
    // The three readers 0x8951dd70 chooses by mode.  In mono (2) they come to the same
    // values.  In stereo (3) each gives its own, which round differently:
    //   the reader 0x8a015478, for the filter and the crossing scan: 0x8951f5b0, or
    //     0x8951f5e0, 0.5*(L+R) in float.  0 outside the wave;
    //   the samples 0x8a015470: 0x8951f6e0, or 0x8951f6f0, (L+R) >> 1, the floor;
    //   the filter 0x8a015474, unchecked: 0x8951f700, or 0x8951f740, 0.5*(FIR(L)+FIR(R)).
    float fetch(int i) const {
        if (i < 0 || i >= n_) return 0.0f;
        return stereo_ ? ((float)x_[2 * i] + (float)x_[2 * i + 1]) * 0.5f : (float)x_[i];
    }
    int sample(int i) const {
        if (i < 0 || i >= n_) return 0;                    // the firmware reads on: zeros in xtFwSoloEncode
        return stereo_ ? ((int)x_[2 * i] + (int)x_[2 * i + 1]) >> 1 : x_[i];
    }
    void designFir();                                     // rom.2 0x8951f380
    float decAtF(int s) const {                           // rom.2 0x8951f680 (float), through the reader
        float acc = c_[0] * fetch(s);
        for (int k = 1; k < 64; ++k) acc += c_[k] * (fetch(s - k) + fetch(s + k));
        return acc;
    }
    float firChannel(int s, int c) const {                // rom.2 0x8951f700 with the stride of a stereo wave
        auto at = [this, c](int i) { return (i >= 0 && i < n_) ? (float)x_[2 * i + c] : 0.0f; };
        float acc = c_[0] * at(s);
        for (int k = 1; k < 64; ++k) acc += c_[k] * (at(s - k) + at(s + k));
        return acc;
    }
    float decFast(int s) const {                          // rom.2 0x8951f700 / 0x8951f740
        return stereo_ ? (firChannel(s, 0) + firChannel(s, 1)) * 0.5f : decAtF(s);
    }
    void setDec(int j, int s) { setF(kDec + j * 4, decAtF(s)); }
    void fill(int b, int to, bool zeroTail, bool fast = false);   // 0x10008a50 / rom.2 0x8951f790; fast: 0x8951f880
    int shift(int pos, int p3);                           // 0x1000a5b0
    int nextEvent();                                      // 0x10008210

    // D/E sums (rom.2 0x89521a00 / 0x89521a50): each float read through FTRC.
    // p is a byte offset in the work memory.
    P48 sumD(int p, int lag, int len) const {
        P48 r; for (int i = 0; i < len; ++i) { const uint32_t d = (uint32_t)ftrc(flt(p + (i - lag) * 4)) - (uint32_t)ftrc(flt(p + i * 4));
            const uint32_t u = r.lo + d * d; r.lo = u & 0xffff; r.hi += u >> 16; } return r;
    }
    P48 sumE(int p, int len) const {
        P48 r; for (int i = 0; i < len; ++i) { const uint32_t v = (uint32_t)ftrc(flt(p + i * 4)), u = r.lo + v * v;
            r.lo = u & 0xffff; r.hi += u >> 16; } return r;
    }
    // rom.2 0x89521a80 / 0x89521900: on the full-rate int16 samples.
    int a100(int fp, int lag, int len) const {
        P48 S, D;
        for (int i = 0; i < len; ++i) {
            const uint32_t a = (uint32_t)full(fp + i - lag), b = (uint32_t)full(fp + i), s = a + b, d = a - b;
            const uint32_t u5 = S.lo + s * s;
            const uint32_t u7 = D.lo + d * d;
            D.lo = u7 & 0xffff; D.hi += u7 >> 16;
            S.hi += u5 >> 16; S.lo = u5 & 0xffff;
        }
        const P48 den = ae80(S, D);
        if (den.lo == 0 && den.hi == 0) return 0x8000;
        return af00(D, den, 0x8000);
    }
    // rom.2 0x89521bb0 / 0x89521970: on the FLOAT decimated values, squares
    // in float (wrapped at 2^32 like a 32-bit product, then truncated).
    int a260(int dp, int lag, int len) const {
        const int p = kDec + dp * 4;
        const int lg = lag >> 2, n = len >> 2;
        P48 S, D;
        for (int i = 0; i < n; ++i) {
            const float x = flt(p + (i - lg) * 4), y = flt(p + i * 4);
            float s2 = x + y; s2 = s2 * s2;
            float d2 = x - y;
            if (2.1474836e+09f <= s2) s2 = s2 - 4.2949673e+09f;
            d2 = d2 * d2;
            if (2.1474836e+09f <= d2) d2 = d2 - 4.2949673e+09f;
            const uint32_t id = (uint32_t)ftrc(d2), is = (uint32_t)ftrc(s2);
            D.hi += (D.lo + id) >> 16; D.lo = (D.lo + id) & 0xffff;
            S.hi += (S.lo + is) >> 16; S.lo = (S.lo + is) & 0xffff;
        }
        const P48 den = ae80(S, D);
        if (den.lo == 0 && den.hi == 0) return 0x8000;
        return af00(D, den, 0x8000);
    }
    static int a370(int p1, int p2, int p3, int* out);
    static int a3f0(P48& p1, P48& p2, P48& p3);
    int s94a0(int buf, int p, int len, P48& D, P48& E, bool fromTracker) const;

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
    int crossing(int pos, int dir, int period) const;      // rom.2 0x895214b0 (XT only)
    int gapFill(Node* p);                                  // 0x10009970
    float drift(const Node* a, const Node* b);             // rom.2 0x89520d30 (XT only)
    void spreadDrift(Node* seg);                           // rom.2 0x89520f30 (XT only)
    void lagTable();                                       // 0x10007be0

    // ── unvoiced fills ─────────────────────────────────────────────────
    int f9c20(int a, int b, int step, int low);
    int f9cf0(int a, int b, int pa, int pb);
    int f9e10(int a, int b, int pa, int pb);
    static int f9de0(int p, int ref) { const int d = wshl(p, 1); if (ref <= d) { if (wshl(ref, 1) < p) return p >> 1; return 0; } return d; }

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
    const int16_t* x_; int n_; bool stereo_;   // n_ frames; stereo_: x_ interleaved
    std::vector<uint64_t>* memTrace_;          // xtSoloEncode's trace
    void traceWork(int kind) { if (memTrace_) memTrace_->push_back(xtEncHash(kind, mem_.data() + kFull, 0xc000)); }
    int rd_ = 0;                         // DAT_1002b800 input read index
    std::vector<int> ev_; int evi_ = 0;  // events, DAT_100227c4
    int minLag_ = 0, maxLag_ = 0;        // 227d4 / 227d8
    int pmin_ = 22;                      // rom.2 0x89ff10b8
    int onsetThr_ = 0, voiceThr_ = 0, oruThr_ = 0, gate_ = 0, hop_ = 0;
    int unvRec_ = 0, unvStep_ = 0, unvLow_ = 0, lo_ = 0x5999, hi_ = 0xa666;   // b850, 227a0, b854
    std::vector<int> lags_;              // 226a0
    float c_[64] = {};
    // rom.2's work buffers lie next to each other: the full-rate samples (int16,
    // 0x8a0090f4), the decimated signal (float, 0x8a00f0f4), the scratch (float,
    // 0x8a0120f4, used from its middle).  The searches read past their ends: above
    // the decimated signal they read the scratch, below it the samples taken as
    // floats.  So one block here too; outside it (rom.2's variables) reads as 0.
    static constexpr int kPad = 0x4000, kFull = kPad, kDec = kFull + kBuf * 2, kScr = kDec + kBuf + 0x1800;
    std::vector<uint8_t> mem_ = std::vector<uint8_t>(kScr + 0x1800 + kPad, 0);
    float flt(int o) const {
        float v = 0.0f;
        if (o >= 0 && (size_t)o + 4 <= mem_.size()) std::memcpy(&v, mem_.data() + o, 4);
        return v;
    }
    void setF(int o, float v) { if (o >= 0 && (size_t)o + 4 <= mem_.size()) std::memcpy(mem_.data() + o, &v, 4); }
    int full(int i) const {
        int16_t v = 0;
        const int o = kFull + i * 2;
        if (o >= 0 && (size_t)o + 2 <= mem_.size()) std::memcpy(&v, mem_.data() + o, 2);
        return v;
    }
    void setFull(int i, int x) { const int16_t v = (int16_t)x; std::memcpy(mem_.data() + kFull + i * 2, &v, 2); }
    std::vector<float> driftBuf_ = std::vector<float>(0x6000 + 2 * kPad / 4, 0.0f);   // rom.2 0x89ff10c8: two of 0x3000, kept between calls
    int off_ = 0;                        // 227ec
    int onsetPos_ = 0, cur_ = 0, back_ = 0, evPos_ = 0, state_ = 0;   // 227e0 227e8 227e4 227dc 227c8
    int fp_ = 0, dp_ = 0;                // 227f4 227f0
    int runEnd_ = 0, runStart_ = 0;      // 2269c 22698
    int period_ = 0;                     // 227c0
    bool started_ = false;               // rom.2's pointer 0x8987caa8 is set
    std::deque<Node> pool_;
    int shifts_ = 0;
    FILE* trace_ = xtSwitch("XT_TRACE") ? std::fopen(xtSwitch("XT_TRACE"), "w") : nullptr;
    Node* head_ = nullptr; Node* tail_ = nullptr;
};

// rom.2 0x8951f380: low-pass table ⊛ Kaiser-windowed high-pass, 64 taps (half).
void Enc::designFir() {
    // The 19-entry low-pass table at rom.2 0x897ee1e4, read from the user's
    // own firmware image (xtLoadTables) — not copied into this source.
    const float* h = g_lp;
    const float fc = ((float)pmin_ + 307.94998f) / 44100.0f;
    float hp[46];
    hp[0] = 1.0f - fc * 2.0f;
    float k = 1.0f; int k2 = 1, odd = 1;
    for (int i = 1; i <= 45; ++i) {
        float t = (std::sqrt((float)(0x7e9 - k2) / 2025.0f) * 1.5064099f) / 3.75f;
        t = t * t;
        float u = k * fc, sg = 1.0f;
        if (u < 0.0f) { u = -u; sg = -1.0f; }
        u = u - (float)ftrc(u);
        if (0.5f < u) { u = 1.0f - u; sg = -sg; }
        if (0.25f < u) u = 0.5f - u;
        odd += 2; k2 += odd;
        const float i0 = (((((t * 0.0045812996f + 0.0360768f) * t + 0.26597318f) * t + 1.2067491f) * t
                           + 3.0899422f) * t + 3.5156229f) * t * 0.60494757f + 0.60494757f;
        const float u2 = u * u;
        hp[i] = ((6.281732f - (41.131832f - u2 * 74.14366f) * u2) * u * sg * -i0) / (k * 3.1415925f);
        k += 1.0f;
    }
    for (int j = 0; j < 64; ++j) {
        float s = 0.0f;
        for (int m = 18; m >= 1; --m) { const int a = std::abs(j + m); s = (m < 19 ? h[m] : 0.0f) * (a < 46 ? hp[a] : 0.0f) + s; }
        for (int m = 0; m < 19; ++m) { const int a = std::abs(j - m); s = h[m] * (a < 46 ? hp[a] : 0.0f) + s; }
        c_[j] = s;
    }
}

// 0x10008a50 / rom.2 0x8951f790: full-rate copy + decimated every 4th.  fast is
// 0x8951f880, the same through the other filter: the fills after a shift away from the end.
void Enc::fill(int b, int to, bool zeroTail, bool fast) {
    int u = b & 3, j = b >> 2;
    if (u == 0) u = 4;
    for (; b < to; ++b, ++u, ++rd_) {
        setFull(b, sample(rd_));
        if (u > 3) { u = 0; if (fast) setF(kDec + j++ * 4, decFast(b + off_)); else setDec(j++, b + off_); }
    }
    if (zeroTail && b < kBuf) {
        setFull(b, 0);
        if (u > 3) setDec(j, b + off_);
        for (++b, ++rd_; b < kBuf; ++b, ++rd_) setFull(b, 0);
    }
    traceWork(0);
}

int Enc::nextEvent() {                                     // 0x10008210
    const int last = (int)ev_.size() - 1;
    if (evi_ > last) evi_ = last;
    while (evi_ < last && ev_[evi_] < 0) ++evi_;
    return buf16(ev_[evi_]);
}

int Enc::shift(int pos, int p3) {                          // 0x1000a5b0
    if (const char* dd = xtSwitch("XT_DUMPDEC"); dd && shifts_ < 3) {   // diagnostic
        char fn[512]; std::snprintf(fn, sizeof fn, "%s.dec%d", dd, shifts_);
        if (FILE* f = std::fopen(fn, "wb")) { std::fwrite(mem_.data() + kDec, 4, 3072, f); std::fclose(f); }
    }
    ++shifts_;
    if (state_ == 1) { state_ = 2; return -1; }
    int u6 = (kBuf - maxLag_ < (pos >> 4)) ? kBuf - 3 * maxLag_ : (pos - maxLag_ * 32) >> 4;
    const int u7 = u6 & ~3;
    const int cnt = kBuf - u7;
    for (int i = 0, k = 0, c = 4; i < cnt; ++i, ++c) {
        setFull(i, full(i + u7));
        if (c > 3) { setF(kDec + k * 4, flt(kDec + (k + (u6 >> 2)) * 4)); ++k; c = 0; }
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
    } else fill(cnt, kBuf, false, true);
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
    const int o = b060(wmul(p1 - p3, 0x10000), den * 2);
    const int v = p2 - (wmul(o, p1 - p3) >> 0x12);
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

// 0x100094a0 / rom.2 0x8951f8f0: lag search on a decimated buffer in steps of 4
// samples.  The XT searches 12/16 to 21/16 of the lag when the caller made E (the
// tracker, the octave-down run), 15/16 to 17/16 when it did not (the octave votes).
int Enc::s94a0(int buf, int p, int len, P48& D, P48& E, bool fromTracker) const {
    int u5 = (p + 2) & ~3;
    const int above = (u5 * (fromTracker ? 21 : 17)) >> 4, below = (u5 * (fromTracker ? 12 : 15)) >> 4;
    const int wl = (len + 2) >> 2;
    D = sumD(buf, (p + 2) >> 2, wl);
    if (fromTracker) E = sumE(buf, (p + 2) >> 2);
    int u1 = u5;
    P48 T;
    bool down = false;
    while (!down) {
        while (true) {
            u1 += 4;
            if (above <= u1) { down = true; break; }
            T = sumD(buf, u1 >> 2, wl);
            if (le(D, T)) break;
            D = T; p = u1;
        }
        if (down) break;
        if (!(T.hi * 2 <= E.hi)) break;
    }
    while (true) {
        u5 -= 4;
        if (u5 < below || u5 < minLag_) break;
        T = sumD(buf, u5 >> 2, wl);
        if (le(D, T)) { if (E.hi < T.hi * 2) break; continue; }
        D = T; p = u5;
    }
    if (p < minLag_) p = minLag_;
    P48 Dm = sumD(buf, (p - 4) >> 2, wl), Dp = sumD(buf, (p + 4) >> 2, wl);
    const int o = a3f0(Dm, D, Dp);
    return wadd(o, wmul(p, 0x4000)) * 4;
}

int Enc::tracker(int pos, int* period) {                   // 0x1000aac0
    if (trace_) std::fprintf(trace_, "tracker %d %d onset=%d cur=%d back=%d off=%d\n", pos, *period, onsetPos_, cur_, back_, off_);
    const int u6 = (*period + 0x8000) >> 16;
    fp_ = (pos + 8) >> 4;
    const int L = (*period + 0x8000) >> 0x12;
    dp_ = (pos + 8) >> 6;
    const int d = kDec + dp_ * 4;
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
    const int p = fp_ - maxLag_;
    if (evPos_ + (maxLag_ + 1) * -0x10 < onsetPos_) return false;
    const int len = maxLag_ * 2;
    int s = 0;
    for (int i = 0; i < len; ++i) {
        const int x = full(p + i), v = x * x;
        s = wadd(s, ((v >> 7) & 1) + (v >> 8));            // a loud wave wraps here and takes the test below
        if (s < 0) { s = 0x7fffffff; break; }
    }
    return s <= gate_ * len;
}

// rom.2 0x8951e900 (the XT's rule; vpenc 0x10008480 used the strongest P/4
// energy window instead): the largest |sample| of the band-passed decimated
// signal over one period, then the nearer zero crossing (ties forward); the
// mark is the first decimated sample past it, ×4, wrapped into [0, P).
int Enc::firstMark(int P) const {
    auto d = [this](int i) { return flt(kDec + (dp_ + i) * 4); };
    const int m = P >> 2;
    float best = -1e10f; int i5 = -1;
    for (int i = 0; i < m; ++i) { const float a = std::fabs(d(i)); if (best < a) { best = a; i5 = i; } }
    if (i5 < 0) return 0;
    const float sg = (0.0f < d(i5)) ? 1.0f : -1.0f;
    int fwd = 0, bwd = 0;
    if (0.0f < sg * d(i5)) {
        do { ++fwd; } while (0.0f < d(i5 + fwd) * sg);
        do { --bwd; } while (0.0f < d(i5 + bwd) * sg);
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
        started_ = true;
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
                else f9cf0(abs16(cur_), a, wshl(tail_->pos - tail_->prev->pos, 12), 0);
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

// rom.2 0x8951fab0.  Every 4th sample through the unchecked filter; within the filter's
// reach of either end of the wave the slower path takes every sample through the reader
// instead, over the same count.
void Enc::scratchFill(int pos, int span) {
    const int u6 = ((span * 9) >> 3) & ~3, h = (span * 9) >> 5;
    const bool nearEnd = pos < u6 + 0x3f || n_ <= pos + u6 + 0x3f;
    for (int j = 0; j < 2 * h; ++j)
        setF(kScr + (j - h) * 4, nearEnd ? decAtF(pos - u6 + j) : decFast(pos - u6 + 4 * j));
    traceWork(1);
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
        P48 E = sumE(kScr, P >> 2), D2, Dk[6], DP;
        s94a0(kScr, P2, P, D2, E, false);
        for (int k = 2; k <= kmax; ++k) {
            const int u = ((b060(iv, k) >> 4) + 2) & ~3;
            if (u < minLag_) { kmax = k - 1; break; }
            s94a0(kScr, u, P, Dk[k - 2], E, false);
        }
        const int q1 = a350(ff0(nd->next->v)), q0 = a350(ff0(nd->v));
        if (q1 < thr || q0 < thr) {
            ++checks;
            s94a0(kScr, P, P, DP, E, false);
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
            return wshl(first->next->pos - first->pos, 12);
        }
    }
    if (merges < 9) return wshl(first->next->pos - first->pos, 12);
    // octave down: drop every other mark
    Node* p4 = first->next;
    int ret = p4->pos - first->pos;
    bool viaGoto = false;
    while (p4 != last) {
        Node* nx = p4->next;
        unlink(p4);
        if (nx == last) {
            cur_ = buf16(last->pos * 2 - last->prev->pos);
            if (p4 != last) { ret = wshl(first->next->pos - first->pos, 12); viaGoto = true; }
            break;
        }
        p4 = nx->next;
        ret = p4->pos - nx->pos;
    }
    if (!viaGoto) { unlink(p4); ret = wshl(ret, 12); }
    if (first == tail_) return ret;
    int pr = first->next->pos - first->pos, at = first->pos - pr;
    bool skipTail = false;
    while (runEnd_ < at) {
        const int ps = pr >> 4, as = at >> 4;
        if (as <= ps * 3 + 4) { if (runEnd_ < at && ps * 3 + 4 < as) skipTail = true; break; }
        scratchFill(as, ((ps * 2 - 1) & ~3) + 4);
        P48 D, E;
        ret = s94a0(kScr, ps, ps, D, E, true);
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
    if (first != tail_) ret = wshl(first->next->pos - first->pos, 12);
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
float Enc::drift(const Node* A, const Node* B) {
    const int P = (B->prev ? B->pos - B->prev->pos : B->next->pos - B->pos) >> 4;
    const int ia = A->pos >> 4, ib = B->pos >> 4;
    const float fa = (float)(A->pos - ia * 16) * 0.0625f, fb = (float)(B->pos - ib * 16) * 0.0625f;
    // Each centred on P in its half of the buffer, which keeps what earlier calls
    // wrote: a scan that finds no crossing within P reads those.  Just under the
    // buffer is the list head (0x89ff10c4), a pointer: as a float tiny and negative.
    // Under that come small integers, which the FPU takes as 0.
    const int pad = kPad / 4, half = 0x3000, size = (int)driftBuf_.size();
    float* buf = driftBuf_.data();
    buf[pad - 1] = -4e-31f;
    auto put = [&](int i, float v) { if (i >= 0 && i < size) buf[i] = v; };
    for (int k = -P; k < P; ++k) { put(pad + P + k, decAtF(ia + k)); put(pad + half + P + k, decAtF(ib + k)); }
    auto a = [&](int k) { const int i = pad + P + k; return (i >= 0 && i < size) ? buf[i] : 0.0f; };
    auto b = [&](int k) { const int i = pad + half + P + k; return (i >= 0 && i < size) ? buf[i] : 0.0f; };
    const float sg = (0.0f < a(0)) ? 1.0f : -1.0f;
    int r = 0;
    if (0.0f < sg * a(0)) { do { ++r; } while (0.0f < a(r) * sg && r < size); }
    float cA = a(r - 1) / (a(r - 1) - a(r));
    cA = cA + (float)r; cA = cA + -1.0f;
    int rb = 0;
    if (0.0f < sg * a(0)) { do { --rb; } while (0.0f < a(rb) * sg && -rb < size); }
    float cAb = (float)rb - a(rb + 1) / (a(rb + 1) - a(rb));
    cAb = cAb + 1.0f;
    float dir;
    if (0.0f < cA + cAb) { cA = cAb; dir = sg; } else dir = -sg;
    const float sgB = (0.0f < b(0)) ? 1.0f : -1.0f;
    const float prod = dir * sgB, s8 = sgB * b(0);
    int q = 0; float cB;
    if (prod < 0.0f) {
        if (0.0f < s8) { do { ++q; } while (0.0f < b(q) * sgB && q < size); }
        cB = b(q - 1) / (b(q - 1) - b(q)); cB = cB + (float)q; cB = cB + -1.0f;
    } else {
        if (0.0f < s8) { do { --q; } while (0.0f < b(q) * sgB && -q < size); }
        cB = (float)q - b(q + 1) / (b(q + 1) - b(q)); cB = cB + 1.0f;
    }
    if (memTrace_) memTrace_->push_back(xtEncHash(2, buf + pad, 0x18000));
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
    // A mark in a run of exact zeros gives 0/0, FTRC makes that 2^31, and the firmware's
    // marks go with it: it then reads outside memory (no result to match).  None here.
    if (!std::isfinite(d)) return;
    float k = 1.0f;
    for (Node* n = seg->next; n; n = n->next, k = k + 1.0f) {
        const float x = (k * d) / (float)count;
        int r;
        if (0.0f < x) r = ftrc(x * 16.0f + 0.5f); else r = wsub(0, ftrc(-x * 16.0f + 0.5f));
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

// rom.2 0x895214b0 (XT only; vpenc steps by the period): from a mark, one whole
// cycle of the wave's own samples in direction dir, up to 882 samples.  What a fill
// steps by: the period, or that cycle if it came out longer.
int Enc::crossing(int pos, int dir, int period) const {
    int i = (pos + 8) >> 4, steps = 0;
    auto step = [&]() {                                    // 0x89521470 clamps
        i += dir;
        if (i < 0) { i = 0; return false; }
        if (n_ - 1 < i) { i = n_ - 1; return false; }
        ++steps;
        return true;
    };
    auto at = [&]() { return ftrc(fetch(i)); };            // through FTRC: in stereo it truncates where the samples floor
    const int sg = at() >= 0 ? 1 : -1;
    while (steps < 882 && step() && at() * sg >= 0) {}
    if (at() * sg <= 0) while (steps < 882 && step() && at() * sg <= 0) {}
    if (wshl(steps, 16) <= period || steps >= 882) return period;
    const int d = wshl(i * 16 - pos, 12);
    return d < 0 ? wsub(0, d) : d;
}

// 0x10009cf0 / rom.2 0x89521750.  The XT's own limit (vpenc: 16000) and steps.  A
// negative period, which the firmware would never finish on, ends here in the plain fill.
int Enc::f9cf0(int a, int b, int pa, int pb) {
    while (true) {
        const int span = b - a;
        if (span < 0x44e0 || span < wshl(wadd(pa, pb) >> 12, 2)) return f9e10(a, b, pa, pb);
        if (pa > 0) { insert(a, lo_, 7); const int p = crossing(a, 1, pa); a += p >> 12; pa = f9de0(p, unvRec_); }
        if (pb > 0) { insert(b, lo_, 7); const int p = crossing(b, -1, pb); b -= p >> 12; pb = f9de0(p, unvRec_); }
        if (pa <= 0 && pb <= 0) break;
    }
    return f9c20(a, b, unvStep_, unvLow_);
}

// 0x10009e10 / rom.2 0x895215a0, which recurses where this loops: A, B the periods
// on entry and, once stepped, what the crossings made of them; na, nb the next ones.
int Enc::f9e10(int a, int b, int pa, int pb) {
    while (true) {
        int g = wadd(pa, pb) >> 0xd, A = pa, B = pb, na = pa, nb = pb;
        if (b - a < wshl(g, 2)) {
            if (A == 0 || B == 0) g = wshl(g, 1);
            if (g != 0) return f9c20(a, b, g, 7);
            return f9c20(a, b, unvStep_, unvLow_);
        }
        if (A > 0) {
            insert(a, lo_, 7);
            A = crossing(a, 1, A);
            a += A >> 12;
            if (b < a) a = b;
            na = f9de0(A, B);
        }
        if (B > 0) {
            if (a < b) {
                insert(b, lo_, 7);
                B = crossing(b, -1, B);
                b -= B >> 12;
                if (b < a) b = a;
                nb = f9de0(B, A);
            } else if (na != 0 && nb != 0) { pa = na; pb = nb; continue; }
            else return f9c20(a, b, (A == 0 ? B : wadd(A, B) >> 1) >> 12, 7);
        }
        if (na == 0 || nb == 0) {
            int s = wadd(A, B) >> 1;
            if (A == 0) s = B;
            if (B == 0) s = A;
            return f9c20(a, b, s >> 12, 7);
        }
        if (na < 0 && nb < 0) return f9c20(a, b, unvStep_, unvLow_);
        pa = na; pb = nb;
    }
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
    pmin_ = 22;
    maxLag_ = std::min(0x7ff, b060(44100, pmin_));
    if ((unsigned)n_ < (unsigned)maxLag_) {                // rom.2 0x8951dd70: under the longest lag, half the wave is it
        pmin_ = b060(44100, n_ >> 1);
        maxLag_ = b060(44100, pmin_);
    }
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
    xtSwitchDump("XT_DUMPFIR", c_, sizeof c_);

    // main loop (0x10007c70)
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
        if (!started_) return {};            // rom.2 0x8951e288: past the end at the first hop (n 2, 2004, 2005): -2, no list
        if (period_ == -0xdd) break;
        // zero crossings over the first period of the decimated signal
        const int span = ((((period_ >> 16) - 1) & ~3) + 4) >> 2;
        int zc = 0;
        float sg = flt(kDec + dp_ * 4) < 0.0f ? -1.0f : 1.0f;
        for (int k = 1; k < span; ++k) if (flt(kDec + (dp_ + k) * 4) * sg < 0.0f) { ++zc; sg = -sg; }
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
                if (!before || !before->prev || (pa = wshl(before->pos - before->prev->pos, 12), wshl(maxLag_, 16) < pa)) pa = 0;
                f9cf0(runEnd_, runStart_, pa, r);
                nextOnset = cur_;
            }
        }
    }
    // finalize (0x100080fe)
    const int endBuf = (n_ - off_) * 16;
    if (cur_ < endBuf) {
        int pa = unvRec_, pb = unvRec_;
        if (tail_ && tail_->prev && tail_->low == 7) { pa = wshl(tail_->pos - tail_->prev->pos, 12); pb = 0; }
        f9cf0(abs16(cur_), abs16(endBuf), pa, pb);
    }
    if (head_ && 0 < head_->pos) insert(0, 0, head_->low);
    const int last = ev_.back() & 0x7fffffff;
    if (tail_ && tail_->pos < last) {                       // rom.2 0x8951e880: one more period if it fits
        if (tail_->prev && 2 * tail_->pos - tail_->prev->pos < last) insert(2 * tail_->pos - tail_->prev->pos, 0, head_->low);
        insert(last, 0, unvLow_);
    }

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

std::vector<XtRecord> xtSoloEncode(const int16_t* x, int n, int channels, std::vector<uint64_t>* trace) {
    if (!x || n <= 0 || !g_lpLoaded || (channels != 1 && channels != 2)) return {};
    // The SH-4 runs with FPSCR RM = 1 (round to zero).  XT_ROUND=nearest
    // gives the P-code emulator's rounding instead (a harness's switch).
    const bool rtz = !xtSwitchNearest();
    const int saved = std::fegetround();
    if (rtz) std::fesetround(FE_TOWARDZERO);
    Enc e(x, n, channels, trace);
    std::vector<XtRecord> out = e.run();
    std::fesetround(saved);
    return out;
}

#ifdef XT_ENCODER_MAIN
#include "XtFirmware.h"
#include <cstdio>
#include <string>
// xt_enc <rom.2.bin> <samp.bin (int16 LE)> [stereo] : prints "start len info"
// (1/16 sample).  "stereo": interleaved L/R, in the XT's stereo mode.
// xt_enc <rom.2.bin> cmp <samp.bin[:2]> ... : against the firmware's encoder (rom.2
// run in the interpreter), every record of each wave; :2 is stereo, the firmware's
// mode 3.  A wave the firmware has no result for (it reads outside memory: see
// spreadDrift) is said so, and the port's records still have to tile it.  Records can
// hide a filter's rounding, so its memory is compared too (xtSoloEncode's trace): said
// only when it differs.
static std::vector<int16_t> readWave(const char* path, int channels) {
    std::vector<int16_t> x;
    FILE* f = std::fopen(path, "rb");
    if (!f) return x;
    int16_t s[2];
    while (std::fread(s, 2, (size_t)channels, f) == (size_t)channels) x.insert(x.end(), s, s + channels);
    std::fclose(f);
    return x;
}
// From 0 with no gap, nothing going backward, up to the end of the wave at least.
static bool tiles(const std::vector<XtRecord>& r, int n) {
    if (r.empty()) return true;
    if (r[0].start16 != 0) return false;
    for (size_t i = 0; i < r.size(); ++i) {
        if (r[i].len16 < 0) return false;
        if (i + 1 < r.size() && r[i + 1].start16 != r[i].start16 + r[i].len16) return false;
    }
    return r.back().start16 + r.back().len16 >= n * 16;
}
static void compareWave(const std::string& spec) {
    const size_t colon = spec.rfind(":2");
    const bool stereo = colon != std::string::npos && colon + 2 == spec.size();
    const std::string path = stereo ? spec.substr(0, colon) : spec;
    const int channels = stereo ? 2 : 1;
    const std::vector<int16_t> x = readWave(path.c_str(), channels);
    if (x.empty()) { std::printf("enc %s: cannot read\n", spec.c_str()); return; }
    const int frames = (int)x.size() / channels;
    std::vector<uint64_t> portMem, fwMem;
    const std::vector<XtRecord> port = xtSoloEncode(x.data(), frames, channels, &portMem);
    std::vector<XtRecord> fw;
    const char* tile = tiles(port, frames) ? "" : ", PORT DOES NOT TILE THE WAVE";
    if (!xtFwSoloEncode(x.data(), frames, fw, channels, &fwMem)) {
        std::printf("enc %s: firmware has no result, port %zu records%s\n", spec.c_str(), port.size(), tile);
        return;
    }
    size_t diff = 0, first = port.size();
    const size_t both = std::min(port.size(), fw.size());
    for (size_t i = both; i-- > 0;)
        if (port[i].start16 != fw[i].start16 || port[i].len16 != fw[i].len16 || port[i].info != fw[i].info) { ++diff; first = i; }
    if (diff == 0 && port.size() == fw.size()) {
        size_t step = 0;
        while (step < portMem.size() && step < fwMem.size() && portMem[step] == fwMem[step]) ++step;
        std::string mem;
        if (step < portMem.size() || step < fwMem.size())
            mem = ", MEMORY DIFFERENT from step " + std::to_string(step) + " (port " + std::to_string(portMem.size()) + " steps, firmware " + std::to_string(fwMem.size()) + ")";
        std::printf("enc %s: %zu records identical%s%s\n", spec.c_str(), fw.size(), mem.c_str(), tile);
        return;
    }
    if (first == port.size()) first = both;
    std::printf("enc %s: port %zu, firmware %zu records, %zu different, first at %zu", spec.c_str(), port.size(), fw.size(), diff, first);
    if (first < both)
        std::printf(": port %d %d 0x%x, firmware %d %d 0x%x", port[first].start16, port[first].len16, port[first].info, fw[first].start16, fw[first].len16, fw[first].info);
    std::printf("%s\n", tile);
}
int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s V-SynthXT.rom.2.bin samp.bin [stereo] | cmp samp.bin[:2] ...\n", argv[0]); return 1; }
    if (!xtLoadTables(argv[1])) { std::fprintf(stderr, "cannot read the low-pass table from %s\n", argv[1]); return 1; }
    if (std::string(argv[2]) == "cmp") {
        if (!xtFwOpen(argv[1])) return 1;
        for (int a = 3; a < argc; ++a) { compareWave(argv[a]); std::fflush(stdout); }
        return 0;
    }
    const int channels = argc > 3 && std::string(argv[3]) == "stereo" ? 2 : 1;
    const std::vector<int16_t> x = readWave(argv[2], channels);
    if (x.empty()) return 1;
    for (const XtRecord& r : xtSoloEncode(x.data(), (int)x.size() / channels, channels))
        std::printf("%d\t%d\t%d\n", r.start16, r.len16, r.info);
    return 0;
}
#endif
