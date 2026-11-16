// The VC-2's vocoder object: see Vc2Vocoder.h (v220).  Each routine's arithmetic in its own order, in XtFloat; every
// address is rom.4's.
#include "Vc2Vocoder.h"
#include "XtFloat.h"

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;

constexpr int32_t kUnset = 3;          // [FW] the builder's mode word before its switch to off: no mode, so it switches
constexpr int32_t kEditTemplate = 16;  // [FW] the settings path's template while editing: the RAM curves
constexpr float kMaxRatio = 2.0f;      // [FW] the glue's highest ratio and q's bound (its fldi1 + fadd)
constexpr int kSides = 2;
} // namespace

Vc2Vocoder::Vc2Vocoder() : edit(vc2VocoderTables().edit) {
    w.mode = kUnset;
    setMode(kOff);
    w.ratio[0] = w.ratio[1] = 1.0f;
    w.offset[0] = w.offset[1] = 0.0f;
    w.gain = mul(xtf::flt(w.hiresoOn), vc2VocoderTables().buildGain);
}

void Vc2Vocoder::setMode(int mode) {
    if (w.mode == mode) return;
    Vc2Engine* const engines[] = { nullptr, &legacy, &hireso };   // by mode
    if (mode < kLegacy || mode > kHireso) {   // off, or no mode: no engine, LEGACY's hook, the mode word 0
        engine = nullptr;
        hook = &Vc2Vocoder::asGiven;
        w.hiresoOn = 0;
        w.mode = kOff;
        return;
    }
    engine = engines[mode];
    hook = mode == kHireso ? &Vc2Vocoder::latencyBack : &Vc2Vocoder::asGiven;
    w.hiresoOn = mode == kHireso ? 1 : 0;
    w.mode = mode;
    engine->reset();
}

const float* Vc2Vocoder::latencyBack(const float*) const {
    int at = stage.s.micIndex - kHiresoLatency;
    if (at < 0) at += Vc2Unvoiced::kMicRing;
    return &stage.mic[(size_t)at];
}

void Vc2Vocoder::settings(Vc2VocoderSettings& in) {
    if (edit.on) {   // the edit page's ratios, offsets where the struct has one, pivots and RAM curves
        for (int side = 0; side < kSides; ++side) {
            w.ratio[side] = edit.ratio[side];
            w.offset[side] = in.offset[side] == 0.0f ? 0.0f : edit.offset[side];
            in.pivot[side] = edit.pivot[side];
        }
        in.formantTemplate = kEditTemplate;
    } else {
        for (int side = 0; side < kSides; ++side) {
            w.ratio[side] = in.ratio[side];
            w.offset[side] = in.offset[side];
            in.pivot[side] = -1;   // the template's
        }
    }
    w.target = mul(mul(in.unvoiceLevel, in.depth), xtf::flt(w.hiresoOn));
    stage.settings(in.detect, in.spread, in.soundLevel);
    if (engine) engine->settings(in);
}

void Vc2Vocoder::block(const float* in, float* frames, int channels, float levelMod) {
    if (!engine) return;
    stage.block(in);
    engine->analyze((this->*hook)(in), stage.voiced());
    if (channels == 1) {
        engine->mono(frames);
    } else {
        engine->stereo(frames);
    }
    const float level = engine->holdLevel();
    stage.consonants(frames, channels, mul(w.gain, vc2VocoderTables().gainScale), level, levelMod);
}

void Vc2Vocoder::hold(int controller) {
    if (engine) engine->hold(controller);
}

void Vc2Vocoder::glue(float scale, float period) {
    if (!engine) return;
    const Vc2VocoderTables& tb = vc2VocoderTables();
    w.gain = add(mul(sub(mul(w.target, scale), w.gain), tb.glide), w.gain);
    if (w.offset[0] == 0.0f && w.offset[1] == 0.0f) {
        for (int side = 0; side < kSides; ++side) engine->formant(side, w.ratio[side]);
        return;
    }
    // q, the tracked pitch's ratio to A440's: d = min(q, 2) − 1 from 1 up, max(2q − 1, 0) − 1 below
    const float q = xtf::div(tb.periodA440, period);
    float d = q;
    if (q > kMaxRatio) {
        d = kMaxRatio;
    } else if (1.0f > q) {
        d = sub(q, sub(1.0f, q));
        if (0.0f > d) d = 0.0f;
    }
    d = add(d, -1.0f);
    for (int side = 0; side < kSides; ++side) {
        float ratio = add(mul(w.offset[side], d), w.ratio[side]);
        if (ratio > kMaxRatio) {
            ratio = kMaxRatio;
        } else if (tb.minRatio > ratio) {
            ratio = tb.minRatio;
        }
        engine->formant(side, ratio);
    }
}

void Vc2Vocoder::editSide(int pivot, int side, float ratio, float offset) {
    if (side < 0 || side >= kSides) return;
    edit.ratio[side] = ratio;
    edit.pivot[side] = pivot;
    edit.offset[side] = offset;
}
