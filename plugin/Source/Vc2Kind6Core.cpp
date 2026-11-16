// Voice kind 6's core, 0x893eca10: see Vc2Kind6.h (v225).  A sample at a time: the epoch's step, a grain's start, the
// two heads under their windows, the lift, the SVF's high-pass, the unvoiced mix and the gain.  Its arithmetic in
// rom.4's order, in XtFloat.
#include "Vc2Kind6.h"
#include "XtFloat.h"
#include <cmath>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;

constexpr int32_t kRingMask = Vc2MicRingWords::kSamples - 1;
constexpr int32_t kHistoryMask = Vc2MicRingWords::kBlocks - 1;
} // namespace

void Vc2Kind6::stepEpoch(const Vc2MicRing& ring) {
    const Vc2Kind6Tables& t = vc2Kind6Tables();
    const float next = add(w.period, w.epoch);
    float now = xtf::flt(w.position);
    if (w.epoch > now) now = add(now, t.ringSpan);
    if (next > now) return;
    w.epoch = t.ringSpan > next ? next : add(next, t.ringBack);
    w.lastPeriod = w.period;
    const int32_t block = xtf::ftrc(add(mul(next, t.coreBlock), t.blockAhead));   // FMAC: the block 50 samples on
    w.period = ring.w.history[block & kHistoryMask];
}

void Vc2Kind6::startGrain(const Vc2MicRing& ring) {
    const float lastLength = w.grain;
    w.sinceGrain = sub(w.sinceGrain, lastLength);
    const float carried = mul(w.windowStep, lastLength);   // where the other head's window was at the last start
    grainLength(ring);
    w.head = (int8_t)(w.head ^ 1);
    const float into = mul(w.sinceGrain, w.windowStep);
    Vc2Kind6Head& starting = w.head == 0 ? w.a : w.b;
    Vc2Kind6Head& other = w.head == 0 ? w.b : w.a;
    starting.window = into;
    other.window = add(carried, into);
    starting.count = w.sinceGrain;
    starting.base = w.epoch;
    starting.period = w.lastPeriod;
    starting.frac = w.frac;
}

float Vc2Kind6::headSample(const Vc2MicRing& ring, Vc2Kind6Head& h) {
    const float count = h.count;
    h.count = add(count, 1.0f);
    const float at = add(mul(w.rate, count), h.base);   // FMAC
    const int32_t whole = xtf::ftrc(at);
    const float a0 = ring.w.ring[(whole + Vc2MicRingWords::kSamples) & kRingMask];
    const float a1 = ring.w.ring[(whole + Vc2MicRingWords::kSamples + 1) & kRingMask];
    return add(mul(sub(a1, a0), sub(at, xtf::flt(whole))), a0);   // FMAC
}

void Vc2Kind6::core(const Vc2MicRing& ring, const float* unvoiced, float* out) {
    const Vc2Kind6Tables& t = vc2Kind6Tables();
    for (int i = 0; i < kBlock; ++i) {
        stepEpoch(ring);
        if (!(w.grain > w.sinceGrain)) startGrain(ring);
        const float windowA = 1.0f > w.a.window ? vc2Window(t.math, w.a.window) : 0.0f;
        const float windowB = 1.0f > w.b.window ? vc2Window(t.math, w.b.window) : 0.0f;
        w.sinceGrain = add(w.sinceGrain, 1.0f);
        w.a.window = add(w.a.window, w.windowStep);
        w.b.window = add(w.b.window, w.windowStep);
        const float sampleA = headSample(ring, w.a);
        const float sampleB = headSample(ring, w.b);
        const float mix = add(mul(sampleB, windowB), mul(windowA, sampleA));   // FMAC
        float lifted = 0.0f;   // below r′ 1, the mix's magnitude by a depth the lift's envelope follows
        if (1.0f > w.effective) {
            float depth = mul(sub(1.0f, w.effective), t.liftGain);
            if (depth > t.liftMost) depth = t.liftMost;
            lifted = mul(std::fabs(mix), mul(depth, w.level));
        }
        const float windows = add(windowA, windowB);
        w.lift = add(mul(sub(lifted, w.lift), t.liftRate), w.lift);
        w.low = add(mul(w.band, w.f), w.low);
        const float in = add(mul(windows, w.lift), mix);
        const float high = sub(in, add(mul(w.damping, w.band), w.low));
        w.band = add(mul(w.f, high), w.band);
        const float u = unvoiced[i];
        const float raw = ring.w.ring[w.position];
        w.position = (w.position + 1) & kRingMask;
        float y = add(mul(mul(raw, u), ring.g.unvoiceLevel), mul(sub(1.0f, u), high));   // FMAC
        w.gain = add(mul(sub(w.gainTarget, w.gain), t.coreBlock), w.gain);
        y = mul(y, w.gain);
        out[2 * i] = y;
        out[2 * i + 1] = y;
    }
}
