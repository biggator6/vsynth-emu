// The LITE player with Vari on: see XtLitePlayer.h.  0x8950a7f0 (mono) and 0x8950a7fc (stereo): six paths each, by direction and
// Loop Mode (the tables at 0x8950a870 and 0x8950a890), over four kernels, forward and backward, mono and stereo (0x8950b0b8,
// 0x8950b1d4, 0x8950b2f4, 0x8950b44a).
#include "XtLitePlayer.h"
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
const float kRate = std::bit_cast<float>(0x38156591u);   // π/2 / 44100, the filter's rate per Hz
// FLOAT of a word the firmware takes unsigned: 2^32 more for one with its top bit set (0x8950b8e6)
float floatU(int32_t v) { float f = (float)v; if (0 > v) f = f + 4294967296.0f; return f; }
}

// A sample as the firmware reads it, an int16.  Past the wave, zeros here; on an XT, what lies there.
int32_t XtLitePlayer::sample(const std::vector<float>& v, int32_t i) const {
    return i >= 0 && (size_t)i < v.size() ? (int32_t)(v[(size_t)i] * 32768.0f) : 0;
}

void XtLitePlayer::noteOn(const std::vector<float>& left, const std::vector<float>& right, float pitch_, float formant_, float walk_, float root) {
    const RoundToZero round;
    x = &left;
    y = right.empty() ? nullptr : &right;
    pitch = pitch_; formant = formant_; walk = walk_;
    const int32_t frames = (int32_t)left.size();
    // 0x8950b8d0: the wave's ends, the loops' rates (the record's +0x14, +0x16), the period
    firstF = floatU(startCfg);
    lastF = floatU(frames - 1);
    fwdRate = loopCfg.fwdRate;
    bwdRate = loopCfg.bwdRate;
    period = 88200.0f / root;
    invPeriod = 1.0f / period;
    // The heads' fractions start at the formant's (sic: +0x148, the formant the note-on's converter gave)
    fracA = formant - (float)xtf::ftrc(formant);
    fracB = formant - (float)xtf::ftrc(formant);
    winA = clkA = 0.0f;
    winB = period * 0.5f;
    clkB = period * 0.5f;
    dcL = dcR = 0.0f;
    dcRate = root * pitch;
    dcRate = dcRate * kRate;
    // The loop record's points, as floats (loop on or off)
    fwdStartF = (float)loopCfg.fwdStart;
    fwdEndF = (float)loopCfg.fwdEnd;
    fwdLenF = (float)(loopCfg.fwdEnd - loopCfg.fwdStart);
    bwdEndF = (float)loopCfg.bwdEnd;
    bwdStartF = (float)loopCfg.bwdStart;
    bwdLenF = (float)(loopCfg.bwdEnd - loopCfg.bwdStart);
    // Walking forward the walk and both heads start at the Start Offset; otherwise at the last half period's start below the end
    if (walk > 0.0f) {
        walkPos = floatU(startCfg);
        hA = hB = startCfg;
    } else {
        const float half = period / 2.0f;
        float e = floatU(frames - 1);
        e = e / half;
        float t = (float)xtf::ftrc(e);
        t = t * half;
        walkPos = t;
        hA = xtf::ftrc(t);
        hB = xtf::ftrc(walkPos);
    }
    pos16 = (int32_t)((uint32_t)xtf::ftrc(walkPos) << 4);
    fwdMode = loopCfg.on ? loopCfg.fwdMode : 0;
    bwdMode = loopCfg.on ? loopCfg.bwdMode : 0;
    // What the beat reads, the cached events, as the other players have them
    if (loopCfg.on) { lp[0] = loopCfg.fwdStart * 16; lp[1] = loopCfg.fwdEnd * 16; lp[2] = loopCfg.bwdEnd * 16; lp[3] = loopCfg.bwdStart * 16; }
    else { lp[0] = 0; lp[1] = lp[2] = 0x7fffffff; lp[3] = 0; }
    const RoundNearest nearest;
    beat_.start(tempoCfg, origCfg, pos16);                // 0x8950f860, from +0x38
}

// 0x8950bc60: what the update sets, the filter's rate from the new pitch
void XtLitePlayer::setRates(float pitch_, float formant_, float walk_, float root) {
    const RoundToZero round;
    pitch = pitch_; formant = formant_; walk = walk_;
    dcRate = root * pitch;
    dcRate = dcRate * kRate;
}

// 0x8950bdda: as the note-on has them (0x8950b8d0), from the voice and the record as they stand
void XtLitePlayer::blockUpdate(const XtLoop& l, int32_t start, int32_t frames) {
    const RoundToZero round;
    firstF = floatU(start);
    lastF = floatU(frames - 1);
    fwdStartF = (float)l.fwdStart;
    fwdEndF = (float)l.fwdEnd;
    fwdLenF = (float)(l.fwdEnd - l.fwdStart);
    bwdEndF = (float)l.bwdEnd;
    bwdStartF = (float)l.bwdStart;
    bwdLenF = (float)(l.bwdEnd - l.bwdStart);
    fwdMode = l.on ? l.fwdMode : 0;                       // 0x8950be5c: the byte 0's Loop bit, just set from the block's +0xb4
    bwdMode = l.on ? l.bwdMode : 0;
}

void XtLitePlayer::seek(int32_t p16) {
    const RoundToZero round;
    const int32_t s = p16 >> 4;
    walkPos = (float)s;
    fracA = fracB = 0.0f;
    hA = hB = s;
    winA = clkA = 0.0f;
    winB = period * 0.5f;
    clkB = period * 0.5f;
    beat_.reset((flags0 & 4) != 0);                       // 0x8950f900
}

// One head's window (0x8950b0c0, the same in all four kernels): a triangle over the phase, 0 to 1 and back over a period.  While
// the clock is under the period it and the phase step by the rate, the phase at least by 1 (as the firmware does it: back by the
// rate, then on by 1).  Past it the head splices to the last half period below the walk, and the phase starts again from the clock.
float XtLitePlayer::window(float& win, float& clk, int32_t& head, float at, float rate) const {
    float w = win * invPeriod;
    w = w + w;
    if (w > 1.0f) w = -w + 2.0f;
    if (0.0f > w) w = 0.0f;
    if (period > clk) {
        clk = clk + rate;
        win = win + rate;
        if (!(rate > 1.0f)) { win = win - rate; win = win + 1.0f; }
    } else {
        clk = clk - period;
        win = clk;
        float t = 2.0f * at;
        t = t * invPeriod;
        t = (float)xtf::ftrc(t);
        float h = 0.5f * period;
        h = h * t;
        head = xtf::ftrc(h);
    }
    return w;
}

// One frame: both windows, then both heads' reads at the formant's rate, interpolated, under their windows; the sum through the
// gain and the filter.  Going backward a head reads the other way, each sample negated, and the scale stays +65536.
template <bool Forward>
void XtLitePlayer::frame(float* lr, float at, float rate, int32_t end) {
    const float wa = window(winA, clkA, hA, at, rate);
    const float wb = window(winB, clkB, hB, at, rate);
    const auto read = [&](const std::vector<float>& v, int32_t head, float frac, float w) {
        float a = (float)sample(v, head);
        if (Forward) {
            float d = (float)sample(v, head + 1);
            d = d - a;
            a = d * frac + a;
        } else {
            a = -a;
            float d = (float)sample(v, head - 1);
            d = d + a;
            a = d * frac + a;
        }
        return a * w;
    };
    const auto step = [&](int32_t& head, float& frac) {
        frac = Forward ? frac + formant : frac - formant;
        const int32_t n = xtf::ftrc(frac);
        head = (int32_t)((uint32_t)head + (uint32_t)n);
        frac = frac - (float)n;
    };
    float aL = 0.0f, aR = 0.0f, bL = 0.0f, bR = 0.0f;
    if (Forward ? end > hA : hA > end) {
        aL = read(*x, hA, fracA, wa);
        if (y) aR = read(*y, hA, fracA, wa);
        step(hA, fracA);
    }
    if (Forward ? end > hB : hB > end) {
        bL = read(*x, hB, fracB, wb);
        if (y) bR = read(*y, hB, fracB, wb);
        step(hB, fracB);
    }
    if (y) {                                              // 0x8950b41a
        float l = bL + aL, r = bR + aR;
        l = l * gain; r = r * gain;
        l = l * kScale; r = r * kScale;
        const float d = gainTarget - gain;
        gain = kGainStep * d + gain;
        l = l - dcL; r = r - dcR;
        lr[0] = l; lr[1] = r;
        const float tl = l * dcRate;
        dcL = dcL + tl;
        const float tr = r * dcRate;
        dcR = dcR + tr;
    } else {                                              // 0x8950b1ae
        float o = bL + aL;
        o = o * gain;
        o = o * kScale;
        const float d = gainTarget - gain;
        o = o - dcL;
        lr[0] = lr[1] = o;
        gain = kGainStep * d + gain;
        const float t = o * dcRate;
        dcL = dcL + t;
    }
}

void XtLitePlayer::block(float* lr) {
    if (!x) { for (int i = 0; i < 64; ++i) lr[i] = 0.0f; return; }
    const RoundToZero round;
    // 0x8950a808: the walk step from the beat (0x8950c270, +0x34), × 2^-25
    const float fs = [&] { const RoundNearest nearest; return beat_.block(flags0, walk, lp); }();
    const float step = (float)xtf::ftrc(fs) * 0x1p-25f;
    float pr = pitch;
    if (step > 0.0f) {
        const int32_t end = xtf::ftrc(lastF);
        if (fwdMode == 1) { if (!(fwdStartF > walkPos) && !(walkPos > fwdEndF)) pr = pr * fwdRate; }
        else if (fwdMode == 2) { const float e = fwdEndF + fwdLenF; if (!(fwdStartF > walkPos) && !(walkPos > e)) pr = pr * fwdRate; }
        for (int i = 0; i < 32; ++i, lr += 2) {
            float at;
            if (fwdMode == 0) {                           // 0x8950a8c0
                if (lastF > walkPos) { at = walkPos; walkPos = walkPos + step; }
                else at = lastF + period;
            } else if (fwdMode == 1) {                    // 0x8950a950: One Way
                if (fwdEndF > lastF && !(lastF > walkPos)) at = lastF + period;
                else {
                    while (walkPos > fwdEndF && fwdLenF > 0.0f) walkPos = walkPos - fwdLenF;   // on the XT a loop of no length never ends
                    at = walkPos;
                    walkPos = walkPos + step;
                }
            } else {                                      // 0x8950aa00: Alternate, the walk running on through the way back, mirrored
                if (fwdEndF > lastF && !(lastF > walkPos)) at = lastF + period;
                else {
                    const float e = fwdEndF + fwdLenF;
                    while (walkPos > e && fwdLenF > 0.0f) { walkPos = walkPos - fwdLenF; walkPos = walkPos - fwdLenF; }
                    at = walkPos;
                    if (!(fwdEndF > walkPos)) { at = -at; at = at + fwdEndF; at = at + fwdEndF; }
                    walkPos = walkPos + step;
                }
            }
            frame<true>(lr, at, pr, end);
        }
    } else {
        const int32_t end = xtf::ftrc(firstF);
        if (bwdMode == 1) { if (!(bwdStartF > walkPos) && !(walkPos > bwdEndF)) pr = pr * bwdRate; }
        else if (bwdMode == 2) { const float e = bwdStartF - bwdLenF; if (!(walkPos > bwdEndF) && !(e > walkPos)) pr = pr * bwdRate; }
        for (int i = 0; i < 32; ++i, lr += 2) {
            float at;
            if (bwdMode == 0) {                           // 0x8950aac0
                if (walkPos > firstF) { at = walkPos; walkPos = walkPos + step; }
                else at = firstF;
            } else if (bwdMode == 1) {                    // 0x8950ab50: One Way
                if (firstF > bwdStartF && !(walkPos > firstF)) at = firstF;
                else {
                    while (bwdStartF > walkPos && bwdLenF > 0.0f) walkPos = walkPos + bwdLenF;
                    at = walkPos;
                    walkPos = walkPos + step;
                }
            } else {                                      // 0x8950ac00: Alternate
                if (firstF > bwdStartF && !(walkPos > firstF)) at = firstF;
                else {
                    while (bwdStartF - bwdLenF > walkPos && bwdLenF > 0.0f) { walkPos = walkPos + bwdLenF; walkPos = walkPos + bwdLenF; }
                    at = walkPos;
                    if (!(walkPos > bwdStartF)) { at = -at; at = at + bwdStartF; at = at + bwdStartF; }
                    walkPos = walkPos + step;
                }
            }
            frame<false>(lr, at, pr, end);
        }
    }
}
