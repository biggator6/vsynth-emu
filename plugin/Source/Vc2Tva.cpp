// The VC-2's TVA: see Vc2Tva.h (v233).  Every address is rom.4's; each constant is its routine's pool word
// (Vc2TvaTables).
#include "Vc2Tva.h"
#include "Vc2TvaTables.h"
#include "XtFloat.h"
#include <algorithm>
#include <cmath>

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

namespace {
// [FW] the block's words it reads: the LFO's depth, the level, the pan LFO's depth, the eight words the note-on copies,
// the word whose value above 0 puts +0x48 and +0x4c on the gains and targets, and those two
constexpr uint32_t kDepth = 0x08, kLevel = 0x0c, kPanDepth = 0x1c, kLevelsFrom = 0x20, kPanned = 0x40;
constexpr uint32_t kLeft = 0x48, kRight = 0x4c;
// [FW] the OS's event a told truncation's end sets, and its flags (0x893b271a)
constexpr uint32_t kFadedEvent = 0x8994fec8, kFadedFlags = 0x1000;

// Within ±1, as rom.4 holds a gain or a target: above 1 to 1, below −1 to −1
float within1(float x) {
    if (x > 1.0f) return 1.0f;
    return -1.0f > x ? -1.0f : x;
}
} // namespace

void Vc2Tva::construct() {
    w.truncatedTold = 0;
    w.stopped = 0;
    w.truncated = 0;
    w.envelope = 0;
    w.lfo = 0;
    w.panLfo = 0;
    w.zero = 0.0f;
    w.one = 1.0f;
    w.fade = 1.0f;
}

// The block's level × the envelope's output, and each LFO's output × its depth from the block
Vc2TvaEnvelope Vc2Tva::products(Vc2TvaParts& parts) {
    const Vc2TvaEnvelope e = parts.envelope(w.envelope);
    w.levelEnv = mul(e.out, blockF(kLevel));
    w.depth = mul(parts.lfoOut(w.lfo), blockF(kDepth));
    w.pan = mul(parts.lfoOut(w.panLfo), blockF(kPanDepth));
    return e;
}

// Two gains or targets from a level, as the note-on and the update form them: (1 + the LFO's product) × the level,
// times 1 − the pan's and 1 + the pan's, then the block's two factors when its +0x40 is above 0, each within ±1
void Vc2Tva::aim(float level, float* to) const {
    const float x = mul(add(w.depth, 1.0f), level);
    to[0] = mul(x, sub(1.0f, w.pan));
    to[1] = mul(x, add(w.pan, 1.0f));
    if (blockF(kPanned) > 0.0f) {
        to[0] = mul(to[0], blockF(kLeft));
        to[1] = mul(to[1], blockF(kRight));
    }
    to[0] = within1(to[0]);
    to[1] = within1(to[1]);
}

void Vc2Tva::stepsOver(float samples) {
    for (size_t c = 0; c < w.step.size(); ++c) w.step[c] = div(sub(w.target[c], w.gain[c]), samples);
}

void Vc2Tva::noteOn(const uint32_t* block, uint32_t oscFlag, Vc2TvaParts& parts) {
    const Vc2TvaTables& t = vc2TvaTables();
    w.oscFlag = oscFlag;
    std::copy_n(block, w.block.size(), w.block.begin());
    w.elapsed = 0.0f;
    const Vc2TvaEnvelope e = products(parts);
    // the ramp's time, ms: the attack's, at most 10; with no attack, the next stage's; with neither, 0.1
    float ms = e.attack;
    if (e.attack == 0.0f) ms = e.next == 0.0f ? t.onShortest : e.next;
    else if (!(t.onLongest > e.attack)) ms = t.onLongest;
    // the gains start from the envelope's onset, and ramp to its output
    aim(mul(e.onset, blockF(kLevel)), w.gain.data());
    aim(w.levelEnv, w.target.data());
    stepsOver(mul(ms, t.onPerMs));
    std::copy_n(w.block.begin() + kLevelsFrom / 4, w.levels.size(), w.levels.begin());
}

void Vc2Tva::newBlock(const uint32_t* block) { std::copy_n(block, w.block.size(), w.block.begin()); }

void Vc2Tva::update(Vc2TvaParts& parts) {
    const Vc2TvaTables& t = vc2TvaTables();
    const Vc2TvaEnvelope e = products(parts);
    aim(w.levelEnv, w.target.data());
    // the ramp, samples: what is left of the stage when that's under 10 ms, 0.1 ms when none is, else 441 (in
    // sustain, or 10 ms and more)
    float samples = t.upSamples;
    if (!(0.0f > e.stageTime)) {
        const float left = sub(e.stageTime, mul(w.elapsed, t.perSample));
        if (!(left > 0.0f)) samples = mul(t.upShortest, t.upPerMs);
        else if (t.upLongest > left) samples = mul(left, t.upPerMs);
    }
    stepsOver(samples);
    w.elapsed = 0.0f;
}

bool Vc2Tva::gainsDown() const {
    const float floor = vc2TvaTables().floor;
    return !(w.gain[0] > floor) && !(w.gain[1] > floor);
}

bool Vc2Tva::over(Vc2TvaParts& parts) const {
    if (w.truncatedTold != 0 || parts.envelope(w.envelope).state != kDone) return false;
    return gainsDown();
}

uint32_t Vc2Tva::stoppedWord() const { return w.stopped; }

float Vc2Tva::envelopeLevel(Vc2TvaParts& parts) const { return parts.envelope(w.envelope).out; }

void Vc2Tva::truncateTold() {
    w.fade = 1.0f;
    w.truncatedTold = 1;
}

void Vc2Tva::truncate() {
    w.fade = 1.0f;
    w.truncated = 1;
}

bool Vc2Tva::silent(Vc2TvaParts& parts) const {
    if (w.truncatedTold != 0 || w.truncated != 0) return !(w.fade > vc2TvaTables().floor);
    return parts.envelope(w.envelope).state == kDone && gainsDown();
}

void Vc2Tva::render(const float* in, float* out, Vc2TvaParts& parts) {
    const Vc2TvaTables& t = vc2TvaTables();
    if (w.truncatedTold == 0 && w.truncated == 0) {
        // a step that would pass its target in the block is shortened to the rest over 32.5, and kept
        for (size_t c = 0; c < w.step.size(); ++c) {
            const float rest = sub(w.target[c], w.gain[c]);
            if (std::fabs(mul(w.step[c], t.frames)) > std::fabs(rest)) w.step[c] = mul(rest, t.shortened);
        }
        float g0 = w.gain[0], g1 = w.gain[1];
        for (int i = 0; i < kFrames; ++i) {
            out[2 * i] = add(mul(in[2 * i], g0), out[2 * i]);   // FMAC: the product, then the sum
            out[2 * i + 1] = add(mul(in[2 * i + 1], g1), out[2 * i + 1]);
            g0 = add(g0, w.step[0]);
            g1 = add(g1, w.step[1]);
        }
        w.gain = { g0, g1 };
    } else if (w.fade > t.renderFloor) {
        fadeOut(in, out, parts);
    }
    w.elapsed = add(w.elapsed, t.frames);
}

// The truncated fade: the fade less its step each sample, the gains times it, to the floor, where it stops and says so
void Vc2Tva::fadeOut(const float* in, float* out, Vc2TvaParts& parts) {
    const float floor = vc2TvaTables().renderFloor;
    for (int i = 0; i < kFrames; ++i) {
        w.fade = sub(w.fade, parts.fadeStep());
        if (!(w.fade > floor)) {
            w.fade = 0.0f;
            if (w.truncated != 0) w.stopped = 1;
            if (w.truncatedTold != 0) parts.setEvent(kFadedEvent, kFadedFlags);
            return;
        }
        out[2 * i] = add(mul(mul(in[2 * i], w.gain[0]), w.fade), out[2 * i]);
        out[2 * i + 1] = add(mul(mul(in[2 * i + 1], w.gain[1]), w.fade), out[2 * i + 1]);
    }
}
