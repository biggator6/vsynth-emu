// The VC-2's voice kind 6 and the mic ring: see Vc2Kind6.h (v225).  Each routine's arithmetic in its own order, in
// XtFloat; every address is rom.4's.  The core (0x893eca10) is Vc2Kind6Core.cpp.
#include "Vc2Kind6.h"
#include "XtFloat.h"

namespace {
using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

constexpr int32_t kRingMask = Vc2MicRingWords::kSamples - 1;
constexpr int32_t kHistoryMask = Vc2MicRingWords::kBlocks - 1;
constexpr int32_t kBehind = 14 << 8;   // [FW] 0x893ec8dc: the start's 0xe00, 4096 − 512, so 512 samples back
constexpr int32_t kModes = 2;          // [FW] the mode setter's 0 and 1
} // namespace

void Vc2MicRing::setGlobals() {
    const Vc2Kind6Tables& t = vc2Kind6Tables();
    g.a440Period = t.a440Period;
    g.unread = t.unreadRate;
    g.perA440 = t.perA440;
}

void Vc2MicRing::reset() {
    w.writeIndex = 0;
    w.started = 0;
    w.historyIndex = 0;
    if (mode != 0) {
        mode = 0;
        w.pitchMode = 1;
        w.started = 0;
    }
    for (float& x : w.ring) x = 0.0f;
    for (float& p : w.history) p = g.a440Period;
}

void Vc2MicRing::write(const float* block, float period) {
    w.history[w.historyIndex] = period;
    float* to = &w.ring[w.writeIndex];   // the block's 32 go on from here: from a multiple of 32 they never pass the end
    w.writeIndex = (w.writeIndex + Vc2Kind6::kBlock) & kRingMask;
    w.historyIndex = (w.historyIndex + 1) & kHistoryMask;
    for (int i = 0; i < Vc2Kind6::kBlock; ++i) to[i] = block[i];
}

void Vc2MicRing::setMode(int32_t m) {
    if (m < 0 || m >= kModes || m == mode) return;
    mode = m;
    w.pitchMode = m == 0 ? 1 : 0;
    w.started = 0;
}

void Vc2Kind6::set(const Vc2Kind6Params* p) {
    if (p == nullptr) return;
    w.ratioTarget = p->ratio;
    w.gainTarget = mul(p->gain, vc2Kind6Tables().gainScale);
    w.rateTarget = p->rate;
    w.levelTarget = p->level;
}

void Vc2Kind6::setAndStart(Vc2MicRing& ring, const Vc2Kind6Params& p) {
    set(&p);
    start(ring);
}

void Vc2Kind6::start(Vc2MicRing& ring) {
    const Vc2Kind6Tables& t = vc2Kind6Tables();
    w.position = (ring.w.writeIndex + kBehind) & kRingMask;
    w.epoch = xtf::flt(w.position);
    const int32_t block = xtf::ftrc(mul(xtf::flt(w.position), t.perBlock));   // the position's block, under 128
    w.lastPeriod = w.period = ring.w.history[block];
    w.gain = 0.0f;
    w.ratio = w.ratioTarget;
    w.rate = w.rateTarget;
    w.level = w.levelTarget;
    w.f = w.low = w.band = 0.0f;
    w.damping = t.startDamping;
    w.head = 0;
    w.sinceGrain = w.a.count = w.b.count = 0.0f;
    w.a.base = w.b.base = w.epoch;
    w.a.window = 0.0f;
    w.b.window = t.startWindow;
    w.lift = 0.0f;
    grainLength(ring);
    w.frac = 1.0f;
    w.a.period = w.b.period = w.period;
    w.a.frac = w.b.frac = w.frac;
    ring.w.started = 1;
}

void Vc2Kind6::render(const Vc2MicRing& ring, const float* unvoiced, float* out) {
    if (ring.w.started == 0) return;
    const float glide = vc2Kind6Tables().glide;
    w.ratio = add(mul(sub(w.ratioTarget, w.ratio), glide), w.ratio);
    w.rate = add(mul(sub(w.rateTarget, w.rate), glide), w.rate);
    w.level = add(mul(sub(w.levelTarget, w.level), glide), w.level);
    filterTargets();
    core(ring, unvoiced, out);
}

void Vc2Kind6::grainLength(const Vc2MicRing& ring) {
    const Vc2Kind6Tables& t = vc2Kind6Tables();
    float now = xtf::flt(w.position);
    if (w.epoch > now) now = add(now, t.span);   // the position has wrapped past the ring's end, the epoch not yet
    const float at = div(sub(now, add(w.sinceGrain, w.epoch)), w.period);
    w.frac = at;
    float between = at;
    if (0.0f > at) {
        w.frac = between = 0.0f;
    }
    if (between > 1.0f) {
        w.frac = between = 1.0f;
    }
    const float in = add(mul(sub(w.period, w.lastPeriod), between), w.lastPeriod);   // T_in
    float ratio = w.ratio;
    if (ring.w.pitchMode != 0) ratio = mul(mul(ratio, in), ring.g.perA440);   // mode 0: P+0 a pitch over 440 Hz
    const float length = div(in, ratio);
    w.effective = ratio;
    w.grain = length;
    const float reach = div(in, w.rate);
    w.windowStep = div(t.half, reach > length ? length : reach);
}

void Vc2Kind6::filterTargets() {
    const Vc2Kind6Tables& t = vc2Kind6Tables();
    float damping = t.damping;
    if (!(1.0f > w.effective)) {
        damping = 2.0f > w.effective ? sub(damping, mul(w.level, add(w.effective, -1.0f))) : sub(damping, w.level);
    }
    float x = div(1.0f, w.grain);   // the output's frequency over the rate, at most ½
    if (x > t.half) x = t.half;
    float f = mul(vc2Cubic(t.math, x), 2.0f);
    const float most = sub(t.fMost, damping);
    if (f > most) f = most;
    w.damping = add(mul(sub(damping, w.damping), t.filterGlide), w.damping);
    w.f = add(mul(sub(f, w.f), t.filterGlide), w.f);
}
