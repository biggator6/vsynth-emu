// Vari off's renders: see Vc2OscVariOffRender.h (v261).  rom.4 writes each way out by hand; here one routine over a way
// (mono, stereo, folded), each sample's arithmetic in rom.4's order, in XtFloat.  Every address is rom.4's.
#include "Vc2OscVariOffRender.h"
#include "Vc2OscTables.h"
#include "XtFloat.h"

namespace {
using xtf::add;
using xtf::flt;
using xtf::mul;
using xtf::sub;

// [FW] the object's words, from the voice's +0x15c: the pitch, the gain's target and the gain; the fraction, the position,
// the samples' address, the last sample, the loop's first, last and length, its rate factor and mode; the stereo render's
// fold, the fade at the last sample, the position a turning loop runs on
constexpr uint32_t kPitchAt = 0x70, kTargetAt = 0x80, kGainAt = 0x84, kFracAt = 0xec, kPosAt = 0xf0, kSamplesAt = 0xf4;
constexpr uint32_t kLastAt = 0xf8, kStartAt = 0xfc, kEndAt = 0x100, kSpanAt = 0x104, kRateAt = 0x108, kModeAt = 0x10c;
constexpr uint32_t kFoldAt = 0x124, kFadeAt = 0x128, kTurnAt = 0x12c;
constexpr uint32_t kLoops = 1, kTurns = 2;   // [FW] the mode's cases (0x893c6410): 0 and any other write nothing
constexpr int kFrames = 32;                  // [FW] a block

uint32_t objectWord(uint32_t at) { return Vc2OscVoice::kObject + at; }
// The SH-4's sum, wrapping
int32_t plusWord(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }

struct VariFrame {
    int l, r;
};
struct VariSound {
    float l, r;
};
// Two frames to read between, and what the object's position word then says (a turning loop's)
struct VariSegment {
    VariFrame from, to;
    int32_t word;
};

// A block's state: the object's words as locals, as rom.4 holds them in registers.  `limit` and `stride` the wrap's.
struct VariRun {
    const Vc2OscSamples& wave;
    const Vc2OscTables& t;
    float* out;
    uint32_t samples;
    int32_t last, start, end, span;
    float target, gain, frac, fade;
    int32_t pos;
    float step = 0;
    int32_t limit = 0, stride = 0, word = 0;
};

// The ways: a frame's samples by its number, and a sound at the gain and scale.  A turning loop's position word, on the
// way up, is the position after the step on the mono render (0x893c65a0) and before it on the stereo's (0x893c68da).
struct VariMono {
    static constexpr bool kWordAfterStep = true;
    static VariFrame frame(const VariRun& r, int32_t i) {
        const int s = r.wave.at(r.samples + 2u * (uint32_t)i);
        return { s, s };
    }
    static VariSound mixed(VariSound s, const VariRun& r) {
        const float v = mul(mul(s.l, r.gain), r.t.variOffScale);
        return { v, v };
    }
};
struct VariStereo {
    static constexpr bool kWordAfterStep = false;
    static VariFrame frame(const VariRun& r, int32_t i) {
        const uint32_t at = r.samples + 4u * (uint32_t)i;
        return { r.wave.at(at), r.wave.at(at + 2u) };
    }
    static VariSound mixed(VariSound s, const VariRun& r) {
        return { mul(mul(s.l, r.gain), r.t.variOffScale), mul(mul(s.r, r.gain), r.t.variOffScale) };
    }
};
struct VariFold : VariStereo {
    static VariSound mixed(VariSound s, const VariRun& r) {
        const float v = mul(mul(add(s.r, s.l), r.gain), r.t.variOffFold);
        return { v, v };
    }
};

// A sample out, then the gain's glide toward its target
void put(VariRun& r, VariSound s) {
    r.out[0] = s.l;
    r.out[1] = s.r;
    r.out += 2;
    r.gain = add(mul(r.t.variOffGlide, sub(r.target, r.gain)), r.gain);
}

// A loop past the last sample stops there
bool pastLast(const VariRun& r) { return r.end > r.last && r.pos >= r.last; }

// At the last sample: it holds, fading to 0
template <class Way>
void holdLast(VariRun& r) {
    r.pos = r.last;
    r.word = r.last;
    const VariFrame f = Way::frame(r, r.last);
    const VariSound s = Way::mixed({ flt(f.l), flt(f.r) }, r);
    const VariSound faded { mul(s.l, r.fade), mul(s.r, r.fade) };
    const float next = sub(r.fade, r.t.variOffFade);
    r.fade = next > 0.0f ? next : 0.0f;
    put(r, faded);
}

// The step: the position moves by the whole samples, wrapped into the loop; the fraction the sample is read at.  rom.4
// never ends the wrap with a length of 0 or less; here it doesn't start.
float advance(VariRun& r) {
    const float was = r.frac;
    const float moved = add(r.frac, r.step);
    const int32_t whole = xtf::ftrc(moved);
    r.frac = sub(moved, flt(whole));
    r.pos = plusWord(r.pos, whole);
    while (r.stride > 0 && r.pos > r.limit) r.pos = plusWord(r.pos, -r.stride);
    return was;
}

VariSound between(VariFrame a, VariFrame b, float frac) {
    return { add(mul(flt(b.l - a.l), frac), flt(a.l)), add(mul(flt(b.r - a.r), frac), flt(a.r)) };
}

VariFrame inverted(VariFrame f) { return { -f.l, -f.r }; }

// Mode 1: forward, the frame after the loop's end its start's
template <class Way>
void loopSample(VariRun& r) {
    if (pastLast(r)) {
        holdLast<Way>(r);
        return;
    }
    const int32_t one = plusWord(r.pos, 1);
    const VariFrame from = Way::frame(r, r.pos);
    const VariFrame to = Way::frame(r, one > r.end ? plusWord(one, -r.span) : one);
    const float was = advance(r);
    put(r, Way::mixed(between(from, to, was), r));
}

// Mode 2's frames: up to the loop's end as they are; from there the way back, the position mirrored about the end and
// the samples inverted; at the way's end, its first frame inverted against the next as it is
template <class Way>
VariSegment turnSegment(const VariRun& r) {
    if (r.end > r.pos) return { Way::frame(r, r.pos), Way::frame(r, plusWord(r.pos, 1)), r.pos };
    if (r.pos == r.end) return { Way::frame(r, r.pos), inverted(Way::frame(r, plusWord(r.pos, -1))), r.pos };
    if (r.limit > r.pos) {
        const int32_t m = plusWord(plusWord(r.end, r.end), -r.pos);
        return { inverted(Way::frame(r, m)), inverted(Way::frame(r, plusWord(m, -1))), m };
    }
    const int32_t m = plusWord(r.pos, -r.stride);
    return { inverted(Way::frame(r, m)), Way::frame(r, plusWord(m, 1)), m };
}

template <class Way>
void turnSample(VariRun& r) {
    if (pastLast(r)) {
        holdLast<Way>(r);
        return;
    }
    const bool rising = r.end > r.pos;
    const VariSegment s = turnSegment<Way>(r);
    const float was = advance(r);
    r.word = rising && Way::kWordAfterStep ? r.pos : s.word;
    put(r, Way::mixed(between(s.from, s.to, was), r));
}

// The pitch, × the loop's rate factor when the block starts inside the loop
float stepFrom(const Vc2OscVoice& v, const VariRun& r, int32_t top) {
    const float pitch = v.f32(objectWord(kPitchAt));
    return r.start > r.pos || r.pos > top ? pitch : mul(pitch, v.f32(objectWord(kRateAt)));
}

template <class Way>
void playLoop(const Vc2OscVoice& v, VariRun& r) {
    r.limit = r.end;
    r.stride = r.span;
    r.step = stepFrom(v, r, r.end);
    for (int k = 0; k < kFrames; ++k) loopSample<Way>(r);
    r.word = r.pos;
}

template <class Way>
void playTurns(const Vc2OscVoice& v, VariRun& r) {
    r.pos = (int32_t)v.u32(objectWord(kTurnAt));
    r.limit = plusWord(r.end, r.span);
    r.stride = plusWord(r.span, r.span);
    r.step = stepFrom(v, r, r.limit);
    for (int k = 0; k < kFrames; ++k) turnSample<Way>(r);
}

template <class Way>
void render(Vc2OscVoice& v, float* out, const Vc2OscSamples& wave) {
    const auto word = [&v](uint32_t at) { return (int32_t)v.u32(objectWord(at)); };
    const auto real = [&v](uint32_t at) { return v.f32(objectWord(at)); };
    VariRun r { wave, vc2OscTables(), out, (uint32_t)word(kSamplesAt), word(kLastAt), word(kStartAt), word(kEndAt),
                word(kSpanAt), real(kTargetAt), real(kGainAt), real(kFracAt), real(kFadeAt), word(kPosAt) };
    r.word = r.pos;
    const uint32_t mode = v.u32(objectWord(kModeAt));
    if (mode == kLoops) playLoop<Way>(v, r);
    if (mode == kTurns) playTurns<Way>(v, r);
    v.setU32(objectWord(kPosAt), (uint32_t)r.word);
    v.setF32(objectWord(kFracAt), r.frac);
    v.setU32(objectWord(kTurnAt), (uint32_t)r.pos);
    v.setF32(objectWord(kGainAt), r.gain);
    v.setF32(objectWord(kFadeAt), r.fade);
}
} // namespace

void vc2OscVariOffMono(Vc2OscVoice& v, float* out, const Vc2OscSamples& wave) { render<VariMono>(v, out, wave); }

void vc2OscVariOffStereo(Vc2OscVoice& v, float* out, const Vc2OscSamples& wave) {
    if (v.u32(objectWord(kFoldAt)) != 0) {
        render<VariFold>(v, out, wave);
    } else {
        render<VariStereo>(v, out, wave);
    }
}
