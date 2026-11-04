// The XT's analog oscillator — see XtAnalog.h.  Single precision as the SH-4
// computes it (XtFloat.h).
#include "XtAnalog.h"
#include "XtCosm.h"
#include "XtFloat.h"
#include "XtOsc.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kRomBase = 0x89300000;
// the parameter descriptors (12 bytes per ID: storage, then int16 centre,
// default, minimum, maximum) and the level curve (128 shorts)
constexpr uint32_t kDescTab = 0x897a167c, kLevelTab = 0x897cfcb8;
// HQ-SAW's and HQ-SQUARE's band-limited impulse (A, by its centre, ±4608)
// and its window (B, ±8192), end to end in rom.2; the routines reach them
// through the pointers at 0x8987c898/0x8987c89c, set at startup
constexpr uint32_t kHqA = 0x897d94dc, kHqB = 0x897e5ce0;
constexpr int kHqAHalf = 4608, kHqBHalf = 8192, kHqN = 2 * kHqAHalf + 1 + 2 * kHqBHalf + 1;
// the LA waveforms' Tone Keyfollow, by its value 0…14
constexpr uint32_t kLaToneKf = 0x897d4c6c;
// SUPER-SAW's Detune curve (128 shorts)
constexpr uint32_t kSsDetuneTab = 0x897cef58;
// the analog calibration's system parameters
constexpr int kSys0 = 0x528, kSysN = 0x55b - 0x528 + 1;
enum : int {
    kLaSawTone = 0x528, kLaSquTone = 0x529, kLaSawToneKf = 0x52a, kLaSquToneKf = 0x52b,
    kAliasCancel = 0x52c, kSawSlope = 0x52d, kSquSlope = 0x52e,
    kBias = 0x530, kLowerKf = 0x53b, kUpperKf = 0x546, kLevel = 0x551,   // + the waveform, SAW…LA-SQUARE
};

struct Tables {
    uint16_t level[128];
    int sys[kSysN];                 // default − centre, as the parameter getter returns them
    float hq[kHqN];                 // A then B
    float toneKf[15];
    uint16_t ssDetune[128];
    const float* hqA = hq + kHqAHalf;
    const float* hqB = hq + 2 * kHqAHalf + 1 + kHqBHalf;
    bool ok = false;
} T;

using namespace xtf;
inline float fmac(float a, float b, float c) { return add(mul(a, b), c); }   // FMAC: the product, then the sum
inline int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
inline float clamp01(float x) { return x > 1.0f ? 1.0f : (0.0f > x ? 0.0f : x); }
constexpr float K(uint32_t u) { return std::bit_cast<float>(u); }               // a constant as rom.2 stores it
inline int sys(int id) { return T.sys[id - kSys0]; }

// (d/63)·|d/63|
inline float sq63(int d) {
    const float x = div(flt((int16_t)d), 63.0f);
    return mul(d < 0 ? -x : x, x);
}

// Fat (0x894e1010): F/127 within 0…1, + (key − 60)·KF/1200 within 0…1
float fatCurve(int f, int kf10, int dk) {
    const float x = clamp01(div(flt((int16_t)f), 127.0f));
    return clamp01(add(div(flt((int16_t)kf10 * (int16_t)dk), 1200.0f), x));
}

// Pulse Width (0x894e0ea0): PW within ±63, + (key − 60)·KF·126/1200, + 63;
// the integer part (toward zero) wrapped into 0…126, the fraction kept; /126
float pwCurve(int pw, int kf10, int dk) {
    float x = flt((int16_t)pw);
    x = x > 63.0f ? 63.0f : (-63.0f > x ? -63.0f : x);
    const float y = add(add(div(mul(flt((int16_t)dk * (int16_t)kf10), 126.0f), 1200.0f), x), 63.0f);
    int i = ftrc(y);
    const float frac = sub(y, flt(i));
    if (i < 0) i = i % 126 + 126;
    else if (i > 126) i = i % 126;
    return div(add(flt(i), frac), 126.0f);
}

// SUPER-SAW's knobs index 0…127 from their value + 64 (below 2: 0), then:
// Pulse Width the saws' spread (0x894e5060): (curve/2 + 2)/32767 + (key − 60)·KF·
// 0.019532/12000, within 6.1e−5…0.019593
float ssDetuneCurve(int pw, int kf10, int dk) {
    int i = (int16_t)(pw + 64);
    i = clampi(i > 1 ? i : 0, 0, 127);
    const float x = add(div(flt((T.ssDetune[i] >> 1) + 2), K(0x46fffe00)),
                        div(mul(flt((int16_t)kf10 * (int16_t)dk), K(0x3ca00193)), 12000.0f));
    return x > K(0x3ca0817f) ? K(0x3ca0817f) : (K(0x387fda3f) > x ? K(0x387fda3f) : x);
}

// Detune the side saws' level (0x894e5140), with Fat's KF: (64·i + 400)/32767
// + (key − 60)·KF·0.248055/12000, within 0.012207…0.260262
float ssMixCurve(int detune, int kf10, int dk) {
    int i = (int16_t)(detune + 64);
    i = clampi(i > 1 ? i : 0, 0, 127);
    const float x = add(div(flt((i << 6) + 400), K(0x46fffe00)),
                        div(mul(flt((int16_t)kf10 * (int16_t)dk), K(0x3e7e0221)), 12000.0f));
    return x > K(0x3e854110) ? K(0x3e854110) : (K(0x3c47ffde) > x ? K(0x3c47ffde) : x);
}

// FEEDBACK-OSC's, by the same index: Pulse Width the second tap's place
// (0x894e5210), i/254 + (key − 60)·KF·½/12000, within 0…½
float fbTapCurve(int pw, int kf10, int dk) {
    int i = (int16_t)(pw + 64);
    i = clampi(i > 1 ? i : 0, 0, 127);
    const float x = add(div(mul(flt((int16_t)kf10 * (int16_t)dk), 0.5f), 12000.0f), div(flt(i), 254.0f));
    return x > 0.5f ? 0.5f : (0.0f > x ? 0.0f : x);
}

// Detune the second tap's level (0x894e5320), with Fat's KF: 1 − ((127 − i)/
// 127)² + (key − 60)·KF/12000, within 0…1
float fbLevelCurve(int detune, int kf10, int dk) {
    int i = (int16_t)(detune + 64);
    i = clampi(i > 1 ? i : 0, 0, 127);
    const float n = div(flt(127 - i), 127.0f);
    return clamp01(add(div(flt((int16_t)kf10 * (int16_t)dk), 12000.0f), sub(1.0f, mul(n, n))));
}

// X-MOD's depth from the Pulse Width (0x894e53d0), by the same index: i/127 +
// (key − 60)·KF/12000, within 0…1
float xmDepthCurve(int pw, int kf10, int dk) {
    int i = (int16_t)(pw + 64);
    i = clampi(i > 1 ? i : 0, 0, 127);
    return clamp01(add(div(flt((int16_t)kf10 * (int16_t)dk), 12000.0f), div(flt(i), 127.0f)));
}

// With the sub oscillator on, the second wave's depths and Detune in cents
// (0x894e1090, 0x894e2974): |d| to 10: d; to 15: 2d − 10; to 31: 5d − 55; to 51:
// 25d − 675; above: 50d − 1950; with d's sign
float subCents(float d) {
    const bool neg = 0.0f > d;
    const float x = neg ? -d : d, s = neg ? -1.0f : 1.0f;
    if (!(x > 10.0f)) return mul(add(x, 0.0f), s);
    if (!(x > 15.0f)) return mul(fmac(x, 2.0f, -10.0f), s);
    if (!(x > 31.0f)) return mul(fmac(x, 5.0f, -55.0f), s);
    if (!(x > 51.0f)) return mul(fmac(x, 25.0f, -675.0f), s);
    return mul(fmac(x, 50.0f, -1950.0f), s);
}

// Detune as octaves (0x894e1090): d/63 at ±12000, within that, through the curve
// /1200, + (key − 60)·KF/120/1200, within ±1
float subDetuneCurve(int detune, int kf10, int dk) {
    const float kf = div(mul(flt((int16_t)kf10 * (int16_t)dk), K(0x3c088889)), 1200.0f);
    float x = div(mul(flt((int16_t)detune), 12000.0f), 63.0f);
    x = x > 12000.0f ? 12000.0f : (-12000.0f > x ? -12000.0f : x);
    const float c = add(div(subCents(mul(div(x, 12000.0f), 63.0f)), 1200.0f), kf);
    return c > 1.0f ? 1.0f : (-1.0f > c ? -1.0f : c);
}

// The waveform's level at the note's pitch (0x894e1350), for the patch's
// waveform; SUPER-SAW, FEEDBACK-OSC and X-MOD take the Saw's
float waveLevel(int wave, int pitch10) {
    const int w = wave >= 0 && wave <= 10 ? wave : 0;
    const int bias = (int8_t)sys(kBias + w), upper = (int8_t)sys(kUpperKf + w), lower = (int8_t)sys(kLowerKf + w);
    const int level = (int8_t)sys(kLevel + w);
    const int d = pitch10 - bias * 1000;
    const int m = d >= 0 ? d * upper : -d * lower;
    float f = add(div(flt(m), 6.0e6f), 1.0f);
    if (0.0f > f) f = 0.0f;
    return mul(div(flt(T.level[level & 0x7f]), 32768.0f), f);
}

constexpr float kHalf = 0.5f;
// the Pulse Width within m…1 − m (0x89507480)
inline float withinM(float x, float m) {
    const float top = sub(1.0f, m);
    return kHalf > x ? (m > x ? m : x) : (x > top ? top : x);
}
constexpr float kPwLo = K(0x3b820880), kPwSpan = K(0x3f7df7de);   // 0.0039683, 0.992063
constexpr float kFatMin = K(0x3b7f9723), kFatFloor = K(0x3b03126f); // 0.0039, 0.002
constexpr float kImpactLog = K(0xc1265ad5);    // the firmware's logf(3.05185e-5) (0x89531b78 on 0x38000100)
constexpr float kImpactPeriod = K(0x42c8745c); // 100.227 = 44100/440
constexpr float kPhaseInc = K(0x3ba377ee);     // 220/44100 per sample at ratio 1: two cycles a phase
constexpr float kFull = K(0x4effffff);         // 2^31 − 128
constexpr float kFull31 = K(0x4f000000);       // 2^31

} // namespace

bool xtAnalogLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    bool ok = std::fseek(f, (long)(kLevelTab - kRomBase), SEEK_SET) == 0 && std::fread(T.level, 2, 128, f) == 128;
    for (int i = 0; ok && i < kSysN; ++i) {
        int16_t d[2];
        ok = std::fseek(f, (long)(kDescTab + 12u * (uint32_t)(kSys0 + i) + 4 - kRomBase), SEEK_SET) == 0 && std::fread(d, 2, 2, f) == 2;
        T.sys[i] = d[1] - d[0];
    }
    ok = ok && std::fseek(f, (long)(kHqA - 4u * kHqAHalf - kRomBase), SEEK_SET) == 0 && std::fread(T.hq, 4, kHqN, f) == (size_t)kHqN;
    ok = ok && std::fseek(f, (long)(kLaToneKf - kRomBase), SEEK_SET) == 0 && std::fread(T.toneKf, 4, 15, f) == 15;
    ok = ok && std::fseek(f, (long)(kSsDetuneTab - kRomBase), SEEK_SET) == 0 && std::fread(T.ssDetune, 2, 128, f) == 128;
    std::fclose(f);
    static_assert(kHqA + 4u * (kHqAHalf + 1) == kHqB - 4u * kHqBHalf, "A ends where B starts");
    T.ok = ok && T.level[127] == 32768 && sys(kSawSlope) == 100 && T.hqA[0] == 2.0f && T.hqB[0] == 1.0f && T.hqA[kHqAHalf] == 0.0f
           && T.toneKf[3] == 0.0f && T.toneKf[14] == 2.0f;
    return T.ok;
}

bool xtAnalogTablesLoaded() { return T.ok; }

int xtAnalogCode(int wave) {
    static const int code[14] = { 2, 3, 4, 6, 5, 7, 0, 1, 8, 10, 9, 11, 12, 13 };
    return wave >= 1 && wave <= 13 ? code[wave] : 2;      // the converter's default case: SAW
}

bool xtAnalogPlays(int wave) { return xtAnalogCode(wave) <= 13; }

void xtAnalogMix(float* lr, const float* b, float scale) {
    for (int i = 0; i < 64; ++i) lr[i] = add(lr[i], b[i] * scale);
}

const XtToneStep* xtToneOrder(int structure) {
    using S = XtToneStep;
    static const XtToneStep order[3][6] = {
        { S::Osc2, S::Osc1, S::Mod, S::Cosm1, S::Cosm2, S::End },
        { S::Osc2, S::Osc1, S::Cosm1, S::Mod, S::Cosm2, S::End },
        { S::Osc2, S::Cosm2OnOsc2, S::Osc1, S::Cosm1, S::Mod, S::End },
    };
    return order[structure];
}

int xtCosmOsc(int structure, int k) { return structure == 2 && k == 1 ? 1 : 0; }

void xtModBlock(const XtModParams& p, XtModBlock& b) {
    const int type = p.on ? p.type : 0;
    if (type < 1 || type > 4) { b.type = 0; b.a = b.b = 1.0f; return; }
    b.type = (uint32_t)type;
    if (type == 4) { b.a = 1.0f; return; }
    b.c = 1.0f;
    if (type == 3) {
        b.a = div(flt(p.attack), 127.0f);
        b.b = div(flt(p.release), 127.0f);
    } else {
        b.a = 1.0f;
        b.b = div(flt(T.level[p.level]), 32768.0f);
    }
}

void XtMod::noteOn(const XtModBlock& b, bool stereo1, bool stereo2, bool osc1Analog, XtModShared& shared, int tone) {
    type_ = b.type;
    t[0] = b.a; t[1] = b.b; t[2] = b.c;
    stereo_ = stereo1 ? (stereo2 ? 1 : 2) : (stereo2 ? 3 : 0);
    analog_ = osc1Analog;
    shared_ = &shared;
    lineL = tone * XtModShared::kLine;
    lineR = (XtModShared::kTones + tone) * XtModShared::kLine;
    if (type_ == 1) t[0] = div(t[0], 2147483648.0f);
    if (type_ == 0) { g[0] = t[0]; g[1] = t[1]; }
    else if (type_ == 1 || type_ == 2) { g[0] = t[0]; g[1] = t[1]; g[2] = t[2]; }
    else if (type_ == 3) { g[0] = t[2]; env_ = 0.0f; }
    else if (type_ == 4) { sync_ = true; g[0] = g[1] = 0.0f; env_ = t[0]; }
    else sync_ = false;
    if (type_ == 2)
        for (int i = 0; i < XtModShared::kLine; ++i) shared.line[lineL + i] = shared.line[lineR + i] = 0.0f;
}

void XtMod::setBlock(const XtModBlock& b) {
    if (type_ == 4) sync_ = true;
    if (type_ == 0) { t[0] = b.a; t[1] = b.b; }
    else if (type_ == 1) { t[0] = div(b.a, 2147483648.0f); t[1] = b.b; t[2] = b.c; }
    else if (type_ == 2 || type_ == 3) { t[0] = b.a; t[1] = b.b; t[2] = b.c; }
    else if (type_ == 4) { env_ = b.a; return; }
    else return;
    sync_ = false;
}

bool XtMod::plain() const {
    return type_ == 0 && t[0] == 1.0f && t[1] == 1.0f && g[0] == 1.0f && g[1] == 1.0f;
}

uint32_t XtMod::routine() const {
    if (type_ == 2) return stereo_ ? 0x89527c50u : 0x89527b80u;
    if (type_ == 3) return stereo_ ? 0x89527aa0u : 0x895279d0u;
    if (type_ == 4) return analog_ ? 0x894f1aa0u : 0x894f1a40u;
    return type_ == 1 ? 0x89527e00u : 0x89527d80u;
}

void XtMod::process(float* out, const float* l1, const float* l2) {
    if (type_ == 4) { if (analog_) syncAnalog(out, l1, l2); else syncHold(out, l1); }
    else if (type_ == 3) envRing(out, l1, l2, stereo_ != 0);
    else if (type_ == 2 && stereo_) fmStereo(out, l1, l2);
    else if (type_ == 2) fm(out, l1, l2);
    else if (type_ == 1) ring(out, l1, l2);
    else mix(out, l1, l2);
}

namespace { const float kModStep = K(0x3c199999); }   // 0.009375, a unit below

void XtMod::mix(float* out, const float* l1, const float* l2) {
    const float da = mul(sub(t[0], g[0]), kModStep), db = mul(sub(t[1], g[1]), kModStep);
    float a = g[0], b = g[1];
    for (int i = 0; i < 32; ++i) {
        const float x = fmac(l2[2 * i], b, mul(l1[2 * i], a)), y = fmac(l2[2 * i + 1], b, mul(l1[2 * i + 1], a));
        out[2 * i] = x;
        out[2 * i + 1] = y;
        a = add(a, da);
        b = add(b, db);
    }
    g[0] = a;
    g[1] = b;
}

void XtMod::ring(float* out, const float* l1, const float* l2) {
    const float da = mul(sub(t[0], g[0]), kModStep), db = mul(sub(t[1], g[1]), kModStep), dc = mul(sub(t[2], g[2]), kModStep);
    float a = g[0], b = g[1], c = g[2];
    for (int i = 0; i < 64; i += 2) {
        const float x = mul(mul(add(mul(l2[i], a), b), c), l1[i]);
        const float y = mul(mul(add(mul(l2[i + 1], a), b), c), l1[i + 1]);
        out[i] = x;
        out[i + 1] = y;
        a = add(a, da);
        b = add(b, db);
        c = add(c, dc);
    }
    g[0] = a;
    g[1] = b;
    g[2] = c;
}

void XtMod::syncAnalog(float* out, const float* l1, const float* l2) {
    const float v = env_, r = sub(1.0f, v);
    for (int i = 0; i < 64; ++i) out[i] = fmac(l1[i], v, mul(l2[i], r));
}

void XtMod::syncHold(float* out, const float* l1) {
    const float v = env_, k = sub(1.0f, mul(v, 0.5f));
    float hl = g[0], hr = g[1];
    for (int i = 0; i < 32; ++i) {
        const float x = l1[2 * i], y = l1[2 * i + 1];
        if (mask_ & (1u << i)) { hl = mul(x, v); hr = mul(y, v); }
        out[2 * i] = mul(sub(x, hl), k);
        out[2 * i + 1] = mul(sub(y, hr), k);
    }
    g[0] = hl;
    g[1] = hr;
}

// ENV RING's level (0x895017e0), of L only in both routines.
static float modLevel(const float* l2) {
    float s = 0.0f;
    for (int i = 0; i < 32; ++i) {
        const float x = std::fabs(l2[2 * i]);
        s = add(s, x);
        if (i % 8 == 7) s = add(s, x);
    }
    return mul(s, K(0x2e7fffff));
}

void XtMod::envRing(float* out, const float* l1, const float* l2, bool stereo) {
    const float dg = mul(sub(t[2], g[0]), kModStep);
    float gain = g[0];
    const float level = modLevel(l2);
    const float tm = last_ > level ? t[1] : t[0];
    last_ = level;
    const float rate = fmac(sub(1.0f, tm), K(0x3ba3d70a), K(0x38d1b717));
    float e = env_;
    for (int i = 0; i < 32; ++i) {
        gain = add(gain, dg);
        const float m = mul(e, gain), x = l1[2 * i], y = l1[2 * i + 1];
        out[2 * i] = mul(x, m);
        out[2 * i + 1] = stereo ? mul(y, m) : out[2 * i];
        if (i % 4 == 3) e = fmac(sub(level, e), rate, e);
    }
    env_ = e;
    g[0] = gain;
}

float XtMod::lineAt(int base, int i) const {
    const long a = (long)base + i;
    return a >= 0 && a < 2 * XtModShared::kTones * XtModShared::kLine ? shared_->line[a] : 0.0f;
}

// One channel: m the modulator, x the input, already in its line at w_.  The firmware's
// ints wrap at 32 bits; only a modulator far past anything a voice makes gets near that.
float XtMod::fmTap(int base, float m, float x, float a, float b, float c) const {
    const float d = mul(mul(add(m, K(0x4effffff)), a), K(0x33f80000));
    const int n = ftrc(d);
    const float f = sub(d, flt(n));
    int r8 = (int)((uint32_t)w_ - (uint32_t)n), r9 = (int)((uint32_t)r8 - 1u);
    if (r8 < 0) r8 += XtModShared::kLine;
    if (r9 < 0) r9 += XtModShared::kLine;
    return mul(add(add(mul(lineAt(base, r9), f), mul(lineAt(base, r8), sub(1.0f, f))), mul(x, b)), c);
}

void XtMod::fm(float* out, const float* l1, const float* l2) {
    const float da = mul(sub(t[0], g[0]), kModStep), db = mul(sub(t[1], g[1]), kModStep), dc = mul(sub(t[2], g[2]), K(0x4effffff));
    float a = g[0], b = g[1], c = g[2];
    for (int i = 0; i < 32; ++i) {
        const float x = l1[2 * i];
        if (++w_ >= XtModShared::kLine) w_ = 0;
        shared_->line[lineL + w_] = x;
        const float v = fmTap(lineL, l2[2 * i], x, a, b, c);
        out[2 * i] = out[2 * i + 1] = v;
        a = add(a, da);
        b = add(b, db);
        c = add(c, dc);
    }
    g[0] = a;
    g[1] = b;
    g[2] = c;
}

void XtMod::fmStereo(float* out, const float* l1, const float* l2) {
    const float da = mul(sub(t[0], g[0]), kModStep), db = mul(sub(t[1], g[1]), kModStep), dc = mul(sub(t[2], g[2]), K(0x4effffff));
    float a = g[0], b = g[1], c = g[2];
    for (int i = 0; i < 32; ++i) {
        const float x = l1[2 * i], y = l1[2 * i + 1];
        if (++w_ >= XtModShared::kLine) w_ = 0;
        shared_->line[lineL + w_] = x;
        shared_->line[lineR + w_] = y;
        out[2 * i] = fmTap(lineL, l2[2 * i], x, a, b, c);
        out[2 * i + 1] = fmTap(lineR, l2[2 * i + 1], y, a, b, c);
        a = add(a, da);
        b = add(b, db);
        c = add(c, dc);
    }
    g[0] = a;
    g[1] = b;
    g[2] = c;
}

XtAnalogBlock xtAnalogBlock(const XtAnalogParams& p, int key, int pitch10, int waveGainDb, float velFat, float velPw, bool osc2) {
    XtAnalogBlock b;
    const int dk = key - 60;
    b.code = xtAnalogCode(p.wave);
    b.impact = div(mul(flt(p.impact & 0xff), 4.0f), 40.0f);
    if (sys(kAliasCancel) && (p.wave <= 1 || p.wave >= 11))                   // SUPER-SAW, FEEDBACK-OSC and X-MOD take the Saw's
        b.slope = div(flt((uint16_t)sys(p.wave == 1 ? kSquSlope : kSawSlope)), 100.0f);
    if (p.wave == 11) {   // SUPER-SAW (0x894e2500)
        constexpr float kDet = K(0x3ca00193), kMix = K(0x3e7e0221);
        b.pw = ssDetuneCurve(p.pw, p.pwKf * 10, dk);
        b.pwEnv = mul(mul(sq63(p.pwEnv), kDet), velPw);
        b.pwLfo = mul(sq63(clampi((int16_t)p.pwLfo, -63, 63)), kDet);
        b.fat = ssMixCurve(p.detune, p.fatKf * 10, dk);
        b.fatEnv = mul(mul(sq63(p.fatEnv), kMix), velFat);
        b.fatLfo = mul(sq63(clampi((int16_t)p.fatLfo, -63, 63)), kMix);
    } else if (p.wave == 12) {   // FEEDBACK-OSC (0x894e22c0)
        b.pw = fbTapCurve(p.pw, p.pwKf * 10, dk);
        b.pwEnv = mul(mul(sq63(p.pwEnv), 0.5f), velPw);
        b.pwLfo = mul(sq63(clampi((int16_t)p.pwLfo, -63, 63)), 0.5f);
        b.fat = fbLevelCurve(p.detune, p.fatKf * 10, dk);
        b.fatEnv = mul(sq63(p.fatEnv), velFat);
        b.fatLfo = sq63(clampi((int16_t)p.fatLfo, -63, 63));
    } else if (p.wave == 13) {   // X-MOD (0x894e208a): OSC 2's has no depth
        b.pw = b.pwEnv = b.pwLfo = 0.0f;
        if (!osc2) {
            b.pw = xmDepthCurve(p.pw, p.pwKf * 10, dk);
            b.pwEnv = mul(sq63(p.pwEnv), velPw);
            b.pwLfo = sq63(clampi((int16_t)p.pwLfo, -63, 63));
        }
        b.fat = fatCurve(p.fat, p.fatKf * 10, dk);
        b.fatEnv = mul(sq63(p.fatEnv), velFat);
        b.fatLfo = sq63(clampi((int16_t)p.fatLfo, -63, 63));
    } else {
        b.pw = pwCurve(p.pw, p.pwKf * 10, dk);
        b.pwEnv = mul(sq63(p.pwEnv), velPw);
        b.pwLfo = sq63(clampi((int16_t)p.pwLfo, -63, 63));
        if (p.subOctave != 0 && p.wave <= 10) {   // 0x894e288c
            b.subOn = 1.0f;
            b.fat = add(subDetuneCurve(p.detune, p.fatKf * 10, dk), p.subOctave == 1 ? -2.0f : p.subOctave == 2 ? -1.0f : 0.0f);
            b.fatEnv = mul(div(subCents(flt((int16_t)p.fatEnv)), 1200.0f), velFat);
            b.fatLfo = div(subCents(flt(clampi((int16_t)p.fatLfo, -63, 63))), 1200.0f);
        } else {
            b.fat = fatCurve(p.fat, p.fatKf * 10, dk);
            b.fatEnv = mul(sq63(p.fatEnv), velFat);
            b.fatLfo = sq63(clampi((int16_t)p.fatLfo, -63, 63));
        }
    }
    b.subLevel = div(flt(T.level[p.subLevel & 0x7f]), 32768.0f);
    b.gain = mul(waveLevel(p.wave, pitch10), xtOscWaveGainFactor(waveGainDb));
    b.tone = flt((uint8_t)sys(p.wave == 10 ? kLaSquTone : kLaSawTone));
    b.toneKf = flt((uint8_t)sys(p.wave == 10 ? kLaSquToneKf : kLaSawToneKf));
    return b;
}

// Fat's and the Pulse Width's targets, from the block and the envelope and
// LFO outputs (the note-on's default case 0x89504e80, the update's 0x89505590)
void XtAnalogOsc::shape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo, bool on) {
    const float fat = clamp01(fmac(lfo, b.fatLfo, fmac(fatEnv, b.fatEnv, b.fat)));
    const float half = sub(kHalf, mul(fat, kHalf));
    if (on) {
        fatT = fatC = half;
        if (!(fatT > kFatMin)) fatT = fatC = kFatFloor;
    } else {
        fatT = fmac(sub(half, fatT), K(0x3f333333), fatT);                      // 70% of the way
        if (!(fatT > kFatMin)) fatT = kFatFloor;
    }
    const float pw = clamp01(fmac(lfo, b.pwLfo, fmac(pwEnv, b.pwEnv, b.pw)));
    float x = fmac(pw, kPwSpan, kPwLo);
    switch (code) {
    case 0: case 1:   // HQ-SAW, HQ-SQUARE: as is; the note-on clears the integrator (0x895060d0)
        if (on) { hy = 0.0f; hs = 0; }
        break;
    case 2: {   // SAW: folded to the upper half, widened by the Slope at the pitch
        if (kHalf > x) x = sub(1.0f, x);
        const float s = mul(mul(ratio_, b.slope), K(0x3c23d709));             // 0.01, a unit below
        x = add(x, s);
        const float top = sub(1.0f, s);
        if (x > top) x = top;
        break;
    }
    case 3: {   // SQUARE: the edge's half-width, by the Slope and a step of the pitch
        const float r = ratio_;
        float f = 0.0f;
        if (!(K(0x3f6411ef) > r)) {                                            // 2^(−2/12)
            if (1.0f > r) f = K(0x3d4ccccd);
            else if (K(0x3f879c7c) > r) f = K(0x3dcccccd);                     // 2^(1/12)
            else if (K(0x3fa14518) > r) f = K(0x3e19999a);                     // 2^(4/12)
            else if (K(0x3fd744fc) > r) f = K(0x3e4ccccd);                     // 2^(9/12)
            else f = 0.25f;
        }
        edge_ = mul(mul(b.slope, mul(r, f)), K(0x3ba377ed));                   // 220/44100, a unit below the phase's
        break;
    }
    case 4: case 6:   // TRIANGLE, SINE: halved
        x = mul(x, kHalf);
        break;
    case 8: {   // NOISE: the hold, 1 + 30·|1 − 2x| samples, as a byte
        const float d = sub(1.0f, mul(x, 2.0f));
        int n = kHalf > x ? ftrc(fmac(d, 30.0f, 1.0f)) : ftrc(sub(1.0f, mul(d, 30.0f)));
        n = (int8_t)n;
        if (on && n == 0) n = 1;                                               // the note-on's, not the update's
        hold = n;
        break;
    }
    default:    // RAMP, JUNO: as is
        break;
    }
    pwT = x;
    if (on) pwC = x;
}

// LA-SQUARE's and LA-SAW's targets (0x89505950, from the note-on and the
// update).  The gain and the shape's targets are per sample, the routine's
// moves being x·0.96875 + target and x·0.998 + target.  The Tone is a corner:
// 156·2^(7·Tone/127) × (1 + the Tone KF's factor·(ratio·2^(9/12) − 1)), at
// most 20 kHz and at least twice the wave's frequency.
void XtAnalogOsc::laShape(const XtAnalogBlock& b, float gain, float fatEnv, float pwEnv, float lfo) {
    const float fat = clamp01(fmac(lfo, b.fatLfo, fmac(fatEnv, b.fatEnv, b.fat)));
    float half = sub(kHalf, mul(fat, kHalf));
    if (!(half > kFatMin)) half = kFatFloor;
    const float x = fmac(clamp01(fmac(lfo, b.pwLfo, fmac(pwEnv, b.pwEnv, b.pw))), kPwSpan, kPwLo);
    const float r = ratio_;
    const float hz = mul(r, code == 9 ? 440.0f : 220.0f);
    laInc = mul(r, code == 9 ? K(0x3ba377ed) : K(0x3b2377ed));
    const float corner = mul(xtCosmExpf(mul(mul(div(b.tone, 127.0f), 7.0f), K(0x3f317219))), 156.0f);   // × the firmware's logf(2)
    float c = mul(fmac(T.toneKf[ftrc(b.toneKf)], fmac(r, K(0x3fd744fc), -1.0f), 1.0f), corner);
    float m = kHalf;
    if (c > 20000.0f) {
        m = div(hz, 20000.0f);
        c = 20000.0f;
    } else {
        const float two = mul(hz, 2.0f);
        if (two > c) c = two;
        else m = div(hz, c);
    }
    gainT = mul(-gain, K(0x4c800000));                                          // 2^26: the sign, and 2^31/32
    pwT = mul(withinM(x, m), kFatFloor);
    fatT = mul(half, kFatFloor);
    qT = mul(div(c, hz), kFatFloor);
}

// The LA waveforms' note-on (0x89504bf6): the current values from the targets,
// the phases from Fat and Impact.  It reads +0x20c, which nothing writes (0):
// the Pulse Width within 0…1, as it is, and ½.
void XtAnalogOsc::laNoteOn(const XtAnalogBlock& b, float gain, float gainStart, float fatEnv, float pwEnv, float lfo) {
    laShape(b, gain, fatEnv, pwEnv, lfo);
    gainC = mul(-gainStart, kFull31);
    pwC = div(pwT, kFatFloor);
    fatC = div(fatT, kFatFloor);
    qC = div(qT, kFatFloor);
    float p = 0.25f;
    if (1.0f > imp) p = fmac(fmac(pwC, kHalf, kHalf), fatC, mul(p, imp));
    if (0.0f > p || p > 1.0f) p = 0.0f;
    ph = p;
    const float z = fmac(sub(mul(fmac(kHalf, kHalf, kHalf), fatC), p), 4.0f, 1.0f);
    ph2 = z > 1.0f ? add(z, -2.0f) : z;
}

// SUPER-SAW's targets (0x89505c90): the spread from the Pulse Width's terms,
// the side saws' level from Fat's
void XtAnalogOsc::ssShape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo) {
    const float det = fmac(lfo, b.pwLfo, fmac(pwEnv, b.pwEnv, b.pw));
    ssDetT = K(0x387fda3f) > det ? K(0x387fda3f) : (det > K(0x3ca0817f) ? K(0x3ca0817f) : det);
    const float mix = fmac(lfo, b.fatLfo, fmac(fatEnv, b.fatEnv, b.fat));
    ssMixT = K(0x3c47ffde) > mix ? K(0x3c47ffde) : (mix > K(0x3e854110) ? K(0x3e854110) : mix);
}

// SUPER-SAW's note-on (0x89505bc0): the six side saws start 0.1 apart from a
// counter all voices share (0x8987c894), which moves on by 0.6
void XtAnalogOsc::ssNoteOn(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo, XtAnalogShared& shared) {
    ssShape(b, fatEnv, pwEnv, lfo);
    pwT = pwC = mul(mul(ratio_, b.slope), K(0x3c23d709));
    ssDetC = ssDetT;
    ssMixC = ssMixT;
    float x = shared.superSaw;
    auto next = [&] {
        x = add(x, K(0x3dcccccd));
        if (!(1.0f > x)) x = add(x, -1.0f);
    };
    for (float& p : ss) { p = x; next(); }
    shared.superSaw = x;
}

// FEEDBACK-OSC's targets (0x895054c0): the second tap's place and level
void XtAnalogOsc::fbShape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo) {
    const float t = fmac(lfo, b.pwLfo, fmac(pwEnv, b.pwEnv, b.pw));
    ssDetT = 0.0f > t ? 0.0f : (t > kHalf ? kHalf : t);
    ssMixT = clamp01(fmac(lfo, b.fatLfo, fmac(fatEnv, b.fatEnv, b.fat)));
}

// FEEDBACK-OSC's note-on (0x89504d30): one of the eighteen delay lines, the
// first free (0x895049f0) or the first if none is, cleared (0x895049c0).
void XtAnalogOsc::fbNoteOn(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo, XtAnalogShared& shared) {
    fbShape(b, fatEnv, pwEnv, lfo);
    pwT = pwC = mul(mul(ratio_, b.slope), K(0x3c23d709));
    ssDetC = ssDetT;
    ssMixC = ssMixT;
    fbZ = 0.0f;
    fbW = 0;
    fbSlot = 0;
    for (int i = 0; i < XtAnalogShared::kLines; ++i)
        if (!shared.lineUsed[i]) { shared.lineUsed[i] = 1; fbSlot = i; break; }
    shared_ = &shared;
    std::memset(shared.line[fbSlot], 0, XtAnalogShared::kLineLen);
}

// X-MOD's targets (0x89505f10, the note-on's first step and the update): the
// depth within 0…1; Fat at once, no 70%.
void XtAnalogOsc::xmShape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo) {
    ssDetT = clamp01(fmac(lfo, b.pwLfo, fmac(pwEnv, b.pwEnv, b.pw)));
    const float fat = clamp01(fmac(lfo, b.fatLfo, fmac(fatEnv, b.fatEnv, b.fat)));
    fatT = sub(kHalf, mul(fat, kHalf));
    if (!(fatT > kFatMin)) fatT = kFatFloor;
}
// X-MOD's note-on (0x89505ec0): the Pulse Width is fixed by the pitch, ½ + the
// Slope's width.
void XtAnalogOsc::xmNoteOn(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo) {
    xmShape(b, fatEnv, pwEnv, lfo);
    pwT = pwC = fmac(mul(ratio_, b.slope), K(0x3c23d709), kHalf);
    ssDetC = ssDetT;
    fatC = fatT;
}

void XtAnalogOsc::release() {
    if (fbSlot >= 0 && shared_) shared_->lineUsed[fbSlot] = 0;
    fbSlot = -1;
}

// The targets with the sub oscillator on (0x89506bf0, the note-on's first step
// and the update).  The second wave's pitch is 2^(octaves) against the first's.
// Each wave's Pulse Width is kept off its ends by a share of its own frequency;
// that share, and each waveform's steps, by the routine.
void XtAnalogOsc::subShape(const XtAnalogBlock& b, float gain, float fatEnv, float pwEnv, float lfo) {
    fatT = xtCosmExpf(mul(fmac(lfo, b.fatLfo, fmac(fatEnv, b.fatEnv, b.fat)), K(0x3f317219)));   // powf(2, x)
    const float sr = fatT;
    const float w = clamp01(fmac(lfo, b.pwLfo, fmac(pwEnv, b.pwEnv, b.pw)));
    if (code <= 1 || code == 8) {       // HQ-SAW, HQ-SQUARE, NOISE: their own routines, no second wave
        gainT = gain;
        pwT = w;
        if (code == 8) {
            const float d = sub(1.0f, mul(w, 2.0f));
            int n = kHalf > w ? ftrc(fmac(d, 30.0f, 1.0f)) : ftrc(sub(1.0f, mul(d, 30.0f)));
            n = (int8_t)n;
            hold = n == 0 ? 1 : n;
        }
        return;
    }
    constexpr float kInc = K(0x3ba377ed), kFs = 44100.0f;
    gainT = mul(gain, K(0x4c800000));                                           // 2^26
    lvlT = mul(b.subLevel, kFatFloor);
    const float r = mul(ratio_, 2.0f);
    auto steps = [&](float a) { laInc = mul(a, kInc); subInc = mul(mul(a, sr), kInc); };
    switch (code) {
    case 3: case 7: {   // SQUARE, JUNO: the steepness' targets too
        const float f = mul(r, 440.0f), f2 = mul(f, 2.0f), g2 = mul(mul(sr, f), 2.0f);
        pwT = mul(withinM(w, mul(div(f2, kFs), 0.25f)), kFatFloor);
        swT = mul(withinM(w, mul(div(g2, kFs), 0.25f)), kFatFloor);
        steps(r);
        sqT = mul(div(kFs, f2), K(0x3c03126f));
        sq2T = mul(div(kFs, g2), K(0x3c03126f));
        break;
    }
    case 4: case 6: {   // TRIANGLE, SINE
        const float f = mul(r, 880.0f);
        pwT = mul(withinM(w, mul(div(f, kFs), 0.25f)), kFatFloor);
        swT = mul(withinM(w, mul(div(mul(sr, f), kFs), 0.25f)), kFatFloor);
        steps(r);
        break;
    }
    case 5: {   // RAMP: the last segment's width too
        const float f = mul(r, 440.0f), e1 = div(mul(f, 2.0f), kFs), e2 = div(mul(mul(sr, f), 2.0f), kFs);
        pwT = mul(withinM(w, mul(e1, 0.125f)), kFatFloor);
        swT = mul(withinM(w, mul(e2, 0.125f)), kFatFloor);
        steps(r);
        rampK = mul(e1, 0.0625f);
        rampK2 = mul(e2, 0.0625f);
        break;
    }
    case 9: case 10: {   // LA-SQUARE, LA-SAW: the Tone's corner for each wave, as the plain ones
        const float a = code == 10 ? mul(r, kHalf) : r;
        const float tk = T.toneKf[ftrc(b.toneKf)];
        laInc = mul(a, kInc);
        const float ar = mul(a, sr), hz = mul(a, 440.0f);
        subInc = mul(ar, kInc);
        const float corner = mul(xtCosmExpf(mul(mul(div(b.tone, 127.0f), 7.0f), K(0x3f317219))), 156.0f);
        float c = mul(fmac(tk, code == 10 ? fmac(a, K(0x3fd744fc), -1.0f) : fmac(a, K(0x3f5744fc), -1.0f), 1.0f), corner);
        if (c > 20000.0f) c = 20000.0f;
        float m = kHalf;
        if (hz > c) c = hz;
        else m = div(mul(a, 220.0f), c);
        const float q1 = mul(div(c, hz), 2.0f);
        float c2 = mul(fmac(tk, fmac(ar, K(0x3fd744fc), -1.0f), 1.0f), corner);
        if (c2 > 20000.0f) c2 = 20000.0f;
        float m2 = kHalf;
        const float t = mul(sr, hz);
        if (hz > c2) c2 = hz;
        else m2 = div(t, c2);
        const float q2 = mul(div(c2, t), 2.0f);
        pwT = mul(withinM(w, m), kFatFloor);
        swT = mul(withinM(w, m2), kFatFloor);
        qT = mul(q1, kFatFloor);
        q2T = mul(q2, kFatFloor);
        break;
    }
    default: {   // SAW: from |w − ½|
        const float f = mul(r, 880.0f), x = std::fabs(add(w, -0.5f));
        steps(r);
        pwT = mul(withinM(x, mul(div(f, kFs), K(0x3eb33332))), kFatFloor);
        swT = mul(withinM(x, mul(div(mul(sr, f), kFs), K(0x3eb33332))), kFatFloor);
        break;
    }
    }
}

// The note-on (0x89506700): the current values from the targets; Impact scales
// the phase a second time; each waveform's start; the second phase in step.
// It reads +0x20c and +0x214 as limits, which nothing writes (0), and +0x208,
// which a RAMP note left on the voice.
void XtAnalogOsc::subNoteOn(const XtAnalogBlock& b, float gain, float gainStart, float fatEnv, float pwEnv, float lfo) {
    subShape(b, gain, fatEnv, pwEnv, lfo);
    gainC = mul(gainStart, kFull31);
    fatC = fatT;
    pwC = div(pwT, kFatFloor);
    swC = div(swT, kFatFloor);
    lvlC = div(lvlT, K(0x3d000000));
    float p = ph;
    if (1.0f > imp) p = mul(p, imp);
    auto steep = [&] { sqC = div(sqT, kFatFloor); sq2C = div(sq2T, kFatFloor); };
    switch (code) {
    case 0: case 1:
        hy = 0.0f;
        hs = 0;
        [[fallthrough]];
    case 8:
        gainC = gainStart;
        pwC = pwT;
        ph = 0.0f > p || p > 1.0f ? 0.0f : p;
        return;
    case 3:
        steep();
        p = add(fmac(withinM(pwC, rampK), kHalf, p), kHalf);
        break;
    case 5:
        p = add(p, 0.0f);
        break;
    case 7:
        steep();
        p = add(p, 0.0f);
        break;
    case 9: case 10: {
        qC = div(qT, kFatFloor);
        q2C = div(q2T, kFatFloor);
        float x = add(fmac(withinM(pwC, 0.0f), kHalf, p), kHalf);
        if (0.0f > x || x > 1.0f) x = 0.0f;
        ph = x;
        sph = mul(fatT, x);
        ph2 = fmac(sub(0.25f, x), 2.0f, 2.0f);
        ph3 = fmac(sub(0.25f, sph), 2.0f, 2.0f);
        if (ph2 > 1.0f) ph2 = add(ph2, -2.0f);
        if (ph3 > 1.0f) ph3 = add(ph3, -2.0f);
        return;
    }
    default:
        p = fmac(withinM(pwC, rampK), kHalf, p);
        break;
    }
    if (0.0f > p || p > 1.0f) ph = sph = 0.0f;
    else { ph = p; sph = mul(fatT, p); }
}

void XtAnalogOsc::noteOn(const XtAnalogBlock& b, float ratio, float gain, float gainStart, float fatEnv, float pwEnv, float lfo, XtAnalogShared& shared) {
    code = b.code;
    subOn_ = b.subOn != 0.0f;
    ratio_ = ratio;
    gainT = gain;
    gainC = gainStart;
    fl = 0;
    imp = b.impact;
    dec = xtCosmExpf(mul(kImpactLog, div(ratio_, kImpactPeriod)));            // powf(3.05185e-5, ratio/100.227)
    if (1.0f > imp) ph = mul(ph, imp);
    if (ph > 1.0f || 0.0f > ph) ph = 0.0f;
    if (subOn_) subNoteOn(b, gain, gainStart, fatEnv, pwEnv, lfo);
    else if (code == 9 || code == 10) laNoteOn(b, gain, gainStart, fatEnv, pwEnv, lfo);
    else if (code == 11) ssNoteOn(b, fatEnv, pwEnv, lfo, shared);
    else if (code == 12) fbNoteOn(b, fatEnv, pwEnv, lfo, shared);
    else if (code == 13) xmNoteOn(b, fatEnv, pwEnv, lfo);
    else shape(b, fatEnv, pwEnv, lfo, true);
}

void XtAnalogOsc::update(const XtAnalogBlock& b, float ratio, float gain, float fatEnv, float pwEnv, float lfo) {
    ratio_ = ratio;
    gainT = gain;
    if (subOn_) subShape(b, gain, fatEnv, pwEnv, lfo);
    else if (code == 9 || code == 10) laShape(b, gain, fatEnv, pwEnv, lfo);
    else if (code == 11) ssShape(b, fatEnv, pwEnv, lfo);
    else if (code == 12) fbShape(b, fatEnv, pwEnv, lfo);
    else if (code == 13) xmShape(b, fatEnv, pwEnv, lfo);
    else shape(b, fatEnv, pwEnv, lfo, false);
}

int XtAnalogOsc::breakpoints(float* p) const {
    const float a = fatC, w = pwC;
    switch (code) {
    case 2: {
        float d = sub(kHalf, w);
        if (0.0f > d) d = add(d, 1.0f);
        const float ad = mul(a, d);
        p[0] = ad; p[1] = a; p[2] = sub(add(a, d), ad);
        return 3;
    }
    case 3: {
        const float s = edge_, pa = mul(w, a), bb = sub(add(a, w), pa);
        p[0] = s; p[1] = sub(pa, s); p[2] = add(pa, s); p[3] = sub(a, s); p[4] = add(a, s); p[5] = sub(bb, s); p[6] = add(bb, s);
        p[7] = sub(1.0f, s);
        return 8;
    }
    case 4: {
        const float ap = mul(a, w), e9 = sub(a, ap);
        p[0] = ap; p[1] = e9; p[2] = a; p[3] = add(w, e9); p[4] = add(sub(1.0f, w), ap);
        return 5;
    }
    case 5: {
        const float ap = mul(w, a);
        p[0] = ap; p[1] = a; p[2] = sub(add(a, w), ap);
        return 3;
    }
    case 6: {
        const float ap = mul(a, w), a2 = mul(kHalf, a), d4 = sub(w, ap);
        p[0] = ap; p[1] = a2; p[2] = sub(a, ap); p[3] = a; p[4] = add(d4, a); p[5] = add(kHalf, a2); p[6] = sub(1.0f, d4);
        return 7;
    }
    case 7: {
        const float h1 = mul(add(1.0f, a), kHalf), a2 = mul(a, kHalf), q0 = mul(a2, w), q1 = mul(sub(h1, a), w);
        p[0] = sub(a2, q0); p[1] = a2; p[2] = sub(a, q0); p[3] = a; p[4] = sub(h1, q1); p[5] = h1; p[6] = sub(1.0f, q1);
        return 7;
    }
    case 0: case 1: {   // in the phase's units: the window's edges ±Pc, and HQ-SAW's wrap or HQ-SQUARE's turns
        const HqFrame h = hqFrame();
        p[0] = h.pc; p[1] = -h.pc;
        if (code == 0) { p[2] = h.p; return 3; }
        p[2] = h.l1; p[3] = -h.l2;
        return 4;
    }
    case 9: case 10:   // the half cycles' turn, and the sample that lands on 1
        p[0] = fatC;
        p[1] = sub(1.0f, laInc);
        return 2;
    case 13: {
        float d = sub(kHalf, w);
        if (0.0f > d) d = add(d, 1.0f);
        const float ad = mul(a, d);
        p[0] = ad; p[1] = a; p[2] = fmac(sub(1.0f, a), d, a);
        return 3;
    }
    case 11: case 12:  // the sample that lands on 1, and the slope's turn
        p[0] = sub(1.0f, mul(ratio_, K(0x3c2377ed)));
        p[1] = pwT;
        return 2;
    default:
        return 0;
    }
}

uint32_t XtAnalogOsc::process(float* lr, uint32_t sync, float* mod) {
    if (subOn_) switch (code) {
    case 2: return subPeak(lr, sync, false, false);
    case 3: return subSquare(lr, sync);
    case 4: return subPeak(lr, sync, true, false);
    case 5: return subRamp(lr, sync);
    case 6: return subPeak(lr, sync, true, true);
    case 7: return subJuno(lr, sync);
    case 9: return subLaSquare(lr, sync);
    case 10: return subLaSaw(lr, sync);
    default: break;       // HQ-SAW, HQ-SQUARE, NOISE: their own routines
    }
    switch (code) {
    case 0: return hqSaw(lr, sync);
    case 1: return hqSquare(lr, sync);
    case 3: return square(lr, sync);
    case 4: return triangle(lr, sync);
    case 5: return ramp(lr, sync);
    case 6: return sine(lr, sync);
    case 7: return juno(lr, sync);
    case 8: return noise(lr);
    case 9: return laSquare(lr, sync);
    case 10: return laSaw(lr, sync);
    case 11: return superSaw(lr, sync);
    case 12: return feedbackOsc(lr, sync);
    case 13: return xMod(lr, sync, mod);
    default: return saw(lr, sync);
    }
}

// The routines share their frame: a = the Fat value (+0x1c4, ½ at Fat 0),
// p = the Pulse Width (+0x1cc), and a phase of 0…1 holding two cycles, the
// first over 0…a.  Each sample: a set sync bit resets the phase; the wave
// y, then ((y + Impact)·gain)·(2^31 − 128) on L and R; the phase steps by
// ratio·220/44100 and wraps; the gain moves 1/32 toward its target, Impact
// by its decay; the half-cycle flags (1 in the first cycle, 2 in the
// second) collect, and both seen set the sample's bit in the returned mask.
// Once a block Fat and PW move 1/32 toward their targets.
constexpr float kStep = K(0x3d000000);     // 1/32
constexpr float kLaMove = K(0x3f7f7ced);   // 0.998, the LA waveforms' moves a sample

inline void XtAnalogOsc::moveShape() {
    fatC = fmac(kStep, sub(fatT, fatC), fatC);
    pwC = fmac(kStep, sub(pwT, pwC), pwC);
}

// SAW (0x89508c20).  d = ½ − p (+1 and the wave inverted when negative); the
// breakpoints a·d, a and b = a + d − a·d; the wave rises over 0…a·d and
// a…b and falls over a·d…a and b…1, each segment −1…+1.
uint32_t XtAnalogOsc::saw(float* lr, uint32_t sync) {
    float d = sub(kHalf, pwC), sign = 1.0f;
    if (0.0f > d) { d = add(d, 1.0f); sign = -1.0f; }
    const float a = fatC, ad = mul(a, d), bb = sub(add(a, d), ad);
    const float kb1 = div(2.0f, sub(bb, 1.0f)), kba = div(2.0f, sub(bb, a)), kad = div(2.0f, sub(ad, a)), k0 = div(2.0f, ad);
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(bb > ph)) { y = mul(sub(ph, 1.0f), kb1); fl |= 2; }
        else if (!(a > ph)) { y = mul(sub(ph, a), kba); fl |= 2; }
        else {
            fl |= 1;
            y = ad > ph ? mul(ph, k0) : mul(sub(ph, a), kad);
        }
        y = sub(y, 1.0f);
        ph = add(ph, inc);
        if (fl == 3) { fl = 0; wrap |= bit; }
        y = mul(y, sign);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
        y = mul(mul(add(y, imp), gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        imp = mul(imp, dec);
        bit <<= 1;
    }
    moveShape();
    return wrap;
}

// SQUARE (0x89508aa0).  Breakpoints p·a, a and b = a + p − p·a: −1 up to
// p·a, +1 to a, −1 to b, +1 to 1; each edge a ramp over ±s around it (s the
// edge half-width, +0x1e0), the one at 0/1 split across the wrap.
uint32_t XtAnalogOsc::square(float* lr, uint32_t sync) {
    const float s = edge_, ks = div(1.0f, s);
    const float a = fatC, p = pwC, pa = mul(p, a), bb = sub(add(a, p), pa);
    const float e0 = sub(pa, s), e1 = add(pa, s), e2 = sub(a, s), e3 = add(a, s), e4 = sub(bb, s), e5 = add(bb, s), e6 = sub(1.0f, s);
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (s > ph) { y = mul(-ph, ks); fl |= 1; }
        else if (e0 > ph) { y = -1.0f; fl |= 1; }
        else if (e1 > ph) { y = mul(sub(ph, pa), ks); fl |= 1; }
        else if (e2 > ph) { y = 1.0f; fl |= 1; }
        else if (e3 > ph) { y = mul(sub(a, ph), ks); fl |= 2; }
        else if (e4 > ph) { y = -1.0f; fl |= 2; }
        else if (e5 > ph) { y = mul(sub(ph, bb), ks); fl |= 2; }
        else if (e6 > ph) { y = 1.0f; fl |= 2; }
        else { y = mul(sub(1.0f, ph), ks); fl |= 2; }
        y = mul(mul(add(y, imp), gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        imp = mul(imp, dec);
        gainC = fmac(sub(gainT, gainC), kStep, gainC);
        ph = add(ph, inc);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
        if (fl == 3) { fl = 0; wrap |= bit; }
        bit <<= 1;
    }
    moveShape();
    return wrap;
}

// TRIANGLE (0x895094c0), p halved.  Each cycle rises 0…1, falls 1…−1 and
// rises −1…0: the first over 0…a·p, …a − a·p, …a; the second over
// a…p + a − a·p, …1 − p + a·p, …1.
uint32_t XtAnalogOsc::triangle(float* lr, uint32_t sync) {
    const float a = fatC, p = pwC, ap = mul(a, p);
    const float k8 = div(1.0f, ap), k4 = div(1.0f, sub(p, ap));
    const float e9 = sub(a, ap), e10 = add(p, e9), e11 = add(sub(1.0f, p), ap);
    const float k12 = div(2.0f, sub(e10, e11)), k16 = div(2.0f, sub(ap, e9));
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(e11 > ph)) { y = mul(sub(ph, 1.0f), k4); fl |= 2; }
        else if (!(e10 > ph)) { y = sub(mul(sub(ph, e11), k12), 1.0f); fl |= 2; }
        else if (!(a > ph)) { y = mul(sub(ph, a), k4); fl |= 2; }
        else if (!(e9 > ph)) { y = mul(sub(ph, a), k8); fl |= 1; }
        else if (!(ap > ph)) { y = sub(mul(sub(ph, e9), k16), 1.0f); fl |= 1; }
        else { y = mul(ph, k8); fl |= 1; }
        y = add(y, imp);
        if (fl == 3) { fl = 0; wrap |= bit; }
        y = mul(mul(y, gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        ph = add(ph, inc);
        bit <<= 1;
        imp = mul(imp, dec);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
    }
    moveShape();
    return wrap;
}

// SINE (0x89509030), p halved: TRIANGLE's frame with each quarter a
// parabola, x·(2 − x) toward a peak and x·(x + 2) toward a trough (x the
// quarter's −1…1), the peaks at a·p and p + a − a·p, the zero crossings at
// a/2 and ½ + a/2.
uint32_t XtAnalogOsc::sine(float* lr, uint32_t sync) {
    const float a = fatC, p = pwC, ap = mul(a, p);
    const float k8 = div(1.0f, ap), a2 = mul(kHalf, a), e8 = sub(a, ap), d4 = sub(p, ap);
    const float e9 = add(d4, a), e10 = add(kHalf, a2), e11 = sub(1.0f, d4), k4 = div(1.0f, d4);
    const float d12 = sub(ap, a2), k12 = div(1.0f, d12);
    const float d16 = sub(p, add(kHalf, d12)), k16 = div(1.0f, d16);
    const float k20 = div(1.0f, sub(sub(add(d16, a), a2), a2));               // d16 again, by another sum
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    auto up = [](float x) { return mul(add(x, 2.0f), x); };
    auto down = [](float x) { return mul(add(-x, 2.0f), x); };
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(e11 > ph)) { y = up(mul(sub(ph, 1.0f), k4)); fl |= 2; }
        else if (!(e10 > ph)) { y = up(mul(sub(ph, e10), k16)); fl |= 2; }
        else if (!(e9 > ph)) { y = down(mul(sub(ph, e10), k20)); fl |= 2; }
        else if (!(a > ph)) { y = down(mul(sub(ph, a), k4)); fl |= 2; }
        else if (!(e8 > ph)) { y = up(mul(sub(ph, a), k8)); fl |= 1; }
        else if (!(a2 > ph)) { y = up(mul(sub(ph, a2), k12)); fl |= 1; }
        else if (!(ap > ph)) { y = down(mul(sub(ph, a2), k12)); fl |= 1; }
        else { y = down(mul(ph, k8)); fl |= 1; }
        y = mul(mul(add(y, imp), gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        ph = add(ph, inc);
        if (fl == 3) { fl = 0; wrap |= bit; }
        bit <<= 1;
        imp = mul(imp, dec);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
    }
    moveShape();
    return wrap;
}

// RAMP (0x89508800).  SAW's breakpoints with d = p and no fold: each cycle
// rises 0…1 (over 0…a·p, a…b) and falls 1…−1 (over a·p…a, b…1).
uint32_t XtAnalogOsc::ramp(float* lr, uint32_t sync) {
    const float a = fatC, p = pwC, ap = mul(p, a), bb = sub(add(a, p), ap);
    const float k0 = div(1.0f, ap), kad = div(2.0f, sub(ap, a)), kba = div(1.0f, sub(bb, a)), kb1 = div(2.0f, sub(bb, 1.0f));
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(bb > ph)) { y = sub(mul(sub(ph, 1.0f), kb1), 1.0f); fl |= 2; }
        else if (!(a > ph)) { y = mul(sub(ph, a), kba); fl |= 2; }
        else if (!(ap > ph)) { y = sub(mul(sub(ph, a), kad), 1.0f); fl |= 1; }
        else { y = mul(ph, k0); fl |= 1; }
        y = mul(add(y, imp), gainC);
        if (fl == 3) { fl = 0; wrap |= bit; }
        ph = add(ph, inc);
        bit <<= 1;
        y = mul(y, kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        imp = mul(imp, dec);
    }
    moveShape();
    return wrap;
}

// JUNO (0x89508db0): a rising saw each cycle (−1…1 over 0…a and a…1),
// held at −1 − Impact by a pulse at twice its rate: low over each half
// cycle's first (1 − p) part, so p is the share it is heard.
uint32_t XtAnalogOsc::juno(float* lr, uint32_t sync) {
    const float a = fatC, p = pwC;
    const float k1 = div(2.0f, add(a, -1.0f)), k0 = div(2.0f, a);
    const float h1 = mul(add(1.0f, a), kHalf), a2 = mul(a, kHalf);
    const float q0 = mul(a2, p), e6 = sub(a2, q0), e8 = sub(a, q0);
    const float q1 = mul(sub(h1, a), p), e9 = sub(h1, q1), e11 = sub(1.0f, q1);
    const float inc = mul(ratio_, kPhaseInc);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (a > ph) { y = mul(k0, ph); fl |= 1; }
        else { y = mul(k1, sub(a, ph)); fl |= 2; }
        y = add(add(y, -1.0f), imp);
        const bool high = ph > e11 || (!(ph > h1) && (ph > e9 || (!(ph > a) && (ph > e8 || (!(ph > a2) && ph > e6)))));
        if (!high) y = sub(-1.0f, imp);
        y = mul(mul(y, gainC), kFull);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(kStep, sub(gainT, gainC), gainC);
        ph = add(ph, inc);
        if (fl == 3) { fl = 0; wrap |= bit; }
        if (!(1.0f > ph)) ph = add(ph, -1.0f);
        bit <<= 1;
        imp = mul(imp, dec);
    }
    moveShape();
    return wrap;
}

// HQ-SAW and HQ-SQUARE: a leaky integrator (+0x1e4) of a band-limited
// impulse, A (interpolated) × B (the window) × k3, fired as the scaled phase
// x crosses 0.  Lengths are in samples × 32768 (P the period; HQ-SQUARE's
// halves L1 = P(1 − p), L2 = P·p).  The impulse is as wide as Pc, the
// shorter half (the whole period for HQ-SAW) within 2…144 samples; x =
// phase·8192/Pc, so the window is |x| < 8192 and x steps by 8192·65536/Pc.
// The impulse's gain g = max(¼, ½ − 1.42845/(Pc/32768 − 1)).  The phase
// (+0x1d4) is kept as x·Pc/8192.
XtAnalogOsc::HqFrame XtAnalogOsc::hqFrame() const {
    HqFrame h;
    const float f = mul(ratio_, 440.0f), half = div(K(0x4e2c4400), f);        // 44100·16384/f
    h.p = mul(half, 2.0f);
    float m = h.p;
    if (code == 1) {
        const float d = mul(sub(1.0f, mul(pwC, 2.0f)), half);
        h.l1 = add(half, d);
        h.l2 = sub(half, d);
        m = d > 0.0f ? h.l2 : h.l1;
    }
    h.pc = m > K(0x4a8ffffc) ? K(0x4a8ffffc) : (65536.0f > m ? 65536.0f : m);
    float g = sub(kHalf, div(K(0x3fb6d75a), fmac(h.pc, K(0x37ffffff), -1.0f)));
    if (0.25f > g) g = 0.25f;
    h.k1 = div(8192.0f, h.pc);
    h.k2 = div(mul(g, K(0x3affffff)), h.k1);                                   // 2^−9, a unit below
    h.k3 = mul(g, K(0x380f1a9f));
    h.leak = add(div(K(0xc6645a1d), h.p), 1.0f);                                // 1 − 14614.5/P
    return h;
}

namespace {
// The impulse's place for x (0x89506080): A at x·k2, interpolated, and B at x
struct HqTap {
    const float* a = nullptr;
    const float* b = nullptr;
    float frac = 0;
    void at(float x, float k2) {
        if (!(8192.0f > x) || !(add(x, 8192.0f) > 0.0f)) { a = nullptr; return; }
        const float t = mul(k2, x);
        const int i = ftrc(t);
        a = T.hqA + i;
        frac = sub(t, flt(i));
        b = T.hqB + ftrc(x);
    }
    float value() const { return mul(fmac(sub(a[1], a[0]), frac, a[0]), *b); }
};
} // namespace

constexpr float kHqY = K(0x57000000), kHqImp = K(0x4f000000), kHqDc = K(0xce800000);   // 2^47, 2^31, −2^30

// HQ-SAW (0x895063f0): x runs over ±P·8192/Pc, an impulse up at each 0 and
// a DC step of −1.118/P every sample; out = (y·2^47 + Impact·2^31 − 2^30)·gain.
// No Fat or PW.
uint32_t XtAnalogOsc::hqSaw(float* lr, uint32_t sync) {
    const HqFrame h = hqFrame();
    const float dc = -div(K(0x3f8f1aa0), h.p);
    const float step = mul(h.k1, 65536.0f), top = mul(h.p, h.k1), back = mul(top, -2.0f);
    float x = mul(ph, h.k1);
    HqTap q;
    q.at(x, h.k2);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) { hy = 0.0f; hs = 0; x = 0.0f; }   // the tap stays where it was
        hy = mul(hy, h.leak);
        if (q.a) hy = fmac(q.value(), h.k3, hy);
        hy = add(hy, dc);
        x = add(x, step);
        if (x > top) { wrap |= bit; x = add(x, back); }
        q.at(x, h.k2);
        const float y = mul(add(fmac(hy, kHqY, mul(imp, kHqImp)), kHqDc), gainC);
        imp = mul(imp, dec);
        lr[2 * i] = lr[2 * i + 1] = y;
        gainC = fmac(sub(gainT, gainC), kStep, gainC);
        bit <<= 1;
    }
    ph = div(x, h.k1);
    return wrap;
}

// HQ-SQUARE (0x895060e0): x rises to L1 and falls to −L2 (×8192/Pc),
// reflected at each (+0x1e8 the direction), an impulse up as it crosses 0
// rising and down falling: high for the (1 − p) share.  The wrap bit at
// each turn at L1.  PW moves 0.2 of the way once a block; Fat not.
uint32_t XtAnalogOsc::hqSquare(float* lr, uint32_t sync) {
    const HqFrame h = hqFrame();
    const float step = mul(h.k1, 65536.0f), a1 = mul(h.l1, h.k1), a2 = mul(h.l2, h.k1);
    const float a1x2 = mul(a1, 2.0f), a2x2 = mul(a2, -2.0f);
    float x = mul(ph, h.k1);
    HqTap q;
    q.at(x, h.k2);
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) { x = 0.0f; hy = 0.0f; hs = 0; }
        hy = mul(hy, h.leak);
        if (hs == 0) {
            if (q.a) hy = fmac(q.value(), h.k3, hy);
            x = add(x, step);
            if (x > a1) { x = sub(a1x2, x); hs = 1; wrap |= bit; }
        } else {
            if (q.a) hy = sub(hy, mul(q.value(), h.k3));
            x = add(x, -step);
            if (-a2 > x) { x = sub(a2x2, x); hs = 0; }
        }
        q.at(x, h.k2);
        const float y = mul(add(fmac(hy, kHqY, mul(imp, kHqImp)), kHqDc), gainC);
        lr[2 * i] = lr[2 * i + 1] = y;
        imp = mul(imp, dec);
        gainC = fmac(sub(gainT, gainC), kStep, gainC);
        bit <<= 1;
    }
    pwC = fmac(sub(pwT, pwC), K(0x3e4ccccd), pwC);
    ph = div(x, h.k1);
    return wrap;
}

// LA-SQUARE and LA-SAW share a frame.  A phase of 0…1 (+0x1d4) holds two
// cycles, the first over 0…a; x = 0…1 within its cycle.  The wave is a
// trapezium, g = (w − |2x − 1|)·q within ±1, shaped by g·(2 − |g|): w the
// Pulse Width (+0x1cc) and q the steepness (+0x228), the corner's ratio to the
// wave.  The gain and these move every sample.  The flags are 2 in the first
// cycle and 1 in the second.  out = (y + Impact)·gain.
inline float laOut(float g) { return mul(g, sub(2.0f, std::fabs(g))); }

// LA-SAW (0x895081f0): the trapezium × a second wave z (+0x218, 4·inc a sample
// down over ±1, shaped alike), which gives the saw's slope.  It moves by four
// subtractions.  A sync bit also sets z to ½.
uint32_t XtAnalogOsc::laSaw(float* lr, uint32_t sync) {
    const float inc = laInc;
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) { ph = 0.0f; ph2 = kHalf; }
        float x;
        if (fatC > ph) { x = div(ph, fatC); fl |= 2; }
        else { x = div(sub(ph, fatC), sub(1.0f, fatC)); fl |= 1; }
        ph = add(ph, inc);
        if (fl == 3) { wrap |= bit; fl = 0; }
        float g = mul(sub(pwC, std::fabs(sub(add(x, x), 1.0f))), qC);
        if (g > 1.0f) g = 1.0f;
        else if (-1.0f > g) g = -1.0f;
        g = laOut(g);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
        const float w = sub(mul(std::fabs(ph2), 2.0f), 1.0f);
        g = mul(g, laOut(w));
        ph2 = sub(sub(sub(sub(ph2, inc), inc), inc), inc);
        if (!(ph2 > -1.0f)) ph2 = add(ph2, 2.0f);
        fatC = add(mul(fatC, kLaMove), fatT);
        pwC = add(mul(pwC, kLaMove), pwT);
        qC = add(mul(qC, kLaMove), qT);
        const float y = mul(add(g, imp), gainC);
        lr[2 * i] = lr[2 * i + 1] = y;
        imp = mul(imp, dec);
        gainC = add(mul(gainC, K(0x3f780000)), gainT);
        bit <<= 1;
    }
    return wrap;
}

// LA-SQUARE (0x89508540): the trapezium alone.  At the turn it compares the
// other way (ph > a), and a sync bit takes the second cycle's branch, so x
// starts at −a/(1 − a).
uint32_t XtAnalogOsc::laSquare(float* lr, uint32_t sync) {
    const float inc = laInc;
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        const bool second = sync & bit ? true : ph > fatC;
        if (sync & bit) ph = 0.0f;
        float x;
        if (second) { x = div(sub(ph, fatC), sub(1.0f, fatC)); fl |= 1; }
        else { x = div(ph, fatC); fl |= 2; }
        ph = add(ph, inc);
        if (fl == 3) { wrap |= bit; fl = 0; }
        float g = mul(sub(pwC, std::fabs(sub(add(x, x), 1.0f))), qC);
        if (g > 1.0f) g = 1.0f;
        else if (-1.0f > g) g = -1.0f;
        g = laOut(g);
        if (!(1.0f > ph)) ph = sub(ph, 1.0f);
        fatC = add(mul(fatC, kLaMove), fatT);
        pwC = add(mul(pwC, kLaMove), pwT);
        qC = add(mul(qC, kLaMove), qT);
        const float y = mul(add(g, imp), gainC);
        lr[2 * i] = lr[2 * i + 1] = y;
        imp = mul(imp, dec);
        gainC = add(mul(gainC, K(0x3f780000)), gainT);
        bit <<= 1;
    }
    return wrap;
}

// SUPER-SAW (0x89507520): seven saws, each its own phase of 0…1 a cycle, at
// ratio·440/44100 and ± the spread × that × 1, 3.2 and 5.6.  Each rises −1…+1
// over 0…s (+0x1c8, ratio·Slope·0.01) and falls back over s…1.  out =
// ((centre·0.2 + sum·m)·4(1 − 2.2·m) + Impact)·gain·2^31, m the side saws'
// level.  The mask follows the centre.  A sync bit sets the phases 0, 0.1 …
// 0.6.  Once a block the spread and m move 0.2 of the way.
uint32_t XtAnalogOsc::superSaw(float* lr, uint32_t sync) {
    const float s = pwT, s1 = sub(1.0f, s);
    const float step = mul(ratio_, K(0x3c2377ed)), d = mul(ssDetC, step), d3 = mul(d, K(0x404ccccd)), d5 = mul(d, K(0x40b33333));
    const float inc[6] = { add(step, d), sub(step, d), add(step, d3), sub(step, d3), add(step, d5), sub(step, d5) };
    const float m = ssMixC, k = mul(sub(1.0f, mul(m, K(0x400ccccd))), 4.0f);
    auto wave = [&](float p) { return fmac(s > p ? div(p, s) : div(sub(1.0f, p), s1), 2.0f, -1.0f); };
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) {
            static constexpr float start[6] = { K(0x3dcccccd), K(0x3e4ccccd), K(0x3e999999), K(0x3ecccccd), K(0x3f000000), K(0x3f199999) };
            ph = 0.0f;
            std::copy(start, start + 6, ss);
        }
        const float y0 = wave(ph);
        ph = add(ph, step);
        if (!(1.0f > ph)) { ph = add(ph, -1.0f); wrap |= bit; }
        float y[6];
        for (int j = 0; j < 6; ++j) {
            y[j] = wave(ss[j]);
            ss[j] = add(ss[j], inc[j]);
            if (!(1.0f > ss[j])) ss[j] = add(ss[j], -1.0f);
        }
        const float sum = add(add(add(add(add(y[0], y[1]), y[2]), y[3]), y[4]), y[5]);
        const float x = fmac(fmac(sum, m, mul(y0, K(0x3e4ccccd))), k, imp);
        lr[2 * i] = lr[2 * i + 1] = mul(mul(x, gainC), kFull31);
        imp = mul(imp, dec);
        gainC = fmac(sub(gainT, gainC), kStep, gainC);
        bit <<= 1;
    }
    ssDetC = fmac(sub(ssDetT, ssDetC), K(0x3e4ccccd), ssDetC);
    ssMixC = fmac(sub(ssMixT, ssMixC), K(0x3e4ccccd), ssMixC);
    return wrap;
}

// A tap of the delay line (0x89504950), t samples back from w
static float lineTap(const int8_t* line, int w, float t) {
    const int n = ftrc(t);
    const float f = sub(t, flt(n));
    int i0 = w - n, i1 = i0 - 1;
    if (i0 < 0) i0 += XtAnalogShared::kLineLen;
    if (i1 < 0) i1 += XtAnalogShared::kLineLen;
    return div(fmac(flt(line[i0]), sub(1.0f, f), mul(flt(line[i1]), f)), 127.0f);
}

// FEEDBACK-OSC (0x89507900): a saw (the phase of 0…1 a cycle at ratio·440/44100;
// rising over 0…s, falling over s…1) into a delay line of signed bytes, 8 bits
// at 44.1 kHz.  Two taps feed back: one period back (t = 1/step, halved until
// it is at most 2699) at 0.32, and t·(½ + +0x250) back at 1.6·+0x258.  The sum
// within ±1, z = x·0.99804 + state, state moving by z·(−0.003919) (+0x274, a
// DC block); z·0.66·127 goes into the line, and out = (z + Impact)·gain·2^31.
// The mask follows the phase.  A sync bit only resets the phase.  Once a block
// +0x250 and +0x258 move 0.2 of the way.
uint32_t XtAnalogOsc::feedbackOsc(float* lr, uint32_t sync) {
    const float s = pwT, s1 = sub(1.0f, s);
    const float step = mul(ratio_, K(0x3c2377ed));
    float t = div(1.0f, step);
    while (t > 2699.0f) t = div(t, 2.0f);
    const float t2 = mul(t, add(ssDetC, kHalf)), lev = mul(ssMixC, K(0x3fcccccd));
    int8_t* line = shared_->line[fbSlot];
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        const float v = s > ph ? div(ph, s) : div(sub(1.0f, ph), s1);
        ph = add(ph, step);
        if (!(1.0f > ph)) { ph = add(ph, -1.0f); wrap |= bit; }
        float x = mul(fmac(v, 2.0f, -1.0f), K(0x3f0ccccc));
        x = fmac(lineTap(line, fbW, t), K(0x3ea3d709), x);
        x = fmac(lineTap(line, fbW, t2), lev, x);
        if (x > 1.0f) x = 1.0f;
        else if (-1.0f > x) x = -1.0f;
        const float z = fmac(x, K(0x3f7f7f8d), fbZ);
        fbZ = fmac(z, K(0xbb806af3), fbZ);
        if (++fbW >= XtAnalogShared::kLineLen) fbW = 0;
        line[fbW] = (int8_t)ftrc(mul(mul(z, K(0x3f28f5c2)), 127.0f));
        lr[2 * i] = lr[2 * i + 1] = mul(mul(add(z, imp), gainC), kFull31);
        imp = mul(imp, dec);
        gainC = fmac(sub(gainT, gainC), kStep, gainC);
        bit <<= 1;
    }
    ssDetC = fmac(sub(ssDetT, ssDetC), K(0x3e4ccccd), ssDetC);
    ssMixC = fmac(sub(ssMixT, ssMixC), K(0x3e4ccccd), ssMixC);
    return wrap;
}

// NOISE (0x89504a40): integer random values (two states: +0x1dc, cleared at
// note-on, and +0x244), each held for the hold's samples, the hold starting
// again every block; (Impact + value)·gain, the value already ±2^31.  No
// phase, sync, Fat or PW; returns 0.
// X-MOD (0x89507b60): SAW's wave with the phase moved by the other OSC's output,
// (L/2^31 + R/2^31)/2 × 0.2·depth per sample, and that buffer cleared.  A wrap
// sets a mask bit only if a·d == a, which never happens: the firmware keeps three
// copies of its loop for that, and the port one test.  The phase comes back up by
// 1 when the modulation takes it to 0 or below, and may stay out of range.  Out =
// (y·sign + Impact)·gain·2^31.  Each block Fat, PW and depth move 0.2 of the way.
uint32_t XtAnalogOsc::xMod(float* lr, uint32_t sync, float* mod) {
    const float inc = mul(ratio_, K(0x3ba377ed));
    float d = sub(kHalf, pwC), sign = 1.0f;
    if (0.0f > d) { sign = -1.0f; d = add(d, 1.0f); }
    const float a = fatC, ad = mul(a, d), bb = fmac(sub(1.0f, a), d, a);
    const float k0 = div(2.0f, ad), kad = div(2.0f, sub(ad, a)), kba = div(2.0f, sub(bb, a)), kb1 = div(2.0f, add(bb, -1.0f));
    const float depth = mul(ssDetC, K(0x3e4ccccd));
    const bool wrapsMask = ad == a;
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = 0.0f;
        float y;
        if (!(bb > ph)) { y = fmac(add(ph, -1.0f), kb1, -1.0f); fl |= 2; }
        else if (!(a > ph)) { y = fmac(add(ph, -a), kba, -1.0f); fl |= 2; }
        else {
            fl |= 1;
            y = ad > ph ? fmac(ph, k0, -1.0f) : fmac(add(ph, -a), kad, -1.0f);
        }
        if (fl == 3) { fl = 0; wrap |= bit; }
        float m = 0.0f;
        if (mod) {
            m = div(add(div(mod[2 * i], kFull31), div(mod[2 * i + 1], kFull31)), 2.0f);
            mod[2 * i] = mod[2 * i + 1] = 0.0f;
        }
        ph = add(fmac(m, depth, ph), inc);
        if (!(1.0f > ph)) {
            ph = add(ph, -1.0f);
            if (wrapsMask) wrap |= bit;
        } else if (!(ph > 0.0f)) ph = add(ph, 1.0f);
        const float out = mul(mul(fmac(y, sign, imp), gainC), kFull31);
        lr[2 * i] = lr[2 * i + 1] = out;
        imp = mul(imp, dec);
        gainC = fmac(sub(gainT, gainC), kStep, gainC);
        bit <<= 1;
    }
    constexpr float kMove = K(0x3e4ccccd);
    fatC = fmac(sub(fatT, fatC), kMove, fatC);
    pwC = fmac(sub(pwT, pwC), kMove, pwC);
    ssDetC = fmac(sub(ssDetT, ssDetC), kMove, ssDetC);
    return wrap;
}

uint32_t XtAnalogOsc::noise(float* lr) {
    const int n0 = hold > 0 ? hold : 1;        // the firmware never ends a block with a hold of 0; it never gets one
    int left = 32;
    while (left > 0) {
        fl = fl * 0xffffff06u + s2 * 127u + 30u;     // −250·s1 + 127·s2 + 30
        s2 = s2 * 252u + fl;
        const float v = flt((int32_t)fl);
        for (int n = n0; n > 0 && left > 0; --n, --left) {
            const float y = mul(add(imp, v), gainC);
            imp = mul(imp, dec);
            lr[0] = lr[1] = y;
            lr += 2;
            gainC = fmac(sub(gainT, gainC), kStep, gainC);
        }
    }
    return 0;
}

// The sub oscillator's routines.  Two waves, each with a phase of 0…1 a cycle
// (+0x1d4 at step +0x1f4, the mask following it; +0x1fc at step +0x1f8) and a
// Pulse Width (+0x1cc, +0x1f0); out = (y + second·level + Impact)·gain.  Pulse
// Widths, steepness and the level move x·0.998 + target every sample, the gain
// x·0.96875 + target.
static inline float clip1(float g) { return g > 1.0f ? 1.0f : (-1.0f > g ? -1.0f : g); }
static inline float subWave(float p, float w) {   // rising to +1 at w, falling back
    const float v = w > p ? div(p, w) : div(sub(1.0f, p), sub(1.0f, w));
    return sub(add(v, v), 1.0f);
}
static inline float trapezium(float p, float w, float q) { return clip1(mul(sub(w, std::fabs(sub(add(p, p), 1.0f))), q)); }

// SAW (0x89508f30), TRIANGLE (0x89509680) and SINE (0x89509230).  A sync bit sets
// the phases to the Pulse Widths (SAW) or half of them.  SINE shapes each y·(2 − |y|),
// the second wave after its level.
uint32_t XtAnalogOsc::subPeak(float* lr, uint32_t sync, bool halfSync, bool shaped) {
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) {
            ph = halfSync ? mul(pwC, kHalf) : pwC;
            sph = halfSync ? mul(swC, kHalf) : swC;
        }
        float y = subWave(ph, pwC);
        pwC = add(mul(pwC, kLaMove), pwT);
        ph = add(ph, laInc);
        if (!(1.0f > ph)) { wrap |= bit; ph = sub(ph, 1.0f); }
        if (shaped) y = mul(y, sub(2.0f, std::fabs(y)));
        float u = subWave(sph, swC);
        swC = add(mul(swC, kLaMove), swT);
        sph = add(sph, subInc);
        if (!(1.0f > sph)) sph = sub(sph, 1.0f);
        u = mul(u, lvlC);
        if (shaped) u = mul(u, sub(2.0f, std::fabs(u)));
        y = add(y, u);
        lvlC = add(mul(lvlC, kLaMove), lvlT);
        lr[2 * i] = lr[2 * i + 1] = mul(add(y, imp), gainC);
        imp = mul(imp, dec);
        gainC = add(mul(gainC, K(0x3f780000)), gainT);
        bit <<= 1;
    }
    return wrap;
}

// SQUARE (0x89509360): trapezia, (w − |2p − 1|)·q within ±1.  A sync bit sets the
// phases to (1 − w)/2.  out = −(y − Impact)·gain.
uint32_t XtAnalogOsc::subSquare(float* lr, uint32_t sync) {
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) {
            ph = mul(sub(1.0f, pwC), kHalf);
            sph = mul(sub(1.0f, swC), kHalf);
        }
        const float g = trapezium(ph, pwC, sqC);
        ph = add(ph, laInc);
        if (!(1.0f > ph)) { wrap |= bit; ph = sub(ph, 1.0f); }
        const float u = mul(trapezium(sph, swC, sq2C), lvlC);
        sph = add(sph, subInc);
        if (!(1.0f > sph)) sph = sub(sph, 1.0f);
        pwC = add(mul(pwC, kLaMove), pwT);
        lr[2 * i] = lr[2 * i + 1] = -mul(sub(add(g, u), imp), gainC);
        imp = mul(imp, dec);
        gainC = add(mul(gainC, K(0x3f780000)), gainT);
        sqC = add(mul(sqC, kLaMove), sqT);
        swC = add(mul(swC, kLaMove), swT);
        lvlC = add(mul(lvlC, kLaMove), lvlT);
        sq2C = add(mul(sq2C, kLaMove), sq2T);
        bit <<= 1;
    }
    return wrap;
}

// RAMP (0x89508960): 0…1 over 0…w, 1…−1 over w…1 − k, −1…0 over the last k (+0x208,
// +0x210).  A sync bit sets the phases to 0.
static inline float subRampWave(float p, float w, float k) {
    if (w > p) return div(p, w);
    const float e = sub(1.0f, k);
    if (e > p) { const float y = div(sub(w, p), sub(e, w)); return add(add(y, y), 1.0f); }
    return div(sub(p, 1.0f), k);
}
uint32_t XtAnalogOsc::subRamp(float* lr, uint32_t sync) {
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = sph = 0.0f;
        const float y = subRampWave(ph, pwC, rampK);
        ph = add(ph, laInc);
        pwC = add(mul(pwC, kLaMove), pwT);
        if (!(1.0f > ph)) { wrap |= bit; ph = sub(ph, 1.0f); }
        const float u = mul(subRampWave(sph, swC, rampK2), lvlC);
        sph = add(sph, subInc);
        if (!(1.0f > sph)) sph = sub(sph, 1.0f);
        lr[2 * i] = lr[2 * i + 1] = mul(add(add(y, u), imp), gainC);
        imp = mul(imp, dec);
        gainC = add(mul(gainC, K(0x3f780000)), gainT);
        lvlC = add(mul(lvlC, kLaMove), lvlT);
        swC = add(mul(swC, kLaMove), swT);
        bit <<= 1;
    }
    return wrap;
}

// JUNO (0x89508060): a rising saw 2p − 1 cut off by a pulse at twice its rate,
// (|2x − 1| − 1 + w)·q, x = 2(p + w/4) wrapped to 0…1; the lower of the two, at
// least −1.  A sync bit sets the phases to 0.
static inline float subJunoWave(float p, float w, float q) {
    float x = add(mul(0.25f, w), p);
    x = add(x, x);
    if (!(1.0f > x)) x = sub(x, 1.0f);
    else if (0.0f > x) x = add(x, 1.0f);
    x = mul(add(sub(std::fabs(sub(add(x, x), 1.0f)), 1.0f), w), q);
    const float s = sub(add(p, p), 1.0f);
    return x > s ? s : (-1.0f > x ? -1.0f : x);
}
uint32_t XtAnalogOsc::subJuno(float* lr, uint32_t sync) {
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = sph = 0.0f;
        const float y = subJunoWave(ph, pwC, sqC);
        ph = add(ph, laInc);
        if (!(1.0f > ph)) { wrap |= bit; ph = sub(ph, 1.0f); }
        const float u = mul(subJunoWave(sph, swC, sq2C), lvlC);
        sph = add(sph, subInc);
        if (!(1.0f > sph)) sph = sub(sph, 1.0f);
        pwC = add(mul(pwC, kLaMove), pwT);
        lr[2 * i] = lr[2 * i + 1] = mul(add(add(y, u), imp), gainC);
        imp = mul(imp, dec);
        gainC = add(mul(gainC, K(0x3f780000)), gainT);
        swC = add(mul(swC, kLaMove), swT);
        sqC = add(mul(sqC, kLaMove), sqT);
        lvlC = add(mul(lvlC, kLaMove), lvlT);
        sq2C = add(mul(sq2C, kLaMove), sq2T);
        bit <<= 1;
    }
    return wrap;
}

// LA-SQUARE (0x89508670): trapezia shaped g·(2 − |g|), the steepness +0x228 and
// +0x230.  A sync bit sets the phases to 0.  out = (−y + Impact)·gain.
uint32_t XtAnalogOsc::subLaSquare(float* lr, uint32_t sync) {
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) ph = sph = 0.0f;
        const float g = laOut(trapezium(ph, pwC, qC));
        ph = add(ph, laInc);
        if (!(1.0f > ph)) { wrap |= bit; ph = sub(ph, 1.0f); }
        const float u = mul(laOut(trapezium(sph, swC, q2C)), lvlC);
        sph = add(sph, subInc);
        if (!(1.0f > sph)) sph = sub(sph, 1.0f);
        const float y = add(g, u);
        qC = add(mul(qC, kLaMove), qT);
        q2C = add(mul(q2C, kLaMove), q2T);
        lvlC = add(mul(lvlC, kLaMove), lvlT);
        pwC = add(mul(pwC, kLaMove), pwT);
        swC = add(mul(swC, kLaMove), swT);
        lr[2 * i] = lr[2 * i + 1] = mul(add(-y, imp), gainC);
        imp = mul(imp, dec);
        gainC = add(mul(gainC, K(0x3f780000)), gainT);
        bit <<= 1;
    }
    return wrap;
}

// LA-SAW (0x89508350): LA-SQUARE's waves, each × a second one (+0x218, +0x21c;
// each step twice a sample, down over ±1, shaped alike).  A sync bit sets the
// phases to 0 and these to ½.
uint32_t XtAnalogOsc::subLaSaw(float* lr, uint32_t sync) {
    auto second = [](float& z, float s) {
        z = sub(sub(z, s), s);
        if (!(z > -1.0f)) z = add(z, 2.0f);
        const float a = std::fabs(z);
        return laOut(sub(add(a, a), 1.0f));
    };
    uint32_t bit = 1, wrap = 0;
    for (int i = 0; i < 32; ++i) {
        if (sync & bit) { ph = sph = 0.0f; ph2 = ph3 = kHalf; }
        float g = laOut(trapezium(ph, pwC, qC));
        g = mul(g, second(ph2, laInc));
        ph = add(ph, laInc);
        if (!(1.0f > ph)) { wrap |= bit; ph = sub(ph, 1.0f); }
        float u = mul(laOut(trapezium(sph, swC, q2C)), lvlC);
        u = mul(u, second(ph3, subInc));
        sph = add(sph, subInc);
        if (!(1.0f > sph)) sph = sub(sph, 1.0f);
        const float y = add(g, u);
        lr[2 * i] = lr[2 * i + 1] = mul(add(-y, imp), gainC);
        imp = mul(imp, dec);
        gainC = add(mul(gainC, K(0x3f780000)), gainT);
        lvlC = add(mul(lvlC, kLaMove), lvlT);
        pwC = add(mul(pwC, kLaMove), pwT);
        swC = add(mul(swC, kLaMove), swT);
        qC = add(mul(qC, kLaMove), qT);
        q2C = add(mul(q2C, kLaMove), q2T);
        bit <<= 1;
    }
    return wrap;
}

#ifdef XT_ANALOG_MAIN
// xt_analog <rom.2.bin> [cases]: the analog converter (0x894e1920), the
// note-on (0x89504af0), the 10-ms update (0x89505400) and SAW (0x89508c20)
// against the firmware in the SH-4 interpreter (XT_ANALOG_ONLY=pcm: the last
// passes alone, a PCM voice on OSC 2 and the stereo flag; XT_ANALOG_SEED: another
// seed than 12345).  The system's analog
// calibration (0x894e5f80) is set from the descriptors' defaults; the matrix
// fetch (0x894d96d0) returns the case's modulation, the controller getters a
// zeroed block.
#include "Sh4Interp.h"
#include "XtEnvelope.h"
#include "XtFirmware.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
int main(int argc, char** argv) {
    if (argc < 2 || !xtAnalogLoadTables(argv[1]) || !xtOscLoadTables(argv[1]) || !xtCosmLoadTables(argv[1]) || !xtFwOpen(argv[1])) {
        std::fprintf(stderr, "usage: xt_analog rom.2.bin [cases]\n");
        return 2;
    }
#if defined(__aarch64__)
    {   // denormals flushed, as the SH-4 does (FPSCR.DN = 1) and the plugin's audio thread does
        uint64_t fpcr;
        __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
        __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
    }
#endif
    {
        std::vector<float> v;
        if (!xtFwOscWaveGains(v)) { std::fprintf(stderr, "wave gains failed\n"); return 1; }
        xtOscSetWaveGains(v.data());
    }
    Sh4 c;
    if (!c.loadRom(argv[1])) return 1;
    const uint32_t ram = 0x8C000000, sp = ram + 0xF0000, P = ram, OUT = ram + 0x400, part = ram + 0x800, zero = ram + 0xC00;
    const uint32_t V = ram + 0x10000, EP = ram + 0x18000, EF = ram + 0x18100, EW = ram + 0x18200, EA = ram + 0x18300, LF = ram + 0x18400,
                   BUF = ram + 0x19000;
    c.mapRam(ram, 0x100000);
    auto f32 = [&](uint32_t a) { const uint32_t v = c.rd32(a); float x; std::memcpy(&x, &v, 4); return x; };
    auto wf = [&](uint32_t a, float x) { uint32_t u; std::memcpy(&u, &x, 4); c.wr32(a, u); };
    auto same = [](float a, float b) { return std::memcmp(&a, &b, 4) == 0; };
    uint32_t seed = std::getenv("XT_ANALOG_SEED") ? (uint32_t)std::atol(std::getenv("XT_ANALOG_SEED")) : 12345u;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 8) & 0xffff; };
    auto frnd = [&](float lo, float hi) { return lo + (hi - lo) * (float)rnd() / 65535.0f; };
    const int nPcm = argc > 2 ? std::atoi(argv[2]) : 3000;
    const bool pcmOnly = std::getenv("XT_ANALOG_ONLY") != nullptr;   // XT_ANALOG_ONLY=pcm: the PCM pass alone
    const int nCases = pcmOnly ? 0 : nPcm;

    // the analog calibration, as the system updater writes it from the parameters
    std::vector<uint8_t> z(0x1000, 0);
    c.hook(0x894796d0, [](Sh4& m) { m.ret(0); });
    c.hook(0x8947d310, [](Sh4& m) {
        const int id = (int16_t)m.r[5];
        m.ret(id >= kSys0 && id < kSys0 + kSysN ? (uint32_t)T.sys[id - kSys0] : 0u);
    });
    if (!c.call(0x894e5f80, { 0x89d1b070u, 0, 0, 0 }, sp)) { std::fprintf(stderr, "system: %s\n", c.error().c_str()); return 1; }
    int mtx = 0;
    c.hook(0x894d96d0, [&](Sh4& m) { m.ret((uint32_t)mtx); });
    for (uint32_t a : { 0x894e7010u, 0x894e70b0u, 0x894e7320u, 0x894e7520u }) c.hook(a, [&](Sh4& m) { m.ret(zero); });

    struct Case {
        XtAnalogParams p;
        int key, key2, vel, coarse, kf, fine, gainDb, level, levelKf, fatCurve, fatSens, pwCurve, pwSens;
    };
    auto newCase = [&](bool wide) {
        Case k;
        k.p.wave = (int)(rnd() % 11);
        k.p.impact = (int)(rnd() % 41);
        k.p.pw = (int)(rnd() % 127) - 63; k.p.pwKf = (int)(rnd() % 41) - 20; k.p.pwLfo = (int)(rnd() % 127) - 63; k.p.pwEnv = (int)(rnd() % 127) - 63;
        k.p.fat = (int)(rnd() % 128); k.p.fatKf = (int)(rnd() % 41) - 20; k.p.fatLfo = (int)(rnd() % 127) - 63; k.p.fatEnv = (int)(rnd() % 127) - 63;
        if (rnd() % 4 == 0) k.p.pwKf = k.p.fatKf = 0;
        k.key = (int)(rnd() % 128); k.key2 = (int)(rnd() % 128); k.vel = 1 + (int)(rnd() % 127);
        k.coarse = (int)(rnd() % 97) - 48; k.kf = (rnd() & 1) ? 1000 : (int)(rnd() % 4001) - 2000; k.fine = (int)(rnd() % 1001) - 500;
        if (!wide) k.coarse = (int)(rnd() % 25) - 12;
        k.gainDb = (int)(rnd() % 25) - 12; k.level = (int)(rnd() % 128); k.levelKf = (int)(rnd() % 41) * 10 - 200;
        k.fatCurve = (int)(rnd() % 8); k.fatSens = (int)(rnd() % 127) - 63; k.pwCurve = (int)(rnd() % 8); k.pwSens = (int)(rnd() % 127) - 63;
        return k;
    };
    // the converter's input, as the OSC table updater (0x894e55d0) lays it out
    int oscIdx = 0;                             // the input's +2: 0 OSC 1, 1 OSC 2
    auto writeInput = [&](const Case& k) {
        c.write(P, z.data(), 0x100); c.write(part, z.data(), 0x200);
        c.wr8(P + 2, (uint8_t)oscIdx);
        c.wr8(P + 3, (uint8_t)k.key); c.wr8(P + 4, (uint8_t)k.vel); c.wr8(P + 0xa, (uint8_t)(int8_t)k.gainDb);
        c.wr16(P + 0xe, (uint16_t)(int16_t)k.kf); c.wr16(P + 0x68, (uint16_t)(int16_t)k.coarse); c.wr32(P + 0x70, (uint32_t)(int32_t)k.fine);
        c.wr8(P + 0x7c, (uint8_t)k.key2);
        c.wr8(P + 0x1c, (uint8_t)k.level); c.wr16(P + 0x1e, (uint16_t)(int16_t)k.levelKf);
        c.wr8(P + 0x21, (uint8_t)xtVelCurve(1)); c.wr16(P + 0x22, (uint16_t)(int16_t)xtVelSens(0));
        c.wr8(P + 0x24, (uint8_t)k.p.wave); c.wr8(P + 0x25, (uint8_t)k.p.impact); c.wr8(P + 0x26, (uint8_t)(int8_t)k.p.pw);
        c.wr16(P + 0x28, (uint16_t)(int16_t)(k.p.pwKf * 10)); c.wr8(P + 0x2a, (uint8_t)(int8_t)k.p.pwLfo); c.wr8(P + 0x2b, (uint8_t)(int8_t)k.p.pwEnv);
        c.wr8(P + 0x2c, (uint8_t)xtVelCurve(k.pwCurve)); c.wr16(P + 0x2e, (uint16_t)(int16_t)xtVelSens(k.pwSens));
        c.wr8(P + 0x30, (uint8_t)k.p.fat); c.wr16(P + 0x32, (uint16_t)(int16_t)(k.p.fatKf * 10));
        c.wr8(P + 0x34, (uint8_t)(int8_t)k.p.fatLfo); c.wr8(P + 0x35, (uint8_t)(int8_t)k.p.fatEnv);
        c.wr8(P + 0x36, (uint8_t)xtVelCurve(k.fatCurve)); c.wr16(P + 0x38, (uint16_t)(int16_t)xtVelSens(k.fatSens));
        c.wr8(P + 0x3a, (uint8_t)(int8_t)k.p.detune); c.wr8(P + 0x3b, (uint8_t)k.p.subOctave); c.wr8(P + 0x3c, (uint8_t)k.p.subLevel);
        c.wr32(P + 0xdc, part);
    };
    auto velFactor = [&](int vel, int curve, int sens) { return div(flt(xtVelCurveValue(vel, xtVelSens(sens), xtVelCurve(curve))), 32768.0f); };
    auto portBlock = [&](const Case& k) {
        XtAnalogParams q = k.p;
        q.pw += mtx; q.pwLfo += mtx; q.pwEnv += mtx; q.fat += mtx; q.fatLfo += mtx; q.fatEnv += mtx; q.detune += mtx;
        return xtAnalogBlock(q, k.key, xtPitch10(k.key, k.coarse, k.kf, k.fine), k.gainDb, velFactor(k.vel, k.fatCurve, k.fatSens),
                             velFactor(k.vel, k.pwCurve, k.pwSens), oscIdx != 0);
    };

    // The converter
    long cb = 0, cn = 0;
    auto converterCases = [&](int n, int wave) { if (pcmOnly) return true; for (int g = 0; g < n; ++g) {
        Case k = newCase(true);
        if (wave >= 0) { k.p.wave = wave; k.p.detune = (int)(rnd() % 127) - 63; }
        if (wave == -2) {                       // the sub oscillator: Sub Octave, Level and Detune; every depth's segment
            k.p.subOctave = (int)(rnd() % 4); k.p.subLevel = (int)(rnd() % 128); k.p.detune = (int)(rnd() % 127) - 63;
        }
        mtx = (g % 3 == 0) ? (int)(rnd() % 75) - 37 : 0;
        writeInput(k);
        c.write(OUT, z.data(), 0x100);
        if (!c.call(0x894e1920, { P, OUT }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return false; }
        const XtAnalogBlock b = portBlock(k);
        ++cn;
        const float fw[14] = { f32(OUT + 0x84), f32(OUT + 0x88), f32(OUT + 0x90), f32(OUT + 0x98), f32(OUT + 0x9c), f32(OUT + 0xa0),
                               f32(OUT + 0xac), f32(OUT + 0xb0), f32(OUT + 0x5c), f32(OUT + 8), f32(OUT + 0xbc), f32(OUT + 0xc0),
                               f32(OUT + 0xb4), f32(OUT + 0xb8) };
        const float po[14] = { b.fat, b.fatEnv, b.fatLfo, b.pw, b.pwEnv, b.pwLfo, b.impact, b.slope, b.gain,
                               xtPitchCents(k.key, k.coarse, k.kf, k.fine, 69, false), b.tone, b.toneKf, b.subOn, b.subLevel };
        static const char* name[14] = { "fat", "fat env", "fat lfo", "pw", "pw env", "pw lfo", "impact", "slope", "gain", "cents", "tone", "tone kf", "sub on", "sub level" };
        // the envelopes (pitch, amp, Fat, PW) and LFOs the binding (0x894f7070) takes: OSC 1's 1…4 and LFO 1, OSC 2's 5…8 and LFO 2
        const uint32_t e0 = oscIdx ? 4 : 0, l0 = oscIdx ? 2 : 1;
        bool ok = (int)c.rd32(OUT + 0x80) == b.code && c.rd32(OUT + 0x8c) == e0 + 3 && c.rd32(OUT + 0x94) == l0
                  && c.rd32(OUT + 0xa4) == e0 + 2 && c.rd32(OUT + 0xa8) == l0 && c.rd32(OUT + 0x20) == e0 + 1 && c.rd32(OUT + 0x38) == l0
                  && c.rd32(OUT + 0x4c) == e0 + 4 && c.rd32(OUT + 0x50) == l0;
        for (int i = 0; i < 14; ++i) ok = ok && same(fw[i], po[i]);
        if (!ok && cb++ < 8) {
            std::printf("  converter wave %d key %d mtx %d pw %d kf %d fat %d kf %d: code %u/%d", k.p.wave, k.key, mtx, k.p.pw, k.p.pwKf, k.p.fat,
                        k.p.fatKf, c.rd32(OUT + 0x80), b.code);
            for (int i = 0; i < 14; ++i)
                if (!same(fw[i], po[i])) std::printf(" %s %.9g/%.9g", name[i], fw[i], po[i]);
            std::printf("\n");
        }
    } return true; };
    if (!converterCases(200000, -1)) return 1;
    std::printf("converter: %ld cases, %ld different\n", cn, cb);

    // The voice: the converter's block, the note-on, the install, then blocks
    // of the installed routine with an update every few blocks, the state,
    // the output and the returned mask compared after each.  Each case
    // takes one of the waveforms the port plays, in turn; HQ-SAW and
    // HQ-SQUARE in a second pass, after the others.
    c.write(V, z.data(), 0x1000);
    c.wr32(V + 0x970, EP); c.wr32(V + 0x974, EF); c.wr32(V + 0x978, EW); c.wr32(V + 0x97c, EA);
    for (uint32_t o = 0x980; o < 0x990; o += 4) c.wr32(V + o, LF);
    constexpr int kSub = 13, kWaves = 24;       // from kSub the same waveforms with the sub oscillator on
    static const int waves[kWaves] = { 0, 1, 2, 3, 4, 5, 8, 6, 7, 10, 9, 11, 12, 0, 1, 2, 3, 4, 5, 8, 6, 7, 10, 9 };
    static const char* waveName[kWaves] = { "saw", "square", "triangle", "sine", "ramp", "juno", "noise", "hq-saw", "hq-square", "la-square", "la-saw", "super-saw",
                                            "feedback-osc", "sub saw", "sub square", "sub triangle", "sub sine", "sub ramp", "sub juno", "sub noise", "sub hq-saw",
                                            "sub hq-square", "sub la-square", "sub la-saw" };
    static const uint32_t routine[13] = { 0x895063f0, 0x895060e0, 0x89508c20, 0x89508aa0, 0x895094c0, 0x89508800, 0x89509030, 0x89508db0,
                                          0x89504a40, 0x89508540, 0x895081f0, 0x89507520, 0x89507900 };
    static const uint32_t subRoutine[11] = { 0x895063f0, 0x895060e0, 0x89508f30, 0x89509360, 0x89509680, 0x89508960, 0x89509230, 0x89508060,
                                             0x89504a40, 0x89508670, 0x89508350 };
    XtAnalogOsc osc;
    XtAnalogShared shared;
    constexpr uint32_t kSsCount = 0x8987c894;
    shared.superSaw = f32(kSsCount);
    constexpr uint32_t kFbFlags = 0x89ff0c4c, kFbLines = 0x89fe4e74;
    long nb = 0, nn = 0, wb = 0, db[kWaves] = {}, dn[kWaves] = {}, mb = 0;
    float peak[kWaves] = {};
    // the eighteen flags and the voice's whole delay line
    auto linesSame = [&] {
        for (int i = 0; i < XtAnalogShared::kLines; ++i)
            if (c.rd8(kFbFlags + (uint32_t)i) != shared.lineUsed[i]) return false;
        const int sl = osc.fbSlotIndex();
        if (sl < 0) return true;
        const uint32_t base = kFbLines + (uint32_t)XtAnalogShared::kLineLen * (uint32_t)sl;
        for (int i = 0; i < XtAnalogShared::kLineLen; ++i)
            if ((int8_t)c.rd8(base + (uint32_t)i) != shared.line[sl][i]) return false;
        return true;
    };
    int code = 2;
    bool allFields = false;                    // the sub passes
    auto stateSame = [&](bool full) {
        bool ok = same(f32(V + 0x144), osc.ratio()) && same(f32(V + 0x154), osc.gainTarget()) && same(f32(V + 0x1c0), osc.fatTarget())
               && same(f32(V + 0x1c8), osc.pwTarget());
        if (code == 3) ok = ok && same(f32(V + 0x1e0), osc.edge());
        if (code == 8) ok = ok && (int)(int8_t)c.rd8(V + 0x248) == osc.noiseHold();
        if (code == 9 || code == 10) {
            ok = ok && same(f32(V + 0x1f4), osc.laStep()) && same(f32(V + 0x224), osc.laSteepTarget()) && c.rd32(V + 0x20c) == 0;
            if (full) ok = ok && same(f32(V + 0x228), osc.laSteep()) && same(f32(V + 0x218), osc.laPhase2());
        }
        if (code == 12) {
            ok = ok && same(f32(V + 0x24c), osc.detuneTarget()) && same(f32(V + 0x254), osc.mixTarget());
            if (full)
                ok = ok && same(f32(V + 0x250), osc.detune()) && same(f32(V + 0x258), osc.mix()) && same(f32(V + 0x274), osc.fbState())
                     && (int)c.rd32(V + 0x278) == osc.fbWrite() && (int)c.rd32(V + 0x27c) == osc.fbSlotIndex();
        }
        if (code == 11) {
            ok = ok && same(f32(V + 0x24c), osc.detuneTarget()) && same(f32(V + 0x254), osc.mixTarget()) && same(f32(kSsCount), shared.superSaw);
            if (full) {
                ok = ok && same(f32(V + 0x250), osc.detune()) && same(f32(V + 0x258), osc.mix());
                for (int i = 0; i < 6; ++i) ok = ok && same(f32(V + 0x25c + 4 * (uint32_t)i), osc.sidePhase(i));
            }
        }
        if (allFields) {                           // every field the sub oscillator's code touches or reads, whatever the waveform
            ok = ok && same(f32(V + 0x1e0), osc.edge()) && (int)(int8_t)c.rd8(V + 0x248) == osc.noiseHold() && same(f32(V + 0x1ec), osc.subPwTarget())
                 && same(f32(V + 0x1f4), osc.laStep()) && same(f32(V + 0x1f8), osc.subStep()) && same(f32(V + 0x200), osc.subLevelTarget())
                 && same(f32(V + 0x208), osc.rampEnd()) && same(f32(V + 0x210), osc.rampEnd2()) && same(f32(V + 0x224), osc.laSteepTarget())
                 && same(f32(V + 0x22c), osc.laSteep2Target()) && same(f32(V + 0x234), osc.steepTarget()) && same(f32(V + 0x23c), osc.steep2Target());
            if (full)
                ok = ok && same(f32(V + 0x1f0), osc.subPw()) && same(f32(V + 0x1fc), osc.subPhase()) && same(f32(V + 0x204), osc.subLevel())
                     && same(f32(V + 0x218), osc.laPhase2()) && same(f32(V + 0x21c), osc.laPhase3()) && same(f32(V + 0x228), osc.laSteep())
                     && same(f32(V + 0x230), osc.laSteep2()) && same(f32(V + 0x238), osc.steep()) && same(f32(V + 0x240), osc.steep2());
        }
        if (full)
            ok = ok && same(f32(V + 0x158), osc.gain()) && same(f32(V + 0x1c4), osc.fat()) && same(f32(V + 0x1cc), osc.pw())
                 && same(f32(V + 0x1d0), osc.decay()) && same(f32(V + 0x1d4), osc.phase()) && same(f32(V + 0x1d8), osc.impact())
                 && c.rd32(V + 0x1dc) == osc.flags() && c.rd32(V + 0x244) == osc.noiseState() && same(f32(V + 0x1e4), osc.hqLevel())
                 && (int)c.rd32(V + 0x1e8) == osc.hqFalling();
        return ok;
    };
    auto dump = [&](const char* what, int g, int blk) {
        std::printf("  %s %d.%d code %d: ratio %.9g/%.9g gain %.9g/%.9g cur %.9g/%.9g fat %.9g/%.9g %.9g/%.9g pw %.9g/%.9g %.9g/%.9g dec %.9g/%.9g "
                    "phase %.9g/%.9g imp %.9g/%.9g flags %u/%u edge %.9g/%.9g noise %u/%u hold %d/%d hq %.9g/%.9g %d/%d\n", what, g, blk, code, f32(V + 0x144),
                    osc.ratio(), f32(V + 0x154), osc.gainTarget(), f32(V + 0x158), osc.gain(), f32(V + 0x1c0), osc.fatTarget(), f32(V + 0x1c4),
                    osc.fat(), f32(V + 0x1c8), osc.pwTarget(), f32(V + 0x1cc), osc.pw(), f32(V + 0x1d0), osc.decay(), f32(V + 0x1d4), osc.phase(),
                    f32(V + 0x1d8), osc.impact(), c.rd32(V + 0x1dc), osc.flags(), f32(V + 0x1e0), osc.edge(), c.rd32(V + 0x244), osc.noiseState(),
                    (int)(int8_t)c.rd8(V + 0x248), osc.noiseHold(), f32(V + 0x1e4), osc.hqLevel(), (int)c.rd32(V + 0x1e8), osc.hqFalling());
        if (code == 11)
            std::printf("    ss: detune %.9g/%.9g %.9g/%.9g mix %.9g/%.9g %.9g/%.9g phases %.9g/%.9g %.9g/%.9g count %.9g/%.9g\n", f32(V + 0x24c), osc.detuneTarget(),
                        f32(V + 0x250), osc.detune(), f32(V + 0x254), osc.mixTarget(), f32(V + 0x258), osc.mix(), f32(V + 0x25c), osc.sidePhase(0), f32(V + 0x270),
                        osc.sidePhase(5), f32(kSsCount), shared.superSaw);
        if (code == 12)
            std::printf("    fb: tap %.9g/%.9g %.9g/%.9g level %.9g/%.9g %.9g/%.9g state %.9g/%.9g w %d/%d line %d/%d\n", f32(V + 0x24c), osc.detuneTarget(),
                        f32(V + 0x250), osc.detune(), f32(V + 0x254), osc.mixTarget(), f32(V + 0x258), osc.mix(), f32(V + 0x274), osc.fbState(),
                        (int)c.rd32(V + 0x278), osc.fbWrite(), (int)c.rd32(V + 0x27c), osc.fbSlotIndex());
        if (allFields)
            std::printf("    sub: pw %.9g/%.9g %.9g/%.9g step %.9g/%.9g %.9g/%.9g phase %.9g/%.9g level %.9g/%.9g %.9g/%.9g ramp %.9g/%.9g %.9g/%.9g\n"
                        "    z %.9g/%.9g %.9g/%.9g q %.9g/%.9g %.9g/%.9g %.9g/%.9g %.9g/%.9g sq %.9g/%.9g %.9g/%.9g %.9g/%.9g %.9g/%.9g\n",
                        f32(V + 0x1ec), osc.subPwTarget(), f32(V + 0x1f0), osc.subPw(), f32(V + 0x1f4), osc.laStep(), f32(V + 0x1f8), osc.subStep(), f32(V + 0x1fc),
                        osc.subPhase(), f32(V + 0x200), osc.subLevelTarget(), f32(V + 0x204), osc.subLevel(), f32(V + 0x208), osc.rampEnd(), f32(V + 0x210), osc.rampEnd2(),
                        f32(V + 0x218), osc.laPhase2(), f32(V + 0x21c), osc.laPhase3(), f32(V + 0x224), osc.laSteepTarget(), f32(V + 0x228), osc.laSteep(), f32(V + 0x22c),
                        osc.laSteep2Target(), f32(V + 0x230), osc.laSteep2(), f32(V + 0x234), osc.steepTarget(), f32(V + 0x238), osc.steep(), f32(V + 0x23c), osc.steep2Target(),
                        f32(V + 0x240), osc.steep2());
        if (code == 9 || code == 10)
            std::printf("    la: step %.9g/%.9g steep %.9g/%.9g %.9g/%.9g z %.9g/%.9g +0x20c %08x\n", f32(V + 0x1f4), osc.laStep(), f32(V + 0x224),
                        osc.laSteepTarget(), f32(V + 0x228), osc.laSteep(), f32(V + 0x218), osc.laPhase2(), c.rd32(V + 0x20c));
    };
    // the voice's words after the converter's block that no port field holds: unchanged by the sub oscillator's code
    long ob = 0;
    std::vector<uint32_t> otherWords;
    for (uint32_t o = 0x144; o < 0x280; o += 4) {
        static const uint32_t held[] = { 0x144, 0x154, 0x158, 0x1bc, 0x1c0, 0x1c4, 0x1c8, 0x1cc, 0x1d0, 0x1d4, 0x1d8, 0x1dc, 0x1e0, 0x1e4, 0x1e8, 0x1ec, 0x1f0, 0x1f4, 0x1f8, 0x1fc,
                                         0x200, 0x204, 0x208, 0x210, 0x218, 0x21c, 0x224, 0x228, 0x22c, 0x230, 0x234, 0x238, 0x23c, 0x240, 0x244, 0x248,
                                         0x24c, 0x250, 0x254, 0x258, 0x25c, 0x260, 0x264, 0x268, 0x26c, 0x270, 0x274, 0x278, 0x27c };
        if (std::find(std::begin(held), std::end(held), o) == std::end(held)) otherWords.push_back(o);
    }
    std::vector<uint32_t> otherAtStart;
    auto otherSame = [&] {
        for (size_t i = 0; i < otherWords.size(); ++i)
            if (c.rd32(V + otherWords[i]) != otherAtStart[i]) {
                if (ob++ < 4) std::printf("  word +0x%x was %08x, now %08x\n", otherWords[i], otherAtStart[i], c.rd32(V + otherWords[i]));
                return false;
            }
        return true;
    };
    auto voiceCase = [&](int g, int wi) {
        Case k = newCase(false);
        k.p.wave = waves[wi];
        if (wi >= kSub) {
            k.p.subOctave = 1 + (int)(rnd() % 3); k.p.subLevel = (int)(rnd() % 128); k.p.detune = (int)(rnd() % 127) - 63;
            if (g % 8 == 6) k.p.detune = 0;        // the second wave an exact octave, or in unison
            if (g % 8 == 7) {                      // Pulse Widths at the ends, and the highest notes: the limits past ½
                k.p.pw = rnd() & 1 ? 63 : -63; k.p.pwKf = k.p.pwEnv = k.p.pwLfo = 0;
                k.key = 90 + (int)(rnd() % 38); k.coarse = (int)(rnd() % 25);
            }
        }
        if (k.p.wave >= 11) k.p.detune = (int)(rnd() % 127) - 63;
        if (k.p.wave >= 9 && k.p.wave <= 10 && g % 8 == 2) {         // the LA note-on's second wave starting exactly on 1
            k.p.impact = 0; k.p.pw = 0; k.p.pwKf = k.p.pwEnv = k.p.pwLfo = 0;
            if (wi >= kSub) { k.p.subOctave = 3; k.p.detune = 0; k.p.fatKf = k.p.fatEnv = k.p.fatLfo = 0; }   // and the second wave's, its pitch the first's
        }
        if (g % 8 == 1) {                          // SQUARE's pitch steps: G4, A4, A#4, C#5, F#5 untuned
            static const int steps[5] = { 67, 69, 70, 73, 78 };
            k.key = steps[rnd() % 5]; k.coarse = 0; k.fine = 0; k.kf = 1000;
        }
        code = xtAnalogCode(k.p.wave);
        if (code == 12 && g % 8 == 4) {            // the lowest notes: the delay halved twice
            k.key = (int)(rnd() % 6); k.coarse = -12; k.fine = 0; k.kf = 1000;
        }
        mtx = 0;
        writeInput(k);
        if (!c.call(0x894e1920, { P, V }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return false; }
        wf(V + 0x990, 1.0f); wf(V + 0x994, 0.0f);
        const XtAnalogBlock b = portBlock(k);
        // the envelopes' and the LFO's outputs; the amp envelope's attack time for the note-on gain
        float eP = frnd(-1, 1), eF = frnd(0, 1), eW = frnd(0, 1), eA = frnd(0, 1), lfo = frnd(-1, 1);
        if (rnd() % 4 == 0) eF = eW = 0;
        const bool atk = rnd() & 1;
        wf(EP + 0x4c, eP); wf(EF + 0x4c, eF); wf(EW + 0x4c, eW); wf(EA + 0x4c, eA); wf(LF + 0x38, lfo);
        wf(EA + 0x1c, atk ? 25.0f : 0.0f); wf(EA + 0x2c, 0.0f);
        auto ratioOf = [&]() { return xtPitchTarget(f32(V + 8), eP, f32(V + 0x1c), lfo, f32(V + 0x34), f32(V + 0xc)); };
        auto gainOf = [&](bool on) { return xtOscGainTarget(f32(V + 0x5c), f32(V + 0x54), on && atk ? 0.0f : eA, lfo, f32(V + 0x58)); };
        if (code == 12 && g % 8 == 3) {            // other lines in use, sometimes all of them
            const bool all = g % 16 == 3;
            for (int i = 0; i < XtAnalogShared::kLines; ++i) {
                const uint8_t u = all ? 1 : (uint8_t)(rnd() & 1);
                c.wr8(kFbFlags + (uint32_t)i, u); shared.lineUsed[i] = u;
            }
        }
        if (code == 11 && g % 16 == 5) {           // the shared counter's step landing exactly on 1
            float x = sub(1.0f, K(0x3dcccccd));
            while (1.0f > add(x, K(0x3dcccccd))) x = std::nextafter(x, 2.0f);
            wf(kSsCount, x); shared.superSaw = x;
        }
        if (!c.call(0x89504af0, { V }, sp)) { std::fprintf(stderr, "note-on: %s\n", c.error().c_str()); return false; }
        osc.noteOn(b, ratioOf(), gainOf(false), gainOf(true), eF, eW, lfo, shared);
        ++nn;
        if (!stateSame(true) && nb++ < 6) dump("note-on", g, 0);
        if (!c.call(0x89505170, { V }, sp)) { std::fprintf(stderr, "install: %s\n", c.error().c_str()); return false; }
        const uint32_t fn = c.rd32(V + 0x968), want = wi >= kSub ? subRoutine[code] : routine[code];
        if (fn != want && wb++ < 4) std::printf("  install code %d: routine %08x, the map has %08x\n", code, fn, want);
        if (code == 12 && !linesSame() && nb++ < 6) dump("lines at note-on", g, 0);
        const int nBlk = code == 12 && g % 8 == 0 ? 120 : 40;     // FEEDBACK-OSC's line wraps after 85
        for (int blk = 0; blk < nBlk; ++blk) {
            if (blk % 5 == 4) {
                eP = frnd(-1, 1); eF = frnd(0, 1); eW = frnd(0, 1); eA = frnd(0, 1); lfo = frnd(-1, 1);
                wf(EP + 0x4c, eP); wf(EF + 0x4c, eF); wf(EW + 0x4c, eW); wf(EA + 0x4c, eA); wf(LF + 0x38, lfo);
                if (!c.call(0x89505400, { V }, sp)) { std::fprintf(stderr, "update: %s\n", c.error().c_str()); return false; }
                osc.update(b, ratioOf(), gainOf(false), eF, eW, lfo);
                ++nn;
                if (!stateSame(false) && nb++ < 6) dump("update", g, blk);
            }
            if (blk % 6 == 2) {                    // the phase exactly on a breakpoint (as the routine forms it), or 0
                float pts[9];
                const int n = osc.breakpoints(pts);
                pts[n] = 0.0f;
                const float p = pts[rnd() % (uint32_t)(n + 1)];
                if (wi < kSub && (code <= 1 || (code != 8 && p >= 0.0f && 1.0f > p))) { wf(V + 0x1d4, p); osc.setPhase(p); }   // HQ: its own units
                if (wi >= kSub && code <= 1) { wf(V + 0x1d4, p); osc.setPhase(p); }
            }
            if (osc.subActive() && code != 0 && code != 1 && code != 8 && blk % 6 == 2) {   // each phase on a turn of its wave, or a step from 1
                auto place = [&](float w, float inc, float end) {
                    float x = w;
                    switch (rnd() % 5) {
                    case 0: x = 0.0f; break;
                    case 1: x = w; break;
                    case 2: x = sub(1.0f, inc); while (1.0f > add(x, inc)) x = std::nextafter(x, 2.0f); break;
                    case 3: x = code == 5 ? sub(1.0f, end) : mul(w, kHalf); break;
                    default:                                // JUNO's pulse turning
                        x = sub(kHalf, mul(0.25f, w));
                        while (1.0f > add(add(mul(0.25f, w), x), add(mul(0.25f, w), x))) x = std::nextafter(x, 2.0f);
                        break;
                    }
                    return 0.0f > x || !(1.0f > x) ? 0.0f : x;
                };
                const float pm = place(osc.pw(), osc.laStep(), osc.rampEnd()), ps = place(osc.subPw(), osc.subStep(), osc.rampEnd2());
                wf(V + 0x1d4, pm); osc.setPhase(pm);
                wf(V + 0x1fc, ps); osc.setSubPhase(ps);
                if (code == 7 && rnd() % 4 == 0) { wf(V + 0x1cc, -0.25f); osc.setPw(-0.25f); }   // JUNO's x below 0, which no note here reaches
            }
            if (osc.subActive() && code == 10 && blk % 6 == 3) {   // LA-SAW's second waves landing on −1 after their two subtractions, or at their turns
                auto landing = [&](float inc) {
                    const float zs[3] = { add(add(-1.0f, inc), inc), 0.0f, 1.0f };
                    float z = zs[rnd() % 3];
                    if (z != 0.0f && z != 1.0f) while (sub(sub(z, inc), inc) > -1.0f) z = std::nextafter(z, -2.0f);
                    return z;
                };
                const float z2 = landing(osc.laStep()), z3 = landing(osc.subStep());
                wf(V + 0x218, z2); osc.setLaPhase2(z2);
                wf(V + 0x21c, z3); osc.setLaPhase3(z3);
            }
            if (!osc.subActive() && code == 10 && blk % 6 == 3) {      // LA-SAW's z landing on −1 after its four subtractions, or at its turns
                const float s = osc.laStep();
                const float zs[3] = { add(add(add(add(-1.0f, s), s), s), s), 0.0f, 1.0f };
                const float z = zs[rnd() % 3];
                wf(V + 0x218, z); osc.setLaPhase2(z);
            }
            if (code == 12 && blk % 8 == 7 && !linesSame() && db[wi]++ < 4) dump("lines", g, blk);
            if (code == 12 && g % 8 == 0 && blk == 90) {  // the line full: a period exactly at the delay's limit, or past twice it
                float r = 0.0167f;
                if (g % 16 == 0) {
                    r = div(div(1.0f, 2699.0f), K(0x3c2377ed));
                    while (div(1.0f, mul(r, K(0x3c2377ed))) > 2699.0f) r = std::nextafter(r, 2.0f);
                    while (2699.0f > div(1.0f, mul(r, K(0x3c2377ed)))) r = std::nextafter(r, 0.0f);
                }
                wf(V + 0x144, r); osc.setRatio(r);
            }
            if (code == 11 && blk % 6 == 3) {      // the side saws landing on 1, or at the Slope's turn
                const bool turn = rnd() & 1;
                for (int i = 0; i < 6; ++i) {
                    const float st = mul(osc.ratio(), K(0x3c2377ed)), d = mul(osc.detune(), st);
                    const float inc[6] = { add(st, d), sub(st, d), add(st, mul(d, K(0x404ccccd))), sub(st, mul(d, K(0x404ccccd))), add(st, mul(d, K(0x40b33333))), sub(st, mul(d, K(0x40b33333))) };
                    const float p = turn ? osc.pwTarget() : sub(1.0f, inc[i]);
                    wf(V + 0x25c + 4 * (uint32_t)i, p); osc.setSidePhase(i, p);
                }
            }
            const uint32_t sync = rnd() % 8 == 0 ? ((uint32_t)rnd() << 16 | rnd()) & ((uint32_t)rnd() << 16 | rnd()) : 0u;
            uint32_t mask = 0;
            if (!c.call(fn, { BUF, V + 0xd4, sync }, sp, &mask)) { std::fprintf(stderr, "%s: %s\n", waveName[wi], c.error().c_str()); return false; }
            float lr[64];
            const uint32_t pm = osc.process(lr, sync);
            ++dn[wi];
            for (int i = 0; i < 64; i += 2) peak[wi] = std::fmax(peak[wi], std::fabs(lr[i]));
            bool ok = stateSame(true);
            for (int i = 0; i < 64; ++i) ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]);
            if (mask != pm && mb++ < 4) std::printf("  %s %d block %d: mask %08x, port %08x\n", waveName[wi], g, blk, mask, pm);
            if (!ok && db[wi]++ < 4) {
                int i = 0;
                while (i < 64 && same(f32(BUF + 4 * (uint32_t)i), lr[i])) ++i;
                std::printf("  %s %d block %d: first difference at %d: fw %.9g port %.9g\n", waveName[wi], g, blk, i,
                            i < 64 ? f32(BUF + 4 * (uint32_t)i) : 0.0f, i < 64 ? lr[i] : 0.0f);
                dump(waveName[wi], g, blk);
            }
        }
        if (code == 12) {                          // the voice's stop frees the line
            if (!linesSame() && nb++ < 6) dump("lines at the end", g, 0);
            if (!c.call(0x895053c0, { V }, sp)) { std::fprintf(stderr, "reset: %s\n", c.error().c_str()); return false; }
            osc.release();
            if (!linesSame() && nb++ < 6) dump("reset", g, 0);
        }
        if (allFields) {
            if (code != 12) {                      // the voice's stop leaves these alone
                if (!c.call(0x895053c0, { V }, sp)) { std::fprintf(stderr, "reset: %s\n", c.error().c_str()); return false; }
                osc.release();
                if (!stateSame(true) && nb++ < 6) dump("stop", g, 0);
            }
            otherSame();
        }
        return true;
    };
    long bad = cb;
    auto report = [&](const char* pass, int cases, int w0, int w1) {
        std::printf("%snote-on and update: %ld cases, %ld different\n", pass, nn, nb);
        std::printf("%sinstall: %d cases, %ld routines not as the map\n", pass, cases, wb);
        bad += nb + wb + mb;
        for (int w = w0; w < w1; ++w) {
            std::printf("%s: %ld blocks of 32 frames, %ld different (peak %.4g of 2^31)\n", waveName[w], dn[w], db[w], peak[w] / 2147483648.0f);
            bad += db[w];
            dn[w] = db[w] = 0;
        }
        std::printf("%swrap masks: %ld different\n", pass, mb);
        nn = nb = wb = mb = 0;
    };
    for (int g = 0; g < nCases; ++g)
        if (!voiceCase(g, g % 7)) return 1;
    report("", nCases, 0, 7);
    const int nHq = nCases * 2 / 7;
    for (int g = 0; g < nHq; ++g)
        if (!voiceCase(nCases + g, 7 + g % 2)) return 1;
    report("hq ", nHq, 7, 9);
    // LA-SQUARE and LA-SAW, then with the system's other Tones and Tone KFs
    const int nLa = nCases * 2 / 7;
    for (int g = 0; g < nLa; ++g)
        if (!voiceCase(nCases + nHq + g, 9 + g % 2)) return 1;
    report("la ", nLa, 9, 11);
    const int savedSys[4] = { sys(kLaSawTone), sys(kLaSquTone), sys(kLaSawToneKf), sys(kLaSquToneKf) };
    const int nCal = nLa * 2;
    for (int g = 0; g < nCal; ++g) {
        T.sys[kLaSawTone - kSys0] = (int)(rnd() % 128); T.sys[kLaSquTone - kSys0] = (int)(rnd() % 128);
        T.sys[kLaSawToneKf - kSys0] = (int)(rnd() % 15); T.sys[kLaSquToneKf - kSys0] = (int)(rnd() % 15);
        if (!c.call(0x894e5f80, { 0x89d1b070u, 0, 0, 0 }, sp)) { std::fprintf(stderr, "system: %s\n", c.error().c_str()); return 1; }
        if (!voiceCase(nCases + nHq + nLa + g, 9 + g % 2)) return 1;
    }
    for (int i = 0; i < 4; ++i) T.sys[kLaSawTone - kSys0 + i] = savedSys[i];
    if (!c.call(0x894e5f80, { 0x89d1b070u, 0, 0, 0 }, sp)) { std::fprintf(stderr, "system: %s\n", c.error().c_str()); return 1; }
    report("la tones ", nCal, 9, 11);
    // SUPER-SAW, its converter case first
    cn = cb = 0;
    if (!converterCases(200000, 11)) return 1;
    std::printf("super-saw converter: %ld cases, %ld different\n", cn, cb);
    bad += cb;
    const int nSs = nCases * 2 / 7;
    for (int g = 0; g < nSs; ++g)
        if (!voiceCase(nCases + nHq + nLa + nCal + g, 11)) return 1;
    report("super-saw ", nSs, 11, 12);
    // FEEDBACK-OSC, its converter case first
    cn = cb = 0;
    if (!converterCases(200000, 12)) return 1;
    std::printf("feedback-osc converter: %ld cases, %ld different\n", cn, cb);
    bad += cb;
    for (int g = 0; g < nSs; ++g)
        if (!voiceCase(nCases + nHq + nLa + nCal + nSs + g, 12)) return 1;
    report("feedback-osc ", nSs, 12, 13);
    // The sub oscillator: the converter, then each waveform with it on; the same
    // voice throughout, so what a note leaves on it (+0x208) reaches the next
    cn = cb = 0;
    if (!converterCases(200000, -2)) return 1;
    std::printf("sub converter: %ld cases, %ld different\n", cn, cb);
    bad += cb;
    allFields = true;
    otherAtStart.clear();
    for (uint32_t o : otherWords) otherAtStart.push_back(c.rd32(V + o));
    const int nSub = nCases * 2 / 7, nSubAll = nSub * (kWaves - kSub);
    for (int g = 0; g < nSubAll; ++g)
        if (!voiceCase(nCases + g, kSub + g % (kWaves - kSub))) return 1;
    report("sub ", nSubAll, kSub, kWaves);
    // LA-SQUARE and LA-SAW with the system's other Tones and Tone KFs
    const int nSubCal = nSub * 2;
    for (int g = 0; g < nSubCal; ++g) {
        T.sys[kLaSawTone - kSys0] = (int)(rnd() % 128); T.sys[kLaSquTone - kSys0] = (int)(rnd() % 128);
        T.sys[kLaSawToneKf - kSys0] = (int)(rnd() % 15); T.sys[kLaSquToneKf - kSys0] = (int)(rnd() % 15);
        if (!c.call(0x894e5f80, { 0x89d1b070u, 0, 0, 0 }, sp)) { std::fprintf(stderr, "system: %s\n", c.error().c_str()); return 1; }
        if (!voiceCase(nCases + nSubAll + g, kSub + 9 + g % 2)) return 1;
    }
    for (int i = 0; i < 4; ++i) T.sys[kLaSawTone - kSys0 + i] = savedSys[i];
    if (!c.call(0x894e5f80, { 0x89d1b070u, 0, 0, 0 }, sp)) { std::fprintf(stderr, "system: %s\n", c.error().c_str()); return 1; }
    report("sub la tones ", nSubCal, kSub + 9, kSub + 11);
    // with and without it on the same voice, in random order
    const int nMix = nSub * 4;
    for (int g = 0; g < nMix; ++g) {
        const uint32_t w = rnd() % (uint32_t)kWaves;
        if (!voiceCase(nCases + nSubAll + nSubCal + g, (int)w)) return 1;
    }
    report("mixed ", nMix, 0, kWaves);
    std::printf("sub other words of the voice: %ld different\n", ob);
    bad += ob;

    // OSC 2.  The converter with the input's +2 set, every waveform and the sub
    // oscillator; then both voices in a tone object: the binding (0x894f7070),
    // the voice setup and note-on (0x894f21c0), the install (0x894f2010), the MOD
    // with its Switch off (0x894e0920, 0x894f1b00, 0x894f1d90), the updates
    // (0x894f2090, OSC 1 then 2) and the tone's render (0x894f6d40), with the COSM
    // sections and the TVA stubbed.  Compared every block: both voices' fields,
    // their buffers and masks, the mix, and the shared delay lines.
    cn = cb = 0;
    oscIdx = 1;
    if (!converterCases(200000, -1)) return 1;
    if (!converterCases(100000, -2)) return 1;
    if (!converterCases(100000, 11)) return 1;
    if (!converterCases(100000, 12)) return 1;
    oscIdx = 0;
    std::printf("osc 2 converter: %ld cases, %ld different\n", cn, cb);
    bad += cb;
    const uint32_t TN = ram + 0x40000, V1 = TN + 0x20, V2 = TN + 0x9d4, MODO = TN + 0x1708, BL1 = ram + 0x3f000, BL2 = ram + 0x3f100,
                   MI = ram + 0x3f200, MB = ram + 0x3f300, STUB = ram + 0x3f400;
    constexpr uint32_t kBuf1 = 0x89d2c810, kBuf2 = 0x89d2c910, kBufMix = 0x89d2ca10;
    auto envAt = [&](uint32_t e) { return TN + 0x1780 + 0x70 * e; };
    auto lfoAt = [&](uint32_t l) { return TN + 0x1da0 + 0x98 * l; };
    uint32_t mask1 = 0, mask2 = 0;
    c.hook(0x894f5570, [](Sh4& m) { m.ret(0); });                 // the TVA
    c.hook(STUB, [](Sh4& m) { m.ret(0); });                       // the COSM sections
    c.hook(0x894f6d78, [&](Sh4& m) { mask2 = m.r[0]; m.r[6] = m.r[0]; m.pc += 2; });       // mov r0,r6: OSC 2's mask
    c.hook(0x894f6da8, [&](Sh4& m) { mask1 = m.r[0]; m.r[9] += 8; m.pc += 2; });           // add #8,r9: after OSC 1
    XtAnalogOsc to[2];
    const uint32_t tv[2] = { V1, V2 }, tb[2] = { BL1, BL2 };
    struct Field { uint32_t off; float v; };
    auto floats = [](const XtAnalogOsc& o) {
        std::vector<Field> f = { { 0x144, o.ratio() }, { 0x154, o.gainTarget() }, { 0x158, o.gain() }, { 0x1c0, o.fatTarget() }, { 0x1c4, o.fat() }, { 0x1c8, o.pwTarget() },
            { 0x1cc, o.pw() }, { 0x1d0, o.decay() }, { 0x1d4, o.phase() }, { 0x1d8, o.impact() }, { 0x1e0, o.edge() }, { 0x1e4, o.hqLevel() }, { 0x1ec, o.subPwTarget() },
            { 0x1f0, o.subPw() }, { 0x1f4, o.laStep() }, { 0x1f8, o.subStep() }, { 0x1fc, o.subPhase() }, { 0x200, o.subLevelTarget() }, { 0x204, o.subLevel() },
            { 0x208, o.rampEnd() }, { 0x210, o.rampEnd2() }, { 0x218, o.laPhase2() }, { 0x21c, o.laPhase3() }, { 0x224, o.laSteepTarget() }, { 0x228, o.laSteep() },
            { 0x22c, o.laSteep2Target() }, { 0x230, o.laSteep2() }, { 0x234, o.steepTarget() }, { 0x238, o.steep() }, { 0x23c, o.steep2Target() }, { 0x240, o.steep2() },
            { 0x24c, o.detuneTarget() }, { 0x250, o.detune() }, { 0x254, o.mixTarget() }, { 0x258, o.mix() }, { 0x274, o.fbState() } };
        for (int i = 0; i < 6; ++i) f.push_back({ 0x25c + 4 * (uint32_t)i, o.sidePhase(i) });
        return f;
    };
    // a fresh tone: the firmware's voices hold what the port's start with
    c.write(TN, std::vector<uint8_t>(0x2400, 0).data(), 0x2400);
    for (int n = 0; n < 2; ++n) {
        for (const Field& f : floats(to[n])) wf(tv[n] + f.off, f.v);
        c.wr8(tv[n] + 0x248, (uint8_t)to[n].noiseHold());
    }
    c.wr32(TN + 0x14d0, STUB); c.wr32(TN + 0x1634, STUB);
    long tb_ = 0, tn_ = 0, tm_ = 0, tl_ = 0;
    long waveBlocks[14] = {};
    auto fieldsSame = [&](int n) {
        const uint32_t Vn = tv[n];
        const XtAnalogOsc& o = to[n];
        bool ok = c.rd32(Vn + 0x1dc) == o.flags() && c.rd32(Vn + 0x244) == o.noiseState() && (int)(int8_t)c.rd8(Vn + 0x248) == o.noiseHold()
               && (int)c.rd32(Vn + 0x1e8) == o.hqFalling() && (int)c.rd32(Vn + 0x278) == o.fbWrite();
        for (const Field& f : floats(o)) if (!same(f32(Vn + f.off), f.v)) {
            if (ok && tb_ < 8) std::printf("  osc %d +0x%x: fw %.9g port %.9g\n", n + 1, f.off, f32(Vn + f.off), f.v);
            ok = false;
        }
        return ok;
    };
    auto toneLines = [&] {                       // the eighteen flags, and each FEEDBACK voice's line
        for (int i = 0; i < XtAnalogShared::kLines; ++i)
            if (c.rd8(kFbFlags + (uint32_t)i) != shared.lineUsed[i]) return false;
        for (const XtAnalogOsc& o : to) {
            const int sl = o.fbSlotIndex();
            if (sl < 0) continue;
            const uint32_t base = kFbLines + (uint32_t)XtAnalogShared::kLineLen * (uint32_t)sl;
            for (int i = 0; i < XtAnalogShared::kLineLen; ++i)
                if ((int8_t)c.rd8(base + (uint32_t)i) != shared.line[sl][i]) return false;
        }
        return true;
    };
    bool modForceC = true;                       // not under FM: its c runs away, see the FM pass
    auto modInput = [&](const XtModParams& p) {
        c.write(MI, z.data(), 0x10);
        c.wr8(MI + 6, p.on ? 1 : 0); c.wr8(MI + 7, (uint8_t)p.type);
        c.wr8(MI + 8, (uint8_t)p.level); c.wr8(MI + 9, (uint8_t)p.attack); c.wr8(MI + 0xa, (uint8_t)p.release);
    };
    auto modParams = [&](int type) {
        XtModParams p;
        p.on = rnd() % 8 != 0;
        p.type = type;
        p.level = rnd() % 4 == 0 ? (rnd() % 2 ? 127 : 0) : (int)(rnd() % 128);
        p.attack = (int)(rnd() % 128); p.release = (int)(rnd() % 128);
        return p;
    };
    auto modBlockSame = [&](uint32_t B, const XtModBlock& b) {
        return c.rd32(B) == b.type && same(f32(B + 4), b.a) && same(f32(B + 8), b.b) && same(f32(B + 0xc), b.c);
    };
    // the input, into the firmware's block and the port's, each holding what it held
    auto modConvert = [&](const XtModParams& p, uint32_t B, XtModBlock& b) {
        modInput(p);
        if (rnd() % 8 == 0) {                    // what the converter may leave there: anything
            b.b = frnd(-1, 2);
            wf(B + 8, b.b);
            if (modForceC) { b.c = frnd(-1, 2); wf(B + 0xc, b.c); }
        }
        xtModBlock(p, b);
        return c.call(0x894e0920, { MI, B }, sp);
    };
    auto modSame = [&](uint32_t O, const XtMod& m) {
        bool ok = c.rd32(O) == m.type() && c.rd32(O + 0x38) == m.stereoCode() && (uint32_t)c.rd8(O + 0x34) == (m.type() & 0xff);
        for (int i = 0; i < 3; ++i) ok = ok && same(f32(O + 4 + 4 * (uint32_t)i), m.target(i)) && same(f32(O + 0x18 + 4 * (uint32_t)i), m.current(i));
        return ok && same(f32(O + 0x28), m.env()) && same(f32(O + 0x2c), m.lastLevel()) && (c.rd32(O + 0x40) != 0) == m.syncOn() && c.rd32(O + 0x44) == m.mask();
    };
    int modType = -1;                            // the tone passes with the MOD on: its Type
    long modL2Blocks = 0, modLaSaw = 0, modSyncBlocks = 0;
    float modL2Max = 0;
    XtMod toneMod;
    XtModBlock toneB;
    XtModShared* modPool = new XtModShared;      // and FM's lines, in step with the firmware's (0 at startup)
    constexpr uint32_t kFmL = 0x89f91c5c, kFmR = 0x89f9d65c;
    auto modLinesSame = [&](int tone) {          // the tone's two lines
        const uint32_t L = kFmL + 1984 * (uint32_t)tone, R = kFmR + 1984 * (uint32_t)tone;
        const float* a = modPool->line + tone * XtModShared::kLine;
        const float* b = modPool->line + (XtModShared::kTones + tone) * XtModShared::kLine;
        for (uint32_t i = 0; i < XtModShared::kLine; ++i)
            if (!same(f32(L + 4 * i), a[i]) || !same(f32(R + 4 * i), b[i])) return false;
        return true;
    };
    bool xmodPass = false;
    auto toneCase = [&](int g) {
        Case k[2] = { newCase(false), newCase(false) };
        bool on[2] = { true, true };
        if (g % 8 == 5) on[0] = false;           // OSC 1 off, OSC 2 alone
        if (g % 8 == 6) on[1] = false;           // OSC 2 off
        for (int n = 0; n < 2; ++n) {
            k[n].p.wave = (int)(rnd() % 13);
            if (xmodPass) k[n].p.wave = n == 0 || g % 4 == 2 ? 13 : (int)(rnd() % 14);
            if (modType >= 0) k[n].p.wave = (int)(rnd() % 14);   // X-MOD would clear OSC 2's output before the MOD reads it
            if (k[n].p.wave >= 11 || rnd() % 3 == 0) k[n].p.detune = (int)(rnd() % 127) - 63;
            if (k[n].p.wave <= 10 && rnd() % 3 == 0) { k[n].p.subOctave = 1 + (int)(rnd() % 3); k[n].p.subLevel = (int)(rnd() % 128); }
        }
        if (g % 8 == 3) k[0].p.wave = k[1].p.wave = 12;          // both on FEEDBACK-OSC: two lines from one voice
        if (g % 8 == 4) k[0].p.wave = k[1].p.wave = 11;          // both on SUPER-SAW: the counter, OSC 1 then 2
        if (g % 16 == 3) {                                       // the eighteen lines all taken: both voices share line 0
            for (int i = 0; i < XtAnalogShared::kLines; ++i) { c.wr8(kFbFlags + (uint32_t)i, 1); shared.lineUsed[i] = 1; }
        }
        float eOut[9], lOut[3];
        bool atk[2];
        auto drawOutputs = [&] {
            for (uint32_t e = 1; e <= 8; ++e) { eOut[e] = frnd(-1, 1); wf(envAt(e) + 0x4c, eOut[e]); }
            for (uint32_t l = 1; l <= 2; ++l) { lOut[l] = frnd(-1, 1); wf(lfoAt(l) + 0x38, lOut[l]); }
        };
        drawOutputs();
        mtx = 0;
        // the blocks, then the binding and note-ons as the tone's note-on runs them
        for (int n = 0; n < 2; ++n) {
            oscIdx = n;
            writeInput(k[n]);
            // the depths the note's pitch and level follow: the OSC's own envelope and LFO
            c.wr8(P + 0xc, (uint8_t)(int8_t)((int)(rnd() % 127) - 63)); c.wr8(P + 0x10, (uint8_t)(int8_t)((int)(rnd() % 127) - 63));
            c.wr8(P + 0x11, (uint8_t)(int8_t)((int)(rnd() % 127) - 63)); c.wr8(P + 0x20, (uint8_t)(int8_t)((int)(rnd() % 127) - 63));
            c.write(tb[n], z.data(), 0xd4);
            c.wr32(tb[n], on[n] ? 3u : 0u);
            if (on[n] && !c.call(0x894e1920, { P, tb[n] }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return false; }
            if (!c.call(0x894f7070, { TN, tb[n], (uint32_t)n }, sp)) { std::fprintf(stderr, "binding: %s\n", c.error().c_str()); return false; }
        }
        oscIdx = 0;
        const uint32_t ampEnv[2] = { 4, 8 };
        for (int n = 0; n < 2; ++n) {
            atk[n] = rnd() & 1;
            wf(envAt(ampEnv[n]) + 0x1c, atk[n] ? 25.0f : 0.0f); wf(envAt(ampEnv[n]) + 0x2c, 0.0f);
        }
        auto ratioOf = [&](int n) {
            const uint32_t Vn = tv[n], e = (uint32_t)n * 4;
            return xtPitchTarget(f32(Vn + 8), eOut[e + 1], f32(Vn + 0x1c), lOut[n + 1], f32(Vn + 0x34), f32(Vn + 0xc));
        };
        auto gainOf = [&](int n, bool start) {
            const uint32_t Vn = tv[n], e = (uint32_t)n * 4;
            return xtOscGainTarget(f32(Vn + 0x5c), f32(Vn + 0x54), start && atk[n] ? 0.0f : eOut[e + 4], lOut[n + 1], f32(Vn + 0x58));
        };
        XtAnalogBlock pb[2];
        // the OSC SYNC pass: the block before the voices, as the tone's note-on's rule reads it.  Now
        // and then a note of another Type after an OSC SYNC note: the flag stays set
        const int mt = modType == 4 && g % 4 == 3 ? (int)(rnd() % 2) : modType;
        if (modType == 4 && !modConvert(modParams(mt), MB, toneB)) { std::fprintf(stderr, "MOD: %s\n", c.error().c_str()); return false; }
        for (int n = 0; n < 2; ++n) {
            oscIdx = n;
            pb[n] = portBlock(k[n]);
            oscIdx = 0;
            if (modType == 4 && n == 1 && c.rd32(MB) == 4 && c.rd32(tb[1]) == 3 && c.rd32(tb[1] + 0x80) == 10) {   // 0x894f6148: LA-SAW to SAW
                c.wr32(tb[1] + 0x80, 2);
                pb[1].code = 2;
                ++modLaSaw;
            }
            if (!c.call(0x894f21c0, { tv[n], tb[n], 0 }, sp)) { std::fprintf(stderr, "voice setup: %s\n", c.error().c_str()); return false; }
            if (!on[n]) { to[n].setRatio(1.0f); continue; }      // the note-on of an OSC that is off (0x894f2f80, kind 0)
            const uint32_t e = (uint32_t)n * 4;
            to[n].noteOn(pb[n], ratioOf(n), gainOf(n, false), gainOf(n, true), eOut[e + 3], eOut[e + 2], lOut[n + 1], shared);
            ++tn_;
            if (!fieldsSame(n) && tb_++ < 8) std::printf("  tone %d note-on osc %d wave %d\n", g, n + 1, k[n].p.wave);
        }
        if (!toneLines() && tl_++ < 4) std::printf("  tone %d note-on: lines differ\n", g);
        if (modType < 0) {
        c.write(MI, z.data(), 0x10);
        if (!c.call(0x894e0920, { MI, MB }, sp) || c.rd32(MB) != 0 || !same(f32(MB + 4), 1.0f) || !same(f32(MB + 8), 1.0f)) {
            std::fprintf(stderr, "MOD off: type %u, gains %.9g %.9g\n", c.rd32(MB), f32(MB + 4), f32(MB + 8));
            return false;
        }
        for (int n = 0; n < 2; ++n)
            if (!c.call(0x894f2010, { tv[n] }, sp)) { std::fprintf(stderr, "install: %s\n", c.error().c_str()); return false; }
        if (!c.call(0x894f1b00, { MODO, MB, c.rd32(V1 + 0x960), c.rd32(V2 + 0x960) }, sp)
            || !c.call(0x894f1d90, { MODO, c.rd32(V1) == 3 ? 1u : 0u }, sp)) { std::fprintf(stderr, "MOD: %s\n", c.error().c_str()); return false; }
        if (c.rd32(MODO + 0x50) != 0x89527d80) { std::fprintf(stderr, "MOD: routine %08x\n", c.rd32(MODO + 0x50)); return false; }
        } else {                                 // as the tone's note-on: the MOD before the installs, the chooser after
            if ((modType != 4 && !modConvert(modParams(modType), MB, toneB))
                || !c.call(0x894f1b00, { MODO, MB, c.rd32(V1 + 0x960), c.rd32(V2 + 0x960) }, sp)) { std::fprintf(stderr, "MOD: %s\n", c.error().c_str()); return false; }
            toneMod.noteOn(toneB, false, false, on[0], *modPool, 0);
            for (int n = 0; n < 2; ++n)
                if (!c.call(0x894f2010, { tv[n] }, sp)) { std::fprintf(stderr, "install: %s\n", c.error().c_str()); return false; }
            if (!c.call(0x894f1d90, { MODO, c.rd32(V1) == 3 ? 1u : 0u }, sp)) { std::fprintf(stderr, "MOD: %s\n", c.error().c_str()); return false; }
            ++tn_;
            if ((!modSame(MODO, toneMod) || c.rd32(MODO + 0x50) != toneMod.routine()) && tb_++ < 8)
                std::printf("  tone %d mod note-on type %u/%u stereo %u/%u routine %08x/%08x\n", g, c.rd32(MODO), toneMod.type(), c.rd32(MODO + 0x38), toneMod.stereoCode(), c.rd32(MODO + 0x50), toneMod.routine());
        }
        for (int blk = 0; blk < 24; ++blk) {
            if (modType >= 0 && blk % 7 == 3) {   // a new block on the held note
                if (!modConvert(modParams((int)(rnd() % 5)), MB, toneB) || !c.call(0x894f1c60, { MODO, MB }, sp)) { std::fprintf(stderr, "MOD update: %s\n", c.error().c_str()); return false; }
                toneMod.setBlock(toneB);
                ++tn_;
                if (!modSame(MODO, toneMod) && tb_++ < 8) std::printf("  tone %d.%d mod update\n", g, blk);
            }
            if (blk % 5 == 4) {
                drawOutputs();
                for (int n = 0; n < 2; ++n) {
                    if (!c.call(0x894f2090, { tv[n], 1 }, sp)) { std::fprintf(stderr, "update: %s\n", c.error().c_str()); return false; }
                    if (!on[n]) continue;
                    const uint32_t e = (uint32_t)n * 4;
                    to[n].update(pb[n], ratioOf(n), gainOf(n, false), eOut[e + 3], eOut[e + 2], lOut[n + 1]);
                    ++tn_;
                    if (!fieldsSame(n) && tb_++ < 8) std::printf("  tone %d.%d update osc %d wave %d\n", g, blk, n + 1, k[n].p.wave);
                }
            }
            if (blk % 6 == 2)                    // each phase exactly on a breakpoint, or on 0
                for (int n = 0; n < 2; ++n) {
                    if (!on[n] || k[n].p.wave == 8 || k[n].p.wave == 6 || k[n].p.wave == 7) continue;
                    if (k[n].p.wave == 13 && rnd() % 2) {   // the sample that lands on 1, with OSC 2's output small enough
                        float p = sub(1.0f, mul(to[n].ratio(), K(0x3ba377ed)));
                        wf(tv[n] + 0x1d4, p); to[n].setPhase(p);
                        continue;
                    }
                    float pts[9];
                    const int m = to[n].breakpoints(pts);
                    pts[m] = 0.0f;
                    const float p = pts[rnd() % (uint32_t)(m + 1)];
                    if (p >= 0.0f && 1.0f > p) { wf(tv[n] + 0x1d4, p); to[n].setPhase(p); }
                }
            mask1 = mask2 = 0xdeadbeef;
            if (!c.call(0x894f6d40, { TN, BUF }, sp)) { std::fprintf(stderr, "render: %s\n", c.error().c_str()); return false; }
            float l[2][64] = {};
            uint32_t pm[2] = {};
            if (on[1]) pm[1] = to[1].process(l[1]);
            uint32_t syncIn = 0;                 // the render: OSC 2's mask, only while the MOD's flag is set
            if (modType >= 0 && toneMod.syncOn()) { syncIn = pm[1]; toneMod.setMask(pm[1]); if (syncIn) ++modSyncBlocks; }
            if (on[0]) pm[0] = to[0].process(l[0], syncIn, l[1]);
            float mix[64];
            std::copy(l[0], l[0] + 64, mix);
            if (modType >= 0) {                  // what the MOD is given: the check is no use on zeros
                float pk = 0;
                for (int i = 0; i < 64; ++i) pk = std::fmax(pk, std::fabs(l[1][i]));
                if (pk > 0) ++modL2Blocks;
                modL2Max = std::fmax(modL2Max, pk);
            }
            if (modType >= 0) toneMod.process(mix, mix, l[1]);
            else xtAnalogMix(mix, l[1]);
            for (int n = 0; n < 2; ++n) if (on[n]) ++waveBlocks[k[n].p.wave];
            if ((mask1 != pm[0] || mask2 != pm[1]) && tm_++ < 4) std::printf("  tone %d.%d masks: fw %08x %08x, port %08x %08x\n", g, blk, mask1, mask2, pm[0], pm[1]);
            bool ok = fieldsSame(0) && fieldsSame(1);
            int bad_i = -1;
            for (int i = 0; i < 64 && bad_i < 0; ++i)
                if (!same(f32(kBuf1 + 4 * (uint32_t)i), l[0][i]) || !same(f32(kBuf2 + 4 * (uint32_t)i), l[1][i]) || !same(f32(kBufMix + 4 * (uint32_t)i), mix[i])) bad_i = i;
            ok = ok && bad_i < 0 && (modType < 0 || (modSame(MODO, toneMod) && (int)c.rd32(MODO + 0x30) == toneMod.writeIndex() && (modType != 2 || modLinesSame(0))));
            if (!ok && tb_++ < 8) {
                std::printf("  tone %d.%d waves %d %d on %d %d: first difference at %d", g, blk, k[0].p.wave, k[1].p.wave, on[0], on[1], bad_i);
                if (bad_i >= 0)
                    std::printf(": osc1 %.9g/%.9g osc2 %.9g/%.9g mix %.9g/%.9g", f32(kBuf1 + 4 * (uint32_t)bad_i), l[0][bad_i], f32(kBuf2 + 4 * (uint32_t)bad_i), l[1][bad_i],
                                f32(kBufMix + 4 * (uint32_t)bad_i), mix[bad_i]);
                std::printf("\n");
            }
            if (blk % 8 == 7 && !toneLines() && tl_++ < 4) std::printf("  tone %d.%d: lines differ\n", g, blk);
        }
        // the tone's stop, OSC 1 then 2 (0x894f1fd0): their lines free
        for (int n = 0; n < 2; ++n) {
            if (!c.call(0x894f1fd0, { tv[n] }, sp)) { std::fprintf(stderr, "stop: %s\n", c.error().c_str()); return false; }
            if (on[n]) to[n].release();
        }
        if (!toneLines() && tl_++ < 4) std::printf("  tone %d stop: lines differ\n", g);
        if (g % 16 == 3)                         // the lines given back
            for (int i = 0; i < XtAnalogShared::kLines; ++i) { c.wr8(kFbFlags + (uint32_t)i, 0); shared.lineUsed[i] = 0; }
        return true;
    };
    const int nTone = nCases;
    for (int g = 0; g < nTone; ++g)
        if (!toneCase(g)) return 1;
    // The order of the note-ons, from the tone's own (0x894f5e60): the whole block
    // but the OSCs' and the MOD's left at zero, the COSM sections, the TVA and the
    // LFOs' note-ons stubbed.  SUPER-SAW's counter and FEEDBACK-OSC's lines go OSC 1
    // then OSC 2.
    const uint32_t TB = ram + 0x3a000;
    for (uint32_t a : { 0x894ee480u, 0x894edf70u, 0x894eef70u, 0x894f5160u, 0x894f50e0u, 0x894f7330u, 0x894f0cf0u })
        c.hook(a, [](Sh4& m) { m.ret(m.r[5]); });
    long oo = 0, on_ = 0;
    auto orderCase = [&](int g) {
        static const int pairs[6][2] = { { 11, 11 }, { 12, 12 }, { 11, 12 }, { 12, 11 }, { 0, 11 }, { 12, 0 } };
        Case k[2] = { newCase(false), newCase(false) };
        for (int n = 0; n < 2; ++n) { k[n].p.wave = pairs[g % 6][n]; k[n].p.detune = (int)(rnd() % 127) - 63; }
        bool on[2] = { true, true };
        if (modType >= 0) {                      // any waveform, an OSC sometimes off
            for (int n = 0; n < 2; ++n) { k[n].p.wave = (int)(rnd() % 14); on[n] = rnd() % 8 != 0; }
        }
        if (g % 12 == 7)                         // the eighteen lines all taken
            for (int i = 0; i < XtAnalogShared::kLines; ++i) { c.wr8(kFbFlags + (uint32_t)i, 1); shared.lineUsed[i] = 1; }
        c.write(TB, std::vector<uint8_t>(0x800, 0).data(), 0x800);
        c.wr32(TB + 4, 0xffffffffu);             // no wave
        for (uint32_t i = 1; i < 8; ++i) c.wr32(TB + 0x24 + (i << 5) + 0x10, 1);   // every LFO key-synced
        mtx = 0;
        const uint32_t oscBlock[2] = { TB + 0x434, TB + 0x508 };
        for (int n = 0; n < 2; ++n) {
            oscIdx = n;
            writeInput(k[n]);
            c.wr32(oscBlock[n], on[n] ? 3u : 0u);
            if (on[n] && !c.call(0x894e1920, { P, oscBlock[n] }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return false; }
        }
        if (modType >= 0) {
            toneB = XtModBlock();
            if (!modConvert(modParams(modType), TB + 0x5dc, toneB)) { std::fprintf(stderr, "MOD: %s\n", c.error().c_str()); return false; }
            // LA-SAW on OSC 2 under OSC SYNC, with a band of cases, and on OSC 1 too, where the rule does not reach
            if (modType == 4 && g % 3 == 0) {
                for (int n = 0; n < 2; ++n) {
                    k[n].p.wave = 9;
                    oscIdx = n;
                    writeInput(k[n]);
                    c.wr32(oscBlock[n], on[n] ? 3u : 0u);
                    if (on[n] && !c.call(0x894e1920, { P, oscBlock[n] }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return false; }
                }
                oscIdx = 0;
            }
        } else {
        c.write(MI, z.data(), 0x10);
        if (!c.call(0x894e0920, { MI, TB + 0x5dc }, sp)) { std::fprintf(stderr, "MOD off: %s\n", c.error().c_str()); return false; }
        }
        c.wr32(0x89f91c50, 0); c.wr32(0x89f91c54, 0); c.wr32(0x89fa9094, 0);       // the lists of sounding tones
        if (!c.call(0x894f5e60, { TN, TB }, sp)) { std::fprintf(stderr, "tone note-on: %s\n", c.error().c_str()); return false; }
        ++on_;
        const int order[2] = { 0, 1 };
        for (int n : order) {
            oscIdx = n;
            XtAnalogBlock pb = portBlock(k[n]);
            oscIdx = 0;
            if (modType == 4 && n == 1 && on[1] && toneB.type == 4 && pb.code == 10) { pb.code = 2; ++modLaSaw; }
            if (!on[n]) { to[n].setRatio(1.0f); continue; }
            const uint32_t Vn = tv[n], e = (uint32_t)n * 4, lf = lfoAt((uint32_t)n + 1) + 0x38;
            const float eFat = f32(envAt(e + 3) + 0x4c), ePw = f32(envAt(e + 2) + 0x4c), ePitch = f32(envAt(e + 1) + 0x4c), eAmp = f32(envAt(e + 4) + 0x4c), l = f32(lf);
            const float ratio = xtPitchTarget(f32(Vn + 8), ePitch, f32(Vn + 0x1c), l, f32(Vn + 0x34), f32(Vn + 0xc));
            // the amp envelope's times are 0 here: the start is its output
            const float gain = xtOscGainTarget(f32(Vn + 0x5c), f32(Vn + 0x54), eAmp, l, f32(Vn + 0x58));
            to[n].noteOn(pb, ratio, gain, gain, eFat, ePw, l, shared);
        }
        if (modType >= 0) toneMod.noteOn(toneB, false, false, on[0], *modPool, 0);
        const bool modOk = modType < 0 || (modSame(MODO, toneMod) && c.rd32(MODO + 0x50) == toneMod.routine() && (modType != 2 || modLinesSame(0)));
        const bool ok = fieldsSame(0) && fieldsSame(1) && toneLines() && same(f32(kSsCount), shared.superSaw) && modOk;
        if (!ok && oo++ < 6) std::printf("  order %d waves %d %d: lines %d counter %.9g/%.9g mod %d type %u/%u stereo %u/%u routine %08x/%08x\n", g, k[0].p.wave, k[1].p.wave, toneLines(), f32(kSsCount), shared.superSaw, modOk, c.rd32(MODO), toneMod.type(), c.rd32(MODO + 0x38), toneMod.stereoCode(), c.rd32(MODO + 0x50), toneMod.routine());
        for (int n = 0; n < 2; ++n) {
            if (!c.call(0x894f1fd0, { tv[n] }, sp)) { std::fprintf(stderr, "stop: %s\n", c.error().c_str()); return false; }
            if (on[n]) to[n].release();
        }
        if (g % 12 == 7)
            for (int i = 0; i < XtAnalogShared::kLines; ++i) { c.wr8(kFbFlags + (uint32_t)i, 0); shared.lineUsed[i] = 0; }
        if (modType >= 0) { c.wr32(0x89f91c50, 0); c.wr32(0x89f91c54, 0); c.wr32(0x89fa9094, 0); }   // the lists sit just outside FM's lines
        return true;
    };
    for (int g = 0; g < nCases / 2; ++g)
        if (!orderCase(g)) return 1;
    std::printf("tone note-on: %ld cases, %ld different\n", on_, oo);
    bad += oo;
    std::printf("tone: %d cases, %ld note-ons and updates\n", nTone, tn_);
    {
        long blocks = 0;
        for (long w : waveBlocks) blocks += w;
        std::printf("tone: %ld oscillator blocks, %ld different; wrap masks %ld different; delay lines %ld different\n", blocks, tb_, tm_, tl_);
    }
    bad += tb_ + tm_ + tl_;

    // X-MOD.  The converter's case on each OSC; then the voice alone, the routine
    // called with a buffer the check fills, so that the phase lands on each
    // turn, on 1, on 0 and below it; then both voices in the tone, X-MOD on OSC 1
    // reading OSC 2's output through the firmware's own render, and on OSC 2.
    cn = cb = 0;
    if (!converterCases(200000, 13)) return 1;
    std::printf("x-mod converter: %ld cases, %ld different\n", cn, cb);
    bad += cb;
    cn = cb = 0;
    oscIdx = 1;
    if (!converterCases(100000, 13)) return 1;
    oscIdx = 0;
    std::printf("x-mod converter, osc 2: %ld cases, %ld different\n", cn, cb);
    bad += cb;
    const uint32_t MODB = ram + 0x3e000;
    long xb = 0, xn = 0, xd = 0, xm = 0;
    float xPeak = 0;
    auto xmCase = [&](int g) {
        const int n = g % 4 == 3 ? 1 : 0;
        const uint32_t Vn = tv[n], Bn = tb[n], e = (uint32_t)n * 4;
        XtAnalogOsc& o = to[n];
        Case k = newCase(false);
        k.p.wave = 13;
        k.p.detune = (int)(rnd() % 127) - 63;
        k.p.subOctave = (int)(rnd() % 4);                 // it has no sub oscillator
        if (g % 8 == 1) { k.p.fat = 0; k.p.fatKf = k.p.fatEnv = k.p.fatLfo = 0; }
        if (g % 8 == 2) { k.p.fat = 127; k.p.fatKf = k.p.fatEnv = k.p.fatLfo = 0; }
        if (g % 8 == 4) { k.p.pw = 63; k.p.pwKf = k.p.pwEnv = k.p.pwLfo = 0; }      // the deepest
        if (g % 8 == 5) { k.p.pw = -63; k.p.pwKf = k.p.pwEnv = k.p.pwLfo = 0; }     // none, the index below 2
        float eOut[9], lOut[3];
        auto drawOutputs = [&] {
            for (uint32_t i = 1; i <= 8; ++i) { eOut[i] = frnd(-1, 1); wf(envAt(i) + 0x4c, eOut[i]); }
            for (uint32_t l = 1; l <= 2; ++l) { lOut[l] = frnd(-1, 1); wf(lfoAt(l) + 0x38, lOut[l]); }
        };
        drawOutputs();
        mtx = 0;
        oscIdx = n;
        writeInput(k);
        const XtAnalogBlock b = portBlock(k);
        oscIdx = 0;
        c.write(Bn, z.data(), 0xd4);
        c.wr32(Bn, 3u);
        if (!c.call(0x894e1920, { P, Bn }, sp) || !c.call(0x894f7070, { TN, Bn, (uint32_t)n }, sp)) { std::fprintf(stderr, "x-mod: %s\n", c.error().c_str()); return false; }
        const bool atk = rnd() & 1;
        wf(envAt(e + 4) + 0x1c, atk ? 25.0f : 0.0f); wf(envAt(e + 4) + 0x2c, 0.0f);
        if (!c.call(0x894f21c0, { Vn, Bn, 0 }, sp)) { std::fprintf(stderr, "x-mod setup: %s\n", c.error().c_str()); return false; }
        auto ratioOf = [&] { return xtPitchTarget(f32(Vn + 8), eOut[e + 1], f32(Vn + 0x1c), lOut[n + 1], f32(Vn + 0x34), f32(Vn + 0xc)); };
        auto gainOf = [&](bool start) { return xtOscGainTarget(f32(Vn + 0x5c), f32(Vn + 0x54), start && atk ? 0.0f : eOut[e + 4], lOut[n + 1], f32(Vn + 0x58)); };
        o.noteOn(b, ratioOf(), gainOf(false), gainOf(true), eOut[e + 3], eOut[e + 2], lOut[n + 1], shared);
        ++xn;
        if (!fieldsSame(n) && xb++ < 6) std::printf("  x-mod %d note-on\n", g);
        if (!c.call(0x894f2010, { Vn }, sp)) { std::fprintf(stderr, "x-mod install: %s\n", c.error().c_str()); return false; }
        if (c.rd32(Vn + 0x96c) != 0x89507b60) { std::fprintf(stderr, "x-mod install: %08x\n", c.rd32(Vn + 0x96c)); return false; }
        for (int blk = 0; blk < 30; ++blk) {
            if (blk % 5 == 4) {
                drawOutputs();
                if (!c.call(0x894f2090, { Vn, 1 }, sp)) { std::fprintf(stderr, "x-mod update: %s\n", c.error().c_str()); return false; }
                o.update(b, ratioOf(), gainOf(false), eOut[e + 3], eOut[e + 2], lOut[n + 1]);
                ++xn;
                if (!fieldsSame(n) && xb++ < 6) std::printf("  x-mod %d.%d update\n", g, blk);
            }
            // the other OSC's output: none, a small tone, full level, far past it, or
            // one channel only
            float mb2[64];
            const int kind = (int)(rnd() % 6);
            const float amp = kind == 1 ? 3.0e7f : kind == 2 ? 2.0e9f : kind == 3 ? 2.0e10f : 0.0f;
            for (int i = 0; i < 64; ++i) mb2[i] = amp * frnd(-1, 1);
            if (kind == 4) for (int i = 0; i < 32; ++i) { mb2[2 * i] = 2.0e9f * frnd(-1, 1); mb2[2 * i + 1] = 0.0f; }
            if (blk % 3 == 1) {                           // the phase on a turn, or a step from 1, or on 0
                float pts[9];
                const int m = o.breakpoints(pts);
                pts[m] = 0.0f;
                pts[m + 1] = sub(1.0f, mul(o.ratio(), K(0x3ba377ed)));
                const float p = pts[rnd() % (uint32_t)(m + 2)];
                wf(Vn + 0x1d4, p); o.setPhase(p);
            }
            if (blk % 7 == 5) {                           // a Pulse Width no note reaches: its move back, and a·d == a, the third copy of the loop
                wf(Vn + 0x1cc, -0.5f); o.setPw(-0.5f);
            }
            if (kind == 5) {                              // the first sample's step landing exactly on 0
                const float p = -mul(o.ratio(), K(0x3ba377ed));
                wf(Vn + 0x1d4, p); o.setPhase(p);
                mb2[0] = mb2[1] = 0.0f;
            }
            c.write(MODB, mb2, sizeof mb2);
            const uint32_t sync = kind != 5 && rnd() % 8 == 0 ? ((uint32_t)rnd() << 16 | rnd()) & ((uint32_t)rnd() << 16 | rnd()) : 0u;
            uint32_t mask = 0;
            if (!c.call(c.rd32(Vn + 0x96c), { BUF, Vn + 0xd4, sync, MODB }, sp, &mask)) { std::fprintf(stderr, "x-mod: %s\n", c.error().c_str()); return false; }
            float lr[64];
            const uint32_t pm = o.process(lr, sync, mb2);
            ++xd;
            if (mask != pm && xm++ < 4) std::printf("  x-mod %d.%d: mask %08x, port %08x\n", g, blk, mask, pm);
            bool ok = fieldsSame(n);
            for (int i = 0; i < 64; ++i) {
                ok = ok && same(f32(BUF + 4 * (uint32_t)i), lr[i]) && c.rd32(MODB + 4 * (uint32_t)i) == 0 && mb2[i] == 0.0f;
                if (blk % 7 != 5) xPeak = std::fmax(xPeak, std::fabs(lr[i]));   // not with the forced Pulse Width: its slope is infinite
            }
            if (!ok && xb++ < 6) {
                int i = 0;
                while (i < 64 && same(f32(BUF + 4 * (uint32_t)i), lr[i])) ++i;
                std::printf("  x-mod %d.%d: first difference at %d: fw %.9g port %.9g; phase %.9g/%.9g\n", g, blk, i, i < 64 ? f32(BUF + 4 * (uint32_t)i) : 0.0f, i < 64 ? lr[i] : 0.0f,
                            f32(Vn + 0x1d4), o.phase());
            }
        }
        return true;
    };
    for (int g = 0; g < nCases; ++g)
        if (!xmCase(g)) return 1;
    std::printf("x-mod: %ld note-ons and updates, %ld blocks of 32 frames, %ld different (peak %.4g of 2^31); masks %ld different\n", xn, xd, xb, xPeak / 2147483648.0f, xm);
    bad += xb + xm;
    tb_ = tm_ = tl_ = tn_ = 0;
    for (long& w : waveBlocks) w = 0;
    xmodPass = true;
    for (int g = 0; g < nCases; ++g)
        if (!toneCase(g)) return 1;
    {
        long blocks = 0;
        for (long w : waveBlocks) blocks += w;
        std::printf("x-mod tone: %d cases, %ld oscillator blocks (%ld of X-MOD), %ld different; wrap masks %ld different; delay lines %ld different\n", nCases, blocks, waveBlocks[13], tb_, tm_, tl_);
    }
    bad += tb_ + tm_ + tl_;

    // The MOD section.  The converter on blocks holding what the last one left;
    // the object alone, note after note with nothing reset, on buffers the check
    // fills, updates with any Type's block; then both voices through the
    // firmware's render with the MOD on; then the firmware's own tone note-on.
    const uint32_t MOBJ = ram + 0x3d000, MOB2 = ram + 0x3d100, ML1 = ram + 0x3d200, ML2 = ram + 0x3d300, MOUT = ram + 0x3d400;
    long mcn = 0, mcb = 0;
    auto modConverter = [&](int n, int types) {
        XtModBlock pb;
        for (const uint32_t w : { 0u, 4u, 8u, 0xcu }) c.wr32(MB + w, 0);
        for (int g = 0; g < n; ++g) {
            XtModParams p = modParams((int)(rnd() % (uint32_t)types));
            if (g % 16 == 5) p.type = 5 + (int)(rnd() % 251);
            if (!modConvert(p, MB, pb)) { std::fprintf(stderr, "mod converter: %s\n", c.error().c_str()); return false; }
            ++mcn;
            if (!modBlockSame(MB, pb) && mcb++ < 6)
                std::printf("  mod converter on %d type %d level %d: type %u/%u a %.9g/%.9g b %.9g/%.9g c %.9g/%.9g\n", p.on, p.type, p.level, c.rd32(MB), pb.type, f32(MB + 4), pb.a,
                            f32(MB + 8), pb.b, f32(MB + 0xc), pb.c);
        }
        return true;
    };
    if (!modConverter(200000, 5)) return 1;
    std::printf("mod converter: %ld cases, %ld different\n", mcn, mcb);
    bad += mcb;
    // the object alone
    long modN = 0, modD = 0, modB = 0, modPlain = 0;
    float modPeak = 0;
    XtMod mObj;
    XtModBlock mB1, mB2;
    auto modFill = [&](float* a, int kind) {
        const float amp = kind == 1 ? 3.0e7f : kind == 2 ? 2.0e9f : kind == 3 ? 2.0e10f : 0.0f;
        for (int i = 0; i < 64; ++i) a[i] = amp * frnd(-1, 1);
        if (kind == 4) for (int i = 0; i < 32; ++i) { a[2 * i] = 2.0e9f * frnd(-1, 1); a[2 * i + 1] = 0.0f; }   // one channel only
        if (kind == 5) for (int i = 0; i < 32; ++i) { a[2 * i] = 2.0e9f * frnd(-1, 1); a[2 * i + 1] = a[2 * i]; }   // the same on both
    };
    auto modObjCase = [&](int g, const std::vector<int>& types) {
        const int type = types[(size_t)g % types.size()];
        XtModParams p = modParams(type);
        if (!modConvert(p, MOB2, mB1)) { std::fprintf(stderr, "mod: %s\n", c.error().c_str()); return false; }
        if (type == 4 && g % 32 == 9) {               // a type no converter writes: the note-on clears the flag, the chooser takes MIX, the update does nothing
            mB1.type = 5 + rnd() % 8;
            c.wr32(MOB2, mB1.type);
        }
        if (type == 4 && mB1.type == 4 && g % 4 == 1) {   // v from the block, which the converter always sets to 1
            mB1.a = frnd(0, 1);
            wf(MOB2 + 4, mB1.a);
        }
        const bool s1 = rnd() % 4 == 0, s2 = rnd() % 4 == 0, an = rnd() & 1;
        const int tone = type != 2 ? 0 : g % 8 == 1 ? 0 : g % 8 == 2 ? XtModShared::kTones - 1 : (int)(rnd() % XtModShared::kTones);
        c.wr32(MOBJ + 0x3c, (uint32_t)tone);
        if (!c.call(0x894f1b00, { MOBJ, MOB2, s1 ? 1u : 0u, s2 ? 1u : 0u }, sp) || !c.call(0x894f1d90, { MOBJ, an ? 1u : 0u }, sp)) { std::fprintf(stderr, "mod note-on: %s\n", c.error().c_str()); return false; }
        mObj.noteOn(mB1, s1, s2, an, *modPool, tone);
        ++modN;
        if ((!modSame(MOBJ, mObj) || c.rd32(MOBJ + 0x50) != mObj.routine() || c.rd32(MOBJ + 0x10) != kFmL + 1984 * (uint32_t)tone || c.rd32(MOBJ + 0x14) != kFmR + 1984 * (uint32_t)tone) && modB++ < 6) std::printf("  mod %d note-on type %u/%u routine %08x/%08x\n", g, c.rd32(MOBJ), mObj.type(), c.rd32(MOBJ + 0x50), mObj.routine());
        for (int blk = 0; blk < 24; ++blk) {
            if (type == 2 && blk == 22 && rnd() % 4 == 0) {   // c away from its target: it runs away at once, so only for the note's last blocks
                const float cb = frnd(-1, 2);
                c.wr32(MOB2, 2); wf(MOB2 + 4, mB1.a); wf(MOB2 + 8, mB1.b); wf(MOB2 + 0xc, cb);
                mB1.type = 2; mB1.c = cb;
                if (!c.call(0x894f1c60, { MOBJ, MOB2 }, sp)) { std::fprintf(stderr, "mod update: %s\n", c.error().c_str()); return false; }
                mObj.setBlock(mB1);
                ++modN;
            }
            if (blk % 5 == 4) {                       // a new block, of any Type, on the held note
                XtModParams q = modParams((int)(rnd() % 5));
                if (!modConvert(q, MOB2, mB1) || !c.call(0x894f1c60, { MOBJ, MOB2 }, sp)) { std::fprintf(stderr, "mod update: %s\n", c.error().c_str()); return false; }
                mObj.setBlock(mB1);
                ++modN;
                if (!modSame(MOBJ, mObj) && modB++ < 6) std::printf("  mod %d.%d update (type %d)\n", g, blk, q.type);
            }
            float l1[64], l2[64];
            modFill(l1, (int)(rnd() % 6));
            modFill(l2, (int)(rnd() % 6));
            if (type == 3) {                          // the level: the same as the last block's; and a unit either side
                const float was = mObj.lastLevel();
                const int k = blk % 8;
                if (k == 2 && was > 0.0f) {           // one sample carries the sum: it lands on, or near, the last level
                    for (float& x : l2) x = 0.0f;
                    float y = div(was, K(0x2e7fffff));
                    if (blk % 16 == 10) y = std::nextafter(y, 0.0f);
                    else if (blk % 16 == 2 && g % 2) y = std::nextafter(y, 1.0e30f);
                    l2[2 * (int)(rnd() % 7)] = g % 3 ? y : -y;
                }
            }
            if (type == 2 && blk % 6 == 1 && mObj.current(0) != 0.0f) {   // L's tap exactly on R's newest sample: 48 times full scale, which no voice reaches
                const float aim = -((float)(XtModShared::kTones * XtModShared::kLine) + 0.5f);
                l2[0] = sub(div(aim, mul(mObj.current(0), K(0x33f80000))), K(0x4effffff));
                l1[1] = 2.0e9f * frnd(-1, 1);
            }
            if (mObj.syncOn()) {                      // as the render does
                const uint32_t mask = rnd() % 4 == 0 ? 0u : ((uint32_t)rnd() << 16 | rnd()) & (rnd() % 2 ? ((uint32_t)rnd() << 16 | rnd()) : 0xffffffffu);
                c.wr32(MOBJ + 0x44, mask);
                mObj.setMask(mask);
            }
            c.write(ML1, l1, sizeof l1);
            c.write(ML2, l2, sizeof l2);
            if (!c.call(0x894f1d50, { MOBJ, ML1, ML2, MOUT }, sp)) { std::fprintf(stderr, "mod: %s\n", c.error().c_str()); return false; }
            const bool plainNow = mObj.plain();
            float sum[64];
            std::copy(l1, l1 + 64, sum);
            xtAnalogMix(sum, l2);
            mObj.process(l1, l1, l2);                // in place, as the synth runs it
            ++modD;
            int bad_i = -1;
            if (plainNow) {                          // the synth's shortcut: the routine, at gains of 1, is the plain sum
                ++modPlain;
                if (std::memcmp(sum, l1, sizeof sum) != 0 && modB++ < 6) std::printf("  mod %d.%d: plain, but not the sum\n", g, blk);
            }
            for (int i = 0; i < 64; ++i) {
                if (bad_i < 0 && !same(f32(MOUT + 4 * (uint32_t)i), l1[i])) bad_i = i;
                modPeak = std::fmax(modPeak, std::fabs(l1[i]));
            }
            const bool linesOk = type != 2 || modLinesSame(tone);
            if ((bad_i >= 0 || !modSame(MOBJ, mObj) || (int)c.rd32(MOBJ + 0x30) != mObj.writeIndex() || !linesOk) && modB++ < 6)
                std::printf("  mod %d.%d type %u: first difference at %d: fw %.9g port %.9g; a %.9g/%.9g b %.9g/%.9g w %d/%d lines %d\n", g, blk, mObj.type(), bad_i, bad_i >= 0 ? f32(MOUT + 4 * (uint32_t)bad_i) : 0.0f,
                            bad_i >= 0 ? l1[bad_i] : 0.0f, f32(MOBJ + 0x18), mObj.current(0), f32(MOBJ + 0x1c), mObj.current(1), (int)c.rd32(MOBJ + 0x30), mObj.writeIndex(), linesOk);
        }
        if (type == 2) {                         // all the lines, as a read outside the tone's own depends on them
            for (int i = 0; i < XtModShared::kTones; ++i)
                if (!modLinesSame(i) && modB++ < 6) std::printf("  mod %d: tone %d's lines\n", g, i);
        }
        return true;
    };
    c.write(MOBJ, z.data(), 0x100);
    auto modObjPass = [&](const char* name, const std::vector<int>& types) {
        modN = modD = modB = modPlain = 0; modPeak = 0;
        for (int g = 0; g < nCases; ++g)
            if (!modObjCase(g, types)) return false;
        std::printf("%s: %ld note-ons and updates, %ld blocks of 32 frames (%ld plain), %ld different (peak %.4g of 2^31)\n", name, modN, modD, modPlain, modB, modPeak / 2147483648.0f);
        bad += modB;
        return true;
    };
    if (!modObjPass("mod mix, ring", { 0, 1 })) return 1;
    // both voices through the firmware's render, with the MOD on; then its note-on
    auto modTonePass = [&](const char* name, int type) {
        modType = type;
        xmodPass = false;
        for (const uint32_t w : { 0u, 4u, 8u, 0xcu }) c.wr32(MB + w, 0);   // the block, in step: it keeps what a conversion leaves
        toneB = XtModBlock();
        modL2Blocks = modLaSaw = modSyncBlocks = 0; modL2Max = 0;
        tb_ = tm_ = tl_ = tn_ = 0;
        for (long& w : waveBlocks) w = 0;
        for (int g = 0; g < nCases; ++g)
            if (!toneCase(g)) return false;
        long blocks = 0;
        for (long w : waveBlocks) blocks += w;
        std::printf("%s tone: %d cases, %ld note-ons and updates, %ld oscillator blocks, %ld different; wrap masks %ld different; delay lines %ld different; OSC 2 sounding under the MOD in %ld blocks, peak %.4g of 2^31\n", name, nCases, tn_, blocks, tb_, tm_, tl_, modL2Blocks, modL2Max / 2147483648.0f);
        bad += tb_ + tm_ + tl_;
        oo = on_ = 0;
        for (int g = 0; g < nCases / 2; ++g)
            if (!orderCase(g)) return false;
        std::printf("%s tone note-on: %ld cases, %ld different\n", name, on_, oo);
        bad += oo;
        modType = -1;
        return true;
    };
    if (!modTonePass("mod ring", 1)) return 1;
    modForceC = false;
    if (!modObjPass("mod fm", { 2 })) return 1;
    if (!modTonePass("mod fm", 2)) return 1;
    modForceC = true;
    if (!modObjPass("mod env ring", { 3 })) return 1;
    if (!modTonePass("mod env ring", 3)) return 1;
    if (!modObjPass("mod osc sync", { 4, 4, 0, 1, 3 })) return 1;
    if (!modTonePass("mod osc sync", 4)) return 1;
    std::printf("mod osc sync tone: %ld blocks with OSC 2's mask at OSC 1, LA-SAW to SAW on %ld notes\n", modSyncBlocks, modLaSaw);

    // The Structure Types.  A tone run by the firmware's own note-on (0x894f5e60), update
    // (0x894f75f0) and render (through its pointer at +0x1774), the two OSCs, the MOD and
    // both COSM sections live, on any of the sixteen types.  Only the envelopes' and LFOs'
    // note-ons and ticks are hooked, for outputs the check sets, and the TVA stubbed.
    for (uint32_t a : { 0x894ee480u, 0x894edf70u, 0x894eef70u, 0x894f7330u }) c.unhook(a);
    const uint32_t kOs = ram + 0x3a900, SIN = ram + 0x3f500, kCosm[2] = { TN + 0x1388, TN + 0x14ec }, kCosmB[2] = { TB + 0x5ec, TB + 0x634 };
    const uint32_t kCosmBuf = ram + 0x50000;
    c.wr32(0x898585dc, kOs);                     // the OS's lock owner
    for (uint32_t a : { 0x89301920u, 0x89301930u, 0x894f54b0u, 0x894f5560u, 0x894f5360u, 0x894efdf0u, 0x894f1050u }) c.hook(a, [](Sh4& m) { m.ret(0); });
    for (uint32_t k : kCosm) if (!c.call(0x894edec0, { k }, sp)) { std::fprintf(stderr, "cosm: %s\n", c.error().c_str()); return 1; }   // its vtable
    if (!c.call(0x894fc4c0, {}, sp)) { std::fprintf(stderr, "tb constants: %s\n", c.error().c_str()); return 1; }                       // run at start-up
    {   // the keyfollow tables, as XtSynth sets them
        std::vector<float> v;
        if (!xtFwCosmKeySquares(v)) { std::fprintf(stderr, "key squares failed\n"); return 1; }
        xtCosmSetKeySquares(v.data());
        if (!xtFwOscTimeKeyScales(v)) { std::fprintf(stderr, "key scales failed\n"); return 1; }
        xtOscSetTimeKeyScales(v.data());
    }
    float sEnv[14], sRate[14], sStart[14], sLfo[8];
    bool sAtk[2] = {};
    c.hook(0x894f0130, [&](Sh4& m) {             // an envelope's note-on: the outputs this note starts from
        const uint32_t e = (m.r[4] - (TN + 0x1780)) / 0x70;
        if (e < 14) {
            const uint32_t E = envAt(e);
            wf(E + 0x4c, sEnv[e]); wf(E + 0x54, sRate[e]); wf(E + 0x58, sStart[e]);
            if (e == 4 || e == 8) { wf(E + 0x1c, sAtk[e / 4 - 1] ? 25.0f : 0.0f); wf(E + 0x2c, 0.0f); }
        }
        m.ret(0);
    });
    c.hook(0x894f0cf0, [&](Sh4& m) {             // a key-synced LFO's
        const uint32_t l = (m.r[4] - (TN + 0x1da0)) / 0x98;
        if (l < 8) wf(lfoAt(l) + 0x38, sLfo[l]);
        m.ret(0);
    });
    uint32_t sBufNext = 0;
    c.hook(0x894ec2b0, [&](Sh4& m) {             // the COSMs' buffers (the pool's slot finder), which the allocator clears
        for (uint32_t i = 0; i < 4; ++i) m.wr32(m.r[5] + 4 * i, kCosmBuf + 0x4000 * sBufNext++);
        m.ret(0);
    });
    uint32_t sm1 = 0, sm2 = 0;                   // the masks, as the renders hold them
    c.hook(0x894f6d78, [&](Sh4& m) { sm2 = m.r[0]; m.r[6] = m.r[0]; m.pc += 2; });        // mov r0,r6
    c.hook(0x894f6e5a, [&](Sh4& m) { sm2 = m.r[0]; m.r[6] = m.r[0]; m.pc += 2; });        // mov r0,r6
    c.hook(0x894f6f7a, [&](Sh4& m) { sm2 = m.r[0]; m.r[2] = 0x15; m.pc += 2; });          // mov #0x15,r2
    c.hook(0x894f6da8, [&](Sh4& m) { sm1 = m.r[0]; m.r[9] += 8; m.pc += 2; });            // add #8,r9
    c.hook(0x894f6e86, [&](Sh4& m) { sm1 = m.r[0]; m.r[2] = 0x14; m.pc += 2; });          // mov #0x14,r2
    c.hook(0x894f6fc4, [&](Sh4& m) { sm1 = m.r[0]; m.r[2] = 0x14; m.pc += 2; });          // mov #0x14,r2
    struct SBlocks {
        XtTvfBlock tvf; XtDualBlock dual; XtTbBlock tb; XtCombBlock comb; XtSbfBlock sbf; XtResBlock res; XtFsBlock fs; XtLofiBlock lofi;
        XtCompBlock comp; XtLimBlock lim; XtWsBlock ws; XtOdBlock od; XtAmpBlock amp; XtSpkBlock spk;
    };
    // a parameter's draws, by the types' ranges
    auto d63 = [&]() { return (int)(rnd() % 127) - 63; };
    auto d127 = [&]() { return (int)(rnd() % 128); };
    auto d20 = [&]() { return (int)(rnd() % 41) - 20; };
    auto d100 = [&]() { return (int)(rnd() % 201) - 100; };
    // COSM section k's block: the firmware's own converter into the tone block, the port's beside it
    auto cosmConvert = [&](int k, int type, int key, SBlocks& b) {
        const float va = frnd(0, 1), vb = frnd(0, 1);
        std::vector<uint8_t> z2(0x80, 0);
        c.write(SIN, z2.data(), 0x80);
        c.wr8(SIN + 2, (uint8_t)k); c.wr8(SIN + 3, (uint8_t)key);
        wf(SIN + 0x34, va); wf(SIN + 0x38, vb);
        std::vector<int> v;
        uint32_t conv = 0;
        switch (type) {
        case 0: case 1: {
            XtTvfParams p;
            p.filter = (int)(rnd() % 12); p.q = d127(); p.qLfo = d63(); p.qEnv = d63(); p.cutoff = d127(); p.keyfollow = d20(); p.cutoffLfo = d63(); p.cutoffEnv = d63();
            if (type == 0) { v = { p.filter, p.q, p.qLfo, p.qEnv, p.cutoff, p.keyfollow, p.cutoffLfo, p.cutoffEnv }; conv = 0x894dd870; b.tvf = xtTvfBlock(p, key, va, vb); }
            else {
                p.qOwn = d63(); p.cutoffOwn = d63();
                v = { p.filter, p.q, p.qLfo, p.qOwn, p.qEnv, p.cutoff, p.keyfollow, p.cutoffLfo, p.cutoffOwn, p.cutoffEnv };
                conv = 0x894dda40; b.tvf = xtDtvfBlock(p, key, va, vb);
            }
            break;
        }
        case 2: {
            XtDualParams p;
            p.type = (int)(rnd() % 3); p.freq1 = d127(); p.kf1 = d20(); p.q1 = d127(); p.lfo1 = d63(); p.env1 = d63(); p.freq2 = d127(); p.kf2 = d20(); p.q2 = d127(); p.lfo2 = d63(); p.env2 = d63();
            v = { p.type, p.freq1, p.kf1, p.q1, p.lfo1, p.env1, p.freq2, p.kf2, p.q2, p.lfo2, p.env2 };
            conv = 0x894dd690; b.dual = xtDualBlock(p, key, va, vb);
            break;
        }
        case 3: {
            XtTbParams p;
            p.lpfCutoff = d127(); p.lpfKf = d20(); p.lpfLfo = d63(); p.lpfEnv = d63(); p.q = d127(); p.qFollow = d63(); p.hpfCutoff = d127(); p.hpfKf = d20(); p.hpfLfo = d63(); p.hpfEnv = d63(); p.gain = d127();
            v = { p.lpfCutoff, p.lpfKf, p.lpfLfo, p.lpfEnv, p.q, p.qFollow, p.hpfCutoff, p.hpfKf, p.hpfLfo, p.hpfEnv, p.gain };
            conv = 0x894de110; b.tb = xtTbBlock(p, key, va, vb);
            break;
        }
        case 4: {
            XtCombParams p;
            p.octave = (int)(rnd() % 5); p.detune = d100(); p.detuneLfo = d63(); p.detuneEnv = d63(); p.tone = d127(); p.toneLfo = d63(); p.toneEnv = d63();
            v = { p.octave, p.detune, p.detuneLfo, p.detuneEnv, p.tone, p.toneLfo, p.toneEnv };
            conv = 0x894dd4f0; b.comb = xtCombBlock(p, va, vb);
            break;
        }
        case 5: case 6: {
            XtSbfParams p;
            p.second = type == 6;
            p.hpf = d127(); p.q = d127(); p.lpf = d127(); p.lpfKf = d20(); p.subDetune = d100(); p.octave = (int)(rnd() % 3); p.detune = d100(); p.detuneLfo = d63(); p.detuneOwn = d63(); p.detuneEnv = d63();
            p.width = d127(); p.widthKf = d20(); p.widthLfo = d63(); p.widthOwn = d63(); p.widthEnv = d63();
            if (type == 5) v = { p.hpf, p.lpf, p.lpfKf, p.subDetune, p.octave, p.detune, p.detuneLfo, p.detuneOwn, p.detuneEnv, p.width, p.widthKf, p.widthLfo, p.widthOwn, p.widthEnv };
            else v = { p.q, p.lpf, p.lpfKf, p.octave, p.detune, p.detuneLfo, p.detuneOwn, p.detuneEnv, p.width, p.widthKf, p.widthLfo, p.widthOwn, p.widthEnv };
            conv = type == 5 ? 0x894dd0a0 : 0x894dd2c0; b.sbf = xtSbfBlock(p, key, va, vb);
            break;
        }
        case 7: {
            XtResParams p;
            p.type = (int)(rnd() % 3); p.size = d127(); p.sizeKf = d20(); p.sizeLfo = d63(); p.sizeEnv = d63(); p.balance = (int)(rnd() % 101); p.balanceLfo = d63(); p.balanceEnv = d63();
            v = { p.type, p.size, p.sizeKf, p.sizeLfo, p.sizeEnv, p.balance, p.balanceLfo, p.balanceEnv };
            conv = 0x894dcf00; b.res = xtResBlock(p, key, va, vb);
            break;
        }
        case 8: {
            XtFsParams p;
            p.depth = d127(); p.depthKf = d20(); p.depthLfo = d63(); p.depthEnv = d63(); p.balance = (int)(rnd() % 101); p.balanceLfo = d63(); p.balanceEnv = d63();
            v = { p.depth, p.depthKf, p.depthLfo, p.depthEnv, p.balance, p.balanceLfo, p.balanceEnv };
            conv = 0x894ddde0; b.fs = xtFsBlock(p, key, va, vb);
            break;
        }
        case 9: {
            XtLofiParams p;
            p.bitDown = (int)(rnd() % 16); p.rateDown = (int)(rnd() % 6); p.filter = d127(); p.filterKf = d20(); p.filterLfo = d63(); p.filterEnv = d63(); p.balance = (int)(rnd() % 101); p.balanceLfo = d63(); p.balanceEnv = d63();
            v = { p.bitDown, p.rateDown, p.filter, p.filterKf, p.filterLfo, p.filterEnv, p.balance, p.balanceLfo, p.balanceEnv };
            conv = 0x894ddf80; b.lofi = xtLofiBlock(p, key, va, vb);
            break;
        }
        case 10: {
            XtCompParams p;
            p.attack = d127(); p.sustain = d127(); p.level = d127();
            v = { p.attack, p.sustain, p.level };
            conv = 0x894ddc50; b.comp = xtCompBlock(p);
            break;
        }
        case 11: {
            XtLimParams p;
            p.threshold = d127(); p.ratio = (int)(rnd() % 4); p.attack = d127(); p.release = d127(); p.level = d127();
            v = { p.threshold, p.ratio, p.attack, p.release, p.level };
            conv = 0x894ddd20; b.lim = xtLimBlock(p);
            break;
        }
        case 12: {
            XtWsParams p;
            p.type = (int)(rnd() % 6); p.level = d127(); p.drive = d127(); p.driveEnv = d63(); p.driveLfo = d63(); p.tone = d127(); p.toneKf = d20(); p.toneLfo = d63(); p.toneEnv = d63();
            v = { p.type, p.level, p.drive, p.driveEnv, p.driveLfo, p.tone, p.toneKf, p.toneLfo, p.toneEnv };
            conv = 0x894dcbf0; b.ws = xtWsBlock(p, key, va, vb);
            break;
        }
        case 13: {
            XtOdParams p;
            p.type = (int)(rnd() & 1); p.level = d127(); p.drive = d127(); p.driveLfo = d63(); p.driveEnv = d63(); p.tone = d127(); p.toneKf = d20(); p.toneLfo = d63(); p.toneEnv = d63();
            v = { p.type, p.level, p.drive, p.driveLfo, p.driveEnv, p.tone, p.toneKf, p.toneLfo, p.toneEnv };
            conv = 0x894dc8b0; b.od = xtOdBlock(p, key, va, vb);
            break;
        }
        case 14: {
            XtAmpParams p;
            p.type = (int)(rnd() % 3); p.bass = d127(); p.middle = d127(); p.treble = d127(); p.level = d127(); p.gain = d127(); p.gainLfo = d63(); p.gainEnv = d63();
            v = { p.type, p.bass, p.middle, p.treble, p.level, p.gain, p.gainLfo, p.gainEnv };
            conv = 0x894dcd80; b.amp = xtAmpBlock(p, va);
            break;
        }
        default: {
            b.spk = { (int)(rnd() % 12) };
            v = { b.spk.type };
            conv = 0x894dcef0;
            break;
        }
        }
        for (size_t i = 0; i < v.size(); ++i) c.wr16(SIN + 0x12 + 2 * (uint32_t)i, (uint16_t)(int16_t)v[i]);
        return c.call(conv, { SIN, kCosmB[k] }, sp);
    };
    // the port's note-on, the envelopes' and LFO's outputs as at the firmware's (XtSynth::startVoice)
    auto cosmNoteOn = [&](XtCosmSection& s, const SBlocks& b, float pitch, float root, int k) {
        const uint32_t ea = 9 + 2 * (uint32_t)k, eb = ea + 1;
        const float aS = sStart[ea], aO = sEnv[ea], aR = sRate[ea], bS = sStart[eb], bO = sEnv[eb], bR = sRate[eb], l = sLfo[3 + k];
        switch (s.type) {
        case 2: s.dual.noteOn(b.dual, aS, aO, aR, l, bS, bO, bR, l); break;
        case 3: s.tb.noteOn(b.tb, aO, l, bO, l); break;
        case 4: s.comb.noteOn(b.comb, pitch, root, bS, bO, bR, l, aS, aO, aR, l); break;
        case 5: case 6: s.sbf.noteOn(b.sbf, pitch, root, bS, bO, bR, l, aS, aO, aR, l); break;
        case 7: s.res.noteOn(b.res, aS, aO, aR, l, bS, bO, bR, l); break;
        case 8: s.fs.noteOn(b.fs, aS, aO, aR, l, bS, bO, bR, l); break;
        case 9: s.lofi.noteOn(b.lofi, aS, aO, aR, l, bS, bO, bR, l); break;
        case 10: s.comp.noteOn(b.comp); break;
        case 11: s.lim.noteOn(b.lim); break;
        case 12: s.ws.noteOn(b.ws, aS, aO, aR, l, bS, bO, bR, l); break;
        case 13: s.od.noteOn(b.od, aS, aO, aR, l, bS, bO, bR, l); break;
        case 14: s.amp.noteOn(b.amp, aS, aO, aR, l); break;
        case 15: s.spk.noteOn(b.spk); break;
        default: s.tvf.noteOn(b.tvf, bS, bO, bR, l, aS, aO, aR, l); break;
        }
    };
    auto sect = std::make_unique<XtCosmSection[]>(2);
    long sN = 0, sB = 0, sD[3] = {}, sNotes[3] = {}, sTypes[16] = {}, sOn2[3] = {}, sSync[3] = {}, sXmod = 0, sOff = 0, sPitch = 0;
    float sPeak = 0;
    static const uint32_t kRender[3] = { 0x894f6d40, 0x894f6e20, 0x894f6f40 };
    // The tone's number, in the middle of FM's pool: past full scale FM reads other tones'
    // lines, and below tone 0's what lies there, which here is real data (v89).  At 12 a
    // modulator 23 times full scale still reads inside the pool, as the port does
    constexpr int kSTone = 12;
    c.wr32(MODO + 0x3c, (uint32_t)kSTone);
    auto absMax = [](const float* a) { float p = 0; for (int i = 0; i < 64; ++i) p = std::fmax(p, std::fabs(a[i])); return p; };
    auto structCase = [&](int g, int structure, int blocks) {
        Case k[2] = { newCase(false), newCase(false) };
        bool on[2] = { rnd() % 8 != 0, rnd() % 8 != 0 };
        const int mt = (int)(rnd() % 5);
        for (int n = 0; n < 2; ++n) {
            k[n].p.wave = (int)(rnd() % 14);
            if (mt != 0 && k[n].p.wave == 13 && n == 0) k[n].p.wave = 2;   // X-MOD would clear OSC 2 before the MOD; it has its own band below
            if (k[n].p.wave >= 11 || rnd() % 3 == 0) k[n].p.detune = d63();
            if (k[n].p.wave <= 10 && rnd() % 3 == 0) { k[n].p.subOctave = 1 + (int)(rnd() % 3); k[n].p.subLevel = (int)(rnd() % 128); }
        }
        if (g % 8 == 3) k[0].p.wave = 13;        // X-MOD on OSC 1: it reads OSC 2's buffer, in the third Structure after COSM 2
        c.write(TB, std::vector<uint8_t>(0x800, 0).data(), 0x800);
        c.wr32(TB, (uint32_t)structure);
        c.wr32(TB + 4, 0xffffffffu);             // no wave
        for (uint32_t i = 1; i < 8; ++i) c.wr32(TB + 0x24 + (i << 5) + 0x10, 1);   // every LFO key-synced
        mtx = 0;
        // the outputs the note starts from
        for (uint32_t e = 1; e <= 13; ++e) { sEnv[e] = frnd(-1, 1); sRate[e] = rnd() & 1 ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f; sStart[e] = frnd(-1, 1); }
        for (uint32_t l = 1; l <= 7; ++l) sLfo[l] = frnd(-1, 1);
        sAtk[0] = rnd() & 1; sAtk[1] = rnd() & 1;
        const uint32_t oscBlock[2] = { TB + 0x434, TB + 0x508 };
        for (int n = 0; n < 2; ++n) {
            oscIdx = n;
            writeInput(k[n]);
            c.wr8(P + 0xc, (uint8_t)(int8_t)d63()); c.wr8(P + 0x10, (uint8_t)(int8_t)d63());
            c.wr8(P + 0x11, (uint8_t)(int8_t)d63()); c.wr8(P + 0x20, (uint8_t)(int8_t)d63());
            c.wr32(oscBlock[n], on[n] ? 3u : 0u);
            if (on[n] && !c.call(0x894e1920, { P, oscBlock[n] }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return false; }
        }
        toneB = XtModBlock();
        if (!modConvert(modParams(mt), TB + 0x5dc, toneB)) { std::fprintf(stderr, "MOD: %s\n", c.error().c_str()); return false; }
        // the COSM sections, one in eight on Through: no converter runs for that (0x894de3b0).
        // Each note from zeros: the firmware's one object holds every type's fields, which the
        // note-on clears only in part, and the port's one object per type, so what a note of
        // another type leaves is not modelled (XT_COSM_MAIN does the same, case by case)
        const int key = k[0].key;
        SBlocks cb[2];
        bool cosmOn[2];
        for (int s = 0; s < 2; ++s) {
            c.write(kCosm[s], std::vector<uint8_t>(0x160, 0).data(), 0x160);
            sect[s] = XtCosmSection();
            cosmOn[s] = rnd() % 8 != 0;
            sect[s].type = (int)(rnd() % 16);
            if (s == 1 && g % 8 == 6) { sect[s].type = 4 + (int)(rnd() % 3); cosmOn[s] = true; }   // a band on the types that read the pitch and root
            if (cosmOn[s]) {
                if (!cosmConvert(s, sect[s].type, key, cb[s])) { std::fprintf(stderr, "cosm converter: %s\n", c.error().c_str()); return false; }
                ++sTypes[sect[s].type];
            }
        }
        c.wr32(0x89f91c50, 0); c.wr32(0x89f91c54, 0); c.wr32(0x89fa9094, 0);       // the lists of sounding tones
        sBufNext = 0;
        if (!c.call(0x894f5e60, { TN, TB }, sp)) { std::fprintf(stderr, "tone note-on: %s\n", c.error().c_str()); return false; }
        c.wr32(TN + 0xc, 2);                     // sounding
        ++sNotes[structure];
        // the render's pointer
        if (c.rd32(TN + 0x1770) != (uint32_t)structure || c.rd32(TN + 0x177c) != kRender[structure]) { std::fprintf(stderr, "render: %08x for %d\n", c.rd32(TN + 0x177c), structure); return false; }
        auto ratioOf = [&](int n) {
            const uint32_t Vn = tv[n], e = (uint32_t)n * 4;
            return xtPitchTarget(f32(Vn + 8), sEnv[e + 1], f32(Vn + 0x1c), sLfo[n + 1], f32(Vn + 0x34), f32(Vn + 0xc));
        };
        auto gainOf = [&](int n, bool start) {
            const uint32_t Vn = tv[n], e = (uint32_t)n * 4;
            return xtOscGainTarget(f32(Vn + 0x5c), f32(Vn + 0x54), start && sAtk[n] ? 0.0f : sEnv[e + 4], sLfo[n + 1], f32(Vn + 0x58));
        };
        // the port, in the order of startVoice
        XtAnalogBlock pb[2];
        for (int n = 0; n < 2; ++n) {
            oscIdx = n;
            pb[n] = portBlock(k[n]);
            oscIdx = 0;
            if (n == 1 && on[1] && toneB.type == 4 && pb[1].code == 10) pb[1].code = 2;   // the tone's note-on
            if (!on[n]) { to[n].setRatio(1.0f); continue; }
            const uint32_t e = (uint32_t)n * 4;
            to[n].noteOn(pb[n], ratioOf(n), gainOf(n, false), gainOf(n, true), sEnv[e + 3], sEnv[e + 2], sLfo[n + 1], shared);
        }
        toneMod.noteOn(toneB, false, false, on[0], *modPool, kSTone);
        for (int s = 0; s < 2; ++s)
            if (cosmOn[s]) cosmNoteOn(sect[s], cb[s], to[xtCosmOsc(structure, s)].ratio(), xtCosmRootHz(60, true), s);
        ++sN;
        if (structure == 2 && cosmOn[1] && sect[1].type >= 4 && sect[1].type <= 6 && on[1] && !same(to[0].ratio(), to[1].ratio())) ++sPitch;
        // what each section was given, and the routines: analog voices have no stereo flag
        bool okOn = fieldsSame(0) && fieldsSame(1) && modSame(MODO, toneMod) && c.rd32(MODO + 0x50) == toneMod.routine();
        for (int s = 0; s < 2; ++s) {
            const int osc = xtCosmOsc(structure, s);
            okOn = okOn && same(f32(kCosm[s]), to[osc].ratio()) && same(f32(kCosm[s] + 4), xtCosmRootHz(60, true)) && c.rd32(kCosm[s] + 8) == 0;
            okOn = okOn && c.rd32(kCosm[s] + 0x10) == (cosmOn[s] ? c.rd32(kCosmB[s]) : 0u) && c.rd32(kCosm[s] + 0x148) == c.rd32(0x8987c7ec + 4 * c.rd32(kCosm[s] + 0x10));   // the mono table
        }
        if (!okOn && sB++ < 8)
            std::printf("  struct %d.%d note-on: osc %d %d mod %d/%d ratio %.9g %.9g root %.9g %.9g flags %u %u\n", structure, g, fieldsSame(0), fieldsSame(1), c.rd32(MODO), toneMod.type(),
                        f32(kCosm[0]), f32(kCosm[1]), f32(kCosm[0] + 4), f32(kCosm[1] + 4), c.rd32(kCosm[0] + 8), c.rd32(kCosm[1] + 8));
        for (int blk = 0; blk < blocks; ++blk) {
            if (blk == 9) c.wr32(TB, (uint32_t)(structure + 1) % 3);   // the patch's Structure changes under the note: it keeps its own
            if (blk % 5 == 4) {                  // the tone's update, on new outputs
                for (uint32_t e = 1; e <= 12; ++e) { sEnv[e] = frnd(-1, 1); sRate[e] = rnd() & 1 ? 1.0f / 32.0f : frnd(0.2f, 1.0f) / 32.0f; wf(envAt(e) + 0x4c, sEnv[e]); wf(envAt(e) + 0x54, sRate[e]); }
                for (uint32_t l = 1; l <= 4; ++l) { sLfo[l] = frnd(-1, 1); wf(lfoAt(l) + 0x38, sLfo[l]); }
                if (!c.call(0x894f75f0, { TN, 1 }, sp)) { std::fprintf(stderr, "tone update: %s\n", c.error().c_str()); return false; }
                for (int n = 0; n < 2; ++n) {
                    if (!on[n]) continue;
                    const uint32_t e = (uint32_t)n * 4;
                    to[n].update(pb[n], ratioOf(n), gainOf(n, false), sEnv[e + 3], sEnv[e + 2], sLfo[n + 1]);
                }
                for (int s = 0; s < 2; ++s) {
                    if (!cosmOn[s]) continue;
                    const uint32_t ea = 9 + 2 * (uint32_t)s, eb = ea + 1;
                    sect[s].update(to[xtCosmOsc(structure, s)].ratio(), sEnv[ea], sRate[ea], sEnv[eb], sRate[eb], sLfo[3 + s]);
                }
                ++sN;
                if ((!fieldsSame(0) || !fieldsSame(1)) && sB++ < 8) std::printf("  struct %d.%d.%d update\n", structure, g, blk);
            }
            sm1 = sm2 = 0xdeadbeef;
            if (!c.call(c.rd32(TN + 0x177c), { TN, BUF }, sp)) { std::fprintf(stderr, "render %d: %s\n", structure, c.error().c_str()); return false; }
            float l1[64] = {}, l2[64] = {}, mix[64] = {};
            uint32_t pm1 = 0, pm2 = 0;
            float pk1 = 0, pk2 = 0;                  // what the MOD and X-MOD were given, the OSCs' with any COSM on them
            bool modDone = false;
            for (const XtToneStep* st = xtToneOrder(structure); *st != XtToneStep::End; ++st) {
                switch (*st) {
                case XtToneStep::Osc2:
                    if (on[1]) pm2 = to[1].process(l2);
                    break;
                case XtToneStep::Cosm2OnOsc2:
                    if (cosmOn[1]) sect[1].process(l2, 1.0f);
                    break;
                case XtToneStep::Osc1: {
                    uint32_t sync = 0;
                    if (toneMod.syncOn()) { sync = pm2; toneMod.setMask(pm2); }
                    pk2 = absMax(l2);
                    if (on[0]) pm1 = to[0].process(l1, sync, l2);
                    break;
                }
                case XtToneStep::Mod:
                    pk1 = absMax(l1);
                    toneMod.process(mix, l1, l2);
                    modDone = true;
                    break;
                case XtToneStep::Cosm1:
                    if (cosmOn[0]) sect[0].process(modDone ? mix : l1, 1.0f);
                    break;
                case XtToneStep::Cosm2:
                    if (cosmOn[1]) sect[1].process(mix, 1.0f);
                    break;
                case XtToneStep::End:
                    break;
                }
            }
            ++sD[structure];
            int bad_i = -1;
            for (int i = 0; i < 64 && bad_i < 0; ++i)
                if (!same(f32(kBuf1 + 4 * (uint32_t)i), l1[i]) || !same(f32(kBuf2 + 4 * (uint32_t)i), l2[i]) || !same(f32(kBufMix + 4 * (uint32_t)i), mix[i])) bad_i = i;
            // what the block ran: the check is no use on zeros
            if (pk2 > 0) {
                ++sOn2[structure];
                if (structure == 2 && on[0] && k[0].p.wave == 13 && cosmOn[1]) ++sXmod;
            }
            if (toneMod.syncOn() && pm2) ++sSync[structure];
            if ((structure == 2 && !on[1] && pk2 > 0) || (structure != 0 && !on[0] && pk1 > 0)) ++sOff;
            sPeak = std::fmax(sPeak, absMax(mix));
            const bool ok = bad_i < 0 && sm1 == pm1 && sm2 == pm2 && fieldsSame(0) && fieldsSame(1) && modSame(MODO, toneMod) && (int)c.rd32(MODO + 0x30) == toneMod.writeIndex() && modLinesSame(kSTone);
            if (!ok && sB++ < 8) {
                std::printf("  struct %d.%d.%d waves %d %d on %d %d mod %u cosm %d:%d %d:%d: first difference at %d, masks %08x/%08x %08x/%08x\n", structure, g, blk, k[0].p.wave, k[1].p.wave, on[0], on[1], toneMod.type(),
                            cosmOn[0], sect[0].type, cosmOn[1], sect[1].type, bad_i, sm1, pm1, sm2, pm2);
                if (bad_i >= 0)
                    std::printf("    osc1 %.9g/%.9g osc2 %.9g/%.9g mix %.9g/%.9g\n", f32(kBuf1 + 4 * (uint32_t)bad_i), l1[bad_i], f32(kBuf2 + 4 * (uint32_t)bad_i), l2[bad_i], f32(kBufMix + 4 * (uint32_t)bad_i), mix[bad_i]);
            }
        }
        for (int n = 0; n < 2; ++n) {
            if (!c.call(0x894f1fd0, { tv[n] }, sp)) { std::fprintf(stderr, "stop: %s\n", c.error().c_str()); return false; }
            if (on[n]) to[n].release();
        }
        if (!toneLines() && sB++ < 8) std::printf("  struct %d.%d stop: lines differ\n", structure, g);
        c.wr32(0x89f91c50, 0); c.wr32(0x89f91c54, 0); c.wr32(0x89fa9094, 0);
        return true;
    };
    for (const uint32_t w : { 0u, 4u, 8u, 0xcu }) c.wr32(TB + 0x5dc + w, 0);
    for (int g = 0; g < nCases; ++g)             // each note a Structure other than the last's
        if (!structCase(g, g % 3, 24)) return 1;
    std::printf("structure tone: %ld notes on Type1, %ld on Type2, %ld on Type3, %ld note-ons and updates, blocks %ld %ld %ld, %ld different\n", sNotes[0], sNotes[1], sNotes[2], sN, sD[0], sD[1], sD[2], sB);
    std::printf("structure tone: OSC 2 sounding in %ld %ld %ld blocks, its mask at OSC 1 in %ld %ld %ld, X-MOD reading OSC 2 after COSM 2 in %ld, a COSM making sound on an OSC that is off in %ld, COSM 2 on OSC 2's other pitch on %ld notes (peak %.4g of 2^31)\n",
                sOn2[0], sOn2[1], sOn2[2], sSync[0], sSync[1], sSync[2], sXmod, sOff, sPitch, sPeak / 2147483648.0f);
    std::printf("structure tone: the COSM sections on each type:");
    for (long t : sTypes) std::printf(" %ld", t);
    std::printf("\n");
    // Which OSC's stereo flag picks each section's routines, the mono table (0x8987c7ec) or
    // the stereo one (0x8987c840): the voices' installs hooked to leave the flags the check
    // sets.  The port has only the mono routines, which is what an analog voice takes.
    long fN = 0, fB = 0;
    uint32_t fFlag[2] = {};
    c.hook(0x894f2010, [&](Sh4& m) { m.wr32(m.r[4] + 0x960, fFlag[m.r[4] == V2 ? 1 : 0]); m.ret(0); });
    for (int st = 0; st < 3; ++st)
        for (uint32_t f = 0; f < 4; ++f) {
            fFlag[0] = f & 1; fFlag[1] = f >> 1;
            c.write(TB, std::vector<uint8_t>(0x800, 0).data(), 0x800);
            c.wr32(TB, (uint32_t)st); c.wr32(TB + 4, 0xffffffffu);
            for (uint32_t i = 1; i < 8; ++i) c.wr32(TB + 0x24 + (i << 5) + 0x10, 1);
            SBlocks cb;
            for (int k = 0; k < 2; ++k)
                if (!cosmConvert(k, 0, 60, cb)) { std::fprintf(stderr, "cosm converter: %s\n", c.error().c_str()); return 1; }
            c.wr32(0x89f91c50, 0); c.wr32(0x89f91c54, 0); c.wr32(0x89fa9094, 0);
            sBufNext = 0;
            if (!c.call(0x894f5e60, { TN, TB }, sp)) { std::fprintf(stderr, "tone note-on: %s\n", c.error().c_str()); return 1; }
            for (int k = 0; k < 2; ++k) {
                ++fN;
                const uint32_t table = fFlag[xtCosmOsc(st, k)] ? 0x8987c840 : 0x8987c7ec;
                if (c.rd32(kCosm[k] + 0x148) != c.rd32(table + 4 * 13) && fB++ < 6) std::printf("  structure %d flags %u %u: COSM %d's routine %08x\n", st, fFlag[0], fFlag[1], k + 1, c.rd32(kCosm[k] + 0x148));
            }
        }
    c.unhook(0x894f2010);
    std::printf("structure stereo flags: %ld routines, %ld different\n", fN, fB);
    bad += fB;
    bad += sB;

    // PCM on OSC 2.  Real encoded waves in the interpreter's memory, each OSC with
    // its own: the VariPhrase converter (0x894e2f20) with the kind its note-time
    // twin (0x894e2d20) gives, the binding, the voice setup and note-on (0x894f21c0,
    // 0x894f3110), the install (0x894f3660), the MOD, the updates (0x894f2090) and
    // the tone's render, with the COSM sections and the TVA stubbed.  Compared every
    // block: the five values the update moves, both buffers and the mix.  OSC 1 is
    // off, analog (X-MOD among them, reading OSC 2's PCM) or PCM.  All bit for bit,
    // the players included.  Energy, which only a new block moves, changes
    // during some notes; some loop, some SOLO notes are Robot Voice, and on some
    // the MOD is RING, FM, ENV RING or OSC SYNC.
    // the stubs this pass runs on, whatever the passes before it left
    c.hook(0x894f5570, [](Sh4& m) { m.ret(0); });
    c.hook(STUB, [](Sh4& m) { m.ret(0); });
    c.wr32(TN + 0x14d0, STUB); c.wr32(TN + 0x1634, STUB);
    if (!xtMbLoadTables(argv[1]) || !xtEnvLoadTables(argv[1])) { std::fprintf(stderr, "pcm: player tables\n"); return 1; }
    {
        std::vector<float> v;
        if (!xtFwOscTimeKeyScales(v)) { std::fprintf(stderr, "pcm: time key scales\n"); return 1; }
        xtOscSetTimeKeyScales(v.data());
        if (!xtFwOscPitchRatios(v)) { std::fprintf(stderr, "pcm: pitch ratios\n"); return 1; }
        xtOscSetPitchRatios(v.data());
    }
    constexpr uint32_t kWaves2 = 0x8D000000;
    c.mapRam(kWaves2, 0x2000000);
    struct PcmWave {
        int type = 1, ch = 1;
        long frames = 0;
        std::vector<int16_t> pcm;
        std::vector<uint8_t> vpdt;
        std::vector<XtRecord> recs;
        XtMbBands bands;
        XtMbWave w;
        XtSoloPlan plan;
        uint32_t vp = 0, samp = 0, band[5] = {}, hdr = 0, tune = 0, cache = 0;
    };
    constexpr int kPw = 6, kPwFrames = 6000;
    constexpr int kPwTempo = 1200000;                     // the header's +0x1c, what the block's +0x60 is set to meet
    static PcmWave pw[kPw];
    XtFwBandReads bandReads;
    xtFwHookBandReads(c, bandReads);
    {
        uint32_t next = kWaves2;
        auto place = [&](const void* d, size_t n) {
            const uint32_t a = next;
            if (d) c.write(a, d, n);
            next = (a + (uint32_t)n + kXtFwWavePad + 0xfffu) & ~0xfffu;   // zeros between: reads past a band find what the port's do
            return a;
        };
        const int types[kPw] = { 3, 3, 5, 5, 1, 1 };
        for (int i = 0; i < kPw; ++i) {
            PcmWave& W = pw[i];
            W.type = types[i]; W.ch = 1 + (i & 1); W.frames = kPwFrames;
            W.pcm.resize((size_t)(W.frames * W.ch));
            uint32_t n = 0x9e3779b9u + (uint32_t)i;
            for (long t = 0; t < W.frames; ++t) {
                const double burst = std::exp(-(double)(t % 2000) / 700.0);
                for (int k = 0; k < W.ch; ++k) {
                    n = n * 1664525u + 1013904223u;
                    const double p = 2.0 * 3.14159265358979 * (double)t;
                    const double x = 9000.0 * std::sin(p / (97.0 + 13.0 * k + 5.0 * i)) + 3500.0 * std::sin(p / (31.0 + 7.0 * k)) + 400.0 * ((double)(n >> 16) / 32768.0 - 1.0);
                    W.pcm[(size_t)(t * W.ch + k)] = (int16_t)(x * (0.35 + burst));
                }
            }
            if (W.type == 1) {
                if (!xtFwSoloEncode(W.pcm.data(), (int)W.frames, W.recs, W.ch) || W.recs.size() < 2) { std::fprintf(stderr, "pcm: solo encode\n"); return 1; }
                XtSoloPlan& Q = W.plan;
                Q.in.resize((size_t)W.frames);
                if (W.ch == 2) Q.inR.resize((size_t)W.frames);
                for (long t = 0; t < W.frames; ++t) {
                    Q.in[(size_t)t] = (float)W.pcm[(size_t)(t * W.ch)] / 32768.0f;
                    if (W.ch == 2) Q.inR[(size_t)t] = (float)W.pcm[(size_t)(2 * t + 1)] / 32768.0f;
                }
                const size_t nc = W.recs.size();
                Q.segStart.resize(nc); Q.segLen.resize(nc); Q.voiced.resize(nc); Q.info.resize(nc);
                for (size_t r = 0; r < nc; ++r) {
                    Q.segStart[r] = W.recs[r].start16 / 16.0;
                    const double end = r + 1 < nc ? W.recs[r + 1].start16 / 16.0 : (double)W.frames;
                    Q.segLen[r] = std::max(1.0 / 16.0, end - Q.segStart[r]);
                    Q.voiced[r] = (W.recs[r].info & 4) != 0;
                    Q.info[r] = W.recs[r].info;
                }
                uint8_t ev[4][kXtFwSoloEventBytes];
                xtFwSoloImage(W.recs, W.frames, W.ch, XtLoop{}, W.vpdt, ev);
                W.vp = place(W.vpdt.data(), W.vpdt.size());
                W.samp = place(W.pcm.data(), W.pcm.size() * 2);
            } else {
                if (!xtFwEncodeMulti(W.type, W.pcm.data(), W.frames, W.ch, W.vpdt, 64) || !xtFwLoadBands(W.type, W.pcm.data(), W.frames, W.ch, W.bands)) {
                    std::fprintf(stderr, "pcm: band encode\n");
                    return 1;
                }
                W.w.vpdt = W.vpdt.data(); W.w.vpdtBytes = W.vpdt.size();
                xtMbAttach(W.bands, W.w);
                W.vp = place(W.vpdt.data(), W.vpdt.size());
                W.samp = place(W.bands.buf[0].data(), W.bands.buf[0].size() * 2);
                bandReads.add(W.samp, W.bands.buf[0].size() * 2);
                for (int k = 0; k < 5; ++k) {
                    W.band[k] = place(W.bands.buf[k + 1].data(), W.bands.buf[k + 1].size() * 2);
                    bandReads.add(W.band[k], W.bands.buf[k + 1].size() * 2);
                }
            }
            W.hdr = place(nullptr, 0x100);
            W.tune = place(nullptr, 0x100);
            W.cache = place(nullptr, 0x400);
            c.wr8(W.hdr + 2, W.ch == 2 ? 1 : 0);
            c.wr16(W.hdr + 8, 44000);                         // the rate ratio (+0x990) comes out 1
            c.wr8(W.hdr + 0xa, 60);
            c.wr32(W.hdr + 0x1c, (uint32_t)kPwTempo);
            c.wr32(W.cache + 0x16c, 0x7fffffff); c.wr32(W.cache + 0xb8, 0x7fffffff);   // no loop points or cache bounds
            wf(W.cache + 0x2d0, 1.0f);
            if (W.type != 1) std::printf("pcm wave %d: type %d, %d channel(s), %zu bytes of vpdt\n", i, W.type, W.ch, W.vpdt.size());
            else std::printf("pcm wave %d: type 1, %d channel(s), %zu records\n", i, W.ch, W.recs.size());
        }
    }
    struct PcmKnobs { int key, key2, vel, coarse, kf, fine, gainDb, level, levelKf, levelLfo, velCurve, velSens, pitch, pEnv, pLfo, time, offset, timeKf, tEnv, tLfo, formant, formantKf, fEnv, fLfo, energy, orig; };
    auto newPcm = [&] {
        PcmKnobs k;
        k.key = 24 + (int)(rnd() % 80); k.key2 = (int)(rnd() % 128); k.vel = 1 + (int)(rnd() % 127);
        k.coarse = (int)(rnd() % 25) - 12; k.kf = (rnd() & 1) ? 1000 : (int)(rnd() % 41) * 100 - 2000; k.fine = (int)(rnd() % 1001) - 500;
        k.gainDb = (int)(rnd() % 25) - 12; k.level = (int)(rnd() % 128); k.levelKf = (int)(rnd() % 41) * 10 - 200; k.levelLfo = (int)(rnd() % 127) - 63;
        k.velCurve = (int)(rnd() % 8); k.velSens = (int)(rnd() % 127) - 63;
        k.pitch = (int)(rnd() % 61) - 30; k.pEnv = (int)(rnd() % 61) - 30; k.pLfo = (int)(rnd() % 61) - 30;
        k.time = (int)(rnd() % 127) - 63; k.offset = (int)(rnd() % 3); k.timeKf = (int)(rnd() % 41) * 10 - 200; k.tEnv = (int)(rnd() % 127) - 63; k.tLfo = (int)(rnd() % 127) - 63;
        k.formant = (int)(rnd() % 127) - 63; k.formantKf = (int)(rnd() % 41) * 10 - 200; k.fEnv = (int)(rnd() % 127) - 63; k.fLfo = (int)(rnd() % 127) - 63;
        k.energy = (rnd() & 1) ? 0 : (int)(rnd() % 128);
        k.orig = 40 + (int)(rnd() % 50);
        return k;
    };
    auto pcmInput = [&](const PcmKnobs& k, const PcmWave& W, int osc, bool loop = false, bool robot = false) {
        c.write(P, z.data(), 0x100); c.write(part, z.data(), 0x200);
        c.wr8(P + 2, (uint8_t)osc); c.wr8(P + 3, (uint8_t)k.key); c.wr8(P + 4, (uint8_t)k.vel); c.wr8(P + 0xa, (uint8_t)(int8_t)k.gainDb);
        c.wr8(P + 0xc, (uint8_t)(int8_t)k.pitch); c.wr16(P + 0xe, (uint16_t)(int16_t)k.kf); c.wr8(P + 0x10, (uint8_t)(int8_t)k.pLfo); c.wr8(P + 0x11, (uint8_t)(int8_t)k.pEnv);
        c.wr8(P + 0x1c, (uint8_t)k.level); c.wr16(P + 0x1e, (uint16_t)(int16_t)k.levelKf); c.wr8(P + 0x20, (uint8_t)(int8_t)k.levelLfo);
        c.wr8(P + 0x21, (uint8_t)xtVelCurve(k.velCurve)); c.wr16(P + 0x22, (uint16_t)(int16_t)xtVelSens(k.velSens));
        c.wr8(P + 0x48, (uint8_t)(int8_t)k.time); c.wr16(P + 0x4a, (uint16_t)(int16_t)k.timeKf); c.wr8(P + 0x4c, (uint8_t)(int8_t)k.tLfo); c.wr8(P + 0x4d, (uint8_t)(int8_t)k.tEnv);
        c.wr8(P + 0x52, (uint8_t)(int8_t)k.formant); c.wr16(P + 0x54, (uint16_t)(int16_t)k.formantKf); c.wr8(P + 0x56, (uint8_t)(int8_t)k.fLfo); c.wr8(P + 0x57, (uint8_t)(int8_t)k.fEnv);
        c.wr8(P + 0x5d, robot ? 1 : 0); c.wr8(P + 0x5f, loop ? 1 : 0); c.wr8(P + 0x60, 1); c.wr8(P + 0x61, (uint8_t)k.offset); c.wr8(P + 0x62, (uint8_t)k.energy);
        c.wr16(P + 0x68, (uint16_t)(int16_t)k.coarse); c.wr32(P + 0x70, (uint32_t)(int32_t)k.fine); c.wr8(P + 0x7c, (uint8_t)k.key2);
        c.wr8(P + 0x84, 127); c.wr8(P + 0x86, 127); c.wr8(P + 0x87, (uint8_t)W.type); c.wr8(P + 0x88, (uint8_t)k.orig);
        c.wr32(P + 0x8c, W.ch == 2 ? 1u : 0u); c.wr8(P + 0x90, 1); c.wr32(P + 0x9c, (uint32_t)W.frames);
        const uint32_t ptrs[12] = { W.vp, W.samp, W.band[0], W.band[1], W.band[2], W.band[3], W.band[4], 0, W.hdr, 0, W.tune, W.cache };
        for (uint32_t i = 0; i < 12; ++i) c.wr32(P + 0xac + 4 * i, ptrs[i]);
        c.wr32(P + 0xdc, part);
    };
    struct PcmVoice {
        const PcmWave* W = nullptr;
        XtMbPlayer mb;
        XtSoloVoice solo;
        PcmKnobs k{};
        XtLoop loop;
        bool robot = false;
        float curA = 1, curF = 1, curWalk = 1, curG = 0, curGain = 0;
    };
    // A wave's loop, in its record and cached events, or none (the end points past every position)
    auto pcmLoop = [&](const PcmWave& W, const XtLoop& l) {
        c.write(W.cache, std::vector<uint8_t>(0x2d0, 0).data(), 0x2d0);
        c.write(W.tune, z.data(), 0x20);
        if (!l.on) { c.wr32(W.cache + 0x16c, 0x7fffffff); c.wr32(W.cache + 0xb8, 0x7fffffff); return; }
        c.wr32(W.tune + 4, (uint32_t)l.fwdStart); c.wr32(W.tune + 8, (uint32_t)l.fwdEnd);
        c.wr32(W.tune + 0xc, (uint32_t)l.bwdEnd); c.wr32(W.tune + 0x10, (uint32_t)l.bwdStart);
        c.wr8(W.tune + 0x18, (uint8_t)l.fwdMode); c.wr8(W.tune + 0x19, (uint8_t)l.bwdMode);
        if (W.type == 1) {
            std::vector<uint8_t> v;
            uint8_t ev[4][kXtFwSoloEventBytes];
            xtFwSoloImage(W.recs, W.frames, W.ch, l, v, ev);
            for (uint32_t i = 0; i < 4; ++i) c.write(W.cache + 0xb4 * i, ev[i], kXtFwSoloEventBytes);
        } else {
            uint8_t ev[4][kXtMbLoopEventBytes];
            if (xtMbLoopEvents(W.w, l, ev))
                for (uint32_t i = 0; i < 4; ++i) c.write(W.cache + (uint32_t)kXtMbLoopEventBytes * i, ev[i], kXtMbLoopEventBytes);
        }
    };
    static PcmVoice pv[2];
    c.hook(0x894f6d78, [&](Sh4& m) { mask2 = m.r[0]; m.r[6] = m.r[0]; m.pc += 2; });
    // the ramps the voice's constructor (0x894f1f50) makes for Time Trip: types 2 and 1, their value 1 when idle
    for (uint32_t Vn : { V1, V2 }) { c.wr32(Vn + 0x8c8, 2); c.wr32(Vn + 0x910, 1); }
    long pn = 0, pd = 0, pb = 0, pm = 0, pm2 = 0, pbl = 0, pOut2 = 0;
    double pMax = 0;
    long pByType[6] = {};
    auto pcmCase = [&](int g) {
        const int osc1 = g % 8;                      // 0 off, 1…3 analog, 4…7 PCM
        const PcmWave* wave[2] = { osc1 >= 4 ? &pw[(g / 8 + 3) % kPw] : nullptr, &pw[g % kPw] };
        Case k1 = newCase(false);
        k1.p.wave = (int)(rnd() % 14);
        const bool analog1 = osc1 >= 1 && osc1 <= 3;
        float eOut[9], lOut[3];
        bool atk[2];
        auto drawOutputs = [&] {
            for (uint32_t e = 1; e <= 8; ++e) { eOut[e] = frnd(-1, 1); wf(envAt(e) + 0x4c, eOut[e]); }
            for (uint32_t l = 1; l <= 2; ++l) { lOut[l] = frnd(-1, 1); wf(lfoAt(l) + 0x38, lOut[l]); }
        };
        drawOutputs();
        mtx = 0;
        for (int n = 0; n < 2; ++n) {
            c.write(tb[n], z.data(), 0xd4);
            if (wave[n]) {
                pv[n].W = wave[n];
                pv[n].k = newPcm();
                pv[n].robot = wave[n]->type == 1 && rnd() % 4 == 0;
                if (n == 1 && wave[0] == wave[1]) pv[1].loop = pv[0].loop;   // one wave, one loop
                else {
                    XtLoop l;
                    if (rnd() % 3 == 0) {                 // its points and modes, as XtSynth's waveLoop gives them
                        const int a = (int)(rnd() % (rnd() & 1 ? 500u : kPwFrames - 2000u)), b = a + 64 + (int)(rnd() % 1800);   // half soon enough to reach in 30 blocks
                        l.on = true;
                        l.fwdMode = l.bwdMode = 1 + (int)(rnd() % 2);
                        l.fwdStart = l.bwdStart = a;
                        l.fwdEnd = l.bwdEnd = b;
                    }
                    pv[n].loop = l;
                    pcmLoop(*wave[n], l);
                }
                pcmInput(pv[n].k, *wave[n], n, pv[n].loop.on, pv[n].robot);
                c.wr32(tb[n], 2u);                    // the kind of an encode type 1, 3 or 5 (0x894e2d20)
                wf(tb[n] + 0x60, 120.0f);             // the tempo (0x894e2d20): the player's +0x4c, against the header's
                if (!c.call(0x894e2f20, { P, tb[n] }, sp)) { std::fprintf(stderr, "pcm converter: %s\n", c.error().c_str()); return false; }
            } else if (n == 0 && analog1) {
                oscIdx = 0;
                writeInput(k1);
                c.wr32(tb[0], 3u);
                if (!c.call(0x894e1920, { P, tb[0] }, sp)) { std::fprintf(stderr, "converter: %s\n", c.error().c_str()); return false; }
            } else c.wr32(tb[n], 0u);
            if (!c.call(0x894f7070, { TN, tb[n], (uint32_t)n }, sp)) { std::fprintf(stderr, "binding: %s\n", c.error().c_str()); return false; }
        }
        const uint32_t ampEnv[2] = { 4, 8 };
        for (int n = 0; n < 2; ++n) {
            atk[n] = rnd() & 1;
            wf(envAt(ampEnv[n]) + 0x1c, atk[n] ? 25.0f : 0.0f); wf(envAt(ampEnv[n]) + 0x2c, 0.0f);
        }
        auto targetsOf = [&](int n, float& A, float& F, float& w, float& e_, float& gt, bool start) {
            const PcmKnobs& k = pv[n].k;
            const uint32_t e = (uint32_t)n * 4;
            const float l = lOut[n + 1];
            A = xtPitchTarget(xtPitchCents(k.key, k.coarse, k.kf, k.fine, k.orig, pv[n].robot), eOut[e + 1], xtPitchDepthCents((float)k.pEnv), l,
                              xtPitchDepthCents((float)k.pLfo), xtPitchRatio(k.pitch));
            F = xtOscFormantRatio(xtOscFormant(k.formant, k.formantKf, k.key), eOut[e + 3], xtOscFormantEnvDepth(k.fEnv), l, xtOscFormantLfoDepth(k.fLfo));
            w = xtOscWalk(xtOscTime(k.time, k.offset, k.timeKf, k.key), eOut[e + 2], xtOscTimeEnvDepth(k.tEnv), l, xtOscTimeLfoDepth(k.tLfo));
            e_ = xtOscEnergy(k.energy);
            gt = xtOscGainTarget(xtOscGain(k.gainDb), xtOscLevel(k.level, k.levelKf, k.key, xtVelGain(k.vel, k.velSens, k.velCurve)),
                                 start && atk[n] ? 0.0f : eOut[e + 4], l, xtOscLevelLfoDepth(k.levelLfo));
        };
        auto valuesSame = [&](int n, const char* where, int blk) {
            const PcmVoice& s = pv[n];
            const uint32_t Vn = tv[n];
            const float fw[5] = { f32(Vn + 0x144), f32(Vn + 0x148), f32(Vn + 0x14c), f32(Vn + 0x150), f32(Vn + 0x154) };
            const float po[5] = { s.curA, s.curF, s.curG, s.curWalk, s.curGain };
            static const char* name[5] = { "ratio", "formant", "energy", "walk", "gain" };
            bool ok = true;
            for (int i = 0; i < 5; ++i)
                if (!same(fw[i], po[i])) {
                    if (pd < 12) std::printf("  pcm %d.%d %s osc %d type %d: %s fw %.9g port %.9g\n", g, blk, where, n + 1, s.W->type, name[i], fw[i], po[i]);
                    ok = false;
                }
            return ok;
        };
        XtAnalogBlock pb1;
        for (int n = 0; n < 2; ++n) {
            if (n == 0 && analog1) {
                oscIdx = 0;
                pb1 = portBlock(k1);
                // the port's oscillator keeps its own state across notes; the firmware's has
                // the players' fields in the same words, so an analog note after a PCM one
                // starts from what the player left (not modelled: the OSC Type changed
                // between notes).  Here it starts from the port's.
                for (const Field& f : floats(to[0])) wf(V1 + f.off, f.v);
                c.wr32(V1 + 0x1dc, to[0].flags()); c.wr32(V1 + 0x244, to[0].noiseState()); c.wr8(V1 + 0x248, (uint8_t)to[0].noiseHold());
                c.wr32(V1 + 0x1e8, (uint32_t)to[0].hqFalling()); c.wr32(V1 + 0x278, (uint32_t)to[0].fbWrite());
            }
            if (!c.call(0x894f21c0, { tv[n], tb[n], 0 }, sp)) { std::fprintf(stderr, "voice setup: %s\n", c.error().c_str()); return false; }
            if ((c.rd32(tv[n] + 0x960) != 0) != (wave[n] && wave[n]->ch == 2) && pd++ < 12)
                std::printf("  pcm %d: osc %d stereo flag %u\n", g, n + 1, c.rd32(tv[n] + 0x960));
            if (!wave[n]) {
                if (n == 0 && analog1) {
                    const float ratio = xtPitchTarget(f32(V1 + 8), eOut[1], f32(V1 + 0x1c), lOut[1], f32(V1 + 0x34), f32(V1 + 0xc));
                    const float gt = xtOscGainTarget(f32(V1 + 0x5c), f32(V1 + 0x54), eOut[4], lOut[1], f32(V1 + 0x58));
                    const float g0 = xtOscGainTarget(f32(V1 + 0x5c), f32(V1 + 0x54), atk[0] ? 0.0f : eOut[4], lOut[1], f32(V1 + 0x58));
                    to[0].noteOn(pb1, ratio, gt, g0, eOut[3], eOut[2], lOut[1], shared);
                    ++pn;
                    if (!fieldsSame(0) && pd++ < 12) std::printf("  pcm %d analog note-on\n", g);
                }
                continue;
            }
            PcmVoice& s = pv[n];
            float gt, g0, ign[4];
            targetsOf(n, s.curA, s.curF, s.curWalk, s.curG, gt, false);
            targetsOf(n, ign[0], ign[1], ign[2], ign[3], g0, true);
            s.curGain = gt;
            if (s.W->type == 1) {
                s.solo.setRobot(s.robot);
                s.solo.setLoop(s.loop);
                s.solo.noteOn(s.W->plan, s.curA, s.curF, s.curWalk, s.curG);
                s.solo.resetGain(g0);
                s.solo.setGain(gt);
            } else {
                s.mb.setLoop(s.loop);
                if (!s.mb.noteOn(s.W->w, s.W->type, s.curA, s.curWalk)) { std::fprintf(stderr, "pcm: port note-on\n"); return false; }
                s.mb.setGainXt(g0, true);
                s.mb.setGainXt(gt);
            }
            ++pn;
            if (!valuesSame(n, "note-on", 0)) ++pd;
        }
        // the MOD: off, or on some notes RING, FM, ENV RING or OSC SYNC with OSC 2's PCM as its modulator, as
        // the tone's note-on: the MOD before the installs, the chooser after
        const bool modOn = g % 8 == 2 || g % 8 == 6;
        XtModBlock pmb;                              // holding what the firmware's holds
        pmb.type = c.rd32(MB); pmb.a = f32(MB + 4); pmb.b = f32(MB + 8); pmb.c = f32(MB + 0xc);
        if (!modConvert(modOn ? modParams(g % 8 == 2 ? 1 : g % 16 == 6 ? 2 : g % 32 == 14 ? 3 : 4) : XtModParams{}, MB, pmb)
            || !c.call(0x894f1b00, { MODO, MB, c.rd32(V1 + 0x960), c.rd32(V2 + 0x960) }, sp)) { std::fprintf(stderr, "MOD: %s\n", c.error().c_str()); return false; }
        XtMod& pMod = toneMod;                       // the same object note after note, as MODO is
        // the flags as XtSynth gives them: a stereo wave's
        pMod.noteOn(pmb, wave[0] && wave[0]->ch == 2, wave[1]->ch == 2, analog1, *modPool, 0);
        for (int n = 0; n < 2; ++n)
            if (!c.call(0x894f2010, { tv[n] }, sp)) { std::fprintf(stderr, "install: %s\n", c.error().c_str()); return false; }
        if (!c.call(0x894f1d90, { MODO, c.rd32(V1) == 3 ? 1u : 0u }, sp)) { std::fprintf(stderr, "MOD: %s\n", c.error().c_str()); return false; }
        if ((!modSame(MODO, pMod) || c.rd32(MODO + 0x50) != pMod.routine()) && pd++ < 12)
            std::printf("  pcm %d: MOD at note-on: type %u/%u stereo %u/%u routine %08x/%08x\n", g, c.rd32(MODO), pMod.type(), c.rd32(MODO + 0x38), pMod.stereoCode(), c.rd32(MODO + 0x50), pMod.routine());
        const int nBlocks = g % 16 == 9 ? 420 : 30;  // some reach the end of the wave
        for (int blk = 0; blk < nBlocks; ++blk) {
            if (blk % 5 == 4) {
                drawOutputs();
                for (int n = 0; n < 2; ++n) {
                    if (wave[n] && blk % 10 == 9) {      // a new Energy, as a new block leaves it at +0x18
                        pv[n].k.energy = (int)(rnd() % 128);
                        wf(tv[n] + 0x18, xtOscEnergy(pv[n].k.energy));
                    }
                    if (!c.call(0x894f2090, { tv[n], 1 }, sp)) { std::fprintf(stderr, "update: %s\n", c.error().c_str()); return false; }
                    if (wave[n]) {
                        PcmVoice& s = pv[n];
                        float A, F, w, e_, gt;
                        targetsOf(n, A, F, w, e_, gt, false);
                        s.curA = xtOscSmooth(s.curA, A); s.curF = xtOscSmooth(s.curF, F); s.curG = xtOscSmooth(s.curG, e_);
                        s.curWalk = xtOscSmooth(s.curWalk, w); s.curGain = xtOscSmooth(s.curGain, gt);
                        if (s.W->type == 1) { s.solo.setRates(s.curA, s.curF, s.curWalk); s.solo.setEnergy(s.curG); s.solo.setGain(s.curGain); }
                        else { s.mb.setRates(s.curA, s.curWalk); s.mb.setGainXt(s.curGain); }
                        ++pn;
                        if (!valuesSame(n, "update", blk)) ++pd;
                    } else if (n == 0 && analog1) {
                        const float ratio = xtPitchTarget(f32(V1 + 8), eOut[1], f32(V1 + 0x1c), lOut[1], f32(V1 + 0x34), f32(V1 + 0xc));
                        const float gt = xtOscGainTarget(f32(V1 + 0x5c), f32(V1 + 0x54), eOut[4], lOut[1], f32(V1 + 0x58));
                        to[0].update(pb1, ratio, gt, eOut[3], eOut[2], lOut[1]);
                        ++pn;
                        if (!fieldsSame(0) && pd++ < 12) std::printf("  pcm %d.%d analog update\n", g, blk);
                    }
                }
            }
            mask2 = 0xdeadbeef;
            if (!c.call(0x894f6d40, { TN, BUF }, sp)) {
                if (c.error().find("read") == std::string::npos) { std::fprintf(stderr, "pcm %d.%d render: %s\n", g, blk, c.error().c_str()); return false; }
                ++pOut2;
                break;
            }
            if (mask2 != 0 && pm2++ < 6) std::printf("  pcm %d.%d: OSC 2's mask %08x\n", g, blk, mask2);
            // OSC 2, then OSC 1, in the XT's units, as XtSynth's mix takes them
            float l[2][64] = {};
            for (int n = 1; n >= 0; --n) {
                if (wave[n]) {
                    PcmVoice& s = pv[n];
                    if (s.W->type == 1) s.solo.renderXt(l[n], 32);
                    else s.mb.block(l[n]);
                    ++pByType[(s.W->type == 1 ? 4 : s.W->type == 3 ? 0 : 2) + (s.W->ch - 1)];
                } else if (n == 0 && analog1) {
                    to[0].process(l[0], 0, l[1]);                 // a PCM OSC 2's mask is 0: nothing to sync to
                    if (!fieldsSame(0) && pd++ < 12) std::printf("  pcm %d.%d analog state\n", g, blk);
                }
            }
            float mix[64];
            std::copy(l[0], l[0] + 64, mix);
            if (modOn) pMod.process(mix, mix, l[1]);
            else xtAnalogMix(mix, l[1]);
            if (!modSame(MODO, pMod) && pd++ < 12) std::printf("  pcm %d.%d: MOD state\n", g, blk);
            ++pbl;
            int firstBad = -1;
            for (int i = 0; i < 64; ++i) {
                const float f1 = f32(kBuf1 + 4 * (uint32_t)i), f2 = f32(kBuf2 + 4 * (uint32_t)i), fm = f32(kBufMix + 4 * (uint32_t)i);
                pMax = std::max(pMax, (double)std::fabs(fm) / 2147483648.0);
                if ((!same(f1, l[0][i]) || !same(f2, l[1][i]) || !same(fm, mix[i])) && firstBad < 0) firstBad = i;
                // the firmware's own sum, whatever the players
                const float sum = add(f1, f2);
                if (!modOn && !same(sum, fm) && pm++ < 6) std::printf("  pcm %d.%d mix %d: fw %.9g, its buffers sum %.9g\n", g, blk, i, fm, sum);
            }
            if (firstBad >= 0 && pb++ < 12) {
                const uint32_t i = (uint32_t)firstBad;
                std::printf("  pcm %d.%d osc1 %s osc2 type %d ch %d: first difference at %d: osc1 %.9g/%.9g osc2 %.9g/%.9g mix %.9g/%.9g\n", g, blk,
                            wave[0] ? "pcm" : analog1 ? "analog" : "off", wave[1]->type, wave[1]->ch, firstBad, f32(kBuf1 + 4 * i), l[0][i], f32(kBuf2 + 4 * i), l[1][i],
                            f32(kBufMix + 4 * i), mix[i]);
            }
        }
        for (int n = 0; n < 2; ++n) {
            if (!c.call(0x894f1fd0, { tv[n] }, sp)) { std::fprintf(stderr, "stop: %s\n", c.error().c_str()); return false; }
            if (n == 0 && analog1) to[0].release();
        }
        return true;
    };
    for (int g = 0; g < nPcm; ++g)
        if (!pcmCase(g)) return 1;
    std::printf("pcm tone: %d cases, %ld note-ons and updates, %ld different; %ld blocks, %ld different; mix of the firmware's buffers %ld different (peak %.4g); %ld notes left off, a read outside memory; %ld band reads far from their wave\n",
                nPcm, pn, pd, pbl, pb, pm, pMax, pOut2, bandReads.far);
    std::printf("pcm tone blocks by OSC wave: backing mono %ld stereo %ld, ensemble mono %ld stereo %ld, solo mono %ld stereo %ld\n",
                pByType[0], pByType[1], pByType[2], pByType[3], pByType[4], pByType[5]);
    std::printf("pcm tone: OSC 2's mask not 0 in %ld blocks\n", pm2);
    bad += pd + pb + pm + pm2;

    // The stereo flag.  A voice's +0x960 is 1 for a stereo wave.  The tone's note-on
    // (0x894f5e60) gives both COSMs' routine pick (0x894eef70) and the TVA (0x894f50e0)
    // OSC 1's alone, so a stereo OSC 2 under a mono OSC 1 goes through the mono routines,
    // which read L only and write it to both (XT_COSM_MAIN feeds them L and R apart).
    // The MOD takes both flags.  The TVA stores its flag and never reads it: its render
    // (0x894f5720) reads L and R apart, with the flag either way.
    uint32_t flagCosm[2] = {}, flagTva = 0;
    int nCosm = 0;
    c.hook(0x894eef70, [&](Sh4& m) { if (nCosm < 2) flagCosm[nCosm++] = m.r[5]; m.ret(m.r[5]); });
    c.hook(0x894f50e0, [&](Sh4& m) { flagTva = m.r[6]; m.ret(m.r[5]); });
    long fn = 0, fb = 0;
    for (int w1 = -1; w1 < 2; ++w1)
        for (int w2 = 0; w2 < 2; ++w2) {
            c.write(TB, std::vector<uint8_t>(0x800, 0).data(), 0x800);
            c.wr32(TB + 4, 0xffffffffu);
            for (uint32_t i = 1; i < 8; ++i) c.wr32(TB + 0x24 + (i << 5) + 0x10, 1);
            const uint32_t oscBlock[2] = { TB + 0x434, TB + 0x508 };
            const PcmWave* wv[2] = { w1 >= 0 ? &pw[w1] : nullptr, &pw[w2] };
            for (int n = 0; n < 2; ++n) {
                if (!wv[n]) continue;
                pcmInput(newPcm(), *wv[n], n);
                c.wr32(oscBlock[n], 2u);
                wf(oscBlock[n] + 0x60, 120.0f);
                if (!c.call(0x894e2f20, { P, oscBlock[n] }, sp)) { std::fprintf(stderr, "flag converter: %s\n", c.error().c_str()); return 1; }
            }
            c.write(MI, z.data(), 0x10);
            if (!c.call(0x894e0920, { MI, TB + 0x5dc }, sp)) { std::fprintf(stderr, "flag MOD: %s\n", c.error().c_str()); return 1; }
            c.wr32(0x89f91c50, 0); c.wr32(0x89f91c54, 0); c.wr32(0x89fa9094, 0);
            flagCosm[0] = flagCosm[1] = flagTva = 0xdeadbeef;
            nCosm = 0;
            if (!c.call(0x894f5e60, { TN, TB }, sp)) { std::fprintf(stderr, "flag note-on: %s\n", c.error().c_str()); return 1; }
            const uint32_t f1 = w1 == 1 ? 1u : 0u, f2 = w2 == 1 ? 1u : 0u;
            const uint32_t modeWant = f1 ? (f2 ? 1u : 2u) : (f2 ? 3u : 0u);
            ++fn;
            if ((flagCosm[0] != f1 || flagCosm[1] != f1 || flagTva != f1 || c.rd32(MODO + 0x38) != modeWant || c.rd32(V2 + 0x960) != f2) && fb++ < 6)
                std::printf("  flags osc 1 %u osc 2 %u: cosm %u %u tva %u mod %u\n", f1, f2, flagCosm[0], flagCosm[1], flagTva, c.rd32(MODO + 0x38));
            for (int n = 0; n < 2; ++n)
                if (!c.call(0x894f1fd0, { tv[n] }, sp)) { std::fprintf(stderr, "flag stop: %s\n", c.error().c_str()); return 1; }
        }
    std::printf("stereo flag at the tone's note-on: %ld combinations, %ld different\n", fn, fb);
    bad += fb;
    {   // the TVA's render: its flag 0 and 1; then R alone changed, which moves R's words of
        // the main pair only (the pair at +8, a send, takes (L + R)/2, and is not modelled)
        constexpr uint32_t TVA = ram + 0x3d000, TOUT = ram + 0x3d800, TIN = ram + 0x3dc00;
        std::vector<float> run[3];
        for (int t = 0; t < 3; ++t) {
            c.write(TVA, z.data(), 0x100);
            c.wr32(TVA + 0x4c, t == 1 ? 1u : 0u);
            for (uint32_t o = 0x20; o < 0x40; o += 4) wf(TVA + o, 0.25f + 0.01f * (float)o);
            for (uint32_t o = 0x68; o < 0x88; o += 4) wf(TVA + o, 0.25f + 0.01f * (float)o);
            wf(TVA + 0x60, 0.75f); wf(TVA + 0x64, 0.75f);
            for (uint32_t i = 0; i < 64; ++i) wf(TIN + 4 * i, (i & 1) ? (t == 2 ? -700.0f : 1000.0f) + (float)i : -500.0f + 3.0f * (float)i);
            c.write(TOUT, std::vector<uint8_t>(0x400, 0).data(), 0x400);
            if (!c.call(0x894f5720, { TVA, TIN, TOUT }, sp)) { std::fprintf(stderr, "tva: %s\n", c.error().c_str()); return 1; }
            for (uint32_t i = 0; i < 256; ++i) run[t].push_back(f32(TOUT + 4 * i));
        }
        long tf = 0, lMoved = 0, rMoved = 0;
        for (uint32_t i = 0; i < 256; ++i) {
            tf += !same(run[0][i], run[1][i]);
            if ((i & 7) < 2 && !same(run[0][i], run[2][i])) ++((i & 1) ? rMoved : lMoved);
        }
        std::printf("TVA render: its flag 0 and 1, %ld words different; R alone changed, %ld of the main pair's L words moved, %ld of its R words\n", tf, lMoved, rMoved);
        bad += tf + lMoved + (rMoved == 0);
    }
    return bad ? 3 : 0;
}
#endif
