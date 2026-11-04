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
              && xtLfoLoadTables(path.c_str()) && xtOscLoadTables(path.c_str()) && xtCosmLoadTables(path.c_str())
              && xtAnalogLoadTables(path.c_str()) && xtFwOpen(path.c_str());
    std::vector<uint32_t> ks;
    if (g_romReady && xtFwEnvKeyScales(ks)) xtEnvSetKeyScales(ks.data());
    std::vector<float> ts;
    if (g_romReady && xtFwOscTimeKeyScales(ts)) xtOscSetTimeKeyScales(ts.data());
    if (g_romReady && xtFwOscWaveGains(ts)) xtOscSetWaveGains(ts.data());
    if (g_romReady && xtFwOscPitchRatios(ts)) xtOscSetPitchRatios(ts.data());
    if (g_romReady && xtFwCosmKeySquares(ts)) xtCosmSetKeySquares(ts.data());
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
        // A stereo wave has the encoder's stereo mode, one list of records from both
        // channels (0x8951e120(3)); the player reads both through it (0x8950d1f0).
        const std::vector<XtRecord> recs = xtSoloEncode(pcm.data(), (int)frames, ch);
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

XtSynth::XtSynth() {
    xtModBlock(ctl.mod, modBlk);
    prepare(44100.0);
}
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

void XtSynth::setSample(std::shared_ptr<const XtEncoded> e, int osc) {
    std::lock_guard<std::mutex> g(swapLock);
    retired[osc].reset();                           // freed here, not on the audio thread
    pending[osc] = std::move(e);
    swapPending[osc] = true;
}

void XtSynth::pickUpSample() {
    std::unique_lock<std::mutex> g(swapLock, std::try_to_lock);
    if (!g.owns_lock() || !(swapPending[0] || swapPending[1])) return;
    for (int k = 0; k < 2; ++k)
        if (swapPending[k]) {
            retired[k] = std::move(cur[k]);
            cur[k] = std::move(pending[k]);
            swapPending[k] = false;
        }
    for (Voice& v : voice) v.active = false;
}

namespace {
// The TVA's constants as rom.2 stores them (each one below the nearest float)
constexpr float kInvMs = 0x1.7384eep-6f;      // 1/44.1
constexpr float kFramesPerMs = 0x1.60cccap+5f; // 44.1
constexpr float kSilent = 0x1.0000fep-16f;     // 2^-16
constexpr float kLand = 0x1.f81f8p-6f;         // 1/32.5
constexpr float kNoAttackMs = 0x1.99999ap-4f;  // 0.1
constexpr float kSendGlide = 0x1.eb851ep-11f;  // 0.0009375: the output bus's level, a share of the gap per sample
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
// wave's Original Key, and so does the analog oscillator (0x894e1920).
void XtSynth::noteTuning(const Voice& v, const OscVoice& s, const OscControls& o, int& coarse, int& fine) const {
    const long shift = std::lround((double)ctl.pitchSemis * 100.0);
    const int semis = (int)std::lround((double)shift / 100.0);
    XtTuning t;
    t.coarse = ctl.patchCoarse + semis;
    t.fine = ctl.patchFine + (int)(shift - semis * 100L);
    t.scaleOn = ctl.scaleTune;
    std::copy(std::begin(ctl.scale), std::end(ctl.scale), t.scale);
    coarse = xtNoteCoarse(t, o.oscCoarse);
    fine = xtNoteFine(t, v.note, o.oscFine, xtPitchBend(bender, ctl.bendUp, ctl.bendDown), s.rnd);
}

float XtSynth::noteCents(const Voice& v, const OscVoice& s, const OscControls& o, int key) const {
    int coarse, fine;
    noteTuning(v, s, o, coarse, fine);
    return xtPitchCents(key, coarse, 10 * o.pitchKF, fine, s.analog ? 69 : o.rootKey, s.robot);
}

// The analog converter's block at the voice's note, formed again at every
// update as the knobs are (the level follows the note's pitch, the bend and
// the random offset included).  The time and formant envelopes' velocity
// curves are off: factors 1.
XtAnalogBlock XtSynth::analogBlock(const Voice& v, const OscVoice& s, const OscControls& o) const {
    int coarse, fine;
    noteTuning(v, s, o, coarse, fine);
    return xtAnalogBlock(o.ana, v.note, xtPitch10(v.note, coarse, 10 * o.pitchKF, fine), o.waveGain, 1.0f, 1.0f, &s == &v.osc2);
}

// Audio thread (it reads the samples the last render picked up).
bool XtSynth::oscPlays(const OscControls& o) const {
    if (!o.on) return false;
    if (o.oscType == 0) return xtAnalogTablesLoaded() && xtAnalogPlays(o.ana.wave);
    return waveOf(o) != nullptr;
}

bool XtSynth::canPlay() const { return oscPlays(ctl) || oscPlays(ctl.osc2); }

void XtSynth::targets(const Voice& v, const OscVoice& s, const OscControls& o, float& A, float& F, float& walk, float& g) const {
    A = xtPitchTarget(noteCents(v, s, o, v.note), s.penv.level(), xtPitchDepthCents((float)std::clamp(o.pDepth, -63, 63)), s.lfo.out(),
                      xtPitchDepthCents((float)std::clamp(o.lfoPitchDepth, -63, 63)), xtPitchRatio(o.oscPitch), s.porta.value());
    F = xtOscFormantRatio(xtOscFormant(o.oscFormant, o.oscFormantKF, v.note), s.fenv.level(),
                          xtOscFormantEnvDepth(o.fDepth), s.lfo.out(), xtOscFormantLfoDepth(o.lfoFormantDepth));
    walk = xtOscWalk(xtOscTime(o.oscTime, o.oscTimeOffset, o.oscTimeKF, v.note), s.tenv.level(),
                     xtOscTimeEnvDepth(o.tDepth), s.lfo.out(), xtOscTimeLfoDepth(o.lfoTimeDepth));
    g = xtOscEnergy(o.energy);
}

// The player's gain target (0x894f3dc0): the OSC gain times the OSC level
// (velocity included) times the OSC amp envelope, with the LFO's tremolo.
// The analog oscillator's gain carries its waveform's level instead of the
// wave's (the converter's +0x5c).
float XtSynth::gainTarget(const Voice& v, const OscVoice& s, const OscControls& o, float env) const {
    const float level = xtOscLevel(o.oscLevel, o.oscLevelKF, v.note, xtVelGain(v.vel, o.oscVelSens, o.oscVelCurve));
    const float gain = s.analog ? analogBlock(v, s, o).gain : xtOscGain(o.waveGain);
    return xtOscGainTarget(gain, level, env, s.lfo.out(), xtOscLevelLfoDepth(o.oscLevelLfoDepth));
}

// The loop the players get (their cached loop events, player +0x6c, and
// byte 0 bit 0): the region in samples, at least 16 long as on the XT; One
// Shot leaves the Loop switch without effect (loop modes 0).
XtLoop XtSynth::waveLoop(const OscVoice& s, const OscControls& o) const {
    XtLoop l;
    const int last = (int)s.wave->frames - 1;
    if (last < 16) return l;
    int a = (int)std::lround((double)std::clamp(o.loopStart, 0.0f, 1.0f) * last);
    int b = (int)std::lround((double)std::clamp(o.loopEnd, 0.0f, 1.0f) * last);
    if (b < a) std::swap(a, b);
    if (b - a < 16) { b = std::min(a + 16, last); a = b - 16; }
    l.on = o.loop;
    l.fwdMode = l.bwdMode = std::clamp(o.loopMode, 0, 2);
    l.fwdStart = l.bwdStart = a;
    l.fwdEnd = l.bwdEnd = b;
    return l;
}

XtEnvParams XtSynth::ampEnvParams(const Voice& v, const OscControls& o) const {
    XtEnvKnobs k;
    k.tempo10 = v.tempo10;
    k.attack = o.oAttack; k.decay = o.oDecay; k.sustain = o.oSustain; k.release = o.oRelease;
    k.aSens = o.oASens; k.dSens = o.oDSens; k.rSens = o.oRSens; k.timeKF = o.oTimeKF;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel, 2);
}

void XtSynth::applyRates(OscVoice& s) {
    if (s.wave->type == 1) { s.solo.setRates(s.curA, s.curF, s.curWalk); s.solo.setEnergy(s.curG); s.solo.setGain(s.curGain); }
    else { s.mb.setRates(s.curA, s.curWalk); s.mb.setGainXt(s.curGain); }
}

XtLfoParams XtSynth::lfoParams(const OscControls& o) const {
    return xtLfoConvert(o.lfoWave, o.lfoRate, o.lfoOffset, o.lfoDelay, o.lfoFadeMode, o.lfoFade, o.lfoKeySync);
}

XtLfoParams XtSynth::clfoParams(int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    return xtLfoConvert(c.lfoWave, c.lfoRate, c.lfoOffset, c.lfoDelay, c.lfoFadeMode, c.lfoFade, c.lfoKeySync);
}

XtLfoParams XtSynth::tlfoParams() const {
    return xtLfoConvert(ctl.tvaLfoWave, ctl.tvaLfoRate, ctl.tvaLfoOffset, ctl.tvaLfoDelay, ctl.tvaLfoFadeMode, ctl.tvaLfoFade, ctl.tvaLfoKeySync);
}

XtTvaBlock XtSynth::tvaBlock(const Voice& v) const {
    XtTvaKnobs k;
    k.on = ctl.tvaOn;
    k.level = ctl.tvaLevel; k.levelKF = ctl.tvaLevelKF; k.levelLfo = ctl.tvaLevelLfo;
    k.pan = ctl.tvaPan; k.panKF = ctl.tvaPanKF; k.panLfo = ctl.tvaPanLfo;
    k.send = ctl.mfxSend;
    k.cc7 = cc7; k.cc11 = cc11; k.cc10 = cc10;
    return xtTvaBlock(k, v.note, v.velGain);
}

// A COSM envelope (ADSR kinds 8..11, curve 2): its times as the other
// envelopes'; its Velocity Curve and Sens scale the COSM's depth instead.
XtEnvParams XtSynth::cosmEnvParams(const Voice& v, const Controls::CosmEnv& e) const {
    XtEnvKnobs k;
    k.tempo10 = v.tempo10;
    k.attack = e.attack; k.decay = e.decay; k.sustain = e.sustain; k.release = e.release;
    k.aSens = e.aSens; k.dSens = e.dSens; k.rSens = e.rSens; k.timeKF = e.timeKF;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel, 2);
}

// A TVF's block (0x894dd870) at the voice's key, with the velocity factors
// of its envelopes (A: the Cutoff's depth, Env 9 or 11; B: the Q's, Env 10
// or 12).
XtTvfBlock XtSynth::cosmBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtTvfParams p;
    p.filter = c.filter; p.q = c.reso; p.qLfo = c.resoLfo; p.qEnv = c.resoEnv;
    p.cutoff = c.cutoff; p.keyfollow = c.kf / 10; p.cutoffLfo = c.cutoffLfo; p.cutoffEnv = c.cutoffEnv;
    p.qOwn = c.resoOwn; p.cutoffOwn = c.cutoffOwn;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return v.cs[k].type == 1 ? xtDtvfBlock(p, v.note, va, vb) : xtTvfBlock(p, v.note, va, vb);
}

// A Dual Filter's block (0x894dd690): Filter 1's Env depth by A, Filter 2's by B.
XtDualBlock XtSynth::dualBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtDualParams p;
    p.type = c.dualType;
    p.freq1 = c.freq1; p.kf1 = c.kf1 / 10; p.q1 = c.q1; p.lfo1 = c.lfo1; p.env1 = c.env1;
    p.freq2 = c.freq2; p.kf2 = c.kf2 / 10; p.q2 = c.q2; p.lfo2 = c.lfo2; p.env2 = c.env2;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtDualBlock(p, v.note, va, vb);
}

// A TB Filter's block (0x894de110): the LPF's Env depth by A, the HPF's by B.
XtTbBlock XtSynth::tbBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtTbParams p;
    p.lpfCutoff = c.tbCutoff; p.lpfKf = c.tbKf / 10; p.lpfLfo = c.tbLfo; p.lpfEnv = c.tbEnv; p.q = c.tbQ; p.qFollow = c.tbQFollow;
    p.hpfCutoff = c.tbHpf; p.hpfKf = c.tbHpfKf / 10; p.hpfLfo = c.tbHpfLfo; p.hpfEnv = c.tbHpfEnv; p.gain = c.tbGain;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtTbBlock(p, v.note, va, vb);
}

// A Comb Filter's block (0x894dd4f0): Tone's Env depth by A, Detune's by B.
XtCombBlock XtSynth::combBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtCombParams p;
    p.octave = c.combOctave; p.detune = c.combDetune; p.detuneLfo = c.combDetuneLfo; p.detuneEnv = c.combDetuneEnv;
    p.tone = c.combTone; p.toneLfo = c.combToneLfo; p.toneEnv = c.combToneEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtCombBlock(p, va, vb);
}

// A Side Band Filter's block (0x894dd0a0, 0x894dd2c0): Width's Env depth by
// A, Detune's by B.
XtSbfBlock XtSynth::sbfBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    const bool second = v.cs[k].type == 6;
    const Controls::Cosm::SideBand& s = c.sb[second ? 1 : 0];
    XtSbfParams p;
    p.second = second;
    p.hpf = s.hpf; p.q = s.q; p.lpf = s.lpf; p.lpfKf = s.lpfKf / 10; p.subDetune = s.subDetune; p.octave = s.octave;
    p.detune = s.detune; p.detuneLfo = s.detuneLfo; p.detuneOwn = s.detuneOwn; p.detuneEnv = s.detuneEnv;
    p.width = s.width; p.widthKf = s.widthKf / 10; p.widthLfo = s.widthLfo; p.widthOwn = s.widthOwn; p.widthEnv = s.widthEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtSbfBlock(p, v.note, va, vb);
}

// A Resonator's block (0x894dcf00): Size's Env depth by A, Balance's by B.
XtResBlock XtSynth::resBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtResParams p;
    p.type = c.resType; p.size = c.resSize; p.sizeKf = c.resSizeKf / 10; p.sizeLfo = c.resSizeLfo; p.sizeEnv = c.resSizeEnv;
    p.balance = c.resBalance; p.balanceLfo = c.resBalanceLfo; p.balanceEnv = c.resBalanceEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtResBlock(p, v.note, va, vb);
}

// A Frequency Shifter's block (0x894ddde0): Depth's Env depth by A,
// Balance's by B.
XtFsBlock XtSynth::fsBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtFsParams p;
    p.depth = c.fsDepth; p.depthKf = c.fsDepthKf / 10; p.depthLfo = c.fsDepthLfo; p.depthEnv = c.fsDepthEnv;
    p.balance = c.fsBalance; p.balanceLfo = c.fsBalanceLfo; p.balanceEnv = c.fsBalanceEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtFsBlock(p, v.note, va, vb);
}

// A Lo-Fi block (0x894ddf80): the Filter's Env depth by A, Balance's by B.
XtLofiBlock XtSynth::lofiBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtLofiParams p;
    p.bitDown = c.lofiBit; p.rateDown = c.lofiRate; p.filter = c.lofiFilter; p.filterKf = c.lofiFilterKf / 10;
    p.filterLfo = c.lofiFilterLfo; p.filterEnv = c.lofiFilterEnv;
    p.balance = c.lofiBalance; p.balanceLfo = c.lofiBalanceLfo; p.balanceEnv = c.lofiBalanceEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtLofiBlock(p, v.note, va, vb);
}

// A Compressor's block (0x894ddc50).
XtCompBlock XtSynth::compBlock(int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtCompParams p;
    p.attack = c.compAttack; p.sustain = c.compSustain; p.level = c.compLevel;
    return xtCompBlock(p);
}

// A Wave Shape block (0x894dcbf0): Drive's Env depth by A, Tone's by B.
XtWsBlock XtSynth::wsBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtWsParams p;
    p.type = c.wsType; p.level = c.wsLevel; p.drive = c.wsDrive; p.driveLfo = c.wsDriveLfo; p.driveEnv = c.wsDriveEnv;
    p.tone = c.wsTone; p.toneKf = c.wsToneKf / 10; p.toneLfo = c.wsToneLfo; p.toneEnv = c.wsToneEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtWsBlock(p, v.note, va, vb);
}

// An OD/Dist block (0x894dc8b0): Drive's Env depth by A, Tone's by B.
XtOdBlock XtSynth::odBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtOdParams p;
    p.type = c.odType; p.level = c.odLevel; p.drive = c.odDrive; p.driveLfo = c.odDriveLfo; p.driveEnv = c.odDriveEnv;
    p.tone = c.odTone; p.toneKf = c.odToneKf / 10; p.toneLfo = c.odToneLfo; p.toneEnv = c.odToneEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtOdBlock(p, v.note, va, vb);
}

// An Amp Simulator block (0x894dcd80): Gain's Env depth by A.
XtAmpBlock XtSynth::ampBlock(const Voice& v, int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtAmpParams p;
    p.type = c.ampType; p.bass = c.ampBass; p.middle = c.ampMiddle; p.treble = c.ampTreble; p.level = c.ampLevel;
    p.gain = c.ampGain; p.gainLfo = c.ampGainLfo; p.gainEnv = c.ampGainEnv;
    return xtAmpBlock(p, xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve));
}

// A Limiter's block (0x894ddd20).
XtLimBlock XtSynth::limBlock(int k) const {
    const Controls::Cosm& c = ctl.cosm[k];
    XtLimParams p;
    p.threshold = c.limThreshold; p.ratio = c.limRatio; p.attack = c.limAttack; p.release = c.limRelease; p.level = c.limLevel;
    return xtLimBlock(p);
}

// The TVF's slot 1 is its Q (envB), the Dual Filter's Filter 1, the TB
// Filter's LPF, the Resonator's Size, the Frequency Shifter's Depth, the
// Lo-Fi Filter, Wave Shape's and OD/Dist's Drive and the Amp's Gain (envA),
// the Comb and Side Band
// Filters' Detune (envB).
void XtSynth::cosmSetBlock(Voice& v, int k) {
    Voice::CosmVoice& c = v.cs[k];
    if (c.type == 4) { c.comb.setBlock(combBlock(v, k), c.heldB, c.heldLfo, c.heldA, c.heldLfo); return; }
    if (c.type == 5 || c.type == 6) { c.sbf.setBlock(sbfBlock(v, k), c.heldB, c.heldLfo, c.heldA, c.heldLfo); return; }
    if (c.type == 7) { c.res.setBlock(resBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 8) { c.fs.setBlock(fsBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 9) { c.lofi.setBlock(lofiBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 10) { c.comp.setBlock(compBlock(k)); return; }
    if (c.type == 11) { c.lim.setBlock(limBlock(k)); return; }
    if (c.type == 12) { c.ws.setBlock(wsBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 13) { c.od.setBlock(odBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 14) { c.amp.setBlock(ampBlock(v, k), c.heldA, c.heldLfo); return; }
    if (c.type == 15) { c.spk.setBlock({ ctl.cosm[k].spkType }); return; }
    if (c.type == 2) c.dual.setBlock(dualBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo);
    else if (c.type == 3) c.tb.setBlock(tbBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo);
    else c.tvf.setBlock(cosmBlock(v, k), c.heldB, c.heldLfo, c.heldA, c.heldLfo);
}

// The LFO constructor (0x894f0b40) seeds each LFO's chaos state from a global
// logistic map (0x894f4cd0, then |2x − 1| by 0x894f4ca0).  A tone is constructed
// for each note, all eight of its LFOs in order, whichever are used.
float XtSynth::drawSeed() {
    using namespace xtf;
    chaosSeed = mul(mul(sub(1.0f, chaosSeed), chaosSeed), 0x1.ff9db0p+1f);
    return std::fabs(add(mul(chaosSeed, 2.0f), -1.0f));
}

XtEnvParams XtSynth::oscEnvParams(const Voice& v, int a, int d, int s, int r) const {
    XtEnvKnobs k;
    k.tempo10 = v.tempo10;
    k.attack = a; k.decay = d; k.sustain = s; k.release = r;
    return xtEnvAdsr(k, v.note, v.vel, v.relVel, 0);
}

XtEnvParams XtSynth::envParams(const Voice& v) const {
    XtEnvKnobs k;
    k.tempo10 = v.tempo10;
    k.attack = ctl.attack; k.decay = ctl.decay; k.sustain = ctl.sustain; k.release = ctl.release;
    k.aSens = ctl.aSens; k.dSens = ctl.dSens; k.rSens = ctl.rSens; k.timeKF = ctl.timeKF;
    XtEnvParams p = xtEnvAdsr(k, v.note, v.vel, v.relVel);
    if (!ctl.tvaOn) {                                  // the TVA Switch off: no times, level 1
        for (float& t : p.time) t = 0.0f;
        p.level[3] = 1.0f;
    }
    return p;
}

// New knob values reach a playing voice at the next update (mixBlock),
// through the voice update's smoothing, as on the XT.
void XtSynth::setControls(const Controls& c) {
    const bool tvaChanged = c.tvaOn != ctl.tvaOn;
    const bool envChanged = tvaChanged || c.attack != ctl.attack || c.decay != ctl.decay || c.sustain != ctl.sustain || c.release != ctl.release
                         || c.aSens != ctl.aSens || c.dSens != ctl.dSens || c.rSens != ctl.rSens || c.timeKF != ctl.timeKF;
    // what changed for each OSC.  The tone update also gives the voices' pitch
    // ramps the new portamento settings (0x894ebd34 → 0x894f48b0) and, when the
    // key-follow key moved (Coarse, Pitch KF), glides again from the voice's
    // source key (0x894f2600)
    struct OscDiff { bool penv, tenv, fenv, oenv, key; };
    auto oscDiff = [&](const OscControls& n, const OscControls& o) {
        OscDiff d;
        d.penv = n.pAttack != o.pAttack || n.pDecay != o.pDecay || n.pSustain != o.pSustain || n.pRelease != o.pRelease;
        d.tenv = n.tAttack != o.tAttack || n.tDecay != o.tDecay || n.tSustain != o.tSustain || n.tRelease != o.tRelease;
        d.fenv = n.fAttack != o.fAttack || n.fDecay != o.fDecay || n.fSustain != o.fSustain || n.fRelease != o.fRelease;
        d.oenv = n.oAttack != o.oAttack || n.oDecay != o.oDecay || n.oSustain != o.oSustain || n.oRelease != o.oRelease
              || n.oASens != o.oASens || n.oDSens != o.oDSens || n.oRSens != o.oRSens || n.oTimeKF != o.oTimeKF;
        d.key = n.oscCoarse != o.oscCoarse || c.patchCoarse != ctl.patchCoarse || n.pitchKF != o.pitchKF || c.pitchSemis != ctl.pitchSemis;
        return d;
    };
    const OscDiff d1 = oscDiff(c, ctl), d2 = oscDiff(c.osc2, ctl.osc2);
    const bool portaChanged = c.portamento != ctl.portamento || c.portaTime != ctl.portaTime || c.portaVeloSens != ctl.portaVeloSens
                           || c.portaType != ctl.portaType;
    // Any change is a tone update (0x894dc0b0), and it gives the sounding voices' COSM
    // sections (0x894ed8f0) and MOD (0x894f1c60) a new block, whatever moved: a block's
    // targets take the envelopes' levels without their rates, and the MOD's clears an
    // old note's sync flag, so it is heard even with no COSM or MOD knob moved.
    // Their envelopes new settings.
    auto envDiff = [](const Controls::CosmEnv& a, const Controls::CosmEnv& b) {
        return a.attack != b.attack || a.decay != b.decay || a.sustain != b.sustain || a.release != b.release || a.aSens != b.aSens
            || a.dSens != b.dSens || a.rSens != b.rSens || a.timeKF != b.timeKF;
    };
    const bool anyChanged = !(c == ctl);
    bool cenvAChanged[2], cenvBChanged[2];
    for (int k = 0; k < 2; ++k) {
        const Controls::Cosm &n = c.cosm[k], &o = ctl.cosm[k];
        cenvAChanged[k] = envDiff(n.envA, o.envA);
        cenvBChanged[k] = envDiff(n.envB, o.envB);
    }
    // the MOD's converter
    const bool modChanged = c.mod.on != ctl.mod.on || c.mod.type != ctl.mod.type || c.mod.level != ctl.mod.level || c.mod.attack != ctl.mod.attack
                         || c.mod.release != ctl.mod.release;
    ctl = c;
    clockFollow();
    if (modChanged) xtModBlock(ctl.mod, modBlk);
    for (Voice& v : voice) {
        if (!v.active) continue;
        if (anyChanged && ctl.on) v.blockTempo = tempoNow();
        if (envChanged) v.env.update(envParams(v));
        if (anyChanged) v.tb = tvaBlock(v);            // the velocity as latched
        auto oscLive = [&](OscVoice& s, const OscControls& o, const OscDiff& d) {
            if (d.penv) s.penv.update(oscEnvParams(v, o.pAttack, o.pDecay, o.pSustain, o.pRelease));
            if (d.tenv) s.tenv.update(oscEnvParams(v, o.tAttack, o.tDecay, o.tSustain, o.tRelease));
            if (d.fenv) s.fenv.update(oscEnvParams(v, o.fAttack, o.fDecay, o.fSustain, o.fRelease));
            if (d.oenv) s.oenv.update(ampEnvParams(v, o));
            if (portaChanged) s.porta.update(xtPortaSettings(ctl.portamento, ctl.portaType, ctl.portaTime, ctl.portaVeloSens, v.vel));
            if (d.key) {
                const int k = keyFollowKey(v, o);
                if (k != s.kfKey) {
                    s.kfKey = k;
                    s.porta.restart(noteCents(v, s, o, v.srcKey), noteCents(v, s, o, v.note));
                }
            }
        };
        if (v.on) oscLive(v, ctl, d1);
        if (v.osc2.on) oscLive(v.osc2, ctl.osc2, d2);
        if (anyChanged) { oscBeat(v, ctl); oscBeat(v.osc2, ctl.osc2); }
        if (anyChanged) v.mod.setBlock(modBlk);
        for (int k = 0; k < 2; ++k) {
            Voice::CosmVoice& cv = v.cs[k];
            if (!cv.on) continue;
            // the block first (0x894dba00: 0x894f5d10, then 0x894f5cc0), on the held levels:
            // a Sustain moved reaches it at the next tick, however many updates come first
            if (anyChanged) cosmSetBlock(v, k);
            if (cenvAChanged[k]) cv.envA.update(cosmEnvParams(v, ctl.cosm[k].envA));
            if (cenvBChanged[k]) cv.envB.update(cosmEnvParams(v, ctl.cosm[k].envB));
        }
        if (anyChanged) lfoUpdates(v);
    }
}

int XtSynth::shiftKey(int key) const {
    return std::clamp(key + 12 * ctl.octave, 0, 127);
}

int XtSynth::keyFollowKey(const Voice& v, const OscControls& o) const {
    const long shift = std::lround((double)ctl.pitchSemis * 100.0);
    XtTuning t;
    t.coarse = ctl.patchCoarse + (int)std::lround((double)shift / 100.0);
    return xtKeyFollowKey(v.note, xtNoteCoarse(t, o.oscCoarse), 10 * o.pitchKF);
}

// The part's note-on (0x894dbcb0): the key state moves on (0x894d6330),
// the key takes the Octave Shift (and the system's Transpose, 0 here).
// POLY: a new voice.  MONO with a key already held and Legato on: the
// sounding voice takes the note (0x894d4e00).  MONO otherwise: the part's
// voices are truncated (0x894d4a80) and a new one starts.
void XtSynth::noteOn(int note, float velocity) {
    pickUpSample();
    if (!canPlay()) return;
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
    xtModBlock(ctl.mod, modBlk);                       // the tables may have loaded since the last knob moved
    // portamento's source key (0x894e4940): the previous key plus the Octave
    // Shift; with LEGATO the note's own (no glide) unless a key was held
    // then (POLY: or Hold is down, not modelled)
    const bool held = ctl.mono ? keys.prevMonoHeld : keys.prevPolyHeld;
    const int srcKey = ctl.portaMode == 1 && !held ? note : shiftKey(ctl.mono ? keys.prevMono : keys.prevPoly);
    Voice* v = nullptr;
    for (Voice& w : voice) if (!w.active) { v = &w; break; }
    if (!v) { v = &voice[0]; for (Voice& w : voice) if (w.age < v->age) v = &w; }   // steal the oldest
    // the voice's stop (0x895053c0) frees FEEDBACK-OSC's line; only a note-on can
    // tell when it happened
    for (Voice& w : voice) if (!w.active) { w.ana.release(); w.osc2.ana.release(); }
    v->ana.release();
    v->osc2.ana.release();
    v->active = true;
    v->releasing = false;
    v->damping = false;
    v->note = note;
    v->raw = raw;
    v->srcKey = srcKey;
    v->age = clock++;
    v->structure = std::clamp(ctl.structure, 0, 2);
    v->vel = vel;
    v->relVel = 64;
    v->tempo10 = tempo10Now();
    if (ctl.on) v->blockTempo = tempoNow();
    v->on = oscPlays(ctl);
    v->osc2.on = oscPlays(ctl.osc2);
    v->analog = ctl.oscType == 0;
    v->osc2.analog = ctl.osc2.oscType == 0;
    v->wave = waveOf(ctl);
    v->osc2.wave = waveOf(ctl.osc2);
    v->robot = ctl.robot && v->on && !v->analog && v->wave->type == 1;   // SOLO only (the converter's SOLO tail)
    v->osc2.robot = ctl.osc2.robot && v->osc2.on && !v->osc2.analog && v->osc2.wave->type == 1;
    // the OSCs' random pitch (0x894e4940, OSC 1 then 2, each whatever its
    // Switch): the shared generator steps once per note for an OSC with a depth
    v->rnd = pitchRandom.next(xtRandomPitchDepth(ctl.randomPitch));
    v->osc2.rnd = pitchRandom.next(xtRandomPitchDepth(ctl.osc2.randomPitch));
    v->velGain = xtVelGain(v->vel, ctl.ampVelSens, ctl.ampVelCurve);
    v->tb = tvaBlock(*v);
    // the tone is constructed for each note (0x894f5860): LFOs 0…7 draw their
    // chaos seeds, and the pitch ramps (0x894f4860) start off, so the note's
    // first pitch has no glide in it
    float seed[8];
    for (float& x : seed) x = drawSeed();
    v->porta = XtPitchRamp{};
    v->osc2.porta = XtPitchRamp{};
    using namespace xtf;
    const XtEnvParams ep = envParams(*v);
    v->env.noteOn(ep);
    // The note-on converters set the player directly (0x894f3a00…)
    // a key-unsynced LFO starts from the newest other tone of the part sounding, if
    // there is one (the tones' lists at 0x89f91c50/54, newest first), else from its
    // free-running phase
    const Voice* other = nullptr;
    for (const Voice& w : voice)
        if (&w != v && w.active && (!other || w.age > other->age)) other = &w;
    oscLfoNoteOn(*v, *v, ctl, freeLfo, other ? &other->lfo : nullptr, seed[1]);
    oscLfoNoteOn(*v, v->osc2, ctl.osc2, freeLfo2, other ? &other->osc2.lfo : nullptr, seed[2]);
    if (ctl.tvaLfoKeySync) v->tlfo.noteOn(tlfoParams(), seed[5], v->blockTempo);
    else if (other) v->tlfo.noteOnFollow(tlfoParams(), other->tlfo);
    else v->tlfo.noteOnFree(tlfoParams(), seed[5], freeTlfo, v->blockTempo);
    // TVA note-on (0x894f50e0), after the LFOs: the gain starts from the level times the
    // envelope's onset level (full with no attack, else 0) and ramps to the level
    // times its output over the attack time, at most 10 ms (0.1 ms with no attack), each
    // with the LFO on the level and the pan.  The output bus's level starts where
    // the block has it.
    tvaGains(*v, mul(v->env.onsetLevel(), v->tb.level), v->cur);
    tvaGains(*v, mul(v->env.level(), v->tb.level), v->target);
    const float ms = ep.time[0] == 0.0f ? (ep.time[1] == 0.0f ? kNoAttackMs : ep.time[1]) : (10.0f > ep.time[0] ? ep.time[0] : 10.0f);
    const float frames = mul(ms, kFramesPerMs);
    v->step[0] = div(sub(v->target[0], v->cur[0]), frames);
    v->step[1] = div(sub(v->target[1], v->cur[1]), frames);
    v->send[0] = v->tb.sendL;
    v->send[1] = v->tb.sendR;
    v->elapsed = 0;
    if (v->on) oscEnvNoteOn(*v, *v, ctl);
    if (v->osc2.on) oscEnvNoteOn(*v, v->osc2, ctl.osc2);
    // the COSM sections (0x894edf70, COSM 1 then 2): their
    // envelopes and LFOs start with the others; the filters' states clear and
    // their coefficients start from the envelopes' start levels
    // the voices' stereo flags as their setups leave them: a stereo wave's on a PCM OSC, and
    // none on an analog voice (0x89505170 clears it) or an OSC that is off
    auto stereo = [](const OscVoice& s) { return s.on && !s.analog && s.wave->channels == 2; };
    for (int k = 0; k < 2; ++k) {
        Voice::CosmVoice& c = v->cs[k];
        c.holdWords();
        c.on = ctl.cosm[k].on;
        c.stereo = stereo(xtCosmOsc(v->structure, k) ? v->osc2 : *v);   // the routines' flag (0x894eef70)
        if (ctl.cosm[k].lfoKeySync) c.lfo.noteOn(clfoParams(k), seed[3 + k], v->blockTempo);
        else if (other) c.lfo.noteOnFollow(clfoParams(k), other->cs[k].lfo);
        else c.lfo.noteOnFree(clfoParams(k), seed[3 + k], freeClfo[k], v->blockTempo);
        if (!c.on) continue;
        c.type = ctl.cosm[k].type;
        c.envA.noteOn(cosmEnvParams(*v, ctl.cosm[k].envA));
        c.envB.noteOn(cosmEnvParams(*v, ctl.cosm[k].envB));
        c.hold();
        if (c.type == 2)
            c.dual.noteOn(dualBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                          c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 3) {
            c.tb.noteOn(tbBlock(*v, k), c.envA.level(), c.lfo.out(), c.envB.level(), c.lfo.out());
            c.tb.setHpWords(c.word8c, c.word90);
        }
        else if (c.type == 7)
            c.res.noteOn(resBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                         c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 8)
            c.fs.noteOn(fsBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                        c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 9)
            c.lofi.noteOn(lofiBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                          c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 10)
            c.comp.noteOn(compBlock(k));
        else if (c.type == 11)
            c.lim.noteOn(limBlock(k));
        else if (c.type == 12)
            c.ws.noteOn(wsBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                        c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 13)
            c.od.noteOn(odBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                        c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 14)
            c.amp.noteOn(ampBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
        else if (c.type == 15)
            c.spk.noteOn({ ctl.cosm[k].spkType });
        else if (c.type >= 4)
            ;                                          // below, once the voice's pitch is set
        else
            c.tvf.noteOn(cosmBlock(*v, k), c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out(),
                         c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
    }
    if (v->on) targets(*v, *v, ctl, v->curA, v->curF, v->curWalk, v->curG);
    else v->curA = 1.0f;                               // the note-on of an OSC that is off (0x894f2f80, kind 0)
    if (v->osc2.on) targets(*v, v->osc2, ctl.osc2, v->osc2.curA, v->osc2.curF, v->osc2.curWalk, v->osc2.curG);
    else v->osc2.curA = 1.0f;
    // the Comb and Side Band Filters' note-ons read the voice's pitch ratio
    // (+0x144) and root frequency (+0x9a4) as the voice's own note-on leaves
    // them (the tone note-on 0x894f5e60 runs the voice first): OSC 1's, also with
    // OSC 2 sounding, and for COSM 2 in the third Structure OSC 2's (xtCosmOsc); with
    // that OSC off a ratio of 1 and a root of 440
    for (int k = 0; k < 2; ++k) {
        Voice::CosmVoice& c = v->cs[k];
        if (!c.on || c.type < 4 || c.type > 6) continue;
        const bool second = xtCosmOsc(v->structure, k) != 0;
        const OscVoice& s = second ? v->osc2 : *v;
        const float root = xtCosmRootHz(second ? ctl.osc2.rootKey : ctl.rootKey, s.robot || s.analog || !s.on);   // the analog voice's root is 440 too (0x894f21c0)
        if (c.type == 4)
            c.comb.noteOn(combBlock(*v, k), s.curA, root, c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(),
                          c.lfo.out(), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
        else
            c.sbf.noteOn(sbfBlock(*v, k), s.curA, root, c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(),
                         c.lfo.out(), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
    }
    // the voices' note-ons, OSC 1 then OSC 2 (0x894f21c0): SUPER-SAW's counter
    // and FEEDBACK-OSC's lines go in that order
    if (v->on) oscStart(*v, *v, ctl);
    if (v->osc2.on) oscStart(*v, v->osc2, ctl.osc2);
    // portamento (0x894f2440, after the voice setup): the pitch ramp from the
    // source key's cents to the note's; it moves from the next update on
    auto portaStart = [&](OscVoice& s, const OscControls& o) {
        s.kfKey = keyFollowKey(*v, o);
        s.porta.start(noteCents(*v, s, o, srcKey), noteCents(*v, s, o, note),
                      xtPortaSettings(ctl.portamento, ctl.portaType, ctl.portaTime, ctl.portaVeloSens, v->vel));
    };
    if (v->on) portaStart(*v, ctl);
    if (v->osc2.on) portaStart(v->osc2, ctl.osc2);
    // the MOD (0x894f1b00, 0x894f1d90)
    v->mod.noteOn(modBlk, stereo(*v), stereo(v->osc2), v->on && v->analog, modShared, (int)(v - voice));
}

// An OSC's LFO at note-on, whatever its Switch: every LFO of the tone starts
// (0x894f5e60), and a key-unsynced one sets its free-running phase's rate.
void XtSynth::oscLfoNoteOn(Voice& v, OscVoice& s, const OscControls& o, XtLfoFree& free, const XtLfo* other, float seed) {
    if (o.lfoKeySync) s.lfo.noteOn(lfoParams(o), seed, v.blockTempo);
    else if (other) s.lfo.noteOnFollow(lfoParams(o), *other);
    else s.lfo.noteOnFree(lfoParams(o), seed, free, v.blockTempo);
}

// An OSC's envelopes at note-on.
void XtSynth::oscEnvNoteOn(Voice& v, OscVoice& s, const OscControls& o) {
    s.penv.noteOn(oscEnvParams(v, o.pAttack, o.pDecay, o.pSustain, o.pRelease));
    s.tenv.noteOn(oscEnvParams(v, o.tAttack, o.tDecay, o.tSustain, o.tRelease));
    s.fenv.noteOn(oscEnvParams(v, o.fAttack, o.fDecay, o.fSustain, o.fRelease));
    s.oenv.noteOn(ampEnvParams(v, o));
}

// An OSC's voice at note-on.  The OSC gain starts from the amp envelope's
// start level, or its output when there is no attack (0x894f3e20), and goes
// to the envelope's output (0x894f3dc0).
void XtSynth::oscStart(Voice& v, OscVoice& s, const OscControls& o) {
    const XtEnvParams oe = ampEnvParams(v, o);
    const float gainStart = gainTarget(v, s, o, oe.time[0] == 0.0f ? s.oenv.level() : oe.level[0]);   // +0x158 (0x894f3e20)
    s.curGain = gainTarget(v, s, o, s.oenv.level());                                                  // +0x154 (0x894f3dc0)
    s.chase = false;                                                                                  // +0x9b0 (0x894f21e6)
    // the player's tempo (0x894f3110): this OSC's block +0x60 × 10000, against the wave's, and
    // the bits as the block has them, the pad's if it is held
    const uint32_t tempoWord = (uint32_t)xtf::ftrc(xtf::mul(tempoNow(), 10000.0f));
    const bool tripNow = o.timeTrip && pad.on();
    if (s.analog) {   // the analog note-on (0x89504af0): Fat on the formant envelope, the Pulse Width on the time envelope
        XtAnalogBlock b = analogBlock(v, s, o);
        if (&s == &v.osc2 && modBlk.type == 4 && b.code == 10) b.code = 2;   // the tone's note-on (0x894f6148): under OSC SYNC OSC 2 plays LA-SAW as SAW
        s.ana.noteOn(b, s.curA, s.curGain, gainStart, s.fenv.level(), s.tenv.level(), s.lfo.out(), anaShared);
    }
    else if (s.wave->type == 1) {
        s.solo.setRobot(s.robot);
        s.solo.setLoop(waveLoop(s, o));
        s.solo.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.solo.setBeatFlags(o.tempoSync, tripNow);
        s.solo.noteOn(s.wave->solo, s.curA, s.curF, s.curWalk, s.curG);
        s.solo.resetGain(gainStart);
        s.solo.setGain(s.curGain);
    }
    else {
        s.mb.setLoop(waveLoop(s, o));
        s.mb.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.mb.setBeatFlags(o.tempoSync, tripNow);
        if (s.mb.noteOn(s.wave->wave, s.wave->type, s.curA, s.curWalk)) {
            s.mb.setGainXt(gainStart, true);
            s.mb.setGainXt(s.curGain);
        } else {
            s.on = false;
            if (!v.on && !v.osc2.on) v.active = false;
        }
    }
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
        v.osc2.rnd = pitchRandom.next(xtRandomPitchDepth(ctl.osc2.randomPitch));
        v.velGain = xtVelGain(v.vel, ctl.ampVelSens, ctl.ampVelCurve);
        v.tb = tvaBlock(v);
        if (ctl.on) v.blockTempo = tempoNow();
        v.env.update(envParams(v));
        auto oscUpdate = [&](OscVoice& s, const OscControls& o) {
            s.penv.update(oscEnvParams(v, o.pAttack, o.pDecay, o.pSustain, o.pRelease));
            s.tenv.update(oscEnvParams(v, o.tAttack, o.tDecay, o.tSustain, o.tRelease));
            s.fenv.update(oscEnvParams(v, o.fAttack, o.fDecay, o.fSustain, o.fRelease));
            s.oenv.update(ampEnvParams(v, o));
        };
        if (v.on) oscUpdate(v, ctl);
        if (v.osc2.on) oscUpdate(v.osc2, ctl.osc2);
        oscBeat(v, ctl);
        oscBeat(v.osc2, ctl.osc2);
        v.mod.setBlock(modBlk);
        for (int k = 0; k < 2; ++k) {
            if (!v.cs[k].on) continue;
            cosmSetBlock(v, k);                        // as in setControls, before the envelopes' update
            v.cs[k].envA.update(cosmEnvParams(v, ctl.cosm[k].envA));
            v.cs[k].envB.update(cosmEnvParams(v, ctl.cosm[k].envB));
        }
        lfoUpdates(v);
        auto oscGlide = [&](OscVoice& s, const OscControls& o) {
            s.porta.update(xtPortaSettings(ctl.portamento, ctl.portaType, ctl.portaTime, ctl.portaVeloSens, v.vel));
            const int k = keyFollowKey(v, o);
            if (k != s.kfKey) {
                s.kfKey = k;
                s.porta.restart(noteCents(v, s, o, v.srcKey), noteCents(v, s, o, v.note));
            }
        };
        if (v.on) oscGlide(v, ctl);
        if (v.osc2.on) oscGlide(v.osc2, ctl.osc2);
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
            v.tlfo.noteOff();
            auto oscRelease = [&](OscVoice& s, const OscControls& o) {
                s.penv.update(oscEnvParams(v, o.pAttack, o.pDecay, o.pSustain, o.pRelease));
                s.penv.noteOff();
                s.tenv.update(oscEnvParams(v, o.tAttack, o.tDecay, o.tSustain, o.tRelease));
                s.tenv.noteOff();
                s.fenv.update(oscEnvParams(v, o.fAttack, o.fDecay, o.fSustain, o.fRelease));
                s.fenv.noteOff();
                s.oenv.update(ampEnvParams(v, o));
                s.oenv.noteOff();
                s.lfo.noteOff();
            };
            if (v.on) oscRelease(v, ctl);
            if (v.osc2.on) oscRelease(v.osc2, ctl.osc2);
            for (int k = 0; k < 2; ++k) {
                Voice::CosmVoice& c = v.cs[k];
                if (!c.on) continue;
                c.envA.update(cosmEnvParams(v, ctl.cosm[k].envA));
                c.envA.noteOff();
                c.envB.update(cosmEnvParams(v, ctl.cosm[k].envB));
                c.envB.noteOff();
                c.lfo.noteOff();
            }
        }
}

void XtSynth::allNotesOff() {
    for (Voice& v : voice) v.active = false;
    for (uint16_t& w : keys.held) w = 0;
    keys.lastMonoHeld = keys.prevMonoHeld = keys.lastPolyHeld = keys.prevPolyHeld = false;
}

// The conversion pass only runs in the tone update (0x894dc0b0: the only caller of
// 0x894ebbd0, and of 0x894dba00), so a bender move reaches a sounding tone as an update:
// the COSMs and the MOD get a new block, as for any knob (setControls).
void XtSynth::pitchBend(int value) {
    const int b = std::clamp(value, 0, 16383);
    if (b == bender) return;
    bender = b;
    for (Voice& v : voice)
        if (v.active) newBlocks(v);
}

// The controllers the TVA's converter reads (0x894e9fe0, C + 0x84 + cc): as the bender,
// an update for the sounding voices.
void XtSynth::controller(int cc, int value) {
    int* const c = cc == 7 ? &cc7 : cc == 11 ? &cc11 : cc == 10 ? &cc10 : nullptr;
    const int x = std::clamp(value, 0, 127);
    if (!c || *c == x) return;
    *c = x;
    for (Voice& v : voice)
        if (v.active) newBlocks(v);
}

void XtSynth::newBlocks(Voice& v) {
    if (ctl.on) v.blockTempo = tempoNow();
    oscBeat(v, ctl);
    oscBeat(v.osc2, ctl.osc2);
    v.mod.setBlock(modBlk);
    v.tb = tvaBlock(v);
    for (int k = 0; k < 2; ++k)
        if (v.cs[k].on) cosmSetBlock(v, k);
    lfoUpdates(v);
}

// All a tone's LFOs take the blocks at any update, those whose OSC or COSM is off too.  For an LFO
// on a note value that is more than its own knobs: a key-unsynced note starts with the clock step
// of the tone it follows, and this sets its own.
void XtSynth::lfoUpdates(Voice& v) {
    v.lfo.update(lfoParams(ctl));
    v.osc2.lfo.update(lfoParams(ctl.osc2));
    for (int k = 0; k < 2; ++k) v.cs[k].lfo.update(clfoParams(k));
    v.tlfo.update(tlfoParams());
}

// The pad reaches a tone in two ways.  A step goes straight to its voices (0x894f4e10 →
// 0x894f7920 → 0x894f25a0), those with Time Trip on.  Touching and letting go are seen at the
// next tone update (input +0x63 and the object's +8, 0x894e477a), which for the sounding
// tones here is now.
void XtSynth::timeTrip(int x, int y) {
    const bool was = pad.on();
    int step = 0;
    pad.setX(x);
    const bool moved = pad.setY(y, step);
    if (moved)
        for (Voice& v : voice) {
            if (!v.active) continue;
            auto give = [&](OscVoice& s) {
                if (!s.on || s.analog) return;
                if (s.wave->type == 1) { if (s.solo.tripOn()) s.solo.setTrip(step); }
                else if (s.mb.tripOn()) s.mb.setTrip(step);
            };
            give(v);
            give(v.osc2);
        }
    if (pad.on() != was)
        for (Voice& v : voice)
            if (v.active) newBlocks(v);
}

void XtSynth::timeTripKnob(int value) {
    if (value <= 0) { timeTrip(0, 0); return; }
    const double a = -2.0 * 3.14159265358979323846 * (double)(std::min(value, 127) - 1) / 126.0;
    timeTrip(64 + (int)std::lround(63.0 * std::cos(a)), 64 + (int)std::lround(63.0 * std::sin(a)));
}

// 0x894f3ec0, a block's bits to a PCM OSC's player.  Letting go of the pad with Beat Keep on
// fades the player out, and the voice update stands aside until it is silent (0x894f20d2).
void XtSynth::oscBeat(OscVoice& s, const OscControls& o) {
    if (!s.on || s.analog) return;
    const bool sync = o.tempoSync, trip = o.timeTrip && pad.on();
    if (s.wave->type == 1) {
        if (o.beatKeep && s.solo.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.solo.setGain(0.0f); }
        s.solo.setBeatFlags(sync, trip);
    } else {
        if (o.beatKeep && s.mb.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.mb.setGainXt(0.0f); }
        s.mb.setBeatFlags(sync, trip);
    }
}

// The clock object at 0x89979bfc, from the Patch Tempo (0x89471ef0, its tenths dropped): the ms of
// a quarter, which is 480 ticks with its +0x10 at 20 (0x894714b0).  The converters read the tempo
// back from it (0x894e2d20), which is why 233 BPM plays at 233.4.
int XtSynth::clockPeriod() const {
    const int t = std::clamp(ctl.patchTempo, 200, 2500);
    return 3000 * 20 / (t / 10);
}

// The readers branch on the object's +0x40: from an outside clock, the ms over its last six clocks,
// which are a quarter of a quarter.  The limits are as they are: a reading under 20.0 BPM is 2.0, over 250.0 is 25.0.
int XtSynth::tempo10Now() const {
    const int t = clockExt ? 150000 / clockSix : 30000 * 20 / clockPeriod();
    return 200 > t ? 20 : t > 2500 ? 250 : t;
}

float XtSynth::tempoNow() const { return xtf::div(xtf::flt(tempo10Now()), 10.0f); }

namespace {
constexpr uint64_t kClockPerBlock = 32 * 24000;   // a block's frames × 24 clocks a quarter × 1000 ms; a clock is clockPeriod() × 44100
}

int XtSynth::clocksDue() const {
    if (clockExt) {
        const double d = std::floor(hostAt + hostStep()) - (double)hostDone;
        return d > 0.0 ? (int)d : 0;
    }
    return (int)((clockAcc + kClockPerBlock) / ((uint64_t)clockPeriod() * 44100));
}

int XtSynth::msDue() const { return (int)((clockMsAcc + 32000) / 44100); }

double XtSynth::hostStep() const { return hostBpm * (32.0 * 24.0) / (60.0 * 44100.0); }

// 0x89471e30, flag 0x40 of the clock's task.  Going to EXTERNAL the object starts over, reading 120 BPM until its first count of six is done.
void XtSynth::clockFollow() {
    const bool ext = ctl.clockSource != 0 && hostBpm > 0.0;
    if (ext == clockExt) return;
    clockExt = ext;
    if (!ext) return;
    clockSeen = -1;
    clockSince = 0;
    clockSix = 125;
    hostDone = (int64_t)std::ceil(hostAt) - 1;
}

// An F8 (0x894712f0).  The object's ticks go on to the next 20, which is what makes it a clock (0x894d4210);
// between F8s the ticks stop short of that (0x894710e0, +0x4a), so there is one clock to an F8.  Every sixth: the count.
void XtSynth::clockIn() {
    if (++clockSeen < 6) return;
    clockSeen = 0;
    clockSix = clockSince ? clockSince : 1;
    clockSince = 0;
}

// Each ms (0x894710e0's end).  The count starts at the first clock, and is given up after 65536 ms short of six.
void XtSynth::clockMs() {
    if (clockSeen < 0) return;
    if (clockSince++ >= 0xffff) {
        clockSeen = -1;
        clockSince = 0;
    }
}

void XtSynth::hostClock(double bpm, bool moving, double ppq) {
    if (bpm > 0.0) hostBpm = bpm;
    clockFollow();
    if (!moving || !(hostBpm > 0.0)) return;
    // Where the next block will sound: the frames between the read position and it, as this host block's first frame stands at ppq
    const double at = (ppq + ((double)srcFrames - readPos) / 44100.0 * hostBpm / 60.0) * 24.0;
    if (std::fabs(at - hostAt) > 1.0) hostDone = (int64_t)std::ceil(at) - 1;   // a locate or a loop's end: what it passed gives no clocks
    hostAt = at;
}

// 0x894f4da0: the time since the last clock from the blocks passed, the free-running LFOs on a
// note value (0x894f17c0), then each tone: its eight LFOs (0x894f78a0) and its voices (0x894f78f0).
void XtSynth::clockTick() {
    using namespace xtf;
    constexpr float kBlockSeconds = 0x1.7ca212p-11f;   // 0.000726 (0x3a3e5109)
    float dt = mul(flt((int32_t)(clockBlocks - clockLast)), kBlockSeconds);
    if (dt == 0.0f) dt = kBlockSeconds;
    clockLast = clockBlocks;
    freeLfo.clock(dt);
    freeLfo2.clock(dt);
    for (XtLfoFree& f : freeClfo) f.clock(dt);
    freeTlfo.clock(dt);
    for (Voice& v : voice) {
        if (!v.active) continue;
        auto oscClock = [&](OscVoice& s) {
            s.lfo.clock(dt);
            if (!s.on || s.analog) return;
            if (s.wave->type == 1) { if (s.solo.syncOn()) s.solo.addClock(1); }
            else if (s.mb.syncOn()) s.mb.addClock(1);
        };
        oscClock(v);
        oscClock(v.osc2);
        for (Voice::CosmVoice& c : v.cs) c.lfo.clock(dt);
        v.tlfo.clock(dt);
    }
}

// A gain or target (0x894f5360, and 0x894f50e0 at note-on): (1 + LFO·depth)·level·env
// times (1 ∓ LFO·pan depth), each channel within ±1.  With both depths 0 the
// factors are exactly 1.
void XtSynth::tvaGains(const Voice& v, float levelEnv, float* lr) const {
    using namespace xtf;
    const float x = mul(add(mul(v.tlfo.out(), v.tb.lfoDepth), 1.0f), levelEnv), p = mul(v.tlfo.out(), v.tb.panLfoDepth);
    lr[0] = std::clamp(mul(x, sub(1.0f, p)), -1.0f, 1.0f);
    lr[1] = std::clamp(mul(x, add(p, 1.0f)), -1.0f, 1.0f);
}

// The tone update's TVA part (0x894f75f0 tail): stop a finished voice
// (0x894f54b0), else aim at the envelope's new output with a linear ramp of
// 10 ms, shorter when the stage itself is (0x894f5360).
void XtSynth::tvaUpdate(Voice& v) {
    if (v.damping) return;                                    // done when the fade is (0x894f5510)
    using namespace xtf;
    if (v.env.done() && v.cur[0] <= kSilent && v.cur[1] <= kSilent) { v.active = false; return; }
    tvaGains(v, mul(v.env.level(), v.tb.level), v.target);
    const float T = v.env.stageTime();
    float frames = 441.0f;
    if (!(0.0f > T)) {
        const float d = sub(T, mul(v.elapsed, kInvMs));       // the stage time less the time since the last update
        if (d > 0.0f) { if (10.0f > d) frames = mul(d, kFramesPerMs); }
        else frames = mul(kNoAttackMs, kFramesPerMs);
    }
    v.step[0] = div(sub(v.target[0], v.cur[0]), frames);
    v.step[1] = div(sub(v.target[1], v.cur[1]), frames);
    v.elapsed = 0;
}

void XtSynth::mixBlock(float* lr) {
    std::fill(lr, lr + 64, 0.0f);
    // The tempo clock, here on the blocks; ahead of the tone update, as one choice of the two
    // tasks' order on the XT.  From outside: the timer's ms in this block first, then its clocks
    if (clockExt) {
        for (int n = msDue(); n > 0; --n) clockMs();
        for (int n = clocksDue(); n > 0; --n) { clockIn(); clockTick(); ++hostDone; }
    } else {
        for (int n = clocksDue(); n > 0; --n) { clockTick(); clockAcc -= (uint64_t)clockPeriod() * 44100; }
        clockAcc += kClockPerBlock;
    }
    clockMsAcc = (clockMsAcc + 32000) % 44100;
    hostAt += hostStep();
    ++clockBlocks;
    // The control clock: the tone update runs between blocks every 441 frames, and
    // the free-running LFOs step with it, a tone sounding or not.
    const bool tick = tickCount >= kTickFrames;
    if (tick) tickCount -= kTickFrames;
    tickCount += 32;
    if (tick) {
        freeLfo.tick(1);
        freeLfo2.tick(1);
        for (XtLfoFree& f : freeClfo) f.tick(1);
        freeTlfo.tick(1);
    }
    if (!cur && std::none_of(std::begin(voice), std::end(voice), [](const Voice& v) { return v.active; })) return;
    float tmp[64], tmp2[64];
    for (Voice& v : voice) {
        if (!v.active) continue;
        if (tick) {
            // the tone update (0x894f75f0): envelopes step, the voice update
            // moves the player 70% toward its targets (0x8950c1b0), then the TVA
            v.env.tick();
            v.tlfo.tick(1);
            auto oscTick = [](OscVoice& s) { s.penv.tick(); s.tenv.tick(); s.fenv.tick(); s.oenv.tick(); s.lfo.tick(1); };
            if (v.on) oscTick(v);
            if (v.osc2.on) oscTick(v.osc2);
            for (Voice::CosmVoice& c : v.cs)
                if (c.on) { c.envA.tick(); c.envB.tick(); c.lfo.tick(1); }
            auto oscUpdate = [&](OscVoice& s, const OscControls& o) {
                if (s.chase) {                                 // 0x894f20d2: going back to the beat, once silent
                    if (s.wave->type == 1) {
                        if (s.solo.gainNow() > kSilent) return;
                        s.solo.seek(s.solo.beat().beatPos >> 5);
                    } else {
                        if (s.mb.gainNow() > kSilent) return;
                        s.mb.seek(s.mb.beat().beatPos >> 5);
                    }
                    s.chase = false;
                }
                s.porta.tick(1);                               // 0x894f4b00, before the voice update
                float A, F, w, g;
                targets(v, s, o, A, F, w, g);
                const float gt = gainTarget(v, s, o, s.oenv.level());
                if (s.analog) {   // the analog update (0x89505400): the pitch at its target, no glide
                    s.curA = A;
                    s.ana.update(analogBlock(v, s, o), A, gt, s.fenv.level(), s.tenv.level(), s.lfo.out());
                } else {
                    s.curA = xtOscSmooth(s.curA, A);
                    s.curF = xtOscSmooth(s.curF, F);
                    s.curWalk = xtOscSmooth(s.curWalk, w);
                    s.curG = xtOscSmooth(s.curG, g);
                    s.curGain = xtOscSmooth(s.curGain, gt);
                    applyRates(s);
                }
            };
            if (v.on) oscUpdate(v, ctl);
            if (v.osc2.on) oscUpdate(v.osc2, ctl.osc2);
            // the COSM updates (0x894ee6a0, COSM 1 then 2), after the voices', before the TVA
            for (int k = 0; k < 2; ++k) {
                Voice::CosmVoice& c = v.cs[k];
                if (!c.on) continue;
                c.hold();
                c.update(xtCosmOsc(v.structure, k) ? v.osc2.curA : v.curA, c.envA.level(), c.envA.smooth(), c.envB.level(), c.envB.smooth(), c.lfo.out());
            }
            tvaUpdate(v);
            if (!v.active) continue;
        }
        // A PCM OSC's player, in the XT's units (int16 × 65536): the SOLO player's ±1 × 2^31;
        // the band players' gain is the firmware's (setGainXt)
        auto pcmBlock = [](OscVoice& s, float* lr) {
            if (s.wave->type == 1) {
                float l[32], r[32];
                s.solo.render(l, r, 32);
                for (int i = 0; i < 32; ++i) { lr[2 * i] = l[i] * 0x1p31f; lr[2 * i + 1] = r[i] * 0x1p31f; }
            } else s.mb.block(lr);
        };
        // The tone's render by its Structure Type (xtToneOrder), all in the XT's units
        uint32_t mask2 = 0;
#ifdef XT_TONE_CHECK
        bool modDone = false;
#endif
        for (const XtToneStep* st = xtToneOrder(v.structure); *st != XtToneStep::End; ++st) {
            switch (*st) {
            case XtToneStep::Osc2:
                if (!v.osc2.on) std::fill(tmp2, tmp2 + 64, 0.0f);
                else if (v.osc2.analog) mask2 = v.osc2.ana.process(tmp2);
                else pcmBlock(v.osc2, tmp2);                   // a PCM OSC's mask is 0
                break;
            case XtToneStep::Cosm2OnOsc2:
                // what follows OSC 2's buffer is the mix's, as the last tone left it
                if (v.cs[1].on) v.cs[1].process(tmp2, 1.0f, mixPast);
                break;
            case XtToneStep::Osc1: {
                // While the MOD's flag is set OSC 2's mask goes to OSC 1's sync input
                // and to the MOD
                uint32_t sync = 0;
                if (v.mod.syncOn()) {
                    sync = mask2;
                    v.mod.setMask(mask2);
                }
                if (!v.on) std::fill(tmp, tmp + 64, 0.0f);
                else if (v.analog) v.ana.process(tmp, sync, tmp2);   // X-MOD reads OSC 2's buffer and clears it
                else pcmBlock(v, tmp);
                break;
            }
            case XtToneStep::Mod:
                // The MOD (0x894f1d50).  Off, or MIX at gains of 1: the sum.  A COSM on
                // an OSC that is off can still put something in its buffer
                if (v.mod.plain()) {
                    if (v.osc2.on || (v.structure == 2 && v.cs[1].on)) xtAnalogMix(tmp, tmp2);
                } else v.mod.process(tmp, tmp, tmp2);
                break;
            case XtToneStep::Cosm1:
                // on OSC 1's own buffer (Types 2, 3) what follows the block is OSC 2's; on the mix
                // it is the bus's first pairs, zeros with Output Assign MFX
                if (v.cs[0].on) v.cs[0].process(tmp, 1.0f, v.structure != 0 ? tmp2 : nullptr);
                break;
            case XtToneStep::Cosm2:
                if (v.cs[1].on) v.cs[1].process(tmp, 1.0f);
                break;
            case XtToneStep::End:
                break;
            }
#ifdef XT_TONE_CHECK
            if (*st == XtToneStep::Mod) { modDone = true; std::copy(tmp, tmp + 64, tapMix); }
            if (!modDone) { std::copy(tmp, tmp + 64, tapOsc1); std::copy(tmp2, tmp2 + 64, tapOsc2); }
#endif
        }
#ifdef XT_TONE_CHECK
        std::copy(tmp, tmp + 64, tapCosm);
#endif
        std::copy(tmp, tmp + 3, mixPast);
        if (v.damping) {
            // truncated (0x894f5570): the gain and the bus's level held, times a fade that
            // falls by 1/(Truncate Time·44.1) each sample; silent and done at 2^-16
            for (int i = 0; i < 32; ++i) {
                v.damp = xtf::sub(v.damp, truncateStep());
                if (!(v.damp > kSilent)) { v.damp = 0.0f; v.active = false; break; }
                lr[2 * i] = xtf::add(lr[2 * i], xtf::mul(xtf::mul(xtf::mul(tmp[2 * i], v.cur[0]), v.damp), v.send[0]));
                lr[2 * i + 1] = xtf::add(lr[2 * i + 1], xtf::mul(xtf::mul(xtf::mul(tmp[2 * i + 1], v.cur[1]), v.damp), v.send[1]));
            }
            continue;
        }
        // TVA render (0x894f5720): for each channel a step that would pass the target
        // within the block is cut to (target − gain)/32.5; gain applied, then stepped.
        // The output bus takes that at its own level, which moves by a share of the gap as
        // the block began.
        float s[2], e[2];
        const float sendTo[2] = { v.tb.sendL, v.tb.sendR };
        for (int c = 0; c < 2; ++c) {
            const float d = xtf::sub(v.target[c], v.cur[c]);
            s[c] = std::fabs(xtf::mul(v.step[c], 32.0f)) > std::fabs(d) ? xtf::mul(d, kLand) : v.step[c];
            e[c] = xtf::mul(xtf::sub(sendTo[c], v.send[c]), kSendGlide);
        }
        for (int i = 0; i < 32; ++i)
            for (int c = 0; c < 2; ++c) {
                lr[2 * i + c] = xtf::add(lr[2 * i + c], xtf::mul(xtf::mul(tmp[2 * i + c], v.cur[c]), v.send[c]));
                v.cur[c] = xtf::add(v.cur[c], s[c]);
                v.send[c] = xtf::add(v.send[c], e[c]);
            }
        v.elapsed += 32.0f;
    }
    // the voices are summed in the XT's units, as its output bus does; ±1 here
    for (int i = 0; i < 64; ++i) lr[i] *= 0x1p-31f;
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
//   XT_ANALOG="wave,impact,pw,pwKf,pwLfo,pwEnv,fat,fatKf,fatLfo,fatEnv[,detune[,subOctave,subLevel]]":
//     OSC Type ANALOG (KFs in %; PW on XT_TENV's envelope, Fat on XT_FENV's).
//   XT_ANALOG2="wave,impact,pw,…,subLevel": OSC 2 on, ANALOG, as XT_ANALOG; its
//     XT_PENV2, XT_TENV2, XT_FENV2, XT_LFO2 (Env 5, 6, 7, LFO 2), XT_OLEV2,
//     XT_OENV2 (Env 8), XT_VEL2="oscSens,oscCurve", XT_PITCH2="pitch,pitchKF,
//     coarse,fine" and XT_TUNE2="random", as OSC 1's.  XT_WAVE2="file,channels,rate,
//     type": OSC 2 on, PCM, its own wave, with XT_OSC2 as XT_OSC, XT_LOOP2, XT_ROBOT2
//     and XT_KEY2 (its Original Key); XT_WAVE1=0: no wave on OSC 1.  XT_OSC1OFF=1: OSC 1's
//     Switch off.
//   XT_TVA_ON=1: the TVA Switch on (off by default: XT_ADSR then does nothing).
//   XT_TVA="level,levelKF,levelLfo,pan,panKF,panLfo,send": the patch TVA (KFs in %), MFX Send Level.
//   XT_LFO5="wave,rate,offset,delay,mode,fade,keysync": LFO 5, on its level and pan.
//   XT_CC="cc7,cc11,cc10": the controllers before the first note.
//   XT_TEMPO=n: the Patch Tempo ×10.  XT_SYNC="sync,origTempo×10000[,sync2,origTempo2]": Tempo Sync
//     and the wave's Original Tempo, OSC 1 and OSC 2.  XT_RATE="lfo,osc2Lfo": LFO rates, 128..144 note values.
//   XT_TRIP="frame,x,y[;frame,x,y...]": the Time Trip pad at frames, x and y 0 lifting it.
//   XT_HOST="bpm[,ppq[,moving]]": Clock Source EXTERNAL on a host's transport at that tempo, told every
//     512 frames, from that position (default 0, moving; moving 0 is a stopped transport).
//   XT_MOD="type,level,attack,release": the MOD on, Type 0 MIX, 1 RING, 2 FM,
//     3 ENV RING, 4 OSC SYNC, Original Level, Attack and Release Times.  XT_MOD_AT="frame,on,type,level,
//     attack,release": new MOD knobs at a frame, on the notes then sounding.
//   XT_STRUCT=n: the Structure Type, 0…2 (xtToneOrder).  XT_STRUCT_AT="frame,n": a new
//     one at a frame; a sounding note keeps its own.
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
//   XT_COSM="filter,cutoff,reso,kf,cutEnv,cutLfo,resoEnv,resoLfo[,type,resoOwn,cutOwn]":
//     COSM 1 on, a TVF (type 0) or Dynamic TVF (1); XT_CENV="a,d,s,r[,velCurve,velSens]" its cutoff envelope
//     (Env 9); XT_CLFO="wave,rate,offset,delay,fadeMode,fade,keySync" LFO 3.
//     XT_COSM2, XT_CENV2, XT_CLFO2: the same for COSM 2 (Env 11, LFO 4).
//   XT_DUAL="type,freq1,kf1,q1,lfo1,env1,freq2,kf2,q2,lfo2,env2": COSM 1 on, a Dual
//     Filter (KF in %); XT_CENV its Filter 1 envelope (Env 9), XT_CENVB its
//     Filter 2 envelope (Env 10). XT_DUAL2, XT_CENVB2: COSM 2's.
//   XT_TB="cutoff,kf,lfo,env,q,qFollow,hpf,hpfKf,hpfLfo,hpfEnv,gain": COSM 1 on, a TB
//     Filter (KF in %); XT_CENV the LPF's envelope, XT_CENVB the HPF's. XT_TB2: COSM 2's.
//   XT_COMB="octave,detune,detuneLfo,detuneEnv,tone,toneLfo,toneEnv": COSM 1 on, a Comb
//     Filter (octave 0..4 = −2..+2); XT_CENV Tone's envelope, XT_CENVB Detune's. XT_COMB2: COSM 2's.
//   XT_SBF1="hpf,lpf,lpfKf,subDetune,octave,detune,detuneLfo,detuneOwn,detuneEnv,width,widthKf,widthLfo,widthOwn,widthEnv":
//     COSM 1 on, the 1st Side Band Filter (KFs in %, octave 0..2 = −2..0); XT_SBF2="q,lpf,…" (no
//     subDetune) the 2nd; XT_CENV Width's envelope, XT_CENVB Detune's. XT_SBF12, XT_SBF22: COSM 2's.
//   XT_RES="type,size,sizeKf,sizeLfo,sizeEnv,balance,balanceLfo,balanceEnv": COSM 1 on, a
//     Resonator (KF in %); XT_CENV Size's envelope, XT_CENVB Balance's. XT_RES2: COSM 2's.
//   XT_FS="depth,depthKf,depthLfo,depthEnv,balance,balanceLfo,balanceEnv": COSM 1 on, a
//     Frequency Shifter (KF in %); XT_CENV Depth's envelope, XT_CENVB Balance's. XT_FS2: COSM 2's.
//   XT_LOFI="bit,rate,filter,filterKf,filterLfo,filterEnv,balance,balanceLfo,balanceEnv": COSM 1 on,
//     Lo-Fi (KF in %); XT_CENV the Filter's envelope, XT_CENVB Balance's. XT_LOFI2: COSM 2's.
//   XT_COMP="attack,sustain,level": COSM 1 on, a Compressor. XT_COMP2: COSM 2's.
//   XT_LIM="threshold,ratio,attack,release,level": COSM 1 on, a Limiter (ratio 0..3). XT_LIM2: COSM 2's.
//   XT_WS="type,level,drive,driveLfo,driveEnv,tone,toneKf,toneLfo,toneEnv": COSM 1 on, Wave Shape
//     (type 0..5, KF in %); XT_CENV Drive's envelope, XT_CENVB Tone's. XT_WS2: COSM 2's.
//   XT_OD="type,level,drive,driveLfo,driveEnv,tone,toneKf,toneLfo,toneEnv": COSM 1 on, OD/Dist
//     (type 0 OD, 1 DS; KF in %); XT_CENV Drive's envelope, XT_CENVB Tone's. XT_OD2: COSM 2's.
//   XT_AMP="type,bass,middle,treble,level,gain,gainLfo,gainEnv": COSM 1 on, the Amp Simulator
//     (type 0..2); XT_CENV Gain's envelope. XT_AMP2: COSM 2's.
//   XT_SPK="type": COSM 1 on, the Speaker Simulator (type 0..11). XT_SPK2: COSM 2's.
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
    if (std::getenv("XT_WAVE1") && std::atoi(std::getenv("XT_WAVE1")) == 0) syn.setSample(nullptr);
    if (const char* e = std::getenv("XT_WAVE2")) {
        char path[1024] = {};
        int ch2 = 1, type2 = 1;
        double rate2 = 44100;
        if (std::sscanf(e, "%1023[^,],%d,%lf,%d", path, &ch2, &rate2, &type2) != 4) { std::fprintf(stderr, "XT_WAVE2: file,channels,rate,type\n"); return 1; }
        FILE* f2 = std::fopen(path, "rb");
        if (!f2) { std::fprintf(stderr, "XT_WAVE2: cannot open %s\n", path); return 1; }
        std::vector<std::vector<float>> chans2((size_t)ch2);
        int16_t x;
        for (size_t n = 0; std::fread(&x, 2, 1, f2) == 1; ++n) chans2[n % (size_t)ch2].push_back(x / 32768.0f);
        std::fclose(f2);
        for (auto& k : chans2) k.resize(chans2.back().size());
        auto E2 = xtSynthEncode(chans2, rate2, type2, 64, &err);
        if (!E2) { std::fprintf(stderr, "encode 2: %s\n", err.c_str()); return 1; }
        syn.setSample(E2, 1);
        c.osc2.on = true;
        c.osc2.oscType = 1;
    }
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
    if (const char* e = std::getenv("XT_ANALOG")) {
        XtAnalogParams& a = c.ana;
        c.oscType = 0;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &a.wave, &a.impact, &a.pw, &a.pwKf, &a.pwLfo, &a.pwEnv, &a.fat, &a.fatKf, &a.fatLfo,
                    &a.fatEnv, &a.detune, &a.subOctave, &a.subLevel);
        a.pwKf /= 10;
        a.fatKf /= 10;
    }
    if (const char* e = std::getenv("XT_ANALOG2")) {
        XtAnalogParams& a = c.osc2.ana;
        c.osc2.on = true;
        c.osc2.oscType = 0;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &a.wave, &a.impact, &a.pw, &a.pwKf, &a.pwLfo, &a.pwEnv, &a.fat, &a.fatKf, &a.fatLfo,
                    &a.fatEnv, &a.detune, &a.subOctave, &a.subLevel);
        a.pwKf /= 10;
        a.fatKf /= 10;
    }
    if (std::getenv("XT_OSC1OFF") && std::atoi(std::getenv("XT_OSC1OFF")) != 0) c.on = false;
    if (const char* e = std::getenv("XT_TVA_ON")) c.tvaOn = std::atoi(e) != 0;
    if (const char* e = std::getenv("XT_TVA"))
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &c.tvaLevel, &c.tvaLevelKF, &c.tvaLevelLfo, &c.tvaPan, &c.tvaPanKF, &c.tvaPanLfo, &c.mfxSend);
    if (const char* e = std::getenv("XT_LFO5")) {
        int ks = 0;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &c.tvaLfoWave, &c.tvaLfoRate, &c.tvaLfoOffset, &c.tvaLfoDelay, &c.tvaLfoFadeMode, &c.tvaLfoFade, &ks);
        c.tvaLfoKeySync = ks != 0;
    }
    if (const char* e = std::getenv("XT_MOD")) {
        c.mod.on = true;
        std::sscanf(e, "%d,%d,%d,%d", &c.mod.type, &c.mod.level, &c.mod.attack, &c.mod.release);
    }
    if (const char* e = std::getenv("XT_STRUCT")) c.structure = std::atoi(e);
    if (const char* e = std::getenv("XT_TEMPO")) c.patchTempo = std::atoi(e);
    double hostBpm = 0.0, hostPpq = 0.0;
    int hostMoving = 1;
    if (const char* e = std::getenv("XT_HOST")) { std::sscanf(e, "%lf,%lf,%d", &hostBpm, &hostPpq, &hostMoving); c.clockSource = 1; }
    if (const char* e = std::getenv("XT_SYNC")) {
        int s1 = 0, s2 = 0;
        std::sscanf(e, "%d,%d,%d,%d", &s1, &c.origTempo, &s2, &c.osc2.origTempo);
        c.tempoSync = s1 != 0;
        c.osc2.tempoSync = s2 != 0;
    }
    if (const char* e = std::getenv("XT_RATE")) std::sscanf(e, "%d,%d", &c.lfoRate, &c.osc2.lfoRate);
    {
        XtSynth::OscControls& o = c.osc2;
        if (const char* e = std::getenv("XT_PENV2")) std::sscanf(e, "%d,%d,%d,%d,%d", &o.pAttack, &o.pDecay, &o.pSustain, &o.pRelease, &o.pDepth);
        if (const char* e = std::getenv("XT_TENV2")) std::sscanf(e, "%d,%d,%d,%d,%d", &o.tAttack, &o.tDecay, &o.tSustain, &o.tRelease, &o.tDepth);
        if (const char* e = std::getenv("XT_FENV2")) std::sscanf(e, "%d,%d,%d,%d,%d", &o.fAttack, &o.fDecay, &o.fSustain, &o.fRelease, &o.fDepth);
        if (const char* e = std::getenv("XT_LFO2")) {
            int ks = 1;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &o.lfoWave, &o.lfoRate, &o.lfoPitchDepth, &o.lfoDelay, &o.lfoFadeMode, &o.lfoFade, &o.lfoOffset, &ks);
            o.lfoKeySync = ks != 0;
        }
        if (const char* e = std::getenv("XT_OLEV2")) std::sscanf(e, "%d,%d,%d,%d", &o.oscLevel, &o.oscLevelKF, &o.oscLevelLfoDepth, &o.waveGain);
        if (const char* e = std::getenv("XT_OENV2"))
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &o.oAttack, &o.oDecay, &o.oSustain, &o.oRelease, &o.oASens, &o.oDSens, &o.oRSens, &o.oTimeKF);
        if (const char* e = std::getenv("XT_VEL2")) std::sscanf(e, "%d,%d", &o.oscVelSens, &o.oscVelCurve);
        if (const char* e = std::getenv("XT_PITCH2")) std::sscanf(e, "%d,%d,%d,%d", &o.oscPitch, &o.pitchKF, &o.oscCoarse, &o.oscFine);
        if (const char* e = std::getenv("XT_TUNE2")) o.randomPitch = std::atoi(e);
        if (const char* e = std::getenv("XT_OSC2"))   // OSC 2's as XT_OSC
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &o.oscTime, &o.oscTimeOffset, &o.oscTimeKF, &o.oscFormant, &o.oscFormantKF,
                        &o.energy, &o.lfoTimeDepth, &o.lfoFormantDepth);
        o.robot = std::getenv("XT_ROBOT2") && std::atoi(std::getenv("XT_ROBOT2")) != 0;
        if (const char* e = std::getenv("XT_LOOP2")) {
            o.loop = true;
            std::sscanf(e, "%d,%f,%f", &o.loopMode, &o.loopStart, &o.loopEnd);
        }
        if (const char* e = std::getenv("XT_KEY2")) o.rootKey = std::atoi(e);
    }
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
    for (int k = 0; k < 2; ++k) {                   // COSM 1 (XT_COSM…), COSM 2 (XT_COSM2…)
        XtSynth::Controls::Cosm& m = c.cosm[k];
        const std::string sfx = k ? "2" : "";
        if (const char* e = std::getenv(("XT_COSM" + sfx).c_str())) {   // "filter,cutoff,reso,kf,cutEnv,cutLfo,resoEnv,resoLfo"
            m.on = true;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.filter, &m.cutoff, &m.reso, &m.kf, &m.cutoffEnv, &m.cutoffLfo, &m.resoEnv,
                        &m.resoLfo, &m.type, &m.resoOwn, &m.cutoffOwn);
        }
        if (const char* e = std::getenv(("XT_DUAL" + sfx).c_str())) {   // "type,freq1,kf1,q1,lfo1,env1,freq2,kf2,q2,lfo2,env2"
            m.on = true;
            m.type = 2;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.dualType, &m.freq1, &m.kf1, &m.q1, &m.lfo1, &m.env1, &m.freq2, &m.kf2,
                        &m.q2, &m.lfo2, &m.env2);
        }
        if (const char* e = std::getenv(("XT_TB" + sfx).c_str())) {     // "cutoff,kf,lfo,env,q,qFollow,hpf,hpfKf,hpfLfo,hpfEnv,gain"
            m.on = true;
            m.type = 3;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.tbCutoff, &m.tbKf, &m.tbLfo, &m.tbEnv, &m.tbQ, &m.tbQFollow, &m.tbHpf,
                        &m.tbHpfKf, &m.tbHpfLfo, &m.tbHpfEnv, &m.tbGain);
        }
        if (const char* e = std::getenv(("XT_COMB" + sfx).c_str())) {   // "octave,detune,detuneLfo,detuneEnv,tone,toneLfo,toneEnv"
            m.on = true;
            m.type = 4;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &m.combOctave, &m.combDetune, &m.combDetuneLfo, &m.combDetuneEnv, &m.combTone, &m.combToneLfo,
                        &m.combToneEnv);
        }
        if (const char* e = std::getenv(("XT_COMP" + sfx).c_str())) {   // "attack,sustain,level"
            m.on = true;
            m.type = 10;
            std::sscanf(e, "%d,%d,%d", &m.compAttack, &m.compSustain, &m.compLevel);
        }
        if (const char* e = std::getenv(("XT_SPK" + sfx).c_str())) {    // "type"
            m.on = true;
            m.type = 15;
            m.spkType = std::atoi(e);
        }
        if (const char* e = std::getenv(("XT_AMP" + sfx).c_str())) {    // "type,bass,middle,treble,level,gain,gainLfo,gainEnv"
            m.on = true;
            m.type = 14;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &m.ampType, &m.ampBass, &m.ampMiddle, &m.ampTreble, &m.ampLevel, &m.ampGain,
                        &m.ampGainLfo, &m.ampGainEnv);
        }
        if (const char* e = std::getenv(("XT_OD" + sfx).c_str())) {     // "type,level,drive,driveLfo,driveEnv,tone,toneKf,toneLfo,toneEnv"
            m.on = true;
            m.type = 13;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.odType, &m.odLevel, &m.odDrive, &m.odDriveLfo, &m.odDriveEnv, &m.odTone,
                        &m.odToneKf, &m.odToneLfo, &m.odToneEnv);
        }
        if (const char* e = std::getenv(("XT_WS" + sfx).c_str())) {     // "type,level,drive,driveLfo,driveEnv,tone,toneKf,toneLfo,toneEnv"
            m.on = true;
            m.type = 12;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.wsType, &m.wsLevel, &m.wsDrive, &m.wsDriveLfo, &m.wsDriveEnv, &m.wsTone,
                        &m.wsToneKf, &m.wsToneLfo, &m.wsToneEnv);
        }
        if (const char* e = std::getenv(("XT_LIM" + sfx).c_str())) {    // "threshold,ratio,attack,release,level"
            m.on = true;
            m.type = 11;
            std::sscanf(e, "%d,%d,%d,%d,%d", &m.limThreshold, &m.limRatio, &m.limAttack, &m.limRelease, &m.limLevel);
        }
        if (const char* e = std::getenv(("XT_LOFI" + sfx).c_str())) {   // "bit,rate,filter,filterKf,filterLfo,filterEnv,balance,balanceLfo,balanceEnv"
            m.on = true;
            m.type = 9;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.lofiBit, &m.lofiRate, &m.lofiFilter, &m.lofiFilterKf, &m.lofiFilterLfo,
                        &m.lofiFilterEnv, &m.lofiBalance, &m.lofiBalanceLfo, &m.lofiBalanceEnv);
        }
        if (const char* e = std::getenv(("XT_FS" + sfx).c_str())) {     // "depth,depthKf,depthLfo,depthEnv,balance,balanceLfo,balanceEnv"
            m.on = true;
            m.type = 8;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &m.fsDepth, &m.fsDepthKf, &m.fsDepthLfo, &m.fsDepthEnv, &m.fsBalance, &m.fsBalanceLfo,
                        &m.fsBalanceEnv);
        }
        if (const char* e = std::getenv(("XT_RES" + sfx).c_str())) {    // "type,size,sizeKf,sizeLfo,sizeEnv,balance,balanceLfo,balanceEnv"
            m.on = true;
            m.type = 7;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &m.resType, &m.resSize, &m.resSizeKf, &m.resSizeLfo, &m.resSizeEnv, &m.resBalance,
                        &m.resBalanceLfo, &m.resBalanceEnv);
        }
        for (int t = 0; t < 2; ++t)
            if (const char* e = std::getenv(("XT_SBF" + std::to_string(t + 1) + sfx).c_str())) {
                XtSynth::Controls::Cosm::SideBand& b = m.sb[t];
                m.on = true;
                m.type = 5 + t;
                if (t == 0)
                    std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &b.hpf, &b.lpf, &b.lpfKf, &b.subDetune, &b.octave, &b.detune,
                                &b.detuneLfo, &b.detuneOwn, &b.detuneEnv, &b.width, &b.widthKf, &b.widthLfo, &b.widthOwn, &b.widthEnv);
                else
                    std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &b.q, &b.lpf, &b.lpfKf, &b.octave, &b.detune, &b.detuneLfo,
                                &b.detuneOwn, &b.detuneEnv, &b.width, &b.widthKf, &b.widthLfo, &b.widthOwn, &b.widthEnv);
            }
        if (const char* e = std::getenv(("XT_CENV" + sfx).c_str()))     // the cutoff's envelope "a,d,s,r[,velCurve,velSens]"
            std::sscanf(e, "%d,%d,%d,%d,%d,%d", &m.envA.attack, &m.envA.decay, &m.envA.sustain, &m.envA.release, &m.envA.velCurve, &m.envA.velSens);
        if (const char* e = std::getenv(("XT_CENVB" + sfx).c_str()))    // Env B "a,d,s,r[,velCurve,velSens]"
            std::sscanf(e, "%d,%d,%d,%d,%d,%d", &m.envB.attack, &m.envB.decay, &m.envB.sustain, &m.envB.release, &m.envB.velCurve, &m.envB.velSens);
        if (const char* e = std::getenv(("XT_CLFO" + sfx).c_str())) {   // its LFO "wave,rate,offset,delay,fadeMode,fade,keySync"
            int ks = 0;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &m.lfoWave, &m.lfoRate, &m.lfoOffset, &m.lfoDelay, &m.lfoFadeMode, &m.lfoFade, &ks);
            m.lfoKeySync = ks != 0;
        }
    }
    int bendValue = 8192;
    long bendAt = 0;
    if (const char* e = std::getenv("XT_BEND")) std::sscanf(e, "%d@%ld", &bendValue, &bendAt);
    syn.setControls(c);
    if (const char* e = std::getenv("XT_CC")) {
        int c7 = 127, c11 = 127, c10 = 64;
        std::sscanf(e, "%d,%d,%d", &c7, &c11, &c10);
        syn.controller(7, c7);
        syn.controller(11, c11);
        syn.controller(10, c10);
    }
    XtSynth::Controls modLater = c;
    long modAt = -1;
    if (const char* e = std::getenv("XT_MOD_AT")) {
        int on = 1;
        std::sscanf(e, "%ld,%d,%d,%d,%d,%d", &modAt, &on, &modLater.mod.type, &modLater.mod.level, &modLater.mod.attack, &modLater.mod.release);
        modLater.mod.on = on != 0;
    }
    if (const char* e = std::getenv("XT_STRUCT_AT")) std::sscanf(e, "%ld,%d", &modAt, &modLater.structure);
    struct Ev { long at; int note; bool on; };
    std::vector<Ev> ev;
    struct Trip { long at; int x, y; };
    std::vector<Trip> trips;
    if (const char* e = std::getenv("XT_TRIP")) {
        Trip t;
        int used = 0;
        while (std::sscanf(e, "%ld,%d,%d;%n", &t.at, &t.x, &t.y, &used) >= 3) {
            trips.push_back(t);
            if (used == 0) break;
            e += used;
            used = 0;
        }
    }
    for (int a = 9; a < argc; ++a) {
        int note = 60; long on = 0, off = -1;
        std::sscanf(argv[a], "%d@%ld:%ld", &note, &on, &off);
        ev.push_back({ on, note, true });
        if (off >= 0) ev.push_back({ off, note, false });
    }
    std::vector<float> L((size_t)frames), R((size_t)frames), out((size_t)frames * 2);
    long done = 0;
    while (done < frames) {
        long next = hostBpm > 0.0 ? std::min(frames, done + 512) : frames;
        if (hostBpm > 0.0) syn.hostClock(hostBpm, hostMoving != 0, hostPpq + (double)done / host * hostBpm / 60.0);
        for (const Ev& e : ev) if (e.at >= done && e.at < next) next = e.at;
        if (bendAt >= done && bendAt < next) next = bendAt;
        if (modAt >= done && modAt < next) next = modAt;
        for (const Trip& t : trips) if (t.at >= done && t.at < next) next = t.at;
        for (const Trip& t : trips) if (t.at == done) syn.timeTrip(t.x, t.y);
        if (bendAt == done) syn.pitchBend(bendValue);
        if (modAt == done) syn.setControls(modLater);
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
