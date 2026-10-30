// Polyphonic XT-firmware playback — see XtSynth.h.
#include "XtSynth.h"
#include "XtFirmware.h"
#include "XtSoloEncoder.h"

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
              && xtFwOpen(path.c_str());
    std::vector<uint32_t> ks;
    if (g_romReady && xtFwEnvKeyScales(ks)) xtEnvSetKeyScales(ks.data());
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
}

// The player's targets (0x894f3ba0 pitch, 0x894f3d40 formant, 0x894f3c90
// time, voice +0x18 Energy): pitch in cents with the pitch envelope at its
// depth, clamped to ±6000; formant clamped to [0.5, 2]; the XT's time range
// is ±5.
void XtSynth::targets(const Voice& v, float& A, float& F, float& walk, float& g) const {
    double cents = ((double)(v.note - ctl.rootKey) + ctl.pitchSemis) * 100.0
                 + (double)v.penv.level() * (double)xtPitchDepthCents((float)ctl.pDepth);
    cents = std::clamp(cents, -6000.0, 6000.0);
    A = (float)std::exp2(cents / 1200.0);
    F = (float)std::clamp(std::pow(2.0, ctl.formantSemis / 12.0), 0.5, 2.0);
    walk = (float)(1.0 / std::clamp((double)ctl.timeStretch, 0.2, 5.0));
    g = ctl.energy;
}

void XtSynth::applyRates(Voice& v) {
    if (cur->type == 1) { v.solo.setRates(v.curA, v.curF, 1.0 / v.curWalk); v.solo.setEnergy(v.curG); }
    else v.mb.setRates(v.curA, v.curWalk);
}

XtEnvParams XtSynth::pitchEnvParams(const Voice& v) const {
    XtEnvKnobs k;
    k.attack = ctl.pAttack; k.decay = ctl.pDecay; k.sustain = ctl.pSustain; k.release = ctl.pRelease;
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
    ctl = c;
    for (Voice& v : voice) {
        if (!v.active) continue;
        if (envChanged) v.env.update(envParams(v));
        if (penvChanged) v.penv.update(pitchEnvParams(v));
    }
}

void XtSynth::noteOn(int note, float velocity) {
    pickUpSample();
    if (!cur) return;
    Voice* v = nullptr;
    for (Voice& w : voice) if (!w.active) { v = &w; break; }
    if (!v) { v = &voice[0]; for (Voice& w : voice) if (w.age < v->age) v = &w; }   // steal the oldest
    v->active = true;
    v->releasing = false;
    v->note = note;
    v->age = clock++;
    v->tail = 0;
    v->gain = std::clamp(velocity, 0.0f, 1.0f);
    v->vel = (int)std::lround(v->gain * 127.0f);
    v->relVel = 64;
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
    v->penv.noteOn(pitchEnvParams(*v));
    targets(*v, v->curA, v->curF, v->curWalk, v->curG);
    if (cur->type == 1) v->solo.noteOn(cur->solo, v->curA, v->curF, 1.0 / v->curWalk, v->curG);
    else if (!v->mb.noteOn(cur->wave, cur->type, v->curA, v->curWalk)) v->active = false;
}

// The release time follows the note-off velocity (R-Sens): the times are
// converted again before the release starts.
void XtSynth::noteOff(int note, float velocity) {
    for (Voice& v : voice)
        if (v.active && v.note == note && !v.releasing) {
            v.releasing = true;
            v.relVel = (int)std::lround(std::clamp(velocity, 0.0f, 1.0f) * 127.0f);
            v.env.update(envParams(v));
            v.env.noteOff();
            v.penv.update(pitchEnvParams(v));
            v.penv.noteOff();
        }
}

void XtSynth::allNotesOff() {
    for (Voice& v : voice) v.active = false;
}

// The tone update's TVA part (0x894f75f0 tail): stop a finished voice
// (0x894f54b0), else aim at the envelope's new output with a linear ramp of
// 10 ms, shorter when the stage itself is (0x894f5360).
void XtSynth::tvaUpdate(Voice& v) {
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
    float tmp[64];
    for (Voice& v : voice) {
        if (!v.active) continue;
        if (tick) {
            // the tone update (0x894f75f0): envelopes step, the voice update
            // moves the player 70% toward its targets (0x8950c1b0), then the TVA
            v.env.tick();
            v.penv.tick();
            float A, F, w, g;
            targets(v, A, F, w, g);
            constexpr float k = 0.699999988f;                  // 0x3f333333
            v.curA = (A - v.curA) * k + v.curA;
            v.curF = (F - v.curF) * k + v.curF;
            v.curWalk = (w - v.curWalk) * k + v.curWalk;
            v.curG = (g - v.curG) * k + v.curG;
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
    if (const char* e = std::getenv("XT_ADSR")) {   // amp envelope knobs "a,d,s,r"
        XtSynth::Controls c;
        std::sscanf(e, "%d,%d,%d,%d", &c.attack, &c.decay, &c.sustain, &c.release);
        syn.setControls(c);
    }
    if (const char* e = std::getenv("XT_PENV")) {   // pitch envelope "a,d,s,r,depth"
        XtSynth::Controls c;
        if (const char* f = std::getenv("XT_ADSR")) std::sscanf(f, "%d,%d,%d,%d", &c.attack, &c.decay, &c.sustain, &c.release);
        std::sscanf(e, "%d,%d,%d,%d,%d", &c.pAttack, &c.pDecay, &c.pSustain, &c.pRelease, &c.pDepth);
        syn.setControls(c);
    }
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
        for (const Ev& e : ev) if (e.at == done) { if (e.on) syn.noteOn(e.note, 1.0f); else syn.noteOff(e.note); }
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
