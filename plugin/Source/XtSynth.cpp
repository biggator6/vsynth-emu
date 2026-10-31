// Polyphonic XT-firmware playback — see XtSynth.h.
#include "XtSynth.h"
#include "XtFirmware.h"
#include "XtSoloEncoder.h"
#include "XtFloat.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

bool g_romReady = false;

// Offline rational-rate conversion with a Kaiser-windowed sinc (β = 8, 32
// zero crossings each side at the lower rate), as the engine uses.
std::vector<double> resample(const std::vector<float>& x, double from, double to) {
    std::vector<double> y;
    if (x.empty()) return y;
    if (std::abs(from - to) < 1e-6) { y.assign(x.begin(), x.end()); return y; }
    const double ratio = to / from, fc = 0.5 * std::min(1.0, ratio);
    const int half = (int)std::ceil(32.0 / (2.0 * fc));
    const long nOut = (long)std::floor((double)x.size() * ratio);
    auto bessel0 = [](double v) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
    const double beta = 8.0, i0b = bessel0(beta);
    y.assign((size_t)nOut, 0.0);
    for (long j = 0; j < nOut; ++j) {
        const double t = (double)j / ratio;
        const long c = (long)std::floor(t);
        double acc = 0;
        for (long k = c - half + 1; k <= c + half; ++k) {
            if (k < 0 || k >= (long)x.size()) continue;
            const double d = t - (double)k, u = d / half;
            if (std::abs(u) >= 1.0) continue;
            const double a = 2 * fc * d * 3.14159265358979323846;
            const double sinc = std::abs(a) < 1e-12 ? 1.0 : std::sin(a) / a;
            acc += x[(size_t)k] * 2 * fc * sinc * bessel0(beta * std::sqrt(1 - u * u)) / i0b;
        }
        y[(size_t)j] = acc;
    }
    return y;
}

inline int16_t to16(double v) { return (int16_t)std::clamp(std::lround(v * 32768.0), -32768L, 32767L); }

} // namespace

bool xtSynthSetRom(const std::string& path) {
    g_romReady = xtLoadTables(path.c_str()) && xtMbLoadTables(path.c_str()) && xtEnvLoadTables(path.c_str())
              && xtLfoLoadTables(path.c_str()) && xtOscLoadTables(path.c_str()) && xtFwOpen(path.c_str());
    std::vector<uint32_t> ks;
    if (g_romReady && xtFwEnvKeyScales(ks)) xtEnvSetKeyScales(ks.data());
    std::vector<float> ts;
    if (g_romReady && xtFwOscTimeKeyScales(ts)) xtOscSetTimeKeyScales(ts.data());
    if (g_romReady && xtFwOscWaveGains(ts)) xtOscSetWaveGains(ts.data());
    if (g_romReady && xtFwOscPitchRatios(ts)) xtOscSetPitchRatios(ts.data());
    return g_romReady;
}
bool xtSynthRomReady() { return g_romReady; }

std::shared_ptr<XtEncoded> xtSynthEncode(const std::vector<std::vector<float>>& channels, double sampleRate,
                                         int type, int depth, std::string* err, int bits) {
    auto fail = [&](const char* m) -> std::shared_ptr<XtEncoded> { if (err) *err = m; return nullptr; };
    if (!g_romReady) return fail("no firmware image (rom.2) loaded");
    if (channels.empty() || channels[0].size() < 64) return fail("sample too short");
    if (type != 1 && type != 3 && type != 5) return fail("unknown encode type");
    const int ch = channels.size() >= 2 ? 2 : 1;
    // to int16 as the XT's WAV readers do (0x8952e4d0 16-bit, 0x8952e740
    // 24-bit, 0x8952eab0 32-bit: the top two bytes), then its converter to
    // 44.1 kHz (0x8952dec0…).  Without the converter (sample too large for the
    // interpreter's memory): the windowed-sinc resampler.
    const long n = (long)channels[0].size();
    std::vector<int16_t> src((size_t)n * ch);
    for (long i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c) {
            const double v = (double)channels[(size_t)c][(size_t)i] * 32768.0;
            const double q = bits > 16 ? std::floor(v) : std::nearbyint(v);
            src[(size_t)(i * ch + c)] = (int16_t)std::clamp(q, -32768.0, 32767.0);
        }
    const int irate = (int)std::lround(sampleRate);
    std::vector<int16_t> pcm;
    if (irate == 44100) pcm = std::move(src);
    else if (!xtFwImport(src.data(), n, ch, irate, pcm)) {
        std::vector<std::vector<double>> r;
        for (int c = 0; c < ch; ++c) r.push_back(resample(channels[(size_t)c], sampleRate, 44100.0));
        pcm.assign(r[0].size() * (size_t)ch, 0);
        for (size_t i = 0; i < r[0].size(); ++i)
            for (int c = 0; c < ch; ++c) pcm[i * (size_t)ch + (size_t)c] = to16(r[(size_t)c][i]);
    }
    const long frames = (long)(pcm.size() / (size_t)ch);

    auto E = std::make_shared<XtEncoded>();
    E->type = type;
    E->frames = frames;
    if (type == 1) {
        // The SOLO encoder reads (L+R) >> 1 for a stereo wave (0x8951f6f0);
        // the player reads both channels through those records (0x8950d1f0).
        std::vector<int16_t> x16((size_t)frames);
        for (long i = 0; i < frames; ++i)
            x16[(size_t)i] = ch == 2 ? (int16_t)(((int)pcm[(size_t)(2 * i)] + (int)pcm[(size_t)(2 * i + 1)]) >> 1) : pcm[(size_t)i];
        const std::vector<XtRecord> recs = xtSoloEncode(x16.data(), (int)frames);
        if (recs.size() < 2) return fail("SOLO encoder found no records");
        XtSoloPlan& P = E->solo;
        P.in.resize((size_t)frames);
        if (ch == 2) P.inR.resize((size_t)frames);
        for (long i = 0; i < frames; ++i) {
            P.in[(size_t)i] = (float)pcm[(size_t)(i * ch)] / 32768.0f;
            if (ch == 2) P.inR[(size_t)i] = (float)pcm[(size_t)(2 * i + 1)] / 32768.0f;
        }
        const size_t nc = recs.size();
        P.segStart.resize(nc); P.segLen.resize(nc); P.voiced.resize(nc); P.info.resize(nc);
        for (size_t i = 0; i < nc; ++i) {
            P.segStart[i] = recs[i].start16 / 16.0;
            const double end = (i + 1 < nc) ? recs[i + 1].start16 / 16.0 : (double)frames;
            P.segLen[i] = std::max(1.0 / 16.0, end - P.segStart[i]);
            P.voiced[i] = (recs[i].info & 4) != 0;
            P.info[i] = recs[i].info;
        }
        E->channels = ch;
    } else {
        if (!xtFwEncodeMulti(type, pcm.data(), frames, ch, E->vpdt, depth)) return fail("multi-band encoder failed");
        if (!xtFwLoadBands(type, pcm.data(), frames, ch, E->bands)) return fail("band rebuild failed");
        E->wave.vpdt = E->vpdt.data();
        E->wave.vpdtBytes = E->vpdt.size();
        xtMbAttach(E->bands, E->wave);
        E->channels = ch;
    }
    return E;
}

// ── Voices ───────────────────────────────────────────────────────────────────

XtSynth::XtSynth() { prepare(44100.0); }
XtSynth::~XtSynth() = default;

void XtSynth::prepare(double rate) {
    hostRate = rate > 0 ? rate : 44100.0;
    step = 44100.0 / hostRate;
    // Polyphase windowed sinc for 44.1 kHz → host rate (16 taps each side).
    const double fc = 0.5 * std::min(1.0, hostRate / 44100.0) * 0.95;   // cycles per source sample
    auto bessel0 = [](double v) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
    const double beta = 8.0, i0b = bessel0(beta);
    const int taps = 2 * kHalf;
    kernel.assign((size_t)(kPhases + 1) * taps, 0.0f);
    for (int p = 0; p <= kPhases; ++p) {
        const double f = (double)p / kPhases;
        double sum = 0;
        for (int k = 0; k < taps; ++k) {
            const double d = f - (double)(k - kHalf + 1), u = d / kHalf;
            double w = 0;
            if (std::abs(u) < 1.0) {
                const double a = 2 * fc * d * 3.14159265358979323846;
                w = 2 * fc * (std::abs(a) < 1e-12 ? 1.0 : std::sin(a) / a) * bessel0(beta * std::sqrt(1 - u * u)) / i0b;
            }
            kernel[(size_t)(p * taps + k)] = (float)w;
            sum += w;
        }
        for (int k = 0; k < taps; ++k) kernel[(size_t)(p * taps + k)] = (float)(kernel[(size_t)(p * taps + k)] / sum);
    }
    src.assign((size_t)kHalf * 2, 0.0f);           // kHalf frames of silent history
    srcFrames = (size_t)kHalf;
    readPos = (double)kHalf;                       // output time 0 = the first mixed frame: no delay
    for (Voice& v : voice) v.active = false;
}

void XtSynth::setSample(std::shared_ptr<const XtEncoded> e) {
    std::lock_guard<std::mutex> g(swapLock);
    retired.reset();                                // freed here, not on the audio thread
    pending = std::move(e);
    swapPending = true;
}

void XtSynth::pickUpSample() {
    std::unique_lock<std::mutex> g(swapLock, std::try_to_lock);
    if (!g.owns_lock() || !swapPending) return;
    retired = std::move(cur);
    cur = std::move(pending);
    swapPending = false;
    for (Voice& v : voice) v.active = false;
}

namespace {
// The TVA's constants as rom.2 stores them (each one below the nearest float)
constexpr float kInvMs = 0x1.7384eep-6f;      // 1/44.1
constexpr float kFramesPerMs = 0x1.60cccap+5f; // 44.1
constexpr float kSilent = 0x1.0000fep-16f;     // 2^-16
constexpr float kLand = 0x1.f81f8p-6f;         // 1/32.5
// The truncate fade's step per sample (0x894ea380): 1/(T/100·44.1), T the
// Truncate Time (system 0x51f, 1/100 ms; its default 70), in the SH-4's
// arithmetic.
float truncateStep() {
    static const float s = xtf::div(1.0f, xtf::mul(xtf::div(xtf::flt(70), 100.0f), kFramesPerMs));
    return s;
}
}

// The player's targets (0x894f3ba0 pitch, 0x894f3d40 formant, 0x894f3c90
// walk, voice +0x18 Energy), from the converters' values (XtOsc): the pitch
// ratio from the note's cents with the pitch envelope and the LFO at their
// depths, within ±32; formant and walk with their envelopes and the LFO,
// the formant ratio within [0.5, 2], the walk within ±5 (negative: the
// players run backward, a note starting at the wave's end).
// The voice's cents at a key (the converter's +8; at the previous key its
// +0x68): the note's Coarse and Fine (+0x68, +0x70) as the per-note setup
// forms them, from the patch's tuning (Pitch Shift as semitones and the cents
// left, ±50, added to Patch Coarse and Fine), the OSC's, the Scale Tune at
// the note's key, the bend and the note's random offset; formed again at
// every update, as an update does on the XT (0x894e4e90, the random kept).
// Robot Voice takes the pitch against A4 (0x894e4030), not against the
// wave's Original Key.
float XtSynth::noteCents(const Voice& v, int key) const {
    const long shift = std::lround((double)ctl.pitchSemis * 100.0);
    const int semis = (int)std::lround((double)shift / 100.0);
    XtTuning t;
    t.coarse = ctl.patchCoarse + semis;
    t.fine = ctl.patchFine + (int)(shift - semis * 100L);
    t.scaleOn = ctl.scaleTune;
    std::copy(std::begin(ctl.scale), std::end(ctl.scale), t.scale);
    const int coarse = xtNoteCoarse(t, ctl.oscCoarse);
    const int fine = xtNoteFine(t, v.note, ctl.oscFine, xtPitchBend(bender, ctl.bendUp, ctl.bendDown), v.rnd);
    return xtPitchCents(key, coarse, 10 * ctl.pitchKF, fine, ctl.rootKey, v.robot);
}

void XtSynth::targets(const Voice& v, float& A, float& F, float& walk, float& g) const {
    A = xtPitchTarget(noteCents(v, v.note), v.penv.level(), xtPitchDepthCents((float)std::clamp(ctl.pDepth, -63, 63)), v.lfo.out(),
                      xtPitchDepthCents((float)std::clamp(ctl.lfoPitchDepth, -63, 63)), xtPitchRatio(ctl.oscPitch), v.porta.value());
    F = xtOscFormantRatio(xtOscFormant(ctl.oscFormant, ctl.oscFormantKF, v.note), v.fenv.level(),
                          xtOscFormantEnvDepth(ctl.fDepth), v.lfo.out(), xtOscFormantLfoDepth(ctl.lfoFormantDepth));
    walk = xtOscWalk(xtOscTime(ctl.oscTime, ctl.oscTimeOffset, ctl.oscTimeKF, v.note), v.tenv.level(),
                     xtOscTimeEnvDepth(ctl.tDepth), v.lfo.out(), xtOscTimeLfoDepth(ctl.lfoTimeDepth));
    g = xtOscEnergy(ctl.energy);
}

// The player's gain target (0x894f3dc0): the OSC gain times the OSC level
// (velocity included) times the OSC amp envelope, with the LFO's tremolo.
float XtSynth::gainTarget(const Voice& v, float env) const {
    const float level = xtOscLevel(ctl.oscLevel, ctl.oscLevelKF, v.note, xtVelGain(v.vel, ctl.oscVelSens, ctl.oscVelCurve));
    return xtOscGainTarget(xtOscGain(ctl.waveGain), level, env, v.lfo.out(), xtOscLevelLfoDepth(ctl.oscLevelLfoDepth));
}

// The loop the players get (their cached loop events, player +0x6c, and
// byte 0 bit 0): the region in samples, at least 16 long as on the XT; One
// Shot leaves the Loop switch without effect (loop modes 0).
XtLoop XtSynth::waveLoop() const {
    XtLoop l;
    const int last = (int)cur->frames - 1;
    if (last < 16) return l;
    int a = (int)std::lround((double)std::clamp(ctl.loopStart, 0.0f, 1.0f) * last);
    int b = (int)std::lround((double)std::clamp(ctl.loopEnd, 0.0f, 1.0f) * last);
    if (b < a) std::swap(a, b);
    if (b - a < 16) { b = std::min(a + 16, last); a = b - 16; }
    l.on = ctl.loop;
    l.fwdMode = l.bwdMode = std::clamp(ctl.loopMode, 0, 2);
    l.fwdStart = l.bwdStart = a;
    l.fwdEnd = l.bwdEnd = b;
    return l;
}

XtEnvParams XtSynth::ampEnvParams(const Voice& v) const {
    XtEnvKnobs k;
    k.attack = ctl.oAttack; k.decay = ctl.oDecay; k.sustain = ctl.oSustain; k.release = ctl.oRelease;
    k.aSens = ctl.oASens; k.dSens = ctl.oDSens; k.rSens = ctl.oRSens; k.timeKF = ctl.oTimeKF;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel, 2);
}

void XtSynth::applyRates(Voice& v) {
    if (cur->type == 1) { v.solo.setRates(v.curA, v.curF, v.curWalk); v.solo.setEnergy(v.curG); v.solo.setGain(v.curGain); }
    else { v.mb.setRates(v.curA, v.curWalk); v.mb.setGain(v.curGain); }
}

XtLfoParams XtSynth::lfoParams() const {
    return xtLfoConvert(ctl.lfoWave, ctl.lfoRate, ctl.lfoOffset, ctl.lfoDelay, ctl.lfoFadeMode, ctl.lfoFade, ctl.lfoKeySync);
}

// The LFO constructor seeds each LFO's chaos state from a global logistic
// map (0x894f4cd0, then |2x − 1| by 0x894f4ca0).
float XtSynth::drawSeed() {
    chaosSeed = (1.0f - chaosSeed) * chaosSeed * 0x1.ff9db0p+1f;
    return std::fabs(chaosSeed * 2.0f - 1.0f);
}

XtEnvParams XtSynth::oscEnvParams(const Voice& v, int a, int d, int s, int r) const {
    XtEnvKnobs k;
    k.attack = a; k.decay = d; k.sustain = s; k.release = r;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel, 0);
}

XtEnvParams XtSynth::envParams(const Voice& v) const {
    XtEnvKnobs k;
    k.attack = ctl.attack; k.decay = ctl.decay; k.sustain = ctl.sustain; k.release = ctl.release;
    k.aSens = ctl.aSens; k.dSens = ctl.dSens; k.rSens = ctl.rSens; k.timeKF = ctl.timeKF;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel);
}

// New knob values reach a playing voice at the next update (mixBlock),
// through the voice update's smoothing, as on the XT.
void XtSynth::setControls(const Controls& c) {
    const bool envChanged = c.attack != ctl.attack || c.decay != ctl.decay || c.sustain != ctl.sustain || c.release != ctl.release
                         || c.aSens != ctl.aSens || c.dSens != ctl.dSens || c.rSens != ctl.rSens || c.timeKF != ctl.timeKF;
    const bool penvChanged = c.pAttack != ctl.pAttack || c.pDecay != ctl.pDecay || c.pSustain != ctl.pSustain || c.pRelease != ctl.pRelease;
    const bool tenvChanged = c.tAttack != ctl.tAttack || c.tDecay != ctl.tDecay || c.tSustain != ctl.tSustain || c.tRelease != ctl.tRelease;
    const bool fenvChanged = c.fAttack != ctl.fAttack || c.fDecay != ctl.fDecay || c.fSustain != ctl.fSustain || c.fRelease != ctl.fRelease;
    const bool oenvChanged = c.oAttack != ctl.oAttack || c.oDecay != ctl.oDecay || c.oSustain != ctl.oSustain || c.oRelease != ctl.oRelease
                          || c.oASens != ctl.oASens || c.oDSens != ctl.oDSens || c.oRSens != ctl.oRSens || c.oTimeKF != ctl.oTimeKF;
    const bool lfoChanged = c.lfoWave != ctl.lfoWave || c.lfoRate != ctl.lfoRate || c.lfoOffset != ctl.lfoOffset || c.lfoDelay != ctl.lfoDelay
                         || c.lfoFadeMode != ctl.lfoFadeMode || c.lfoFade != ctl.lfoFade;
    // the tone update also gives the voices' pitch ramps the new portamento
    // settings (0x894ebd34 → 0x894f48b0) and, when the key-follow key moved
    // (Coarse, Pitch KF), glides again from the voice's source key (0x894f2600)
    const bool portaChanged = c.portamento != ctl.portamento || c.portaTime != ctl.portaTime || c.portaVeloSens != ctl.portaVeloSens
                           || c.portaType != ctl.portaType;
    const bool keyChanged = c.oscCoarse != ctl.oscCoarse || c.patchCoarse != ctl.patchCoarse || c.pitchKF != ctl.pitchKF
                         || c.pitchSemis != ctl.pitchSemis;
    ctl = c;
    if (lfoChanged && freeLfoOn) freeLfo.update(lfoParams());
    for (Voice& v : voice) {
        if (!v.active) continue;
        if (envChanged) v.env.update(envParams(v));
        if (penvChanged) v.penv.update(oscEnvParams(v, ctl.pAttack, ctl.pDecay, ctl.pSustain, ctl.pRelease));
        if (tenvChanged) v.tenv.update(oscEnvParams(v, ctl.tAttack, ctl.tDecay, ctl.tSustain, ctl.tRelease));
        if (fenvChanged) v.fenv.update(oscEnvParams(v, ctl.fAttack, ctl.fDecay, ctl.fSustain, ctl.fRelease));
        if (oenvChanged) v.oenv.update(ampEnvParams(v));
        if (lfoChanged) v.lfo.update(lfoParams());
        if (portaChanged) v.porta.update(xtPortaSettings(ctl.portamento, ctl.portaType, ctl.portaTime, ctl.portaVeloSens, v.vel));
        if (keyChanged) {
            const int k = keyFollowKey(v);
            if (k != v.kfKey) {
                v.kfKey = k;
                v.porta.restart(noteCents(v, v.srcKey), noteCents(v, v.note));
            }
        }
    }
}

int XtSynth::shiftKey(int key) const {
    return std::clamp(key + 12 * ctl.octave, 0, 127);
}

int XtSynth::keyFollowKey(const Voice& v) const {
    const long shift = std::lround((double)ctl.pitchSemis * 100.0);
    XtTuning t;
    t.coarse = ctl.patchCoarse + (int)std::lround((double)shift / 100.0);
    return xtKeyFollowKey(v.note, xtNoteCoarse(t, ctl.oscCoarse), 10 * ctl.pitchKF);
}

// The part's note-on (0x894dbcb0): the key state moves on (0x894d6330),
// the key takes the Octave Shift (and the system's Transpose, 0 here).
// POLY: a new voice.  MONO with a key already held and Legato on: the
// sounding voice takes the note (0x894d4e00).  MONO otherwise: the part's
// voices are truncated (0x894d4a80) and a new one starts.
void XtSynth::noteOn(int note, float velocity) {
    pickUpSample();
    if (!cur) return;
    keys.press(note);
    keys.noteOn(note, (int32_t)std::floor((double)hostFrames * 1000.0 / hostRate));
    const int key = shiftKey(note), vel = (int)std::lround(std::clamp(velocity, 0.0f, 1.0f) * 127.0f);
    if (ctl.mono) {
        if (keys.prevMonoHeld && ctl.legato) { legatoNote(key, vel); return; }
        for (Voice& v : voice) if (v.active) truncate(v);
    }
    startVoice(note, key, vel);
}

void XtSynth::startVoice(int raw, int note, int vel) {
    // portamento's source key (0x894e4940): the previous key plus the Octave
    // Shift; with LEGATO the note's own (no glide) unless a key was held
    // then (POLY: or Hold is down, not modelled)
    const bool held = ctl.mono ? keys.prevMonoHeld : keys.prevPolyHeld;
    const int srcKey = ctl.portaMode == 1 && !held ? note : shiftKey(ctl.mono ? keys.prevMono : keys.prevPoly);
    Voice* v = nullptr;
    for (Voice& w : voice) if (!w.active) { v = &w; break; }
    if (!v) { v = &voice[0]; for (Voice& w : voice) if (w.age < v->age) v = &w; }   // steal the oldest
    v->active = true;
    v->releasing = false;
    v->damping = false;
    v->note = note;
    v->raw = raw;
    v->srcKey = srcKey;
    v->age = clock++;
    v->tail = 0;
    v->vel = vel;
    v->relVel = 64;
    v->robot = ctl.robot && cur->type == 1;           // SOLO only (the converter's SOLO tail)
    // the OSC's random pitch (0x894e4940): the shared generator steps once
    // per note when the OSC has a depth
    v->rnd = pitchRandom.next(xtRandomPitchDepth(ctl.randomPitch));
    v->gain = xtVelGain(v->vel, ctl.ampVelSens, ctl.ampVelCurve);
    // TVA note-on (0x894f5160): the gain starts from the envelope's onset
    // level (full with no attack, else 0) and ramps to its output over the
    // attack time, at most 10 ms (0.1 ms with no attack).
    const XtEnvParams ep = envParams(*v);
    v->env.noteOn(ep);
    v->cur = v->env.onsetLevel();
    v->target = v->env.level();
    const float ms = ep.time[0] == 0.0f ? (ep.time[1] == 0.0f ? 0.1f : ep.time[1]) : std::min(ep.time[0], 10.0f);
    v->step = (v->target - v->cur) / (ms * kFramesPerMs);
    v->elapsed = 0;
    // The note-on converters set the player directly (0x894f3a00…)
    v->penv.noteOn(oscEnvParams(*v, ctl.pAttack, ctl.pDecay, ctl.pSustain, ctl.pRelease));
    v->tenv.noteOn(oscEnvParams(*v, ctl.tAttack, ctl.tDecay, ctl.tSustain, ctl.tRelease));
    v->fenv.noteOn(oscEnvParams(*v, ctl.fAttack, ctl.fDecay, ctl.fSustain, ctl.fRelease));
    const XtEnvParams oe = ampEnvParams(*v);
    v->oenv.noteOn(oe);
    v->lfo.noteOn(lfoParams(), drawSeed());
    if (!ctl.lfoKeySync && freeLfoOn) v->lfo.setPhase(freeLfo.phase());
    targets(*v, v->curA, v->curF, v->curWalk, v->curG);
    // the OSC gain starts from the amp envelope's start level, or its output
    // when there is no attack (0x894f3e20)
    v->curGain = gainTarget(*v, oe.time[0] == 0.0f ? v->oenv.level() : oe.level[0]);
    if (cur->type == 1) {
        v->solo.setRobot(v->robot);
        v->solo.setLoop(waveLoop());
        v->solo.noteOn(cur->solo, v->curA, v->curF, v->curWalk, v->curG);
        v->solo.resetGain(v->curGain);
    }
    else {
        v->mb.setLoop(waveLoop());
        if (!v->mb.noteOn(cur->wave, cur->type, v->curA, v->curWalk)) v->active = false;
        else v->mb.setGain(v->curGain, true);
    }
    // portamento (0x894f2440, after the voice setup): the pitch ramp from the
    // source key's cents to the note's; it moves from the next update on
    v->kfKey = keyFollowKey(*v);
    v->porta.start(noteCents(*v, srcKey), noteCents(*v, note),
                   xtPortaSettings(ctl.portamento, ctl.portaType, ctl.portaTime, ctl.portaVeloSens, v->vel));
}

// A sounding voice takes a note (0x894dc1e0 for each voice of the part not
// released): the per-note setup again (0x894e4c30: the key, velocity, a new
// random offset; portamento from the previous MONO key), then the voice
// takes the new settings without a restart (0x894db8a0): the envelopes and
// the LFO update, the pitch ramp takes the note's portamento settings and,
// when the key-follow key changed, glides again (0x894f2600).
void XtSynth::legatoNote(int key, int vel) {
    for (Voice& v : voice) {
        if (!v.active || v.releasing || v.damping) continue;
        v.note = key;
        v.raw = keys.cur;
        v.srcKey = shiftKey(keys.prevMono);
        v.vel = vel;
        v.rnd = pitchRandom.next(xtRandomPitchDepth(ctl.randomPitch));
        v.gain = xtVelGain(v.vel, ctl.ampVelSens, ctl.ampVelCurve);
        v.env.update(envParams(v));
        v.penv.update(oscEnvParams(v, ctl.pAttack, ctl.pDecay, ctl.pSustain, ctl.pRelease));
        v.tenv.update(oscEnvParams(v, ctl.tAttack, ctl.tDecay, ctl.tSustain, ctl.tRelease));
        v.fenv.update(oscEnvParams(v, ctl.fAttack, ctl.fDecay, ctl.fSustain, ctl.fRelease));
        v.oenv.update(ampEnvParams(v));
        v.lfo.update(lfoParams());
        v.porta.update(xtPortaSettings(ctl.portamento, ctl.portaType, ctl.portaTime, ctl.portaVeloSens, v.vel));
        const int k = keyFollowKey(v);
        if (k != v.kfKey) {
            v.kfKey = k;
            v.porta.restart(noteCents(v, v.srcKey), noteCents(v, v.note));
        }
    }
}

// Truncate (0x894d5ba0 → 0x894f5500): the TVA holds its gain and fades out
// linearly over the Truncate Time (0x51f, 0.70 ms; 0x894f5570).
void XtSynth::truncate(Voice& v) {
    if (v.damping) return;
    v.damping = true;
    v.damp = 1.0f;
}

// The release time follows the note-off velocity (R-Sens): the times are
// converted again before the release starts.
// The part's note-off (0x894dbea0): the key state (0x894d63c0); a MONO part
// whose sounding key was released while others are held takes the highest
// of them (Legato on or off), else the note's voices release.
void XtSynth::noteOff(int note, float velocity) {
    keys.release(note);
    const int r = keys.noteOff(note);
    if (ctl.mono && r >= 0)
        for (const Voice& v : voice)
            if (v.active && !v.releasing && !v.damping) { legatoNote(shiftKey(r), v.vel); break; }
    for (Voice& v : voice)
        if (v.active && v.raw == note && !v.releasing && !v.damping) {
            v.releasing = true;
            v.relVel = (int)std::lround(std::clamp(velocity, 0.0f, 1.0f) * 127.0f);
            v.env.update(envParams(v));
            v.env.noteOff();
            v.penv.update(oscEnvParams(v, ctl.pAttack, ctl.pDecay, ctl.pSustain, ctl.pRelease));
            v.penv.noteOff();
            v.tenv.update(oscEnvParams(v, ctl.tAttack, ctl.tDecay, ctl.tSustain, ctl.tRelease));
            v.tenv.noteOff();
            v.fenv.update(oscEnvParams(v, ctl.fAttack, ctl.fDecay, ctl.fSustain, ctl.fRelease));
            v.fenv.noteOff();
            v.oenv.update(ampEnvParams(v));
            v.oenv.noteOff();
            v.lfo.noteOff();
        }
}

void XtSynth::allNotesOff() {
    for (Voice& v : voice) v.active = false;
    for (uint16_t& w : keys.held) w = 0;
    keys.lastMonoHeld = keys.prevMonoHeld = keys.lastPolyHeld = keys.prevPolyHeld = false;
}

void XtSynth::pitchBend(int value) {
    bender = std::clamp(value, 0, 16383);
}

// The tone update's TVA part (0x894f75f0 tail): stop a finished voice
// (0x894f54b0), else aim at the envelope's new output with a linear ramp of
// 10 ms, shorter when the stage itself is (0x894f5360).
void XtSynth::tvaUpdate(Voice& v) {
    if (v.damping) return;                                    // done when the fade is (0x894f5510)
    if (v.env.done() && v.cur <= kSilent) { v.active = false; return; }
    v.target = v.env.level();
    const float T = v.env.stageTime();
    float frames = 441.0f;
    if (!(0.0f > T)) {
        const float d = T - v.elapsed * kInvMs;               // the stage time less the time since the last update
        if (d > 0.0f) { if (10.0f > d) frames = d * kFramesPerMs; }
        else frames = 0.1f * kFramesPerMs;
    }
    v.step = (v.target - v.cur) / frames;
    v.elapsed = 0;
}

void XtSynth::mixBlock(float* lr) {
    std::fill(lr, lr + 64, 0.0f);
    if (!cur) return;
    // The control clock: the tone update runs between blocks every 441 frames.
    const bool tick = tickCount >= kTickFrames;
    if (tick) tickCount -= kTickFrames;
    tickCount += 32;
    if (tick) {   // the free-running LFO (no delay or fade: only its phase is used)
        if (!freeLfoOn) {
            XtLfoParams fp = lfoParams();
            fp.delayMs = fp.fadeMs = 0.0f;
            fp.fadeMode = 0;
            freeLfo.noteOn(fp);
            freeLfoOn = true;
        }
        freeLfo.tick(1);
    }
    float tmp[64];
    for (Voice& v : voice) {
        if (!v.active) continue;
        if (tick) {
            // the tone update (0x894f75f0): envelopes step, the voice update
            // moves the player 70% toward its targets (0x8950c1b0), then the TVA
            v.env.tick();
            v.penv.tick();
            v.tenv.tick();
            v.fenv.tick();
            v.oenv.tick();
            v.lfo.tick(1);
            v.porta.tick(1);                                   // 0x894f4b00, before the voice update
            float A, F, w, g;
            targets(v, A, F, w, g);
            const float gt = gainTarget(v, v.oenv.level());
            v.curA = xtOscSmooth(v.curA, A);
            v.curF = xtOscSmooth(v.curF, F);
            v.curWalk = xtOscSmooth(v.curWalk, w);
            v.curG = xtOscSmooth(v.curG, g);
            v.curGain = xtOscSmooth(v.curGain, gt);
            applyRates(v);
            tvaUpdate(v);
            if (!v.active) continue;
        }
        float scale = v.gain;
        if (cur->type == 1) {
            float l[32], r[32];
            v.solo.render(l, r, 32);
            for (int i = 0; i < 32; ++i) { tmp[2 * i] = l[i]; tmp[2 * i + 1] = r[i]; }
            if (v.solo.ended()) v.tail += 32;
        } else {
            v.mb.block(tmp);
            scale /= 32768.0f;
            if (v.mb.ended()) v.tail += 32;
        }
        if (v.damping) {
            // truncated (0x894f5570): the gain held, times a fade that falls
            // by 1/(Truncate Time·44.1) each sample; silent and done at 2^-16
            for (int i = 0; i < 32; ++i) {
                v.damp -= truncateStep();
                if (!(v.damp > kSilent)) { v.damp = 0.0f; v.active = false; break; }
                const float g = scale * v.cur * v.damp;
                lr[2 * i] += tmp[2 * i] * g;
                lr[2 * i + 1] += tmp[2 * i + 1] * g;
            }
            continue;
        }
        // TVA render (0x894f5720): a step that would pass the target within
        // the block is cut to (target − gain)/32.5; gain applied, then stepped.
        const float d = v.target - v.cur;
        const float s = std::fabs(v.step * 32.0f) > std::fabs(d) ? d * kLand : v.step;
        for (int i = 0; i < 32; ++i) {
            const float g = scale * v.cur;
            lr[2 * i] += tmp[2 * i] * g;
            lr[2 * i + 1] += tmp[2 * i + 1] * g;
            v.cur += s;
        }
        v.elapsed += 32.0f;
        if (v.tail > 8192) v.active = false;
    }
}

void XtSynth::render(float* left, float* right, int n) {
    pickUpSample();
    const int taps = 2 * kHalf;
    const bool direct = std::abs(step - 1.0) < 1e-12;
    for (int j = 0; j < n; ++j) {
        const size_t need = (size_t)std::floor(readPos) + (direct ? 1 : (size_t)kHalf + 1);
        while (srcFrames < need) {
            if (src.size() < (srcFrames + 32) * 2) src.resize((srcFrames + 32) * 2);
            mixBlock(src.data() + srcFrames * 2);
            srcFrames += 32;
        }
        if (direct) {
            const size_t i = (size_t)readPos;
            left[j] = src[2 * i];
            right[j] = src[2 * i + 1];
            readPos += 1.0;
        } else {
            const long i0 = (long)std::floor(readPos);
            const double f = readPos - (double)i0;
            const double ph = f * kPhases;
            const int p0 = std::min((int)ph, kPhases - 1);
            const float a = (float)(ph - p0);
            const float* k0 = kernel.data() + (size_t)p0 * taps;
            const float* k1 = k0 + taps;
            float l = 0, r = 0;
            for (int k = 0; k < taps; ++k) {
                const size_t s = (size_t)(i0 - kHalf + 1 + k);
                const float w = k0[k] + (k1[k] - k0[k]) * a;
                l += src[2 * s] * w;
                r += src[2 * s + 1] * w;
            }
            left[j] = l;
            right[j] = r;
            readPos += step;
        }
    }
    hostFrames += n;
    // drop consumed frames, keeping the history the kernel needs
    const long keep = (long)std::floor(readPos) - kHalf;
    if (keep > 4096) {
        std::memmove(src.data(), src.data() + keep * 2, (srcFrames - (size_t)keep) * 2 * sizeof(float));
        srcFrames -= (size_t)keep;
        readPos -= (double)keep;
    }
}

#ifdef XT_SYNTH_MAIN
// xt_synth <rom.2.bin> <samp.bin int16> <channels> <rate> <type 1|3|5> <hostRate> <frames> <out.f32> note[@onset[:off]]...
//   Renders notes (onset/off in host frames) to float32 L/R pairs.
//   XT_ADSR="a,d,s,r": amp envelope knobs (default 0,0,127,16).
//   XT_PENV="a,d,s,r,depth": pitch envelope knobs.
//   XT_TENV, XT_FENV="a,d,s,r,depth": time and formant envelope knobs.
//   XT_LFO="wave,rate,depth,delay,mode,fade,offset,keysync": the LFO.
//   XT_OSC="time,offset,timeKF,formant,formantKF,energy[,lfoTime,lfoFormant]":
//     the VariPhrase OSC knobs (Time Offset 0 BWD, 1 ZERO, 2 FWD).
//   XT_OLEV="level,levelKF,levelLfoDepth,waveGainDb": the OSC TVA.
//   XT_OENV="a,d,s,r[,aSens,dSens,rSens,timeKF]": the OSC amp envelope.
//   XT_VEL="ampSens,ampCurve,oscSens,oscCurve": velocity to level.
//   XT_VELOCITY=v: note-on velocity 1..127 (default 127).
//   XT_ROBOT=1: Robot Voice.
//   XT_LOOP="mode,start,end": Loop on, Loop Mode (0 One Shot, 1 One Way,
//     2 Alternate) and the region as fractions of the wave.
//   XT_PORTA="mode,type,time,veloSens": Portamento on, Mode (0 NORMAL,
//     1 LEGATO), Type (0 RATE, 1 TIME), Time 0..127, Time Velo Sens.
//   XT_PITCH="shift,pitch,pitchKF,coarse,fine[,origKey]": Pitch Shift
//     (semitones), the Pitch knob, Pitch KF (%), Coarse, Fine (cents) and the
//     Original Key (default 0,0,100,0,0,60).
//   XT_TUNE="patchCoarse,patchFine,random[,bendUp,bendDown]": the patch's
//     Coarse/Fine Tune, the OSC's Random Pitch Depth (0..30) and the Bend
//     Range (default 0,0,0,2,2).
//   XT_SCALE="c,c#,…,b": Scale Tune on, the 12 values in cents.
//   XT_BEND="value[@frame]": the bender (0..16383), from a frame on (0).
//   XT_MONO="mono,legato[,octave]": Mono/Poly (1 MONO), Legato Switch and
//     Octave Shift (default 0,1,0).
#include <cstdio>
#include <cstdlib>
int main(int argc, char** argv) {
    if (argc < 10) { std::fprintf(stderr, "usage: %s rom samp.bin ch rate type hostRate frames out.f32 note[@on[:off]]...\n", argv[0]); return 1; }
    if (!xtSynthSetRom(argv[1])) { std::fprintf(stderr, "rom not usable\n"); return 1; }
    FILE* f = std::fopen(argv[2], "rb");
    if (!f) return 1;
    std::vector<int16_t> raw;
    int16_t s;
    while (std::fread(&s, 2, 1, f) == 1) raw.push_back(s);
    std::fclose(f);
    const int ch = std::atoi(argv[3]);
    const double rate = std::atof(argv[4]), host = std::atof(argv[6]);
    const int type = std::atoi(argv[5]);
    const long frames = std::atol(argv[7]);
    std::vector<std::vector<float>> chans((size_t)ch);
    for (size_t i = 0; i + (size_t)ch <= raw.size(); i += (size_t)ch)
        for (int c = 0; c < ch; ++c) chans[(size_t)c].push_back(raw[i + (size_t)c] / 32768.0f);
    std::string err;
    auto E = xtSynthEncode(chans, rate, type, 64, &err);
    if (!E) { std::fprintf(stderr, "encode: %s\n", err.c_str()); return 1; }
    XtSynth syn;
    syn.prepare(host);
    syn.setSample(E);
    XtSynth::Controls c;
    if (const char* e = std::getenv("XT_ADSR"))     // amp envelope knobs "a,d,s,r"
        std::sscanf(e, "%d,%d,%d,%d", &c.attack, &c.decay, &c.sustain, &c.release);
    if (const char* e = std::getenv("XT_LFO")) {    // LFO "wave,rate,depth,delay,mode,fade,offset,keysync"
        int ks = 1;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &c.lfoWave, &c.lfoRate, &c.lfoPitchDepth, &c.lfoDelay, &c.lfoFadeMode, &c.lfoFade,
                    &c.lfoOffset, &ks);
        c.lfoKeySync = ks != 0;
    }
    if (const char* e = std::getenv("XT_PENV"))     // pitch envelope "a,d,s,r,depth"
        std::sscanf(e, "%d,%d,%d,%d,%d", &c.pAttack, &c.pDecay, &c.pSustain, &c.pRelease, &c.pDepth);
    if (const char* e = std::getenv("XT_TENV"))
        std::sscanf(e, "%d,%d,%d,%d,%d", &c.tAttack, &c.tDecay, &c.tSustain, &c.tRelease, &c.tDepth);
    if (const char* e = std::getenv("XT_FENV"))
        std::sscanf(e, "%d,%d,%d,%d,%d", &c.fAttack, &c.fDecay, &c.fSustain, &c.fRelease, &c.fDepth);
    if (const char* e = std::getenv("XT_OSC"))
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &c.oscTime, &c.oscTimeOffset, &c.oscTimeKF, &c.oscFormant, &c.oscFormantKF,
                    &c.energy, &c.lfoTimeDepth, &c.lfoFormantDepth);
    if (const char* e = std::getenv("XT_OLEV"))
        std::sscanf(e, "%d,%d,%d,%d", &c.oscLevel, &c.oscLevelKF, &c.oscLevelLfoDepth, &c.waveGain);
    if (const char* e = std::getenv("XT_OENV"))
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &c.oAttack, &c.oDecay, &c.oSustain, &c.oRelease, &c.oASens, &c.oDSens, &c.oRSens, &c.oTimeKF);
    if (const char* e = std::getenv("XT_VEL"))
        std::sscanf(e, "%d,%d,%d,%d", &c.ampVelSens, &c.ampVelCurve, &c.oscVelSens, &c.oscVelCurve);
    const float noteVel = std::getenv("XT_VELOCITY") ? std::atoi(std::getenv("XT_VELOCITY")) / 127.0f : 1.0f;
    c.robot = std::getenv("XT_ROBOT") && std::atoi(std::getenv("XT_ROBOT")) != 0;
    if (const char* e = std::getenv("XT_LOOP")) {
        c.loop = true;
        std::sscanf(e, "%d,%f,%f", &c.loopMode, &c.loopStart, &c.loopEnd);
    }
    if (const char* e = std::getenv("XT_PORTA")) {
        c.portamento = true;
        std::sscanf(e, "%d,%d,%d,%d", &c.portaMode, &c.portaType, &c.portaTime, &c.portaVeloSens);
    }
    if (const char* e = std::getenv("XT_PITCH"))
        std::sscanf(e, "%f,%d,%d,%d,%d,%d", &c.pitchSemis, &c.oscPitch, &c.pitchKF, &c.oscCoarse, &c.oscFine, &c.rootKey);
    if (const char* e = std::getenv("XT_TUNE"))
        std::sscanf(e, "%d,%d,%d,%d,%d", &c.patchCoarse, &c.patchFine, &c.randomPitch, &c.bendUp, &c.bendDown);
    if (const char* e = std::getenv("XT_SCALE")) {
        c.scaleTune = true;
        int* t = c.scale;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", t, t + 1, t + 2, t + 3, t + 4, t + 5, t + 6, t + 7, t + 8, t + 9, t + 10, t + 11);
    }
    if (const char* e = std::getenv("XT_MONO")) {
        int m = 0, l = 1;
        std::sscanf(e, "%d,%d,%d", &m, &l, &c.octave);
        c.mono = m != 0;
        c.legato = l != 0;
    }
    int bendValue = 8192;
    long bendAt = 0;
    if (const char* e = std::getenv("XT_BEND")) std::sscanf(e, "%d@%ld", &bendValue, &bendAt);
    syn.setControls(c);
    struct Ev { long at; int note; bool on; };
    std::vector<Ev> ev;
    for (int a = 9; a < argc; ++a) {
        int note = 60; long on = 0, off = -1;
        std::sscanf(argv[a], "%d@%ld:%ld", &note, &on, &off);
        ev.push_back({ on, note, true });
        if (off >= 0) ev.push_back({ off, note, false });
    }
    std::vector<float> L((size_t)frames), R((size_t)frames), out((size_t)frames * 2);
    long done = 0;
    while (done < frames) {
        long next = frames;
        for (const Ev& e : ev) if (e.at >= done && e.at < next) next = e.at;
        if (bendAt >= done && bendAt < next) next = bendAt;
        if (bendAt == done) syn.pitchBend(bendValue);
        for (const Ev& e : ev) if (e.at == done) { if (e.on) syn.noteOn(e.note, noteVel); else syn.noteOff(e.note); }
        if (next == done) next = done + 1;
        syn.render(L.data() + done, R.data() + done, (int)(next - done));
        done = next;
    }
    for (long i = 0; i < frames; ++i) { out[(size_t)(2 * i)] = L[(size_t)i]; out[(size_t)(2 * i + 1)] = R[(size_t)i]; }
    FILE* fo = std::fopen(argv[8], "wb");
    if (!fo) return 1;
    std::fwrite(out.data(), sizeof(float), out.size(), fo);
    std::fclose(fo);
    return 0;
}
#endif
