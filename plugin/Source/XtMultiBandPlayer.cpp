// The V-Synth XT's multi-band VariPhrase players, ported from rom.2.
//
// BACKING (encode type 3): band 0 at the full rate and five decimated bands
// (×4 … ×64), each read as grains that re-trigger at the encoder's segment
// starts and follow the per-record repeat budget, then recombined by a tree
// of ×2 recursive interpolators.  Routines, in the order a block runs them:
//   0x8950e6e0 block: 0x8950c280 (walk step), 0x8950e760 (band 0),
//              0x8950eab0 ×5 (bands 1..5), 0x8950c570 (tree + output gain)
//   0x8950e760 / 0x8950e860: the 16-entry walk-position list (two output
//              samples per entry); 0x8950e980 linear band-0 reads
//   0x8950eab0 / 0x8950eb60: band reads with the 64×6 kernel and a one-pole
//              smoother; 0x8950f400 the grain stepper (repeat budget, cost
//              0x8950f620); records 0x8950f5a0, segments 0x8950f6a0
//   0x8950c650 / 0x8950c720: the recombination tree
// Note-on: 0x89510980 → 0x8950fa60 → 0x8950f6f0 (records), 0x8950f860.
//
// ENSEMBLE (encode type 5): band 0 as in BACKING; bands 1..5 are six cosine
// oscillators each, driven by the load-time frames (12 int16 per channel:
// six phase/amplitude pairs), into the same recombination tree.
//   0x8950db10 block: 0x8950c280, 0x8950e760, 0x8950dbe0 ×5, 0x8950c570
//   0x8950dbe0: frame index and fraction from the walk position;
//              0x8950dcb0 loads the next frame as {delta, base} pairs;
//              0x8950dc50 the oscillators (two output samples)
// Note-on: 0x89510130 → 0x89510b20 (0x8950fa60, band 0 only).
//
// Arithmetic follows the SH-4 code operation by operation (float, no
// contraction; fmac as a product then a sum, as the P-code emulator runs it;
// fipr as a double sum rounded once).  Validated against
// research/ghidra_scripts/EmuBacking.java.  Ported: forward playback from the
// start event with no loop points and default voice flags.  Not ported:
// reverse playback, loops (0x8950c880 / 0x8950ca70 at the bounds), event
// seeks (0x89510bf0), the voice flags 0x2 / 0x4 / 0x8 / 0x20 and the cache
// bounds in the loop struct.
#include "XtMultiBandPlayer.h"

#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON

namespace {

constexpr uint32_t kRomBase = 0x89300000;
// rom.2 table addresses
constexpr uint32_t kKernel = 0x8950ee00;   // 64 phases × 6 taps (float)
constexpr uint32_t kTree   = 0x8950c7a0;   // 7 floats: the ×2 stages
constexpr uint32_t kFinal  = 0x8950c7bc;   // 3 floats: the last stage
constexpr uint32_t kFShift = 0x897edf84;   // u16 ×6: fraction shift
constexpr uint32_t kSShift = 0x897edf90;   // u16 ×6: read-step shift
constexpr uint32_t kOffset = 0x897edf9c;   // u32 ×6: read start offset (1/16 sample)
constexpr uint32_t kMask   = 0x897edfb4;   // u32 ×6: fraction mask
constexpr uint32_t kStride = 0x897edfcc;   // u32 ×6: walk-list stride
constexpr uint32_t kEShift = 0x897edf78;   // s16 ×6: ENSEMBLE frame shift
constexpr uint32_t kOscC   = 0x8950dd9c;   // u32 ×12: oscillator phase constants (bands 1..4, band 5)
constexpr uint32_t kCos    = 0x8950ddd0;   // 256 floats: cosine table
// load-time filter bank: half-kernels {float length, centre tap, taps…}
constexpr uint32_t kDec4 = 0x897ee24c, kDec2 = 0x897ee39c, kInt4 = 0x897ee44c, kInt2 = 0x897ee4fc;

struct Tables {
    float kernel[64][6];
    float tree[7], fin[3];
    uint16_t fshift[6], sshift[6];
    uint32_t offset[6], mask[6], stride[6];
    int16_t eshift[6];
    uint32_t oscC[12];
    float cosTab[256];
    std::vector<float> dec4, dec2, int4, int2;
    bool ok = false;
} T;

template <class V> bool readAt(FILE* f, uint32_t addr, V* v, size_t n) {
    return std::fseek(f, (long)(addr - kRomBase), SEEK_SET) == 0 && std::fread(v, sizeof(V), n, f) == n;
}

// SH-4 helpers
inline float fipr(const float* a, const float* b) {   // FIPR: inner product, rounded once
    double s = 0;
    for (int i = 0; i < 4; ++i) s += (double)a[i] * (double)b[i];
    return (float)s;
}
inline int ftrc(float x) {                            // FTRC saturates
    if (!(x == x)) return 0;
    if (x >= 2147483648.0f) return 0x7fffffff;
    if (x < -2147483648.0f) return (int)0x80000000;
    return (int)x;
}

} // namespace

bool xtMbLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    bool ok = readAt(f, kKernel, &T.kernel[0][0], 64 * 6) && readAt(f, kTree, T.tree, 7) && readAt(f, kFinal, T.fin, 3)
           && readAt(f, kFShift, T.fshift, 6) && readAt(f, kSShift, T.sshift, 6) && readAt(f, kOffset, T.offset, 6)
           && readAt(f, kMask, T.mask, 6) && readAt(f, kStride, T.stride, 6) && readAt(f, kEShift, T.eshift, 6)
           && readAt(f, kOscC, T.oscC, 12) && readAt(f, kCos, T.cosTab, 256);
    auto half = [&](uint32_t a, std::vector<float>& v, int lo, int hi) {
        float n = 0;
        if (!readAt(f, a, &n, 1) || !(n >= lo && n <= hi)) return false;
        v.resize((size_t)n);
        return readAt(f, a + 4, v.data(), v.size());
    };
    ok = ok && half(kDec4, T.dec4, 16, 256) && half(kDec2, T.dec2, 8, 128) && half(kInt4, T.int4, 8, 128) && half(kInt2, T.int2, 2, 64);
    std::fclose(f);
    if (!ok) return false;
    // Sanity: each kernel phase is an interpolator (taps sum to about 0.9);
    // masks are 2^k − 1; strides are powers of two up to 16.
    for (int p = 0; p < 64; ++p) {
        float s = 0;
        for (int j = 0; j < 6; ++j) s += T.kernel[p][j];
        if (!(s > 0.8f && s < 1.05f)) return false;
    }
    for (int k = 0; k < 6; ++k) {
        if ((T.mask[k] & (T.mask[k] + 1)) != 0 || T.mask[k] == 0) return false;
        if (T.stride[k] == 0 || T.stride[k] > 16 || (T.stride[k] & (T.stride[k] - 1))) return false;
        if (T.fshift[k] > 24 || T.sshift[k] > 24) return false;
    }
    // the cosine table starts at 1 and is periodic
    if (!(std::fabs(T.cosTab[0] - 1.0f) < 1e-3f && std::fabs(T.cosTab[64]) < 1e-2f && std::fabs(T.cosTab[128] + 1.0f) < 1e-3f)) return false;
    T.ok = true;
    return true;
}

bool xtMbTablesLoaded() { return T.ok; }

struct XtMbPlayer::Band {           // firmware band struct (player +0xe8 + 0x88·k)
    long base = 0;                  // +0x00: data origin, frames (−2 for bands 1..5)
    int shift = 0;                  // +0x04: band index − 1 (position shift)
    int stride = 1;                 // +0x08: walk-list stride
    int fshift = 19, sshift = 21;   // +0x0c / +0x0e
    int budget = 0;                 // +0x10: repeat budget
    int grainStart = 0;             // +0x14
    int nextSeg = 0;                // +0x18: start of the next segment
    long segList = 0;               // +0x1c: next segment-list entry (vpdt offset)
    long nextRecs = 0;              // +0x20: that segment's record stream
    long recPtr = 0;                // +0x24: next record
    int recStart = 0, recEnd = 0;   // +0x28 / +0x2c
    int startOff = 0;               // +0x30: read start offset
    int readPos = 0, readFrac = 0;  // +0x34 / +0x38 (1/16 sample, fraction)
    int mask = 0;                   // +0x3c
    float gain = 1;                 // +0x40
    float lp[2] = { 0, 0 };         // +0x44 / +0x48: smoother state L, R
    // ENSEMBLE bands 1..5 (same struct, other use of the fields)
    int frame = 0;                  // +0x0c: current frame index
    uint32_t packed[2][12] = {};    // +0x10 / +0x4c: {delta, base} per value, L and R
    uint16_t phase[2][6] = {};      // +0x40 / +0x7c: oscillator phases, L and R
};

struct XtMbPlayer::State {
    XtMbWave w;
    int mode = 3;
    bool stereo = false;
    bool rtz = true;
    uint8_t flags0 = 0, flags3 = 0, ended = 0;   // +0x00, +0x03, +0x04
    uint8_t flags5 = 0, flags6 = 0;              // +0x05, +0x06
    int stride8 = 46, off7 = 14, stride9 = 6;    // bytes +0x08, +0x07, +0x09
    int16_t step16 = 0;                          // +0x0a
    uint16_t ev = 0, evLast = 0;                 // +0x0c, +0x0e
    uint16_t evStart = 0, evEnd = 0xffff;        // +0x14, +0x16
    int startPos = 0, endPos = 0;                // +0x18, +0x1c
    int lastEvPos = 0, evPos = 0, nextEvPos = 0; // +0x20, +0x24, +0x28
    int limit = 0;                               // +0x2c
    int rate = 0, walkStep = 0;                  // +0x30, +0x34
    int pos = 0, posFrac = 0;                    // +0x38, +0x3c
    float pitch = 1, walk = 1;                   // +0x70, +0x7c
    float gainTarget = 1.0f / 65536, gain = 1.0f / 65536, gainCoef = 1.0f / 64;  // +0x80..+0x88
    float lpc = 0;                               // +0x94
    float rateF = 1, f9c = 1;                    // +0x98, +0x9c
    int list[16] = {};                           // +0xa8
    Band b[6];                                   // +0xe8 + 0x88·k
    float M[2 * 114] = {};                       // +0x418 … +0x7a8: band outputs and tree state (R at +0x1c8)

    // vpdt access (offsets from the chunk start; out of range reads 0)
    uint32_t rd32(long o) const { if (o < 0 || o + 4 > (long)w.vpdtBytes) return 0; uint32_t v; std::memcpy(&v, w.vpdt + o, 4); return v; }
    int rd16s(long o) const { if (o < 0 || o + 2 > (long)w.vpdtBytes) return 0; int16_t v; std::memcpy(&v, w.vpdt + o, 2); return v; }
    unsigned rd16u(long o) const { return (uint16_t)rd16s(o); }
    long entry(unsigned e) const { return ((long)e * stride8 + off7) * 2; }

    // band sample (frame index relative to the band's data, channel c)
    float smp(int k, long frame, int c) const {
        const XtMbBand& d = w.band[k];
        if (!d.data || frame < d.lo || frame >= d.hi) return 0.0f;
        return (float)d.data[frame * (stereo ? 2 : 1) + c];
    }

    int  segment(Band& B);          // 0x8950f6a0
    int  record(Band& B);           // 0x8950f5a0
    int  cost(const Band& B, int* c) const;   // 0x8950f620
    void step(Band& B, int fshift, int walkPos);   // 0x8950f400
    void initRecords();             // 0x8950f6f0
    void walkList();                // 0x8950e860
    void trigger(Band& B, int walkPos);
    void readBand0(float*& o);      // 0x8950e980
    void readBand(int k, float*& o);// 0x8950eb60
    void stage(int in, int out, int st, int n, const float* c);   // 0x8950c720
    void tree(int X);               // 0x8950c650
    void output(float* out);        // 0x8950c570
    // ENSEMBLE
    int16_t frameVal(int k, long f, int j, int c) const {   // value j of frame f, channel c
        const XtMbBand& d = w.band[k];
        const long i = f * 12 + j;
        if (!d.data || i < d.lo || i >= d.hi) return 0;
        return d.data[i * (stereo ? 2 : 1) + c];
    }
    void loadFrame(Band& B, int k, int f);              // 0x8950dcb0
    void oscillators(Band& B, int c, float frac, const uint32_t* C, float*& o);  // 0x8950dc50
    void ensembleBand(int k, int n, const uint32_t* C, float*& o);              // 0x8950dbe0
};

// 0x8950f6a0: the next segment-list entry {u32 record offset, u32 flags, u32 start}.
int XtMbPlayer::State::segment(Band& B) {
    long p = B.segList;
    uint32_t off = rd32(p), start = rd32(p + 8);
    B.segList = p + 12;
    B.nextRecs = (long)(off & ~1u);
    B.nextSeg = (int)start;
    return (int)start;
}

// 0x8950f5a0: the next record {u16 length (0xffff continues), u16 info}.
int XtMbPlayer::State::record(Band& B) {
    long p = B.recPtr;
    int total = 0, info;
    unsigned len;
    do { len = rd16u(p); info = rd16s(p + 2); p += 4; total += (int)len; } while (len == 0xffff);
    B.recPtr = p;
    B.recStart = B.recEnd;
    B.recEnd = B.recEnd + total;
    B.budget = (int)(((unsigned)info << 6) & 0x7e00);
    return B.budget;
}

// 0x8950f620: the repeat cost at this pitch and walk speed (as SOLO's).
int XtMbPlayer::State::cost(const Band& B, int* c) const {
    const float a = rateF;
    float t = (float)walkStep * 2.98023224e-8f;   // 2^-25
    float c0 = -0.5f, c1 = 1.5f;
    if (1.0f > a) { c0 = -2.0f; c1 = 3.0f; }
    float x = a * c0;
    x = x + c1;
    if (0.0f > x) x = 0.0f;
    if (0.0f > t) t = -t;
    x = t * x;
    x = x * 536870912.0f;                          // 2^29
    int r4 = (int)((uint32_t)ftrc(x) >> 16);
    if (0x800 > r4) { r4 = 2 * r4 - 0x800; if (r4 < 0) r4 = 0; }
    *c = r4;
    return B.budget;
}

// 0x8950f400: advance a band's read position by one walk-list entry; at a
// record end, repeat the record if the walk position is still inside it and
// the budget allows, else move on (skipping records the walk has passed).
void XtMbPlayer::State::step(Band& B, int fshift, int walkPos) {
    int r2 = B.readFrac + (rate >> 1);
    int r1 = B.readPos + (r2 >> fshift);
    B.readPos = r1;
    B.readFrac = r2 & B.mask;
    r2 = B.recEnd;
    if (r1 < r2) return;
    // (walkStep < 0: 0x8950f4b0, reverse playback — not ported)
    bool repeat = false;
    int r0 = B.budget;
    if (r2 > walkPos && r0 > 0) {
        if (r0 == 0x7e00) repeat = true;
        else {
            int c;
            r0 = cost(B, &c);
            if (r0 >= c) { B.budget = r0 - c; repeat = true; }
        }
    }
    if (!repeat && r2 >= endPos) repeat = true;
    if (repeat) {                                   // 0x8950f44a
        int r3 = B.recStart;
        r1 -= r2;
        bool more;
        do { r1 += r3; more = r1 >= r2; r3 -= r2; } while (more);
        B.readPos = r1;
    } else {                                        // 0x8950f466
        for (;;) {
            if (record(B) <= 0) break;
            if (walkPos < B.recEnd) break;
            B.readPos = B.readPos + B.recEnd - B.recStart;
        }
    }
    // 0x8950f490
    float g = 0.0f;
    if (ended != 1 && !(B.recEnd > endPos)) g = 1.0f;
    B.gain = g;
}

// 0x8950f6f0 (via 0x8950f6c0): every band's segments and first record at
// the current event.
void XtMbPlayer::State::initRecords() {
    const long e = entry(ev);                       // 0x8950c830
    flags5 = (uint8_t)(rd16u(e) & 0xb);
    const int r7 = (int)rd32(e + 4);
    evPos = r7;
    long r5 = e + 8;
    pos += r7;
    posFrac = 0;
    for (int& v : list) v = r7;
    const float sign = (0.0f > walk) ? -1.0f : 1.0f;
    for (int k = 0; k < 6; ++k) {
        Band& B = b[k];
        B.readPos = B.startOff + r7;
        B.readFrac = 0;
        B.segList = (long)rd32(r5);
        B.recPtr = (long)rd32(r5 + 4);
        const int mark = (int)rd16u(r5 + 8);
        r5 += 8 + (stride9 - 4) * 2;
        B.grainStart = segment(B);
        segment(B);
        long p = B.recPtr;
        unsigned len = rd16u(p);
        int info = rd16s(p + 2);
        p += 4;
        B.recStart = r7 + mark - (int)len;
        int total = (int)len;
        while (len == 0xffff) { len = rd16u(p); info = rd16s(p + 2); p += 4; total += (int)len; }
        B.recEnd = B.recStart + total;
        B.recPtr = p;
        B.budget = (int)(((unsigned)info << 6) & 0x7e00);
        B.gain = sign;
        if (k == 0) {
            if (flags3 & 0x80) break;
            r5 += stride9 * 2;                      // skip the empty slot after band 0
        }
    }
    int r2 = lastEvPos + 0x10;
    uint8_t f = 0;
    if ((int16_t)evLast > (int16_t)ev) { const long n = e + stride8 * 2; f = (uint8_t)(rd16u(n) & 0xb); r2 = (int)rd32(n + 4); }
    flags6 = f;
    nextEvPos = r2;
}

bool XtMbPlayer::noteOn(const XtMbWave& w, int mode, float pitch, float walk) {
    if (!T.ok || !w.vpdt || w.vpdtBytes < 64 || (w.channels != 1 && w.channels != 2) || !(walk >= 0.0f)) return false;
    if (mode != 3 && mode != 5) return false;
    if (!s) s = new State;
    *s = State();                                   // no allocation: safe on the audio thread
    State& S = *s;
    S.w = w;
    S.mode = mode;
    S.stereo = w.channels == 2;
    S.flags0 = S.stereo ? 0x40 : 0;
    S.pitch = pitch;
    S.walk = walk;
    if (const char* r = std::getenv("XT_ROUND"); r && std::string(r) == "nearest") S.rtz = false;
    // 0x89510980
    for (int k = 0; k < 6; ++k) {
        Band& B = S.b[k];
        B.base = k == 0 ? 0 : -2;
        B.shift = k - 1;
        B.stride = (int)T.stride[k];
        B.fshift = T.fshift[k];
        B.sshift = T.sshift[k];
        B.startOff = (int)T.offset[k];
        B.mask = (int)T.mask[k];
        B.gain = 1.0f;
    }
    S.off7 = 0xe;
    S.stride8 = S.stereo ? 0x58 : 0x2e;
    S.stride9 = S.stereo ? 0xc : 6;
    // 0x8950fa60
    S.evLast = (uint16_t)((S.rd32(8) >> 16) - 1);
    S.lastEvPos = (int)S.rd32(S.entry(S.evLast) + 4);
    if (S.evLast < S.evStart) S.evStart = S.evLast;
    if (S.evLast < S.evEnd) S.evEnd = S.evLast;
    S.startPos = (int)S.rd32(S.entry(S.evStart) + 4);
    S.endPos = (int)S.rd32(S.entry(S.evEnd) + 4);
    S.ev = (0.0f > walk) ? S.evEnd : S.evStart;
    S.pos = 0;
    if (mode == 5) S.flags3 = 0x80;                 // 0x89510130: band 0's records only
    S.initRecords();
    // tree state is zeroed; 0x89510760: f9c = 1; 0x8950f860: (rate ratio 1)
    S.f9c = 1.0f;
    if (mode == 5) {                                // 0x89510130: oscillator bands
        for (int k = 1; k <= 5; ++k) {
            Band& B = S.b[k];
            B.base = 0;
            B.shift = T.eshift[k];
            B.frame = S.pos >> (B.shift + 6);
            // the first frame is packed {base, delta} (later frames {delta, base})
            for (int c = 0; c < (S.stereo ? 2 : 1); ++c)
                for (int j = 0; j < 12; ++j) {
                    const int16_t v0 = S.frameVal(k, B.frame, j, c), v1 = S.frameVal(k, B.frame + 1, j, c);
                    B.packed[c][j] = (uint32_t)(uint16_t)v0 | (uint32_t)(uint16_t)(int16_t)(v1 - v0) << 16;
                }
        }
    }
    return true;
}

bool XtMbPlayer::ended() const { return !s || s->ended; }

void XtMbPlayer::setRates(float pitch, float walk) {
    if (!s || !(walk >= 0.0f)) return;   // the backward path (walk < 0) is not ported
    s->pitch = pitch;   // the block reads +0x70 / +0x7c each time (0x8950c280, 0x8950e760)
    s->walk = walk;
}

// 0x8950e860: fill the walk-position list for this block (forward).
void XtMbPlayer::State::walkList() {
    int r1 = pos, r2 = posFrac;
    const int r3 = walkStep >> 1, r4 = endPos;
    if (r4 > r1)
        for (int i = 0; i < 16; ++i) { r2 += r3; const int r0 = r2; list[i] = r1; r2 &= 0x7ffff; r1 += r0 >> 19; }
    if (r1 >= r4) {                                 // end of the wave (no loop)
        ended = 1;
        r1 = r4;
        r2 = 0;
        for (int& v : list) v = r1;
    }
    pos = r1;
    posFrac = r2;
    // (r1 ≥ limit calls the event walker 0x8950c880 — not ported)
}

// A band reaching its next segment start begins a new grain there.
void XtMbPlayer::State::trigger(Band& B, int walkPos) {
    const int r2 = B.nextSeg;
    if (walkPos < r2) return;
    const long r6 = B.nextRecs;
    const int r1 = rd16s(r6) + r2;
    if (!(endPos > r1)) return;
    B.grainStart = r2;
    B.recEnd = r2;
    B.recPtr = r6;
    B.readFrac = 0;
    B.readPos = r2 + B.startOff;
    segment(B);
    record(B);
}

// 0x8950e980: band 0, two output samples by linear interpolation.
void XtMbPlayer::State::readBand0(float*& o) {
    const Band& B = b[0];
    const float g = B.gain;
    for (int h = 0; h < 2; ++h) {
        const int p = h == 0 ? B.readPos : B.readPos + step16;
        const long i = p >> 4;
        float f = (float)(p & 15);
        f = f * 0.0625f;
        for (int c = 0; c < (stereo ? 2 : 1); ++c) {
            float s0 = smp(0, B.base + i, c), s1 = smp(0, B.base + i + 1, c);
            float d = s1 - s0;
            float y = f * d;
            y = y + s0;
            y = y * g;
            o[c * 114] = y;
        }
        ++o;
    }
}

// 0x8950eb60: band k ≥ 1, two output samples through the 64×6 kernel and
// the one-pole smoother y = z + (x·g − z)·c.
void XtMbPlayer::State::readBand(int k, float*& o) {
    Band& B = b[k];
    const float c = lpc, g = B.gain;
    for (int h = 0; h < 2; ++h) {
        int p = B.readPos >> B.shift;
        if (h) p += step16;
        const float* K = T.kernel[p & 63];
        const long i = p >> 6;
        for (int ch = 0; ch < (stereo ? 2 : 1); ++ch) {
            float y = smp(k, B.base + i, ch) * K[0];
            for (int j = 1; j < 6; ++j) { float t = K[j] * smp(k, B.base + i + j, ch); y = t + y; }
            y = y * g;
            const float z = B.lp[ch];
            y = y - z;
            y = y * c;
            y = y + z;
            B.lp[ch] = y;
            o[ch * 114] = y;
        }
        ++o;
    }
}

// 0x8950c720: one ×2 stage of the recombination tree: n input samples at
// M[r4] are upsampled and added into M[r5] (2n samples), state at M[r6].
// Indices are float slots from +0x418; transliterated register by register.
void XtMbPlayer::State::stage(int r4, int r5, int r6, int n, const float* C) {
    float f[4];                          // fr0..fr3 (fv0); fv4 = C[0..3]
    const float fr8 = C[4], fr9 = C[5], fr10 = C[6];
    float t;
    f[0] = M[r6]; f[1] = M[r6 + 1]; f[2] = M[r6 + 2]; f[3] = M[r6 + 3];
    for (;;) {
        f[3] = fipr(f, C);
        f[1] = M[r4];
        f[3] = f[3] + f[1];
        M[--r6] = f[3];
        f[3] = f[3] * fr8;
        r6 += 2;
        f[0] = f[0] + f[2];
        f[2] = M[r5];
        t = f[0] * fr9;  f[3] = t + f[3];
        f[0] = M[r6];
        r6 += 2;
        t = f[0] * fr10; f[3] = t + f[3];
        f[0] = M[r6];
        r6 -= 4;
        t = f[0] * fr8;  f[3] = t + f[3];
        f[3] = f[3] + f[2];
        M[r5] = f[3];
        f[0] = M[r6]; f[1] = M[r6 + 1]; f[2] = M[r6 + 2]; f[3] = M[r6 + 3];
        r6 += 3;
        r5 += 1;
        f[3] = fipr(f, C);
        f[1] = M[r4++];
        r6 -= 4;
        f[3] = f[3] + f[1];
        M[r6] = f[3];
        f[3] = f[3] * fr8;
        r6 += 2;
        f[0] = f[0] + f[2];
        f[2] = M[r5];
        t = f[0] * fr9;  f[3] = t + f[3];
        f[0] = M[r6];
        r6 += 2;
        t = f[0] * fr10; f[3] = t + f[3];
        f[0] = M[r6];
        r6 -= 4;
        t = f[0] * fr8;  f[3] = t + f[3];
        f[0] = M[r6++];
        f[3] = f[3] + f[2];
        f[1] = M[r6++];
        M[r5] = f[3];
        f[2] = M[r6++];
        r5 += 1;
        f[3] = M[r6];
        r6 += 2;
        M[r6] = f[3];
        M[--r6] = f[2];
        M[--r6] = f[1];
        M[--r6] = f[0];
        if (--n == 0) break;
    }
}

// 0x8950c650: the tree for one channel (X = 0 for L, 114 for R).  Bands 4
// and 5 are summed (2 samples at ×16), then each ×2 stage adds into the next
// band up: 2 → 4 (band 3) → 8 (band 2) → 16 (band 1) → 32 (band 0).
void XtMbPlayer::State::tree(int X) {
    float f1 = M[X + 62], f2 = M[X + 63], f3 = M[X + 60], f4 = M[X + 61];
    f3 = f3 + f1;
    f4 = f4 + f2;
    M[X + 61] = f4;
    M[X + 60] = f3;
    stage(X + 60, X + 56, X + 110, 2, T.tree);
    stage(X + 56, X + 48, X + 104, 4, T.tree);
    stage(X + 48, X + 32, X + 98, 8, T.tree);
    // last stage (0x8950c6a0): 16 inputs at +0x80 into the 32 band-0 slots
    float f[4];
    const float fv4[4] = { T.fin[0], T.fin[1], T.fin[2], 1.0f };
    int r4 = X + 32, r5 = X + 1, r6 = X + 74;
    for (int n = 16; n > 0; --n) {
        f[3] = M[r5];
        f[2] = M[r6++];
        f[1] = M[r6++];
        r5 -= 1;
        f[0] = M[r6++];
        float fr8 = M[r5];
        f[3] = fipr(f, fv4);
        fr8 = fr8 + f[0];
        f[0] = M[r6++];
        f[1] = M[r6++];
        f[2] = M[r6++];
        M[r5] = fr8;
        r5 += 1;
        f[3] = fipr(f, fv4);
        f[2] = M[r4++];
        M[r5] = f[3];
        r5 += 2;
        M[r6] = f[2];
        M[r6 - 16] = f[2];
        r6 -= 5;
    }
}

// 0x8950c570: tree(s), then the smoothed output gain (once per two frames).
void XtMbPlayer::State::output(float* out) {
    tree(0);
    if (stereo) tree(114);
    const float fr7 = 65536.0f, fr1 = gainTarget;
    float fr2 = gain, fr3 = gainCoef;
    fr3 = fr3 + fr3;
    for (int i = 0; i < 16; ++i) {
        const int a = 2 * i;
        float fr0 = fr1 - fr2;
        float t = fr0 * fr3;
        fr2 = t + fr2;
        float fr4 = fr7 * fr2;
        if (!stereo) {
            const float y0 = M[a] * fr4, y1 = M[a + 1] * fr4;
            out[2 * a] = y0; out[2 * a + 1] = y0; out[2 * a + 2] = y1; out[2 * a + 3] = y1;
        } else {
            out[2 * a] = M[a] * fr4;       out[2 * a + 1] = M[114 + a] * fr4;
            out[2 * a + 2] = M[a + 1] * fr4; out[2 * a + 3] = M[114 + a + 1] * fr4;
        }
    }
    gain = fr2;
}

void XtMbPlayer::block(float* out) {
    if (!s) { std::fill(out, out + 64, 0.0f); return; }
    // The SH-4 runs with FPSCR RM = 1 (round to zero).  XT_ROUND=nearest
    // selects the P-code emulator's rounding (for validation).
    const int saved = std::fegetround();
    if (s->rtz) std::fesetround(FE_TOWARDZERO);
    blockImpl(*s, out);
    std::fesetround(saved);
}

void XtMbPlayer::blockImpl(State& S, float* out) {
    // 0x8950c280 (default flags): the walk step for this block
    float w = 1.0f;
    w = w * S.walk;
    w = w * 33554432.0f;
    S.walkStep = ftrc(w);
    // 0x8950e760: band 0
    S.limit = S.nextEvPos;                          // 0x8950ccc0 (no loop bounds)
    S.rateF = S.pitch * S.f9c;
    S.rate = ftrc(S.rateF * 33554432.0f);
    S.walkList();
    float* o = S.M;
    for (int i = 0; i < 16; ++i) {
        Band& B = S.b[0];
        S.trigger(B, S.list[i]);
        S.step16 = (int16_t)(S.rate >> 21);
        S.readBand0(o);
        S.step(B, 19, S.list[i]);
    }
    if (S.mode == 5) {                              // 0x8950db20
        S.rate = ftrc(S.rateF * 134217728.0f);      // 2^27
        const float g = S.b[0].gain;
        if (g > 0.5f || -0.5f > g) {
            for (int k = 1; k <= 5; ++k)
                S.ensembleBand(k, k == 5 ? 1 : 8 >> (k - 1), T.oscC + (k == 5 ? 6 : 0), o);
        } else {
            for (int i = 32; i < 64; ++i) { S.M[i] = 0.0f; S.M[114 + i] = 0.0f; }
        }
        S.output(out);
        return;
    }
    // 0x8950e6e0: bands 1..5 (0x8950eab0), smoother coefficient min(1, 0.95·rate)
    for (int k = 1; k <= 5; ++k) {
        float c = S.rateF * (k == 5 ? 0.75f : 0.95f);
        if (c > 1.0f) c = 1.0f;
        S.lpc = c;
        Band& B = S.b[k];
        const int n = k == 5 ? 1 : 8 >> (k - 1);
        int li = 0;
        for (int j = 0; j < n; ++j) {
            S.trigger(B, S.list[li]);
            S.step16 = (int16_t)(S.rate >> B.sshift);
            S.readBand(k, o);
            S.step(B, B.fshift, S.list[li]);
            li += B.stride;
        }
    }
    S.output(out);
}

// 0x8950dcb0 (forward): move to frame f.  Each value becomes {delta, base}
// with base = old base + old delta and delta = value(f + 1) − base (16-bit).
void XtMbPlayer::State::loadFrame(Band& B, int k, int f) {
    B.frame = f;
    for (int c = 0; c < (stereo ? 2 : 1); ++c)
        for (int j = 0; j < 12; ++j) {
            uint32_t r5 = B.packed[c][j];
            const uint32_t sw = (r5 >> 16) | (r5 << 16);
            r5 += sw;
            uint32_t r2 = (uint32_t)(int32_t)frameVal(k, (long)f + 1, j, c) - r5;
            r5 <<= 16;
            B.packed[c][j] = (r2 & 0xffff) + r5;
        }
}

// 0x8950dc50: six oscillators, two output samples.  Phase step = high word
// of (frame delta + constant) × rate·2^27; amplitude = delta·frac + base.
void XtMbPlayer::State::oscillators(Band& B, int c, float frac, const uint32_t* C, float*& o) {
    float fr3 = 0.0f, fr4 = 0.0f;
    for (int j = 0; j < 6; ++j) {
        const uint32_t fq = B.packed[c][2 * j], am = B.packed[c][2 * j + 1];
        const int32_t r1 = (int32_t)(int16_t)(fq & 0xffff) + (int32_t)C[j];
        const int32_t inc = (int32_t)(((int64_t)rate * (int64_t)r1) >> 32);
        const uint32_t p1 = (uint32_t)((int32_t)(int16_t)B.phase[c][j] + inc);
        const uint32_t p2 = p1 + (uint32_t)inc;
        B.phase[c][j] = (uint16_t)p2;
        const float lo = (float)(int16_t)(am & 0xffff), hi = (float)(int16_t)(am >> 16);
        float t = lo * frac;
        const float a = t + hi;
        t = T.cosTab[(p1 & 0xffff) >> 8] * a; fr3 = t + fr3;
        t = T.cosTab[(p2 & 0xffff) >> 8] * a; fr4 = t + fr4;
    }
    o[c * 114] = fr3;
    o[c * 114 + 1] = fr4;
}

// 0x8950dbe0: band k ≥ 1 of ENSEMBLE, n walk-list entries.
void XtMbPlayer::State::ensembleBand(int k, int n, const uint32_t* C, float*& o) {
    Band& B = b[k];
    int li = 0;
    for (int e = 0; e < n; ++e) {
        const uint32_t p = (uint32_t)list[li] >> B.shift;
        const int f = (int)(p >> 6);
        float frac = (float)(int)(p & 63);
        frac = frac * 0.015625f;
        if (f != B.frame) loadFrame(B, k, f);
        if (stereo) oscillators(B, 1, frac, C, o);
        oscillators(B, 0, frac, C, o);
        o += 2;
        li += B.stride;
    }
}

// ── Load-time band rebuild (BACKING) ────────────────────────────────────────
namespace {

inline int16_t sat16(long v) { return (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v); }

// A signal on indices [-off, size - off), zero outside.
struct Sig {
    std::vector<int32_t> v;
    long off = 0;
    int32_t at(long i) const { i += off; return i < 0 || i >= (long)v.size() ? 0 : v[(size_t)i]; }
};

// 0x89523aa0: decimating FIR at centre i, c0·x0 + Σ ci·(x_i + x_−i).
float firDec(const Sig& x, long i, const std::vector<float>& c) {
    float acc = (float)x.at(i) * c[0];
    for (size_t k = 1; k < c.size(); ++k) { float t = (float)(x.at(i + (long)k) + x.at(i - (long)k)) * c[k]; acc = t + acc; }
    return acc;
}

// One stage: decimate x by D into `low`, interpolate back (×4: the full
// half-kernel, 0x89523c60; ×2: the half-band 0x89523fd0) and write the
// residual x − D·interp on [0, L).  Signals are kept `M` samples beyond both
// ends, which covers the filters' tails (the XT carries them as history).
void stage(const Sig& x, long L, int D, const std::vector<float>& cd, const std::vector<float>& ci, Sig& low, std::vector<int32_t>& res) {
    const long M = 256;
    const long Lo = L / D;
    low.off = M;
    low.v.assign((size_t)(Lo + 2 * M), 0);
    for (long m = -M; m < Lo + M; ++m) {
        const float y = firDec(x, D * m, cd);
        low.v[(size_t)(m + M)] = sat16(ftrc(y));
    }
    auto z = [&](long t) -> int32_t { return (t % D) ? 0 : low.at(t / D); };   // zero-stuffed low
    res.assign((size_t)L, 0);
    for (long t = 0; t < L; ++t) {
        float acc;
        if (D == 2) {
            acc = (float)z(t) * 0.5f;
            for (size_t j = 0; j < ci.size(); ++j) { const long d = 2 * (long)j + 1; float u = (float)(z(t + d) + z(t - d)) * ci[j]; acc = u + acc; }
        } else {
            acc = (float)z(t) * ci[0];
            for (size_t k = 1; k < ci.size(); ++k) { float u = (float)(z(t + (long)k) + z(t - (long)k)) * ci[k]; acc = u + acc; }
        }
        float y = acc * (float)D;
        res[(size_t)t] = sat16((long)x.at(t) - ftrc(y));
    }
}

} // namespace

bool xtMbBuildBacking(const int16_t* pcm, long frames, int channels, XtMbBands& out) {
    if (!T.ok || !pcm || frames <= 0 || (channels != 1 && channels != 2)) return false;
    // The SH-4's rounding (round to zero); XT_ROUND=nearest: the emulator's.
    const int saved = std::fegetround();
    const char* rm = std::getenv("XT_ROUND");
    if (!(rm && std::string(rm) == "nearest")) std::fesetround(FE_TOWARDZERO);
    out.channels = channels;
    const long n64 = (frames + 63) / 64 * 64;                 // chunks are rounded up to 64 frames
    const int shift[6] = { 0, 2, 3, 4, 5, 6 };
    for (int k = 0; k < 6; ++k) {
        out.len[k] = n64 >> shift[k];
        out.buf[k].assign((size_t)((out.len[k] + 256) * channels), 0);
    }
    for (int c = 0; c < channels; ++c) {
        Sig x;
        x.v.resize((size_t)n64, 0);
        for (long i = 0; i < frames; ++i) x.v[(size_t)i] = pcm[i * channels + c];
        std::vector<int32_t> res;
        for (int k = 0; k < 5; ++k) {
            Sig low;
            stage(x, out.len[k], k == 0 ? 4 : 2, k == 0 ? T.dec4 : T.dec2, k == 0 ? T.int4 : T.int2, low, res);
            int16_t* b = out.buf[k].data();
            for (long t = 0; t < out.len[k]; ++t) b[t * channels + c] = (int16_t)res[(size_t)t];
            if (k >= 1) {   // padding: the stage input past the end, ten reversed samples from before 0
                const long carry = k == 1 ? (long)T.int4.size() : (long)T.int2.size();
                for (long j = 0; j < carry; ++j) b[(out.len[k] + j) * channels + c] = (int16_t)x.at(out.len[k] + j);
                for (long j = 0; j < 10; ++j) b[(out.len[k] + 128 + j) * channels + c] = (int16_t)x.at(-1 - j);
            }
            x = std::move(low);
        }
        int16_t* b = out.buf[5].data();                       // band 5: the last low-pass
        for (long t = 0; t < out.len[5]; ++t) b[t * channels + c] = (int16_t)x.at(t);
        for (long j = 0; j < (long)T.int2.size(); ++j) b[(out.len[5] + j) * channels + c] = (int16_t)x.at(out.len[5] + j);
        for (long j = 0; j < 10; ++j) b[(out.len[5] + 128 + j) * channels + c] = (int16_t)x.at(-1 - j);
    }
    std::fesetround(saved);
    return true;
}

void xtMbAttach(const XtMbBands& b, XtMbWave& w) {
    w.channels = b.channels;
    for (int k = 0; k < 6; ++k) {
        w.band[k].data = b.buf[k].data();
        w.band[k].lo = 0;
        w.band[k].hi = (long)(b.buf[k].size() / (size_t)b.channels);
    }
}

XtMbPlayer::XtMbPlayer() : s(new State) {}
XtMbPlayer::~XtMbPlayer() { delete s; }

#ifdef XT_MBPLAYER_MAIN
// xt_mbplay <rom.2.bin> <vpdt.bin> <mem.samp> <band1..5 hex,> <channels> <out.f32> <pitch> <walk> [frames]
// (XT_MODE=5: ENSEMBLE)
// The same inputs as EmuBacking.java: sample memory from 0x8C100000 with the
// band pointers into it.  Writes float32 L/R pairs scaled to ±1.
#include <cstdlib>
#include <string>
static std::vector<uint8_t> slurp(const char* p) {
    std::vector<uint8_t> v;
    FILE* f = std::fopen(p, "rb");
    if (!f) return v;
    std::fseek(f, 0, SEEK_END); v.resize((size_t)std::ftell(f)); std::fseek(f, 0, SEEK_SET);
    if (std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
    std::fclose(f);
    return v;
}
int main(int argc, char** argv) {
    // xt_mbplay rom.2.bin --bands samp.bin <channels> out.samp : the load-time
    // band rebuild, written as the XT's sample memory (band k after band k−1)
    if (argc >= 6 && std::string(argv[2]) == "--bands") {
        if (!xtMbLoadTables(argv[1])) { std::fprintf(stderr, "cannot read the tables from %s\n", argv[1]); return 1; }
        std::vector<uint8_t> raw = slurp(argv[3]);
        const int ch = std::atoi(argv[4]);
        XtMbBands b;
        if (!xtMbBuildBacking(reinterpret_cast<const int16_t*>(raw.data()), (long)(raw.size() / (2 * ch)), ch, b)) return 1;
        FILE* fo = std::fopen(argv[5], "wb");
        if (!fo) return 1;
        long addr = 0x8C100000L;
        for (int k = 0; k < 6; ++k) {
            if (k) std::printf("%s%lx", k > 1 ? "," : "", addr);
            std::fwrite(b.buf[k].data(), 2, b.buf[k].size(), fo);
            addr += (long)b.buf[k].size() * 2;
        }
        std::printf("\n");
        std::fclose(fo);
        return 0;
    }
    if (argc < 9) { std::fprintf(stderr, "usage: %s rom.2.bin vpdt.bin mem.samp b1,b2,b3,b4,b5 channels out.f32 pitch walk [frames]\n", argv[0]); return 1; }
    if (!xtMbLoadTables(argv[1])) { std::fprintf(stderr, "cannot read the player tables from %s\n", argv[1]); return 1; }
    std::vector<uint8_t> vpdt = slurp(argv[2]), mem = slurp(argv[3]);
    if (vpdt.empty() || mem.empty()) return 1;
    const int ch = std::atoi(argv[5]);
    const long fb = 2L * ch, memFrames = (long)mem.size() / fb;
    const int16_t* m16 = reinterpret_cast<const int16_t*>(mem.data());
    XtMbWave w;
    w.vpdt = vpdt.data(); w.vpdtBytes = vpdt.size(); w.channels = ch;
    long ptr[6] = { 0x8C100000L };
    std::string bl = argv[4];
    for (int k = 1; k <= 5; ++k) { size_t c = bl.find(','); ptr[k] = std::strtol(bl.substr(0, c).c_str(), nullptr, 16); bl = c == std::string::npos ? "" : bl.substr(c + 1); }
    for (int k = 0; k < 6; ++k) {
        const long f0 = (ptr[k] - 0x8C100000L) / fb;   // band start frame in the image
        w.band[k].data = m16 + f0 * ch;
        w.band[k].lo = -f0;
        w.band[k].hi = memFrames - f0;
    }
    const float pitch = (float)std::atof(argv[7]), walk = (float)std::atof(argv[8]);
    XtMbPlayer pl;
    const char* md = std::getenv("XT_MODE");   // 5 = ENSEMBLE
    if (!pl.noteOn(w, md && std::string(md) == "5" ? 5 : 3, pitch, walk)) { std::fprintf(stderr, "noteOn failed\n"); return 1; }
    long frames = argc > 9 ? std::atol(argv[9]) : 0;
    if (frames <= 0) {   // n / walk, n from the end event
        if (!(walk > 0.0f)) { std::fprintf(stderr, "give the frame count when walk is 0\n"); return 1; }
        uint32_t cnt = 0; std::memcpy(&cnt, vpdt.data() + 8, 4); cnt >>= 16;
        const long e = ((long)(cnt - 1) * (ch == 2 ? 0x58 : 0x2e) + 0xe) * 2;
        uint32_t endPos = 0; std::memcpy(&endPos, vpdt.data() + e + 4, 4);
        frames = std::lround((endPos / 16) / walk);
    }
    FILE* fo = std::fopen(argv[6], "wb");
    if (!fo) return 1;
    float blk[64], lr[64];
    for (long done = 0; done < frames; done += 32) {
        pl.block(blk);
        const long nn = std::min(32L, frames - done);
        for (long i = 0; i < 2 * nn; ++i) lr[i] = blk[i] / 32768.0f;
        std::fwrite(lr, sizeof(float), (size_t)(2 * nn), fo);
    }
    std::fclose(fo);
    return 0;
}
#endif
