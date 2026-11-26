// The VC-2 tone's OSC mix, its eight renders: see Vc2ToneMix.h (v269).  Each is rom.4's routine on 32 stereo frames of the
// OSC voices' buffers a and b into out, single precision as the SH-4 computes it (XtFloat.h).
#include "Vc2ToneMix.h"
#include "Vc2ToneMixRoutines.h"
#include "Vc2ToneMixTables.h"
#include "XtFloat.h"
#include <algorithm>
#include <cmath>

Vc2ToneMixRings::Ring& Vc2ToneMixRings::a(uint32_t address) { return rings_[(address - kAAt) / kBytes]; }
Vc2ToneMixRings::Ring& Vc2ToneMixRings::b(uint32_t address) { return rings_[kTones + (address - kBAt) / kBytes]; }

// v337: by its place among all 48, A's then B's end to end as rom.4 lays them; past them 0.  [FW] v364: its address the
// ring's and 4 × i in 32 bits, as rom.4's delay renders form it (0x893e47d4, 0x893e48c2, 0x893e490e and their pairs)
float Vc2ToneMixRings::at(const Ring& ring, int i) const {
    constexpr uint32_t kFloat = sizeof(float);
    const uint32_t at = kAAt + (uint32_t)(&ring - rings_.data()) * kBytes + (uint32_t)i * kFloat;
    const uint32_t k = (at - kAAt) / kFloat;
    if (k < (uint32_t)rings_.size() * kSamples) return rings_[k / kSamples][k % kSamples];
    if (past != nullptr) ++*past;
    return 0.0f;
}

namespace {
using namespace xtf;
constexpr int kFrames = 32, kSamples = 2 * kFrames;
constexpr int kFollowerRun = 8, kHoldFrames = 4;   // [FW] 0x893bf120's run of frames; a follower's hold moves each 4
constexpr int kRamped = 3;                         // [FW] the gains a ramping render moves
using Ramped = std::array<float, kRamped>;

struct MixBlock {
    Vc2MixWords& w;
    Vc2ToneTail& tail;
    Vc2ToneMixRings& rings;
    const float* a;
    const float* b;
    float* out;
};

// A block whose first three gains ramp toward their targets, `frame(sample, gains)` for each stereo frame
template <class Frame>
void rampedBlock(MixBlock& m, Frame frame) {
    Ramped g {}, step {};
    std::copy_n(m.w.gain.begin(), kRamped, g.begin());
    for (size_t k = 0; k < g.size(); ++k) step[k] = mul(sub(m.w.target[k], g[k]), vc2ToneMixTables().ramp);
    for (int s = 0; s < kSamples; s += 2) {
        frame(s, g);
        for (size_t k = 0; k < g.size(); ++k) g[k] = add(g[k], step[k]);
    }
    std::copy_n(g.begin(), kRamped, m.w.gain.begin());
}

// 0x893e4600: each OSC by two gains, its left channel by the first and its right by the second
void fourGains(MixBlock& m) {
    auto& g = m.w.gain;
    for (int s = 0; s < kSamples; s += 2) {
        m.out[s] = add(mul(m.b[s], g[2]), mul(m.a[s], g[0]));
        m.out[s + 1] = add(mul(m.b[s + 1], g[3]), mul(m.a[s + 1], g[1]));
    }
    const float settle = vc2ToneMixTables().settle;
    for (size_t k = 0; k < g.size(); ++k) g[k] = add(mul(settle, sub(m.w.target[k], g[k])), g[k]);
}

// 0x893e4680: OSC 2 scaled and offset, times OSC 1
void ringProduct(MixBlock& m) {
    rampedBlock(m, [&](int s, const Ramped& g) {
        for (int c = s; c < s + 2; ++c) m.out[c] = mul(mul(add(mul(m.b[c], g[0]), g[1]), g[2]), m.a[c]);
    });
}

// one-off: XtAnalog's lineTap has this shape on a line of signed bytes scaled by 1/127; this one reads floats as they stand
// A delay line read `delay` samples behind `position`, between its two nearest samples, each brought back once by the
// line's length: from v337 where that leaves it, in the line or beside it (Vc2ToneMixRings::at)
float delayedSample(const Vc2ToneMixRings& rings, const Vc2ToneMixRings::Ring& ring, int position, float delay) {
    const int whole = ftrc(delay);
    const float part = sub(delay, flt(whole));
    // [FW] v364: rom.4's sub and add #-1 (0x893e48a8, 0x893e48b0) in 32 bits, which a whole of -2^31 wraps; C++'s int
    // subtract would overflow there
    int near = (int)((uint32_t)position - (uint32_t)whole), far = (int)((uint32_t)near - 1u);
    if (near < 0) near += Vc2ToneMixRings::kSamples;
    if (far < 0) far += Vc2ToneMixRings::kSamples;
    return add(mul(rings.at(ring, near), sub(1.0f, part)), mul(rings.at(ring, far), part));
}

int nextPosition(int position) { return position + 1 >= Vc2ToneMixRings::kSamples ? 0 : position + 1; }

// 0x893e4740: OSC 1 through one delay line, OSC 2's left channel moving the delay, both channels alike
void delayMono(MixBlock& m) {
    const Vc2ToneMixTables& t = vc2ToneMixTables();
    Vc2ToneMixRings::Ring& ring = m.rings.a(m.w.ringA);
    int position = m.w.position;
    rampedBlock(m, [&](int s, const Ramped& g) {
        position = nextPosition(position);
        ring[(size_t)position] = m.a[s];
        const float delay = div(mul(mul(add(m.b[s], t.fullScale), g[0]), t.delaySpan), t.fullScale);
        const float wet = add(mul(m.a[s], g[1]), delayedSample(m.rings, ring, position, delay));
        m.out[s] = m.out[s + 1] = mul(wet, g[2]);
    });
    m.w.position = position;
}

// 0x893e4820: each channel of OSC 1 through its own delay line, moved by that channel of OSC 2
void delayStereo(MixBlock& m) {
    const Vc2ToneMixTables& t = vc2ToneMixTables();
    Vc2ToneMixRings::Ring* const rings[2] = { &m.rings.a(m.w.ringA), &m.rings.b(m.w.ringB) };
    int position = m.w.position;
    rampedBlock(m, [&](int s, const Ramped& g) {
        position = nextPosition(position);
        const float depth = mul(g[0], t.delaySpan);
        for (int c = 0; c < 2; ++c) (*rings[c])[(size_t)position] = m.a[s + c];
        for (int c = 0; c < 2; ++c) {
            const float delay = div(mul(add(m.b[s + c], t.fullScale), depth), t.fullScale);
            const float wet = add(mul(m.a[s + c], g[1]), delayedSample(m.rings, *rings[c], position, delay));
            m.out[s + c] = mul(wet, g[2]);
        }
    });
    m.w.position = position;
}

// 0x893bf120: OSC 2's left channel's level over the block, the last frame of each run counted twice
float followerLevel(const float* b) {
    float sum = 0.0f;
    for (int f = 0; f < kFrames; ++f) {
        const float x = std::fabs(b[2 * f]);
        sum = add(sum, x);
        if (f % kFollowerRun == kFollowerRun - 1) sum = add(sum, x);
    }
    return mul(vc2ToneMixTables().follower, sum);
}

// 0x893e4250 (mono: OSC 1's left on both channels) and 0x893e4320 (stereo): OSC 1 by a gain following OSC 2's level
void followed(MixBlock& m, int rightOffset) {
    const Vc2ToneMixTables& t = vc2ToneMixTables();
    float gain = m.w.gain[0], hold = m.w.hold;
    const float step = mul(sub(m.w.target[2], gain), t.ramp);
    const float level = followerLevel(m.b);
    const float setting = m.w.level > level ? m.w.target[1] : m.w.target[0];
    const float rate = add(mul(sub(1.0f, setting), t.rateSpan), t.rateFloor);
    m.w.level = level;
    for (int f = 0; f < kFrames; ++f) {
        gain = add(gain, step);
        const float g = mul(hold, gain);
        m.out[2 * f] = mul(m.a[2 * f], g);
        m.out[2 * f + 1] = mul(m.a[2 * f + rightOffset], g);
        if (f % kHoldFrames == kHoldFrames - 1) hold = add(mul(sub(level, hold), rate), hold);
    }
    m.w.gain[0] = gain;
    m.w.hold = hold;
}

void followedMono(MixBlock& m) { followed(m, 0); }
void followedStereo(MixBlock& m) { followed(m, 1); }

// 0x893adb90: from OSC 2 to OSC 1 by the hold
void blend(MixBlock& m) {
    const float g = m.w.hold, other = sub(1.0f, g);
    for (int s = 0; s < kSamples; ++s) m.out[s] = add(mul(m.a[s], g), mul(m.b[s], other));
}

// 0x893adb30: OSC 1 less what it held at the last frame the sync mask marks
void heldOff(MixBlock& m) {
    const float g = m.w.hold, scale = sub(1.0f, mul(g, vc2ToneMixTables().half));
    auto& held = m.w.gain;
    for (int f = 0; f < kFrames; ++f) {
        for (int c = 0; c < 2; ++c) {
            if ((m.tail.sync >> f & 1u) != 0) held[(size_t)c] = mul(m.a[2 * f + c], g);
            m.out[2 * f + c] = mul(sub(m.a[2 * f + c], held[(size_t)c]), scale);
        }
    }
}

using MixRender = void (*)(MixBlock&);
constexpr std::array<MixRender, kVc2MixRoutines.size()> kRenders = {
    fourGains, ringProduct, delayMono, delayStereo, followedMono, followedStereo, blend, heldOff,
};
} // namespace

void Vc2ToneMix::render(const float* a, const float* b, float* out, Vc2ToneTail& tail, Vc2ToneMixRings& rings) {
    const auto* found = std::ranges::find(kVc2MixRoutines, tail.unread[kVc2MixRoutineWord]);
    if (found == kVc2MixRoutines.end()) return;
    MixBlock block { w, tail, rings, a, b, out };
    kRenders[(size_t)(found - kVc2MixRoutines.begin())](block);
}
