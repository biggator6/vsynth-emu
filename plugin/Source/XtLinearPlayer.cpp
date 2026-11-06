// The Vari off player: see XtLinearPlayer.h.  0x89509870, 0x89509f52 and their six paths, by Loop Mode and direction.
#include "XtLinearPlayer.h"
#include "XtFloat.h"

#include <bit>
#include <cfenv>
#include <cmath>

#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON

namespace {
struct RoundToZero {
    int saved;
    RoundToZero() : saved(std::fegetround()) { if (!xtf::nearest()) std::fesetround(FE_TOWARDZERO); }
    ~RoundToZero() { std::fesetround(saved); }
};
struct RoundNearest {  // for XtBeat, which is in XtFloat: its sums' TwoSum needs this, as in the other players
    int saved;
    RoundNearest() : saved(std::fegetround()) { std::fesetround(FE_TONEAREST); }
    ~RoundNearest() { std::fesetround(saved); }
};
constexpr float kScale = 65536.0f, kGainStep = 0.03125f;
const float kFadeStep = std::bit_cast<float>(0x3d8b51d9u);   // 0.068: about 15 samples to silence
inline int32_t wrapAdd(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
}

void XtLinearPlayer::noteOn(const std::vector<float>& left, const std::vector<float>& right, float pitch_, float walk_) {
    x = &left;
    y = right.empty() ? nullptr : &right;
    pitch = pitch_;
    walk = walk_;
    const int32_t frames = (int32_t)left.size();
    fwd = Way {};
    bwd = Way {};
    fwd.end = frames - 1;                               // 0x894f3460: the block's +0xbc − 1
    bwd.end = startCfg;                                 // and +0xb8, the Start Offset
    fwd.a = loopCfg.fwdStart; fwd.b = loopCfg.fwdEnd; fwd.len = loopCfg.fwdEnd - loopCfg.fwdStart;
    bwd.a = loopCfg.bwdEnd; bwd.b = loopCfg.bwdStart; bwd.len = loopCfg.bwdEnd - loopCfg.bwdStart;
    fwd.mode = loopCfg.on ? loopCfg.fwdMode : 0;
    bwd.mode = loopCfg.on ? loopCfg.bwdMode : 0;
    fwd.rate = loopCfg.fwdRate;
    bwd.rate = loopCfg.bwdRate;
    frac = 0.0f;
    ramp = 1.0f;
    pos = turning = 0.0f > walk ? fwd.end : bwd.end;    // the walk's sign at the note's start, not the rate's
    // What the beat reads, the cached events as XtSoloVoice has them; the SOLO setup still runs beneath this (0x8950bf70)
    if (loopCfg.on) { lp[0] = loopCfg.fwdStart * 16; lp[1] = loopCfg.fwdEnd * 16; lp[2] = loopCfg.bwdEnd * 16; lp[3] = loopCfg.bwdStart * 16; }
    else { lp[0] = 0; lp[1] = lp[2] = 0x7fffffff; lp[3] = 0; }
    const RoundNearest nearest;
    // where the SOLO setup's seek left it (0x8950fed0); a LITE wave's, its first read, the voice's +0x10c (0x8950b7ec)
    beat_.start(tempoCfg, origCfg, liteCfg ? pos * 16 : 0.0f > walk ? frames * 16 : startCfg * 16);
}

void XtLinearPlayer::seek(int32_t pos16) {
    pos = pos16 >> 4;
    frac = 0.0f;
    beat_.reset((flags0 & 4) != 0);                     // 0x8950f900
}

// A sample as the firmware reads it, an int16.  Past the wave it reads what lies there, which is zeros in the check and something else on an XT.
int32_t XtLinearPlayer::sample(const std::vector<float>& v, int32_t i) const {
    return i >= 0 && (size_t)i < v.size() ? (int32_t)(v[(size_t)i] * 32768.0f) : 0;
}

// Past an end: that sample, fading.  Going backward the factor is −65536 and the sample's negated after it (0x89509c70).  The fade stops at
// `floor`: 0, but for the stereo routine's backward Alternate hold (setStaleFr11).
void XtLinearPlayer::hold(float* lr, int32_t at, float sign, float floor) {
    const float keep = ramp;
    float next = keep - kFadeStep;
    if (!(next > floor)) next = 0.0f;
    ramp = next;
    const auto one = [&](const std::vector<float>& v) {
        float o = (float)sample(v, at);
        o = o * gain;
        o = o * (sign * kScale);
        if (sign < 0.0f) o = -o;
        return o * keep;
    };
    if (y) { lr[0] = one(*x); lr[1] = one(*y); }
    else lr[0] = lr[1] = one(*x);
}

void XtLinearPlayer::block(float* lr) {
    if (!x) { for (int i = 0; i < 64; ++i) lr[i] = 0.0f; return; }
    const RoundToZero round;
    const float fs = [&] { const RoundNearest nearest; return beat_.block(flags0, walk, lp); }();   // 0x8950c270
    float rate;
    if (flags0 & 6) rate = (float)xtf::ftrc(fs) * 0x1p-25f;
    else rate = pitch * walk;

    const bool forward = rate > 0.0f;
    const Way& w = forward ? fwd : bwd;
    const float scale = forward ? kScale : -kScale;

    // One frame: x + d·f, FMAC, then the gain, then the scale, from the path's two samples.  `sx` is the first sample's sign in x; `sb` and `sa` the second's and the first's in d.
    const auto read = [&](float* out, float fo, int32_t ia, int sx, int32_t ib, int sb, int sa) {
        const auto one = [&](const std::vector<float>& v) {
            const int32_t a = sample(v, ia), b = sample(v, ib);
            const float p = (float)(sb * b + sa * a) * fo;
            const float o = p + (float)(sx * a);
            return (o * gain) * scale;
        };
        if (y) { out[0] = one(*x); out[1] = one(*y); }
        else out[0] = out[1] = one(*x);
    };
    const auto gainStep = [&] { const float d = gainTarget - gain; const float p = kGainStep * d; gain = p + gain; };
    // The fraction moves, and the whole part goes to the position
    const auto advance = [&](int32_t& at, float& whole) {
        frac = frac + rate;
        const int32_t n = xtf::ftrc(frac);
        whole = (float)n;
        at = wrapAdd(at, n);
    };

    if (forward && w.mode == 0) {                       // 0x89509980
        for (int i = 0; i < 32; ++i, lr += 2) {
            if (pos >= w.end) { pos = w.end; hold(lr, pos, 1.0f); gainStep(); continue; }
            const float fo = frac;
            const int32_t at = pos;
            float whole;
            advance(pos, whole);
            read(lr, fo, at, 1, at + 1, 1, -1);
            frac = frac - whole;
            gainStep();
        }
        turning = pos;
    }
    else if (forward && w.mode == 1) {                  // 0x89509a10
        if (!(w.a > pos) && !(pos > w.b)) rate = rate * w.rate;
        for (int i = 0; i < 32; ++i, lr += 2) {
            if (w.b > w.end && pos >= w.end) { pos = w.end; hold(lr, pos, 1.0f); gainStep(); continue; }
            const float fo = frac;
            const int32_t at = pos;
            int32_t next = wrapAdd(pos, 1);
            if (next > w.b) next = wrapAdd(next, -w.len);
            float whole;
            advance(pos, whole);
            while (pos > w.b && w.len > 0) pos = wrapAdd(pos, -w.len);   // on the XT a loop of no length would never end
            read(lr, fo, at, 1, next, 1, -1);
            frac = frac - whole;
            gainStep();
        }
        turning = pos;
    }
    else if (forward) {                                 // 0x89509ac0: Alternate.  `v` runs on through the way back, b to b + len
        int32_t v = turning;
        const int32_t top = wrapAdd(w.b, w.len), both = (int32_t)((uint32_t)w.len << 1);
        if (!(w.a > v) && !(v > top)) rate = rate * w.rate;
        const auto wrap = [&] { while (v > top && both > 0) v = wrapAdd(v, -both); };
        for (int i = 0; i < 32; ++i, lr += 2) {
            const float fo = frac;
            if (w.b > w.end && v >= w.end) { v = w.end; pos = w.end; hold(lr, pos, 1.0f); gainStep(); continue; }
            float whole;
            if (w.b > v) {                              // on the way there
                const int32_t at = v;
                advance(v, whole);
                wrap();
                pos = y ? at : v;                       // the stereo routine saves it before the step, the mono one after
                read(lr, fo, at, 1, at + 1, 1, -1);
            } else if (v == w.b) {                      // the turn: it interpolates toward the next sample back, with its sign flipped
                pos = v;
                const int32_t at = v;
                advance(v, whole);
                wrap();
                read(lr, fo, at, 1, at - 1, -1, -1);
            } else if (top > v) {                       // on the way back: mirrored, all flipped
                const int32_t m = wrapAdd((int32_t)((uint32_t)w.b << 1), -v);
                pos = m;
                advance(v, whole);
                wrap();
                read(lr, fo, m, -1, m - 1, -1, 1);
            } else {                                    // the other turn, into the first sample and on
                const int32_t m = wrapAdd(v, -both);
                pos = m;
                advance(v, whole);
                wrap();
                read(lr, fo, m, -1, m + 1, 1, 1);
            }
            frac = frac - whole;
            gainStep();
        }
        turning = v;
    }
    else if (w.mode == 0) {                             // 0x89509c70
        for (int i = 0; i < 32; ++i, lr += 2) {
            if (w.end >= pos) { pos = w.end; hold(lr, pos, -1.0f); gainStep(); continue; }
            const float fo = frac;
            const int32_t at = pos;
            float whole;
            advance(pos, whole);
            read(lr, fo, at, -1, at - 1, 1, -1);
            frac = frac - whole;
            gainStep();
        }
        turning = pos;
    }
    else if (w.mode == 1) {                             // 0x89509d00
        if (!(w.b > pos) && !(pos > w.a)) rate = rate * w.rate;
        for (int i = 0; i < 32; ++i, lr += 2) {
            if (w.end > w.b && w.end >= pos) { pos = w.end; hold(lr, pos, -1.0f); gainStep(); continue; }
            const float fo = frac;
            const int32_t at = pos;
            int32_t prev = wrapAdd(pos, -1);
            if (w.b > prev) prev = wrapAdd(prev, w.len);
            float whole;
            advance(pos, whole);
            while (w.b >= pos && w.len > 0) pos = wrapAdd(pos, w.len);
            read(lr, fo, at, -1, prev, 1, -1);
            frac = frac - whole;
            gainStep();
        }
        turning = pos;
    }
    else {                                              // 0x89509db0: Alternate going backward.  `v` runs on below the loop, to b − len
        int32_t v = turning;
        const int32_t bottom = wrapAdd(w.b, -w.len), both = (int32_t)((uint32_t)w.len << 1);
        if (!(v > w.a) && !(bottom > v)) rate = rate * w.rate;
        const auto wrap = [&] { while (bottom > v && both > 0) v = wrapAdd(v, both); };
        float cmp = y ? fr11 : 0.0f;                    // the stereo routine's hold compares with FR11 (setStaleFr11), the mono one with 0
        for (int i = 0; i < 32; ++i, lr += 2) {
            const float fo = frac;
            if (w.end > w.b && w.end >= v) { v = w.end; pos = w.end; hold(lr, pos, -1.0f, cmp); gainStep(); continue; }
            if (y) cmp = rate;                          // 0x8950a5bc: FR10:FR11 ← FR8:FR9, the fraction and the rate
            float whole;
            if (v > w.b) {                              // on the way there
                const int32_t at = v;
                advance(v, whole);
                wrap();
                pos = y ? at : v;
                read(lr, fo, at, -1, at - 1, 1, -1);
            } else if (v == w.b) {                      // the turn
                pos = v;
                const int32_t at = v;
                advance(v, whole);
                wrap();
                read(lr, fo, at, -1, at + 1, -1, -1);
            } else if (v > bottom) {                    // on the way back: mirrored
                const int32_t m = wrapAdd((int32_t)((uint32_t)w.b << 1), -v);
                pos = m;
                advance(v, whole);
                wrap();
                read(lr, fo, m, 1, m + 1, -1, 1);
            } else {                                    // the other turn: at the loop's other end, whatever `v` is
                pos = w.a;
                advance(v, whole);
                wrap();
                read(lr, fo, w.a, 1, w.a - 1, 1, 1);
            }
            frac = frac - whole;
            gainStep();
        }
        turning = v;
    }
}
