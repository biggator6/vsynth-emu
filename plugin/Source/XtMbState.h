#pragma once
// XtMbPlayer's state: the firmware's player object as the port holds it, a band's struct and the player's (moved here
// from XtMultiBandPlayer.cpp in v332, as they stood but for what v332 adds), whose routines XtMultiBandPlayer.cpp has.
// From v332 the walk step given, and what the state's caller may take outside: the walk list's crossing and its wrap
// at an end (XtMbOutside), by which the VC-2's voice runs these routines on a copy of its own object's words
// (Vc2OscBands.cpp).
#include "XtFloat.h"
#include "XtMultiBandPlayer.h"
#include <cstring>

class XtMbOutside;

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
    uint8_t flags0 = 0, flags3 = 0;              // +0x00, +0x03
    int8_t ended = 0;                            // +0x04: 1 at the end, −1 at the start
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
    XtBeat beat;                                 // +0x40, +0x41, +0x48…+0x64, +0x8c, +0x90
    float walkFloat = 0;                         // walk·factor·2^25, this block's
    float gainTarget = 1.0f / 65536, gain = 1.0f / 65536, gainCoef = 1.0f / 64;  // +0x80..+0x88
    float lpc = 0;                               // +0x94
    float rateF = 1, f9c = 1;                    // +0x98, +0x9c
    float loopFwdRate = 1, loopBwdRate = 1;      // +0xa0, +0xa4 (0x89510760)
    int list[16] = {};                           // +0xa8
    Band b[6];                                   // +0xe8 + 0x88·k
    // Loops: the switch (byte 0 bit 0), the cached events (+0x6c → 0xb4
    // bytes each), their positions and the modes at the forward end and the
    // backward start (records 1 and 3)
    bool cached = false;                         // the note-on found the loop's events
    bool pastVpdt = false;                       // records past the vpdt that leave nothing to step (startAt, the repeats): the XT reads RAM there
    uint8_t lrec[4][kXtMbLoopEventBytes] = {};
    int lp[4] = { 0, 0x7fffffff, 0x7fffffff, 0 };
    int lmode1 = 0, lmode3 = 0;
    float M[2 * 114] = {};                       // +0x418 … +0x7a8: band outputs and tree state (R at +0x1c8)
    XtMbOutside* outside = nullptr;              // v332: what is taken outside; none, rom.2's own
    bool stepGiven = false;                      // v332: the walk step given (+0x34), the VC-2's beat block's

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
    int  recordPrev(Band& B);       // 0x8950f5d0
    int  cost(const Band& B, int* c) const;   // 0x8950f620
    void step(Band& B, int fshift, int walkPos);   // 0x8950f400
    void stepBack(Band& B, int fshift, int walkPos);   // 0x8950f4c0
    void initRecords();             // 0x8950f6f0
    void restore(int r7, const uint8_t* slots, long slotBytes);   // 0x8950f722
    void startAt(int target);       // 0x89510bf0
    void startBands(int target);    // v333: its bands' part, the VC-2's band seek's too (0x893cd440)
    void loopJump(const uint8_t* c);   // 0x8950f710, on a cached loop event's bytes (v332: by its bytes)
    void loopJump(int r) { loopJump(lrec[r]); }   // the cache's record r
    void crossings();               // 0x8950ccc0
    void crossForward();            // 0x8950c880 (type 0x10)
    void crossBackward();           // 0x8950ca70 (type 0x10)
    void cross(bool forward);       // v332: the walk list's crossing, outside or rom.2's own, then the next crossing
    void eventSetup();              // 0x8950c830
    void walkList();                // 0x8950e860 / 0x8950e8f0
    bool wrapped(int r1, int r4, uint16_t to);   // v332: the walk list's wrap at an end, outside (0x8950e87e, 0x8950e916)
    void stepFromWalk();            // 0x8950c280's tail, blockImpl's first unless stepGiven (moved here in v332)
    void trigger(Band& B, int walkPos);
    void triggerBack(Band& B, int walkPos);
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

// v332: what the state's caller takes outside it (the VC-2's voice, Vc2OscBands.cpp): the walk list's crossing, in
// place of crossForward or crossBackward and crossings; and its wrap at an end, rom.2's code (0x8950e87e, 0x8950e916)
// that only the VC-2 reaches, nothing in rom.2's XtSynth setting byte 0's 0x08, so ported there
class XtMbOutside {
public:
    virtual void cross(XtMbPlayer::State& s, bool forward) = 0;
    virtual bool wrapped(XtMbPlayer::State& s, int r1, int r4, uint16_t to) = 0;

protected:
    ~XtMbOutside() = default;
};

// 0x8950c280's tail, moved here from blockImpl in v332: the walk step for this block, turned round inside an
// Alternate bounce until the walk itself turns; held at 0 after a mode-3 loop until the walk turns away from the loop
inline void XtMbPlayer::State::stepFromWalk() {
    State& S = *this;
    const float w = S.walkFloat;
    if (!(S.flags3 & 0x40)) {
        int st = xtf::ftrc(w);
        const bool neg = 0.0f > w;
        if (S.flags3 & 8) { if (neg) S.flags3 &= 0xf7; else st = -st; }
        if (S.flags3 & 4) { if (neg) st = -st; else S.flags3 &= 0xf7; }   // (sic: 0x08)
        S.walkStep = st;
    } else {
        if ((S.flags3 & 0x20) ? 0.0f > w : w > 0.0f) { S.f9c = 1.0f; S.flags3 &= 0x8f; }
        S.walkStep = 0;
    }
}

// v332: the walk list's crossing, 0x8950c880 forward or 0x8950ca70 back with the band players' index (16), then
// 0x8950ccc0; outside when the state's caller takes it
inline void XtMbPlayer::State::cross(bool forward) {
    if (outside != nullptr) {
        outside->cross(*this, forward);
        return;
    }
    if (forward) crossForward();
    if (!forward) crossBackward();
    crossings();
}

inline bool XtMbPlayer::State::wrapped(int r1, int r4, uint16_t to) {
    return outside != nullptr && outside->wrapped(*this, r1, r4, to);
}

// 0x89510bf0's bands, moved here from startAt in v333 as they stood: each band's segments and records forward to the
// position, its read there, band 0's alone with byte 3's 0x80 (ENSEMBLE's); rom.4's band seek 0x893cd440 runs the same
// code after its jump (Vc2OscBands.cpp)
inline void XtMbPlayer::State::startBands(int target) {
    // A seek to the wave's end on the last event (a Beat Keep's way back after the walk reached it) reads the last event's entry, whose
    // segments and records run past the vpdt.  The XT reads RAM there and gets out of these loops; the port reads zeros there, so it
    // stops at a step past the vpdt that moves nothing, from which it could never get out.  (Inside the vpdt a record may be empty.)
    for (int k = 0; k < 6; ++k) {
        Band& B = b[k];
        while (target >= B.nextSeg && lastEvPos > B.nextSeg) {
            const int was = B.nextSeg;
            B.grainStart = B.nextSeg;
            B.recEnd = B.nextSeg;
            B.recPtr = B.nextRecs;
            segment(B);
            record(B);
            if (B.nextSeg == was && (B.segList < 0 || B.segList >= (long)w.vpdtBytes)) { pastVpdt = true; break; }
        }
        while (target >= B.recEnd) { const int was = B.recEnd; record(B); if (B.recEnd == was && (B.recPtr < 0 || B.recPtr >= (long)w.vpdtBytes)) { pastVpdt = true; break; } }
        B.readPos = B.startOff + target;
        B.readFrac = 0;
        if (flags3 & 0x80) break;
    }
}
