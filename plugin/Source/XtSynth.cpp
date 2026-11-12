// Polyphonic XT-firmware playback — see XtSynth.h.
#include "XtSynth.h"
#include "XtFirmware.h"
#include "XtSoloEncoder.h"
#include "XtFloat.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

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

// xtSynthSetRom and xtSynthRomReady are in XtSynthRom.cpp (v173)

std::shared_ptr<XtEncoded> xtSynthEncode(const std::vector<std::vector<float>>& channels, double sampleRate,
                                         int type, int depth, std::string* err, int bits, const std::vector<int32_t>* events,
                                         const std::vector<uint8_t>* stored) {
    auto fail = [&](const char* m) -> std::shared_ptr<XtEncoded> { if (err) *err = m; return nullptr; };
    if (!xtSynthRomReady()) return fail("no firmware image (rom.2) loaded");
    if (channels.empty() || channels[0].size() < 64) return fail("sample too short");
    if (type != 0 && type != 1 && type != 3 && type != 5) return fail("unknown encode type");
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
    // A SOLO wave's own events as the markers the detection would leave (in samples, the first 0, the last the end), when they fit
    // this audio: a V-Trainer wave's can fall between samples, which the markers can't hold (they're taken down to one)
    std::vector<uint32_t> marks;
    if (events && events->size() >= 2 && irate == 44100 && events->front() == 0 && (long)events->back() == frames * 16)
        for (int32_t e : *events) marks.push_back(marks.empty() || (uint32_t)(e / 16) > marks.back() ? (uint32_t)(e / 16) : marks.back());
    const bool own = !marks.empty();
    // A wave's own vpdt, played as it is when its events fit this audio: from 0 to the end (frames·16), never back (two at the end are the
    // XT's own, on 18 of the image's waves), in the type's layout (a SOLO event every 10 or 16 halfwords from 8, a band one every 0x2e or
    // 0x58 from 14; its position a word on).  A SOLO wave's may fall between samples (V-Trainer 1.00's, VP9SAMnoise1's) and its records must
    // reach its last event, or stop short of it with the terminator past it, as the player walks them (xtSoloReadVpdt; preset 270's stop
    // 18/16 short).  A band wave's must be on whole samples: no band wave there is has one between them, so the band player's way with one
    // isn't checked.  Every V-Synth wave there is plays as it is (HARDWARE_FIRMWARE.md, v158–v160)
    bool fits = false;
    if (stored && irate == 44100 && type != 0 && stored->size() >= 64) {
        const auto r32 = [&](size_t o) { uint32_t x = 0; if (o + 4 <= stored->size()) std::memcpy(&x, stored->data() + o, 4); return x; };
        const uint32_t nev = r32(8) >> 16, st = type == 1 ? (ch == 2 ? 16 : 10) : (ch == 2 ? 0x58 : 0x2e), first = type == 1 ? 8 : 14;
        fits = nev >= 2 && nev < 4096 && (st * (nev - 1) + first) * 2 + 8 <= stored->size() && r32(first * 2 + 4) == 0
            && (long)r32((st * (nev - 1) + first) * 2 + 4) == frames * 16;
        for (uint32_t e = 1; fits && e < nev; ++e) fits = r32((st * e + first) * 2 + 4) >= r32((st * (e - 1) + first) * 2 + 4);
        for (uint32_t e = 0; fits && type != 1 && e < nev; ++e) fits = r32((st * e + first) * 2 + 4) % 16 == 0;
    }
    if (type == 0 || type == 1) {
        // LITE (0) is the samples alone, as the XT keeps them: no analysis, so nothing to encode.
        // A stereo SOLO wave has the encoder's stereo mode, one list of records from both
        // channels (0x8951e120(3)); the player reads both through it (0x8950d1f0).
        XtSoloPlan& P = E->solo;
        P.in.resize((size_t)frames);
        if (ch == 2) P.inR.resize((size_t)frames);
        for (long i = 0; i < frames; ++i) {
            P.in[(size_t)i] = (float)pcm[(size_t)(i * ch)] / 32768.0f;
            if (ch == 2) P.inR[(size_t)i] = (float)pcm[(size_t)(2 * i + 1)] / 32768.0f;
        }
        E->channels = ch;
        if (type == 0) return E;
        // The attacks first: the XT's detection at the Encode Depth (as a band wave's), or the wave's own events.  With any, the XT's
        // own SOLO encode (0x89515e28) in the interpreter splits the records at them, and its vpdt gives the records, the events and
        // the segments.  With none, the port: the same records as the firmware's, laid out with no event between the start and the end.
        std::vector<XtRecord> recs;
        std::vector<uint8_t> vp;
        if (fits && xtSoloReadVpdt(*stored, ch, recs, P) && P.evPos.size() >= 2 && (long)P.evPos.back() == frames * 16) { vp = *stored; E->stored = true; }
        else { recs.clear(); P.evPos.clear(); P.evKind.clear(); P.evSeg.clear(); P.evRec.clear(); P.evRecStart.clear(); P.segPos.clear(); P.segRec.clear(); P.segFirst.clear(); }
        const bool xt = E->stored || ((own ? marks.size() > 2 : depth >= 0)
            && xtFwEncodeSolo(pcm.data(), frames, ch, depth, vp, nullptr, true, own ? &marks : nullptr) && xtSoloReadVpdt(vp, ch, recs, P) && P.evPos.size() > 2);
        if (!xt) {
            recs = xtSoloEncode(pcm.data(), (int)frames, ch);
            if (recs.size() < 2) return fail("SOLO encoder found no records");
            uint8_t ev[4][kXtFwSoloEventBytes];
            xtFwSoloImage(recs, frames, ch, XtLoop{}, vp, ev);
            std::vector<XtRecord> back;
            if (!xtSoloReadVpdt(vp, ch, back, P)) return fail("SOLO layout unreadable");
        }
        if (recs.size() < 2) return fail("SOLO encoder found no records");
        E->vpdt = std::move(vp);
        E->vpnoCount = (int)P.evPos.size();            // its events in order, as the XT's import writes them
        for (int k = 0; k < E->vpnoCount; ++k) E->vpno.push_back((uint16_t)k);
        const size_t nc = recs.size();
        P.segStart.resize(nc); P.segLen.resize(nc); P.voiced.resize(nc); P.info.resize(nc);
        for (size_t i = 0; i < nc; ++i) {
            P.segStart[i] = recs[i].start16 / 16.0;
            // the last a vpdt's own length (preset 270's stops short of the wave's end), the port's encoder's to the end
            const double end = (i + 1 < nc) ? recs[i + 1].start16 / 16.0 : xt ? (recs[i].start16 + recs[i].len16) / 16.0 : (double)frames;
            P.segLen[i] = std::max(1.0 / 16.0, end - P.segStart[i]);
            P.voiced[i] = (recs[i].info & 4) != 0;
            P.info[i] = recs[i].info;
        }
        E->channels = ch;
    } else {
        // A band wave on its own vpdt, or the detection's encode: its own events alone can't be given, as the band encode also reads
        // each marker's second word, which the detection takes from the audio and the file doesn't keep (HARDWARE_FIRMWARE.md, "A wave's own events")
        if (fits) { E->vpdt = *stored; E->stored = true; }
        else if (!xtFwEncodeMulti(type, pcm.data(), frames, ch, E->vpdt, depth)) return fail("multi-band encoder failed");
        if (!xtFwLoadBands(type, pcm.data(), frames, ch, E->bands)) return fail("band rebuild failed");
        E->wave.vpdt = E->vpdt.data();
        E->wave.vpdtBytes = E->vpdt.size();
        xtMbAttach(E->bands, E->wave);
        E->channels = ch;
        // its events in order (the XT's import writes the same `vpno`): the vpdt's count is u32[2]'s high half
        uint32_t w2 = 0;
        if (E->vpdt.size() >= 12) std::memcpy(&w2, E->vpdt.data() + 8, 4);
        E->vpnoCount = (int)(w2 >> 16);
        for (int k = 0; k < E->vpnoCount; ++k) E->vpno.push_back((uint16_t)k);
    }
    return E;
}

// ── Voices ───────────────────────────────────────────────────────────────────

XtSynth::XtSynth() {
    ccDefaults();
    for (int z = 0; z < kZones; ++z) xtModBlock(zc(z).mod, modBlk[z]);
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

void XtSynth::setSample(std::shared_ptr<const XtEncoded> e, int osc, int number) {
    std::lock_guard<std::mutex> g(swapLock);
    retired[osc].reset();                           // freed here, not on the audio thread
    pending[osc] = std::move(e);
    pendingNumber[osc] = number;
    swapPending[osc] = true;
}

void XtSynth::setZones(std::shared_ptr<const Zones> z) {
    std::lock_guard<std::mutex> g(swapLock);
    retiredZones.reset();
    pendingZones = std::move(z);
    zonesPending = true;
}

void XtSynth::pickUpSample() {
    std::unique_lock<std::mutex> g(swapLock, std::try_to_lock);
    if (!g.owns_lock() || !(swapPending[0] || swapPending[1] || zonesPending)) return;
    for (int k = 0; k < 2; ++k)
        if (swapPending[k]) {
            retired[k] = std::move(cur[k]);
            cur[k] = std::move(pending[k]);
            swapPending[k] = false;
            if (pendingNumber[k] < 0 || pendingNumber[k] != curNumber[k]) stepCount[0][k] = 0;   // another wave (0x894e5ad4)
            curNumber[k] = pendingNumber[k];
        }
    if (zonesPending) {
        retiredZones = std::move(zones);
        zones = std::move(pendingZones);
        zonesPending = false;
        for (int z = 0; z < kZones; ++z) zoneUpper[z] = zones ? zones->upper[z] : 127;
        for (int z = 1; z < kZones; ++z)
            for (int k = 0; k < 2; ++k) {
                const int n = zones ? zones->number[z][k] : 0;
                if (n <= 0 || n != zoneNumber[z][k]) stepCount[z][k] = 0;
                zoneNumber[z][k] = n;
            }
        zonesFollow();
        for (int z = 1; z < kZones; ++z) xtModBlock(zoneCtl[z].mod, modBlk[z]);
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

// Audio thread (it reads the samples the last render picked up).
bool XtSynth::oscPlays(int zone, int osc) const {
    const OscControls& o = osc ? zc(zone).osc2 : zc(zone);
    if (!o.on) return false;
    if (o.oscType == 0) return xtAnalogTablesLoaded() && xtAnalogPlays(o.ana.wave);
    return waveOf(zone, osc) != nullptr;
}

bool XtSynth::canPlay(int zone) const { return oscPlays(zone, 0) || oscPlays(zone, 1); }

// The converter's (0x894e4448): the Start Offset, or the last sample if it's not below the wave's length, both unsigned.
int32_t XtSynth::waveStart(const OscVoice& s, const OscControls& o) {
    const uint32_t frames = (uint32_t)s.wave->frames, at = (uint32_t)o.startOffset;
    return frames == 0 ? 0 : (int32_t)(at >= frames ? frames - 1 : at);
}

// Playback Mode STEP (0x894e42e0) and EVENT (0x894e4070), on a band or SOLO wave with a `vpno`: the event its list gives at an index, and the one
// after it, by the vpdt's numbers.  STEP's index is the zone's count of notes modulo the list's count less one, and with fewer than two it
// plays as RETRIGGER.  EVENT's is the key from 60 up, wrapping each list rounded up to octaves (0x894e1200).
// The tails clear the block's Loop bit (+0xb4 bit 0x01) and its 0x80 then, at every conversion: so a wave's loop is off under them.
// Vari off plays a SOLO wave on the linear player, whose converter (0x894f3460) takes samples, not events.
bool XtSynth::eventPlay(const OscVoice& s, const OscControls& o) {
    return o.playbackMode >= 2 && !s.analog && !s.lite && !s.linear && s.wave && (s.wave->type == 1 || s.wave->type == 3 || s.wave->type == 5)
        && (o.playbackMode == 3 || s.wave->vpnoCount >= 2);
}

bool XtSynth::waveEvents(const Voice& v, const OscVoice& s, const OscControls& o, int& first, int& last) const {
    if (!eventPlay(s, o)) return false;
    const int n = s.wave->vpnoCount;
    const auto at = [&](int i) { return i >= 0 && i < (int)s.wave->vpno.size() ? (int)s.wave->vpno[(size_t)i] : 0; };
    int idx = 0;
    if (o.playbackMode == 2) idx = (int)s.steps % (n - 1);
    else {
        const int m = (uint16_t)(n - 1);
        if (m != 0) {
            const int r = (m + 11) / 12 * 12, k = v.note - 60;
            int x = v.note >= 60 ? k : k % r + r;
            x %= r;
            idx = (uint16_t)(x % m);
        }
    }
    first = at(idx);
    last = at(idx + 1);
    return true;
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
    l.fwdRate = s.wave->loopRate[0];
    l.bwdRate = s.wave->loopRate[1];
    if (s.wave->bwdMode >= 0) {
        l.bwdMode = std::clamp(s.wave->bwdMode, 0, 2);
        l.bwdStart = s.wave->bwdStart;
        l.bwdEnd = s.wave->bwdEnd;
    }
    return l;
}

void XtSynth::applyRates(OscVoice& s) {
    if (s.linear) { s.lin.setRates(s.curA, s.curWalk); s.lin.setGainXt(s.curGain); }
    else if (s.lite) { s.xfg.setRates(s.curA, s.curF, s.curWalk, s.root); s.xfg.setGainXt(s.curGain); }
    else if (s.wave->type == 1) { s.solo.setRates(s.curA, s.curF, s.curWalk); s.solo.setEnergy(s.curG); s.solo.setGain(s.curGain); }
    else { s.mb.setRates(s.curA, s.curWalk); s.mb.setGainXt(s.curGain); }
}

void XtSynth::tvaNewBlock(Voice& v) const {
    const float l = v.tb.bus[XtBusDirL], r = v.tb.bus[XtBusDirR];
    v.tb = tvaBlock(v);
    v.tb.bus[XtBusDirL] = l;
    v.tb.bus[XtBusDirR] = r;
}

// A TVF's block (0x894dd870) at the voice's key, with the velocity factors
// of its envelopes (A: the Cutoff's depth, Env 9 or 11; B: the Q's, Env 10
// or 12).
XtTvfBlock XtSynth::cosmBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtTvfParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.filter = c.filter; p.q = c.reso + m[xtmx::CosmPrm2]; p.qLfo = c.resoLfo + m[xtmx::CosmLfoPrm2]; p.qEnv = c.resoEnv;
    p.cutoff = c.cutoff + m[xtmx::CosmPrm1]; p.keyfollow = c.kf / 10; p.cutoffLfo = c.cutoffLfo + m[xtmx::CosmLfoPrm1]; p.cutoffEnv = c.cutoffEnv;
    p.qOwn = c.resoOwn; p.cutoffOwn = c.cutoffOwn;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return v.cs[k].type == 1 ? xtDtvfBlock(p, v.note, va, vb) : xtTvfBlock(p, v.note, va, vb);
}

// The Matrix's sums on a COSM (0x894de400…): four for each section, added to the knobs each type names; ahead of the
// clamps its converter has.  A Detune ranges ±100 and a Balance 0…100, so those take the sum ·200/127 and ·100/127.

// A Dual Filter's block (0x894dd690): Filter 1's Env depth by A, Filter 2's by B.
XtDualBlock XtSynth::dualBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtDualParams p;
    p.type = c.dualType;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.freq1 = c.freq1 + m[xtmx::CosmPrm1]; p.kf1 = c.kf1 / 10; p.q1 = c.q1; p.lfo1 = c.lfo1 + m[xtmx::CosmLfoPrm1]; p.env1 = c.env1;
    p.freq2 = c.freq2 + m[xtmx::CosmPrm2]; p.kf2 = c.kf2 / 10; p.q2 = c.q2; p.lfo2 = c.lfo2 + m[xtmx::CosmLfoPrm2]; p.env2 = c.env2;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtDualBlock(p, v.note, va, vb);
}

// A TB Filter's block (0x894de110): the LPF's Env depth by A, the HPF's by B.
XtTbBlock XtSynth::tbBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtTbParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.lpfCutoff = c.tbCutoff + m[xtmx::CosmPrm1]; p.lpfKf = c.tbKf / 10; p.lpfLfo = c.tbLfo + m[xtmx::CosmLfoPrm1]; p.lpfEnv = c.tbEnv; p.q = c.tbQ + m[xtmx::CosmPrm2]; p.qFollow = c.tbQFollow;
    p.hpfCutoff = c.tbHpf; p.hpfKf = c.tbHpfKf / 10; p.hpfLfo = c.tbHpfLfo; p.hpfEnv = c.tbHpfEnv; p.gain = c.tbGain;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtTbBlock(p, v.note, va, vb);
}

// A Comb Filter's block (0x894dd4f0): Tone's Env depth by A, Detune's by B.
XtCombBlock XtSynth::combBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtCombParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.octave = c.combOctave; p.detune = c.combDetune + 200 * m[xtmx::CosmPrm2] / 127; p.detuneLfo = c.combDetuneLfo + m[xtmx::CosmLfoPrm2]; p.detuneEnv = c.combDetuneEnv;
    p.tone = c.combTone + m[xtmx::CosmPrm1]; p.toneLfo = c.combToneLfo + m[xtmx::CosmLfoPrm1]; p.toneEnv = c.combToneEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtCombBlock(p, va, vb);
}

// A Side Band Filter's block (0x894dd0a0, 0x894dd2c0): Width's Env depth by
// A, Detune's by B.
XtSbfBlock XtSynth::sbfBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    const bool second = v.cs[k].type == 6;
    const Controls::Cosm::SideBand& s = c.sb[second ? 1 : 0];
    XtSbfParams p;
    p.second = second;
    p.hpf = s.hpf; p.q = s.q; p.lpf = s.lpf; p.lpfKf = s.lpfKf / 10; p.subDetune = s.subDetune; p.octave = s.octave;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.detune = s.detune + 200 * m[xtmx::CosmPrm2] / 127; p.detuneLfo = s.detuneLfo + m[xtmx::CosmLfoPrm2]; p.detuneOwn = s.detuneOwn; p.detuneEnv = s.detuneEnv;
    p.width = s.width + m[xtmx::CosmPrm1]; p.widthKf = s.widthKf / 10; p.widthLfo = s.widthLfo + m[xtmx::CosmLfoPrm1]; p.widthOwn = s.widthOwn; p.widthEnv = s.widthEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtSbfBlock(p, v.note, va, vb);
}

// A Resonator's block (0x894dcf00): Size's Env depth by A, Balance's by B.
XtResBlock XtSynth::resBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtResParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.type = c.resType; p.size = c.resSize + m[xtmx::CosmPrm1]; p.sizeKf = c.resSizeKf / 10; p.sizeLfo = c.resSizeLfo + m[xtmx::CosmLfoPrm1]; p.sizeEnv = c.resSizeEnv;
    p.balance = c.resBalance + 100 * m[xtmx::CosmPrm2] / 127; p.balanceLfo = c.resBalanceLfo + m[xtmx::CosmLfoPrm2]; p.balanceEnv = c.resBalanceEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtResBlock(p, v.note, va, vb);
}

// A Frequency Shifter's block (0x894ddde0): Depth's Env depth by A,
// Balance's by B.
XtFsBlock XtSynth::fsBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtFsParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.depth = c.fsDepth + m[xtmx::CosmPrm1]; p.depthKf = c.fsDepthKf / 10; p.depthLfo = c.fsDepthLfo + m[xtmx::CosmLfoPrm1]; p.depthEnv = c.fsDepthEnv;
    p.balance = c.fsBalance + 100 * m[xtmx::CosmPrm2] / 127; p.balanceLfo = c.fsBalanceLfo + m[xtmx::CosmLfoPrm2]; p.balanceEnv = c.fsBalanceEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtFsBlock(p, v.note, va, vb);
}

// A Lo-Fi block (0x894ddf80): the Filter's Env depth by A, Balance's by B.
XtLofiBlock XtSynth::lofiBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtLofiParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.bitDown = c.lofiBit; p.rateDown = c.lofiRate; p.filter = c.lofiFilter + m[xtmx::CosmPrm1]; p.filterKf = c.lofiFilterKf / 10;
    p.filterLfo = c.lofiFilterLfo + m[xtmx::CosmLfoPrm1]; p.filterEnv = c.lofiFilterEnv;
    p.balance = c.lofiBalance + 100 * m[xtmx::CosmPrm2] / 127; p.balanceLfo = c.lofiBalanceLfo + m[xtmx::CosmLfoPrm2]; p.balanceEnv = c.lofiBalanceEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtLofiBlock(p, v.note, va, vb);
}

// A Compressor's block (0x894ddc50).
XtCompBlock XtSynth::compBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtCompParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.attack = c.compAttack + m[xtmx::CosmPrm1]; p.sustain = c.compSustain + m[xtmx::CosmPrm2]; p.level = c.compLevel;
    return xtCompBlock(p);
}

// A Wave Shape block (0x894dcbf0): Drive's Env depth by A, Tone's by B.
XtWsBlock XtSynth::wsBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtWsParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.type = c.wsType; p.level = c.wsLevel; p.drive = c.wsDrive + m[xtmx::CosmPrm1]; p.driveLfo = c.wsDriveLfo + m[xtmx::CosmLfoPrm1]; p.driveEnv = c.wsDriveEnv;
    p.tone = c.wsTone + m[xtmx::CosmPrm2]; p.toneKf = c.wsToneKf / 10; p.toneLfo = c.wsToneLfo + m[xtmx::CosmLfoPrm2]; p.toneEnv = c.wsToneEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtWsBlock(p, v.note, va, vb);
}

// An OD/Dist block (0x894dc8b0): Drive's Env depth by A, Tone's by B.
XtOdBlock XtSynth::odBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtOdParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.type = c.odType; p.level = c.odLevel; p.drive = c.odDrive + m[xtmx::CosmPrm1]; p.driveLfo = c.odDriveLfo + m[xtmx::CosmLfoPrm1]; p.driveEnv = c.odDriveEnv;
    p.tone = c.odTone + m[xtmx::CosmPrm2]; p.toneKf = c.odToneKf / 10; p.toneLfo = c.odToneLfo + m[xtmx::CosmLfoPrm2]; p.toneEnv = c.odToneEnv;
    const float va = xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve), vb = xtCosmVelocity(v.vel, c.envB.velSens, c.envB.velCurve);
    return xtOdBlock(p, v.note, va, vb);
}

// An Amp Simulator block (0x894dcd80): Gain's Env depth by A.
XtAmpBlock XtSynth::ampBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtAmpParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.type = c.ampType; p.bass = c.ampBass; p.middle = c.ampMiddle; p.treble = c.ampTreble + m[xtmx::CosmPrm2]; p.level = c.ampLevel;
    p.gain = c.ampGain + m[xtmx::CosmPrm1]; p.gainLfo = c.ampGainLfo + m[xtmx::CosmLfoPrm1]; p.gainEnv = c.ampGainEnv;
    return xtAmpBlock(p, xtCosmVelocity(v.vel, c.envA.velSens, c.envA.velCurve));
}

// A Limiter's block (0x894ddd20).
XtLimBlock XtSynth::limBlock(const Voice& v, int k) const {
    const Controls& Z = zc(v);
    const Controls::Cosm& c = Z.cosm[k];
    XtLimParams p;
    const int16_t* m = v.mx + (k ? xtmx::Cosm2 : xtmx::Cosm);
    p.threshold = c.limThreshold; p.ratio = c.limRatio; p.attack = c.limAttack + m[xtmx::CosmPrm1]; p.release = c.limRelease + m[xtmx::CosmPrm2]; p.level = c.limLevel;
    return xtLimBlock(p);
}

// The TVF's slot 1 is its Q (envB), the Dual Filter's Filter 1, the TB
// Filter's LPF, the Resonator's Size, the Frequency Shifter's Depth, the
// Lo-Fi Filter, Wave Shape's and OD/Dist's Drive and the Amp's Gain (envA),
// the Comb and Side Band
// Filters' Detune (envB).
void XtSynth::cosmSetBlock(Voice& v, int k) {
    const Controls& Z = zc(v);
    Voice::CosmVoice& c = v.cs[k];
    if (c.type == 4) { c.comb.setBlock(combBlock(v, k), c.heldB, c.heldLfo, c.heldA, c.heldLfo); return; }
    if (c.type == 5 || c.type == 6) { c.sbf.setBlock(sbfBlock(v, k), c.heldB, c.heldLfo, c.heldA, c.heldLfo); return; }
    if (c.type == 7) { c.res.setBlock(resBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 8) { c.fs.setBlock(fsBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 9) { c.lofi.setBlock(lofiBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 10) { c.comp.setBlock(compBlock(v, k)); return; }
    if (c.type == 11) { c.lim.setBlock(limBlock(v, k)); return; }
    if (c.type == 12) { c.ws.setBlock(wsBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 13) { c.od.setBlock(odBlock(v, k), c.heldA, c.heldLfo, c.heldB, c.heldLfo); return; }
    if (c.type == 14) { c.amp.setBlock(ampBlock(v, k), c.heldA, c.heldLfo); return; }
    if (c.type == 15) { c.spk.setBlock({ Z.cosm[k].spkType }); return; }
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

void XtSynth::liveUpdate(Voice& v, const Controls& c, const Controls& was, bool anyChanged) {
    const bool tvaChanged = c.tvaOn != was.tvaOn;
    const bool envChanged = tvaChanged || c.attack != was.attack || c.decay != was.decay || c.sustain != was.sustain || c.release != was.release
                         || c.aSens != was.aSens || c.dSens != was.dSens || c.rSens != was.rSens || c.timeKF != was.timeKF;
    // what changed for each OSC.  The tone update also gives the voices' pitch
    // ramps the new portamento settings (0x894ebd34 → 0x894f48b0) and, when the
    // key-follow key moved (Coarse, Pitch KF), glides again from the voice's
    // source key (0x894f2600)
    struct OscDiff { bool penv, tenv, fenv, oenv, key; };
    auto oscDiff = [&](const OscControls& n, const OscControls& o) {
        OscDiff d;
        d.penv = n.pAttack != o.pAttack || n.pDecay != o.pDecay || n.pSustain != o.pSustain || n.pRelease != o.pRelease || n.pVel != o.pVel;
        d.tenv = n.tAttack != o.tAttack || n.tDecay != o.tDecay || n.tSustain != o.tSustain || n.tRelease != o.tRelease || n.tVel != o.tVel;
        d.fenv = n.fAttack != o.fAttack || n.fDecay != o.fDecay || n.fSustain != o.fSustain || n.fRelease != o.fRelease || n.fVel != o.fVel;
        d.oenv = n.oAttack != o.oAttack || n.oDecay != o.oDecay || n.oSustain != o.oSustain || n.oRelease != o.oRelease
              || n.oASens != o.oASens || n.oDSens != o.oDSens || n.oRSens != o.oRSens || n.oTimeKF != o.oTimeKF;
        d.key = n.oscCoarse != o.oscCoarse || c.patchCoarse != was.patchCoarse || n.pitchKF != o.pitchKF || c.pitchSemis != was.pitchSemis;
        return d;
    };
    const OscDiff d1 = oscDiff(c, was), d2 = oscDiff(c.osc2, was.osc2);
    const bool portaChanged = c.portamento != was.portamento || c.portaTime != was.portaTime || c.portaVeloSens != was.portaVeloSens
                           || c.portaType != was.portaType;
    // Any change is a tone update (0x894dc0b0), and it gives the sounding voices' COSM
    // sections (0x894ed8f0) and MOD (0x894f1c60) a new block, whatever moved: a block's
    // targets take the envelopes' levels without their rates, and the MOD's clears an
    // old note's sync flag, so it is heard even with no COSM or MOD knob moved.
    // Their envelopes new settings.
    auto envDiff = [](const Controls::CosmEnv& a, const Controls::CosmEnv& b) {
        return a.attack != b.attack || a.decay != b.decay || a.sustain != b.sustain || a.release != b.release || a.aSens != b.aSens
            || a.dSens != b.dSens || a.rSens != b.rSens || a.timeKF != b.timeKF;
    };
    bool cenvAChanged[2], cenvBChanged[2];
    for (int k = 0; k < 2; ++k) {
        const Controls::Cosm &n = c.cosm[k], &o = was.cosm[k];
        cenvAChanged[k] = envDiff(n.envA, o.envA);
        cenvBChanged[k] = envDiff(n.envB, o.envB);
    }
    const bool moved = anyChanged && matrixUpdate(v);   // a sum moved: each setting as a tone update gives it
    if (anyChanged && c.on) v.blockTempo = tempoNow();
    if (envChanged || moved) v.env.update(envParams(v));
    if (anyChanged) tvaNewBlock(v);                // the velocity as latched
    auto oscLive = [&](OscVoice& s, const OscControls& o, const OscDiff& d) {
        if (d.penv || moved) s.penv.update(oscEnvParams(v, s, 0, o.pAttack, o.pDecay, o.pSustain, o.pRelease));
        if (d.tenv || moved) s.tenv.update(oscEnvParams(v, s, 1, o.tAttack, o.tDecay, o.tSustain, o.tRelease));
        if (d.fenv || moved) s.fenv.update(oscEnvParams(v, s, 2, o.fAttack, o.fDecay, o.fSustain, o.fRelease));
        if (d.oenv || moved) s.oenv.update(ampEnvParams(v, o));
        oscFollows(v, s, o, portaChanged || moved, d.key || moved);   // a term may have moved them
    };
    if (v.on) oscLive(v, c, d1);
    if (v.osc2.on) oscLive(v.osc2, c.osc2, d2);
    if (anyChanged) { oscBeat(v, c); oscBeat(v.osc2, c.osc2); }
    if (anyChanged) v.mod.setBlock(modBlk[v.zone]);
    for (int k = 0; k < 2; ++k) {
        Voice::CosmVoice& cv = v.cs[k];
        if (!cv.on) continue;
        // the block first (0x894dba00: 0x894f5d10, then 0x894f5cc0), on the held levels:
        // a Sustain moved reaches it at the next tick, however many updates come first
        if (anyChanged) cosmNewBlock(v, k);
        if (cenvAChanged[k] || moved) cv.envA.update(cosmEnvParams(v, k, c.cosm[k].envA));
        if (cenvBChanged[k] || moved) cv.envB.update(cosmEnvParams(v, k, c.cosm[k].envB));
    }
    if (anyChanged) lfoUpdates(v);
}

int XtSynth::shiftKey(int key) const {
    return std::clamp(key + 12 * ctl.octave, 0, 127);
}

// Playback Mode LEGATO (0x894f2d00): the voice takes the earlier note's whole player (its +0xd4…+0x87c), its own pitch, formant,
// walk, Energy and gain target (+0x144…+0x154) put back, and the gain from 0 (+0x158).  Not if that player is done (its +4).
void XtSynth::legatoTake(OscVoice& s, const OscVoice& from) {
    s.staleEnd = from.staleEnd;                     // the copy's +4
    if (s.analog) s.ana = from.ana;
    else if (s.linear) s.lin = from.lin;
    else if (s.lite) s.xfg = from.xfg;
    else if (s.wave->type == 1) s.solo = from.solo;
    else s.mb.takeState(from.mb);
    if (s.analog) return;
    if (s.linear) s.lin.setGainXt(0.0f, true);
    else if (s.lite) s.xfg.setGainXt(0.0f, true);
    else if (s.wave->type == 1) s.solo.resetGain(0.0f);
    else s.mb.setGainXt(0.0f, true);
    if (s.lite && !s.linear) { s.xfg.takeRates(s.curA, s.curF, s.curWalk); s.xfg.setGainXt(s.curGain); }   // its filter's rate is the player's, kept
    else applyRates(s);
}

void XtSynth::startVoice(int raw, int note, int vel, int zone) {
    const Controls& Z = zc(zone);                      // the zone's, all through the note's start
    xtModBlock(Z.mod, modBlk[zone]);                 // the tables may have loaded since the last knob moved
    // portamento's source key (0x894e4940): the previous key plus the Octave
    // Shift; with LEGATO the note's own (no glide) unless a key was held
    // then (POLY: or Hold is down)
    const bool held = Z.mono ? keys.prevMonoHeld : keys.prevPolyHeld || hold >= 64;
    const int srcKey = Z.portaMode == 1 && !held ? note : shiftKey(Z.mono ? keys.prevMono : keys.prevPoly);
    Voice* v = nullptr;
    for (Voice& w : voice) if (!w.active) { v = &w; break; }
    if (!v) { v = &voice[0]; for (Voice& w : voice) if (w.age < v->age) v = &w; }   // steal the oldest
    // the voice's stop (0x895053c0) frees FEEDBACK-OSC's line; only a note-on can
    // tell when it happened
    for (Voice& w : voice) if (!w.active) { w.ana.release(); w.osc2.ana.release(); }
    v->ana.release();
    v->osc2.ana.release();
    // The allocator's earlier note of the part and zone (0x894d4c04), among those not released, oldest first: the first whose key is
    // down, else the last.  The new note takes its chain key (+0x6c4), and with Playback Mode LEGATO and its key down, its player.
    const Voice* prev = nullptr;
    bool prevHeld = false;
    {
        const Voice* order[kVoices];
        int n = 0;
        for (const Voice& w : voice)
            if (&w != v && w.active && !w.releasing && w.zone == zone) order[n++] = &w;
        std::sort(order, order + n, [](const Voice* a, const Voice* b) { return a->age < b->age; });
        for (int i = 0; i < n && !prevHeld; ++i) { prev = order[i]; prevHeld = (order[i]->keyFlags & 1) != 0; }
    }
    v->chainKey = prev ? prev->chainKey : note;
    v->steps = stepCount[zone][0]++;                 // the per-note OSC setup (0x894e4940): the count, then one more
    v->osc2.steps = stepCount[zone][1]++;
    v->active = true;
    v->zone = zone;
    v->releasing = false;
    v->damping = false;
    v->note = note;
    v->raw = raw;
    v->srcKey = srcKey;
    v->age = clock++;
    v->structure = std::clamp(Z.structure, 0, 2);
    v->vel = vel;
    v->relVel = 64;
    v->keyFlags = 7;
    v->mVel = vel; v->mKey = note; v->mRaw = raw;
    matrixNoteOn(*v);
    v->tempo10 = tempo10Now();
    if (Z.on) v->blockTempo = tempoNow();
    v->on = oscPlays(zone, 0);
    v->osc2.on = oscPlays(zone, 1);
    v->analog = Z.oscType == 0;
    v->osc2.analog = Z.osc2.oscType == 0;
    // LEGATO taken up: the PCM converter's tail (0x894e2dac) sets the block's +0x64 with the earlier note's key down.  It also picks a
    // wave set's wave at the chain's key (0x894e1570), which a single wave doesn't have
    const bool legato1 = prevHeld && Z.playbackMode == 1 && v->on && !v->analog, legato2 = prevHeld && Z.osc2.playbackMode == 1 && v->osc2.on && !v->osc2.analog;
    v->wave = waveOf(zone, 0);
    v->osc2.wave = waveOf(zone, 1);
    v->robotKnob = Z.robot && v->on && !v->analog && v->wave->type == 1;   // SOLO only (the converter's SOLO tail)
    v->osc2.robotKnob = Z.osc2.robot && v->osc2.on && !v->osc2.analog && v->osc2.wave->type == 1;
    v->robot = v->robotKnob && Z.vari;                                     // the block's flag 0x10 takes Vari Sw on too
    v->osc2.robot = v->osc2.robotKnob && Z.osc2.vari;
    v->lite = v->on && !v->analog && v->wave->type == 0;
    v->linear = !Z.vari && v->on && !v->analog && (v->wave->type == 1 || v->lite);
    v->variBit = Z.vari;
    v->osc2.variBit = Z.osc2.vari;
    v->syncBit = Z.tempoSync;
    v->osc2.syncBit = Z.osc2.tempoSync;
    v->osc2.lite = v->osc2.on && !v->osc2.analog && v->osc2.wave->type == 0;
    // A LITE voice's root frequency (+0x9a4), which its Vari-on player's period and filter go by
    if (v->lite) v->root = xtCosmRootHz(Z.rootKey, false, v->wave->tune.offset, v->wave->tune.detune);
    if (v->osc2.lite) v->osc2.root = xtCosmRootHz(Z.osc2.rootKey, false, v->osc2.wave->tune.offset, v->osc2.wave->tune.detune);
    v->osc2.linear = !Z.osc2.vari && v->osc2.on && !v->osc2.analog && (v->osc2.wave->type == 1 || v->osc2.lite);
    // the OSCs' random pitch (0x894e4940, OSC 1 then 2, each whatever its
    // Switch): the shared generator steps once per note for an OSC with a depth
    v->rnd = pitchRandom.next(xtRandomPitchDepth(Z.randomPitch));
    v->osc2.rnd = pitchRandom.next(xtRandomPitchDepth(Z.osc2.randomPitch));
    v->velGain = xtVelGain(v->vel, Z.ampVelSens, Z.ampVelCurve);
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
    // a key-unsynced LFO starts from the newest other tone of the part and zone sounding, if
    // there is one (the tones' lists at 0x89f91c50/54, newest first, by the tone's +0x1c), else
    // from its zone's free-running phase
    const Voice* other = nullptr;
    for (const Voice& w : voice)
        if (&w != v && w.active && w.zone == zone && (!other || w.age > other->age)) other = &w;
    oscLfoNoteOn(*v, *v, Z, freeLfo[zone], other ? &other->lfo : nullptr, seed[1]);
    oscLfoNoteOn(*v, v->osc2, Z.osc2, freeLfo2[zone], other ? &other->osc2.lfo : nullptr, seed[2]);
    if (Z.tvaLfoKeySync) v->tlfo.noteOn(tlfoParams(*v), seed[5], v->blockTempo);
    else if (other) v->tlfo.noteOnFollow(tlfoParams(*v), other->tlfo);
    else v->tlfo.noteOnFree(tlfoParams(*v), seed[5], freeTlfo[zone], v->blockTempo);
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
    std::copy(std::begin(v->tb.bus), std::end(v->tb.bus), v->bus);
    v->elapsed = 0;
    if (v->on) oscEnvs(*v, *v, Z, true);
    if (v->osc2.on) oscEnvs(*v, v->osc2, Z.osc2, true);
    // the COSM sections (0x894edf70, COSM 1 then 2): their
    // envelopes and LFOs start with the others; the filters' states clear and
    // their coefficients start from the envelopes' start levels
    // the voices' stereo flags as their setups leave them: a stereo wave's on a PCM OSC, and
    // none on an analog voice (0x89505170 clears it) or an OSC that is off
    auto stereo = [](const OscVoice& s) { return s.on && !s.analog && s.wave->channels == 2; };
    for (int k = 0; k < 2; ++k) {
        Voice::CosmVoice& c = v->cs[k];
        c.holdWords();
        c.on = Z.cosm[k].on;
        c.stereo = stereo(xtCosmOsc(v->structure, k) ? v->osc2 : *v);   // the routines' flag (0x894eef70)
        if (Z.cosm[k].lfoKeySync) c.lfo.noteOn(clfoParams(*v, k), seed[3 + k], v->blockTempo);
        else if (other) c.lfo.noteOnFollow(clfoParams(*v, k), other->cs[k].lfo);
        else c.lfo.noteOnFree(clfoParams(*v, k), seed[3 + k], freeClfo[zone][k], v->blockTempo);
        if (!c.on) continue;
        c.type = Z.cosm[k].type;
        c.envA.noteOn(cosmEnvParams(*v, k, Z.cosm[k].envA));
        c.envB.noteOn(cosmEnvParams(*v, k, Z.cosm[k].envB));
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
            c.comp.noteOn(compBlock(*v, k));
        else if (c.type == 11)
            c.lim.noteOn(limBlock(*v, k));
        else if (c.type == 12)
            c.ws.noteOn(wsBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                        c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 13)
            c.od.noteOn(odBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out(),
                        c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out());
        else if (c.type == 14)
            c.amp.noteOn(ampBlock(*v, k), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
        else if (c.type == 15)
            c.spk.noteOn({ Z.cosm[k].spkType });
        else if (c.type >= 4)
            ;                                          // below, once the voice's pitch is set
        else
            c.tvf.noteOn(cosmBlock(*v, k), c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(), c.lfo.out(),
                         c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
    }
    if (v->on) targets(*v, *v, Z, v->curA, v->curF, v->curWalk, v->curG, true);
    else v->curA = 1.0f;                               // the note-on of an OSC that is off (0x894f2f80, kind 0)
    if (v->osc2.on) targets(*v, v->osc2, Z.osc2, v->osc2.curA, v->osc2.curF, v->osc2.curWalk, v->osc2.curG, true);
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
        const XtWaveTune tune = s.on && !s.analog && s.wave ? s.wave->tune : XtWaveTune{};
        const float root = xtCosmRootHz(second ? Z.osc2.rootKey : Z.rootKey, s.robot || s.analog || !s.on, tune.offset, tune.detune);   // the analog voice's root is 440 too (0x894f21c0)
        if (c.type == 4)
            c.comb.noteOn(combBlock(*v, k), s.curA, root, c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(),
                          c.lfo.out(), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
        else
            c.sbf.noteOn(sbfBlock(*v, k), s.curA, root, c.envB.onsetLevel(), c.envB.level(), c.envB.smooth(),
                         c.lfo.out(), c.envA.onsetLevel(), c.envA.level(), c.envA.smooth(), c.lfo.out());
    }
    // the voices' note-ons, OSC 1 then OSC 2 (0x894f21c0): SUPER-SAW's counter
    // and FEEDBACK-OSC's lines go in that order
    // LEGATO (0x894f6176…): each OSC whose earlier voice is of the same kind and mode takes its player, once its own note-on has run
    // (0x894f2d00) and not one at its wave's end, its +4 at 1 (0x894f2d24): a SOLO or band player's own flag, LITE's what it holds, and
    // Vari off's SOLO setup clears it, as the band and SOLO note-ons do, and nothing sets it there.  Vari Sw is the block's flag 0x20, not
    // the voice's mode, so across a Vari switch the XT finds a SOLO wave's two players alike and copies one into the other, which then reads
    // anywhere: here a player is taken only by its own kind (`linear`), as each player keeps its own seek (v151)
    const auto sameKind = [](const OscVoice& a, const OscVoice& b) {
        return a.on == b.on && a.analog == b.analog && a.lite == b.lite && a.linear == b.linear && (a.analog || !a.on || a.wave->type == b.wave->type);
    };
    const auto atEnd = [](const OscVoice& s) {
        const int f = s.lite ? s.staleEnd : s.linear ? 0 : s.wave->type == 1 ? s.solo.endFlag() : s.mb.endFlag();
        return (int8_t)f == 1;
    };
    if (v->on) oscStart(*v, *v, Z);
    if (legato1 && sameKind(*v, *prev) && !atEnd(*prev)) legatoTake(*v, *prev);
    if (v->osc2.on) oscStart(*v, v->osc2, Z.osc2);
    if (legato2 && sameKind(v->osc2, prev->osc2) && !atEnd(prev->osc2)) legatoTake(v->osc2, prev->osc2);
    // portamento (0x894f2440, after the voice setup): the pitch ramp from the
    // source key's cents to the note's; it moves from the next update on
    auto portaStart = [&](OscVoice& s, const OscControls& o) {
        s.kfKey = keyFollowKey(*v, o);
        s.porta.start(noteCents(*v, s, o, srcKey), noteCents(*v, s, o, note),
                      portaSettings(*v));
    };
    if (v->on) portaStart(*v, Z);
    if (v->osc2.on) portaStart(v->osc2, Z.osc2);
    // the MOD (0x894f1b00, 0x894f1d90)
    v->mod.noteOn(modBlk[zone], stereo(*v), stereo(v->osc2), v->on && v->analog, modShared, (int)(v - voice));
}

// An OSC's LFO at note-on, whatever its Switch: every LFO of the tone starts
// (0x894f5e60), and a key-unsynced one sets its free-running phase's rate.
void XtSynth::oscLfoNoteOn(const Voice& v, OscVoice& s, const OscControls& o, XtLfoFree& free, const XtLfo* other, float seed) {
    if (o.lfoKeySync) s.lfo.noteOn(lfoParams(v, s, o), seed, v.blockTempo);
    else if (other) s.lfo.noteOnFollow(lfoParams(v, s, o), *other);
    else s.lfo.noteOnFree(lfoParams(v, s, o), seed, free, v.blockTempo);
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
        if (&s == &v.osc2 && modBlk[v.zone].type == 4 && b.code == 10) b.code = 2;   // the tone's note-on (0x894f6148): under OSC SYNC OSC 2 plays LA-SAW as SAW
        s.ana.noteOn(b, s.curA, s.curGain, gainStart, s.fenv.level(), s.tenv.level(), s.lfo.out(), anaShared);
    }
    else if (s.linear) {
        s.lin.setLoop(waveLoop(s, o));
        s.lin.setStart(waveStart(s, o));
        s.liteStart = waveStart(s, o);
        s.lin.setLite(s.lite);
        s.lin.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.lin.setBeatFlags(o.tempoSync, tripNow);
        s.lin.noteOn(s.wave->solo.in, s.wave->solo.inR, s.curA, s.curWalk);
        s.lin.setGainXt(gainStart, true);
        s.lin.setGainXt(s.curGain);
    }
    else if (s.lite) {                                // Vari on (0x8950b8d0)
        s.xfg.setLoop(waveLoop(s, o));
        s.xfg.setStart(waveStart(s, o));
        s.liteStart = waveStart(s, o);
        s.xfg.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.xfg.setBeatFlags(o.tempoSync, tripNow);
        s.xfg.noteOn(s.wave->solo.in, s.wave->solo.inR, s.curA, s.curF, s.curWalk, s.root);
        s.xfg.setGainXt(gainStart, true);
        s.xfg.setGainXt(s.curGain);
    }
    else if (s.wave->type == 1) {
        int first = 0, last = 0;
        s.solo.setRobot(s.robot);
        s.solo.setLoop(waveLoop(s, o));
        if (waveEvents(v, s, o, first, last)) s.solo.setEvents(first, last);
        else s.solo.setStart(waveStart(s, o));
        s.solo.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.solo.setBeatFlags(o.tempoSync, tripNow);
        s.solo.noteOn(s.wave->solo, s.curA, s.curF, s.curWalk, s.curG);
        s.solo.resetGain(gainStart);
        s.solo.setGain(s.curGain);
    }
    else {
        // STEP and EVENT play between two events with no loop: their tails clear the block's flags 0x01 (the Loop), 0x80 and 0x08,
        // which the player keeps apart from the loop's cached events (setEvents)
        int first = 0, last = 0;
        s.mb.setLoop(waveLoop(s, o));
        if (waveEvents(v, s, o, first, last)) s.mb.setEvents(first, last);
        else s.mb.setStart(waveStart(s, o));
        s.mb.setVari(o.vari);
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
        const Controls& Z = zc(v);                     // its own zone's
        v.note = key;
        v.raw = keys.cur;
        v.srcKey = shiftKey(keys.prevMono);
        v.vel = vel;
        v.keyFlags = 7;
        matrixUpdate(v);                               // for the pad, which moves its source with no update of its own
        v.rnd = pitchRandom.next(xtRandomPitchDepth(Z.randomPitch));
        v.osc2.rnd = pitchRandom.next(xtRandomPitchDepth(Z.osc2.randomPitch));
        v.velGain = xtVelGain(v.vel, Z.ampVelSens, Z.ampVelCurve);
        tvaNewBlock(v);
        if (Z.on) v.blockTempo = tempoNow();
        v.env.update(envParams(v));
        if (v.on) oscEnvs(v, v, Z, false);
        if (v.osc2.on) oscEnvs(v, v.osc2, Z.osc2, false);
        oscBeat(v, Z);
        oscBeat(v.osc2, Z.osc2);
        v.mod.setBlock(modBlk[v.zone]);
        for (int k = 0; k < 2; ++k) {
            if (!v.cs[k].on) continue;
            cosmNewBlock(v, k);                        // as in setControls, before the envelopes' update
            v.cs[k].envA.update(cosmEnvParams(v, k, Z.cosm[k].envA));
            v.cs[k].envB.update(cosmEnvParams(v, k, Z.cosm[k].envB));
        }
        lfoUpdates(v);
        if (v.on) oscFollows(v, v, Z, true, true);
        if (v.osc2.on) oscFollows(v, v.osc2, Z.osc2, true, true);
    }
}

// Truncate (0x894d5ba0 → 0x894f5500): the TVA holds its gain and fades out
// linearly over the Truncate Time (0x51f, 0.70 ms; 0x894f5570).
void XtSynth::truncate(Voice& v) {
    if (v.damping) return;
    v.damping = true;
    v.damp = 1.0f;
}

// The part's note-off (0x894dbea0): the key state (0x894d63c0); a MONO part
// whose sounding key was released while others are held takes the highest
// of them (Legato on or off).  Then the keys' release for the notes on that key (0x894d4e60).
void XtSynth::tgNoteOff(int note, float velocity) {
    keys.release(note);
    const int r = keys.noteOff(note);
    if (ctl.mono && r >= 0)
        for (const Voice& v : voice)
            if (v.active && !v.releasing && !v.damping) { legatoNote(shiftKey(r), v.vel); break; }
    lastRelease = (int)std::lround(std::clamp(velocity, 0.0f, 1.0f) * 127.0f);
    for (Voice& v : voice)
        if (v.active && v.raw == note && !v.releasing && !v.damping) keyRelease(v, lastRelease);
}

// A key let go (0x894d58b0).  Hold, down now, keeps the note; so does Sostenuto if this key
// was held when it went down.  The release velocity is latched here, for whenever the note stops.
void XtSynth::keyRelease(Voice& v, int velocity) {
    if (!(v.keyFlags & 1)) return;
    if (hold < 64) v.keyFlags &= ~2;
    const uint16_t bit = (uint16_t)(1u << (v.raw & 15));
    if (!(sost >= 64 && (sostKeys[(v.raw & 127) >> 4] & bit))) v.keyFlags &= ~4;
    v.keyFlags &= ~1;
    v.relHeld = velocity;
    if (!(v.keyFlags & 7)) voiceOff(v);
}

// The note's release (0x894dc270).  The release time follows the note-off velocity (R-Sens):
// the times are converted again before the release starts.
void XtSynth::voiceOff(Voice& v) {
    const Controls& Z = zc(v);
    v.releasing = true;
    v.relVel = v.relHeld;
    matrixRelease(v);
    v.env.update(envParams(v));
    v.env.noteOff();
    v.tlfo.noteOff();
    auto oscRelease = [&](OscVoice& s, const OscControls& o) {
        s.penv.update(oscEnvParams(v, s, 0, o.pAttack, o.pDecay, o.pSustain, o.pRelease));
        s.penv.noteOff();
        s.tenv.update(oscEnvParams(v, s, 1, o.tAttack, o.tDecay, o.tSustain, o.tRelease));
        s.tenv.noteOff();
        s.fenv.update(oscEnvParams(v, s, 2, o.fAttack, o.fDecay, o.fSustain, o.fRelease));
        s.fenv.noteOff();
        s.oenv.update(ampEnvParams(v, o));
        s.oenv.noteOff();
        s.lfo.noteOff();
    };
    if (v.on) oscRelease(v, Z);
    if (v.osc2.on) oscRelease(v.osc2, Z.osc2);
    for (int k = 0; k < 2; ++k) {
        Voice::CosmVoice& c = v.cs[k];
        if (!c.on) continue;
        c.envA.update(cosmEnvParams(v, k, Z.cosm[k].envA));
        c.envA.noteOff();
        c.envB.update(cosmEnvParams(v, k, Z.cosm[k].envB));
        c.envB.noteOff();
        c.lfo.noteOff();
    }
}

// The panic of the plugin and the check, not a message of the XT's: everything stops at once.
void XtSynth::tgAllNotesOff() {
    for (Voice& v : voice) v.active = false;
    keysCleared();
    hold = sost = 0;
    std::fill(std::begin(sostKeys), std::end(sostKeys), (uint16_t)0);
}

// What the mode messages do to the key state (0x894d6040, 0x894d6f80): no key is held.
void XtSynth::keysCleared() {
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

// The channel's controllers at power-on (0x89475290): a patch sounds as stored at these.
void XtSynth::ccDefaults() {
    std::fill(std::begin(cc), std::end(cc), 0);
    cc[7] = cc[11] = cc[91] = cc[93] = 127;
    cc[10] = 64;
}

// A controller.  The channel takes the value if it keeps that number (0x89475370), then its handler
// runs (0x894d6f10; the table is built at 0x894d7400).  Nearly all of them are the plain handler,
// 0x894d7760, an update of the part's notes (0x894d4fe0), whatever the number and whether or not the
// value moved: the same pass as the bender's.  It is queued (0x894dc2c0), so it runs after the rest.
void XtSynth::tgController(int number, int value) {
    if (number < 0 || number > 127) return;
    const int x = std::clamp(value, 0, 127);
    if (number == 121 && x == 0) {                 // the channel's part of Reset All Controllers (0x89475200)
        for (int n : { 1, 2, 11, 16, 17, 18, 19, 34, 48, 49, 50, 51, 64, 66, 67, 69, 80, 81, 82, 83 })
            cc[n] = n == 11 ? 127 : 0;
        bender = 8192;
        pressure = 0;
        std::fill(std::begin(polyPress), std::end(polyPress), 0);
    }
    if (number < 96) cc[number] = x;
    // the mode messages: no update, and only at these values
    const bool cleared = number == 120 || (number >= 123 && number <= 125) || number == 127 ? x == 0 : number == 126 && x <= 16;
    if (number == 120 || (number >= 123 && number <= 127)) {
        if (!cleared) return;
        keysCleared();
        for (Voice& v : voice) {
            if (!v.active || v.releasing || v.damping) continue;
            if (number == 120) truncate(v);            // All Sound Off (0x894d4f40)
            else keyRelease(v, lastRelease);           // the others (0x894d4f90)
        }
        return;
    }
    if (number == 64 || number == 121) {           // Hold (0x894d77f0); Reset All Controllers (0x894d7940) lets both go
        hold = number == 64 ? x : 0;
        if (hold < 64)                             // 0x894d4ec0, 0x894d5970
            for (Voice& v : voice)
                if (v.active && !v.releasing && !v.damping && (v.keyFlags & 2) && !(v.keyFlags & 1)) {
                    v.keyFlags &= ~2;
                    if (!(v.keyFlags & 7)) voiceOff(v);
                }
    }
    if (number == 66 || number == 121) {           // Sostenuto (0x894d7880): the keys held as it goes down (0x894d5fd0)
        const int y = number == 66 ? x : 0;
        if ((sost ^ y) & 0x40) {
            if (y >= 64) std::copy(std::begin(keys.held), std::end(keys.held), sostKeys);
            else std::fill(std::begin(sostKeys), std::end(sostKeys), (uint16_t)0);
        }
        sost = y;
        if (sost < 64)                             // 0x894d4f00, 0x894d59d0
            for (Voice& v : voice)
                if (v.active && !v.releasing && !v.damping && (v.keyFlags & 4) && !(v.keyFlags & 1)) {
                    v.keyFlags &= ~4;
                    if (!(v.keyFlags & 7)) voiceOff(v);
                }
    }
    padController(number, x);                      // then 0x894d6f10 gives it to the pad; the queued update is after that
    for (Voice& v : voice)
        if (v.active) newBlocks(v);
}

// The pressures: as a controller (0x894d6ff0, 0x894d7010), an update of the part's notes.
void XtSynth::channelPressure(int value) {
    pressure = std::clamp(value, 0, 127);
    for (Voice& v : voice)
        if (v.active) newBlocks(v);
}

void XtSynth::polyPressure(int key, int value) {
    polyPress[key & 127] = std::clamp(value, 0, 127);
    for (Voice& v : voice)
        if (v.active) newBlocks(v);
}

// The pad reaches a tone in two ways.  A step goes straight to its voices (0x894f4e10 →
// 0x894f7920 → 0x894f25a0), those with Time Trip on.  Touching and letting go are seen at the
// next tone update (input +0x63 and the object's +8, 0x894e477a), which for the sounding
// tones here is now.
void XtSynth::padSteps(int step) {
    for (Voice& v : voice) {
        if (!v.active) continue;
        auto give = [&](OscVoice& s) {
            if (!s.on || s.analog) return;
            if (s.linear) { if (s.lin.tripOn()) s.lin.setTrip(step); }
            else if (s.lite) { if (s.xfg.tripOn()) s.xfg.setTrip(step); }
            else if (s.wave->type == 1) { if (s.solo.tripOn()) s.solo.setTrip(step); }
            else if (s.mb.tripOn()) s.mb.setTrip(step);
        };
        give(v);
        give(v.osc2);
    }
}

// 0x894d8360, as every controller reaches it: X and then Y, each if the number is its assign, of which
// 0 and 32 are none (0x894d82e0).  The two can be the same controller, and it is then both.
void XtSynth::padController(int number, int value) {
    const auto assigned = [number](int a) { return a != 0 && a != 32 && a == number; };
    if (assigned(ctl.assign[10])) pad.setX(value);
    if (assigned(ctl.assign[11])) {
        int step = 0;
        if (pad.setY(value, step)) padSteps(step);
    }
}

// The plugin's knob: the pad's two controllers in one breath, on no controller's number
void XtSynth::timeTrip(int x, int y) {
    const bool was = pad.on();
    int step = 0;
    pad.setX(x);
    const bool moved = pad.setY(y, step);
    if (moved) padSteps(step);
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
    s.variBit = o.vari;                                // the voice's +0xb4, after a knob's update has worked out the walk (0x894f2788)
    s.syncBit = o.tempoSync;
    const bool sync = o.tempoSync, trip = o.timeTrip && pad.on();
    // A LITE wave's (0x8950bcc0, kind 1's) then gives its player the first and last samples from the voice's own block, and the loop from the
    // wave's record with its modes by the Loop bit, both as they stand: which after a LEGATO copy are this note's, not the earlier one's
    const XtLoop rec = s.lite ? waveLoop(s, o) : XtLoop {};
    if (s.linear) {
        if (o.beatKeep && s.lin.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.lin.setGainXt(0.0f); }
        s.lin.setBeatFlags(sync, trip);
        if (s.lite) s.lin.liteUpdate(rec, s.liteStart, (int32_t)s.wave->frames, s.curWalk);
    } else if (s.lite) {
        if (o.beatKeep && s.xfg.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.xfg.setGainXt(0.0f); }
        s.xfg.setBeatFlags(sync, trip);
        s.xfg.blockUpdate(rec, s.liteStart, (int32_t)s.wave->frames);
    } else if (s.wave->type == 1) {
        if (o.beatKeep && s.solo.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.solo.setGain(0.0f); }
        s.solo.setBeatFlags(sync, trip);
        s.solo.setRobot(s.robotKnob && o.vari);        // the block's byte, all but the Vari bit, to the player's (0x894f3fa8): Robot follows
        s.solo.setLoopBit(o.loop && !eventPlay(s, o)); // and the Loop bit, which Playback Mode's tails clear
    } else {
        if (o.beatKeep && s.mb.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.mb.setGainXt(0.0f); }
        s.mb.setBeatFlags(sync, trip);
        s.mb.setLoopBit(o.loop && !eventPlay(s, o));   // the block's byte to the player's (0x894f3fb6): the Loop bit, which Playback Mode's tails clear
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
void XtSynth::clockCount() {
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
    for (int z = 0; z < kZones; ++z) {
        freeLfo[z].clock(dt);
        freeLfo2[z].clock(dt);
        for (XtLfoFree& f : freeClfo[z]) f.clock(dt);
        freeTlfo[z].clock(dt);
    }
    for (Voice& v : voice) {
        if (!v.active) continue;
        auto oscClock = [&](OscVoice& s) {
            s.lfo.clock(dt);
            if (!s.on || s.analog) return;
            if (s.linear) { if (s.lin.syncOn()) s.lin.addClock(1); }
            else if (s.lite) { if (s.xfg.syncOn()) s.xfg.addClock(1); }
            else if (s.wave->type == 1) { if (s.solo.syncOn()) s.solo.addClock(1); }
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
    using namespace xtf;
    // this test does not ask whether the tone is truncated, so a finished voice truncated just before a tick stops here
    if (v.env.done() && v.cur[0] <= kSilent && v.cur[1] <= kSilent) { v.active = false; return; }
    if (v.damping) return;                                    // done when the fade is (0x894f5510)
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
    std::fill(std::begin(frame), std::end(frame), 0.0f);
    fxTempo.store(ctl.patchTempo, std::memory_order_relaxed);
    fxSix.store(clockExt ? clockSix : 0, std::memory_order_relaxed);
    // The tempo clock, here on the blocks; ahead of the tone update, as one choice of the two
    // tasks' order on the XT.  From outside: the timer's ms in this block first, then its clocks
    if (clockExt) {
        for (int n = msDue(); n > 0; --n) clockMs();
        for (int n = clocksDue(); n > 0; --n) { clockIn(); clockTick(); ++hostDone; }
    } else {
        for (int n = clocksDue(); n > 0; --n) { clockTick(); clockAcc -= (uint64_t)clockPeriod() * 44100; }
        clockAcc += kClockPerBlock;
    }
    arpBlock();                                   // the arpeggio's ms, then the ms timer on (XtSynthArp.cpp)
    hostAt += hostStep();
    ++clockBlocks;
    // The control clock: the tone update runs between blocks every 441 frames, and
    // the free-running LFOs step with it, a tone sounding or not.
    const bool tick = tickCount >= kTickFrames;
    if (tick) tickCount -= kTickFrames;
    tickCount += 32;
    if (tick) {
        for (int z = 0; z < kZones; ++z) {
            freeLfo[z].tick(1);
            freeLfo2[z].tick(1);
            for (XtLfoFree& f : freeClfo[z]) f.tick(1);
            freeTlfo[z].tick(1);
        }
    }
    if (!cur && std::none_of(std::begin(voice), std::end(voice), [](const Voice& v) { return v.active; })) { busOut(lr); return; }   // the effects' tails go on
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
                    // (The XT picks a SOLO wave's seek by Vari Sw as it stands, 0x894f2ea4.  Switched during the note, that runs one player's seek
                    // on the other's state, which leaves it reading anywhere: here each player keeps its own.)
                    if (s.linear) {
                        if (s.lin.gainNow() > kSilent) return;
                        s.lin.seek(s.lin.beat().beatPos >> 5);
                    } else if (s.lite) {
                        if (s.xfg.gainNow() > kSilent) return;
                        s.xfg.seek(s.xfg.beat().beatPos >> 5);
                    } else if (s.wave->type == 1) {
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
                } else if (s.lite) {   // kind 1's update (0x8950bbb0): the values as they are, and no Energy
                    s.curA = A; s.curF = F; s.curWalk = w; s.curGain = gt;
                    applyRates(s);
                } else {
                    s.curA = xtOscSmooth(s.curA, A);
                    s.curF = xtOscSmooth(s.curF, F);
                    s.curWalk = xtOscSmooth(s.curWalk, w);
                    s.curG = xtOscSmooth(s.curG, g);
                    s.curGain = xtOscSmooth(s.curGain, gt);
                    applyRates(s);
                }
            };
            if (v.on) oscUpdate(v, zc(v));
            if (v.osc2.on) oscUpdate(v.osc2, zc(v).osc2);
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
        // A PCM OSC's player, in the XT's units (int16 × 65536); the players' gains are the firmware's
        auto pcmBlock = [](OscVoice& s, float* lr) {
            if (s.linear) s.lin.block(lr);
            else if (s.lite) s.xfg.block(lr);
            else if (s.wave->type == 1) s.solo.renderXt(lr, 32);
            else s.mb.block(lr);
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
        // A tone nearly always goes on MULTI alone, and a level of 0 adds nothing to the bus.  So the other six only when one of them is in use.
        const auto otherBuses = [&v] {
            for (const int b : { XtBusMainL, XtBusMainR, XtBusReverb, XtBusChorus, XtBusDirL, XtBusDirR }) if (v.bus[b] != 0.0f || v.tb.bus[b] != 0.0f) return true;
            return false;
        };
        const bool others = otherBuses();
        // One sample on the bus: the channels through the gain as a and b; the sends take their mean.
        const auto onBus = [&v, others](float* f, float a, float b) {
            f[FrameMultiL] = xtf::add(f[FrameMultiL], xtf::mul(a, v.bus[XtBusMultiL]));
            f[FrameMultiR] = xtf::add(f[FrameMultiR], xtf::mul(b, v.bus[XtBusMultiR]));
            if (!others) return;
            f[FrameMainL] = xtf::add(f[FrameMainL], xtf::mul(a, v.bus[XtBusMainL]));
            f[FrameMainR] = xtf::add(f[FrameMainR], xtf::mul(b, v.bus[XtBusMainR]));
            f[FrameDirL] = xtf::add(f[FrameDirL], xtf::mul(a, v.bus[XtBusDirL]));
            f[FrameDirR] = xtf::add(f[FrameDirR], xtf::mul(b, v.bus[XtBusDirR]));
            const float mean = xtf::mul(xtf::add(a, b), 0.5f);
            f[FrameReverb] = xtf::add(f[FrameReverb], xtf::mul(mean, v.bus[XtBusReverb]));
            f[FrameChorus] = xtf::add(f[FrameChorus], xtf::mul(mean, v.bus[XtBusChorus]));
        };
        if (v.damping) {
            // truncated (0x894f5570): the gain and the bus's levels held, times a fade that
            // falls by 1/(Truncate Time·44.1) each sample; silent and done at 2^-16
            for (int i = 0; i < 32; ++i) {
                v.damp = xtf::sub(v.damp, truncateStep());
                if (!(v.damp > kSilent)) { v.damp = 0.0f; v.active = false; break; }
                onBus(frame + kFrameWords * i, xtf::mul(xtf::mul(tmp[2 * i], v.cur[0]), v.damp), xtf::mul(xtf::mul(tmp[2 * i + 1], v.cur[1]), v.damp));
            }
            continue;
        }
        // TVA render (0x894f5720): for each channel a step that would pass the target
        // within the block is cut to (target − gain)/32.5; gain applied, then stepped.
        // The output bus takes that at its own levels, each moving by a share of the gap as
        // the block began.  DIR's never move: they start at the block's, and that never changes.
        float s[2], e[XtBusDirL];
        for (int c = 0; c < 2; ++c) {
            const float d = xtf::sub(v.target[c], v.cur[c]);
            s[c] = std::fabs(xtf::mul(v.step[c], 32.0f)) > std::fabs(d) ? xtf::mul(d, kLand) : v.step[c];
        }
        for (int b = 0; b < XtBusDirL; ++b) e[b] = xtf::mul(xtf::sub(v.tb.bus[b], v.bus[b]), kSendGlide);
        for (int i = 0; i < 32; ++i) {
            onBus(frame + kFrameWords * i, xtf::mul(tmp[2 * i], v.cur[0]), xtf::mul(tmp[2 * i + 1], v.cur[1]));
            for (int c = 0; c < 2; ++c) v.cur[c] = xtf::add(v.cur[c], s[c]);
            v.bus[XtBusMultiL] = xtf::add(v.bus[XtBusMultiL], e[XtBusMultiL]);
            v.bus[XtBusMultiR] = xtf::add(v.bus[XtBusMultiR], e[XtBusMultiR]);
            if (others)
                for (const int b : { XtBusMainL, XtBusMainR, XtBusReverb, XtBusChorus }) v.bus[b] = xtf::add(v.bus[b], e[b]);
        }
        v.elapsed += 32.0f;
    }
    busOut(lr);
}

// What the plugin plays: through the effects if they're in, or the pairs that reach the outs.  Without them MULTI has no MFX, and the sends no effects, to go to.
// At the XT's level both ways: a full-scale wave, 2^31 in the frame, comes out at ⅛, as it leaves the chip (XtFxUnit), by Out Gain for TG's −24 and the chip's +6 dB.  A power of two moves no bits.
void XtSynth::busOut(float* lr) {
    if (XtBusSink* s = sinkOn ? sink.load(std::memory_order_acquire) : nullptr) { s->process(frame, lr, 32); return; }
    for (int i = 0; i < 32; ++i) {
        const float* f = frame + kFrameWords * i;
        lr[2 * i] = (f[FrameMultiL] + f[FrameMainL] + f[FrameDirL]) * 0x1p-34f;
        lr[2 * i + 1] = (f[FrameMultiR] + f[FrameMainR] + f[FrameDirR]) * 0x1p-34f;
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
//   XT_BUS="chorus,reverb,assign,parallel,outGain": Chorus and Reverb Send Levels, Output Assign, Output Mode, Out Gain for TG.
//   XT_LFO5="wave,rate,offset,delay,mode,fade,keysync": LFO 5, on its level and pan.
//   XT_CC="cc7,cc11,cc10": the controllers before the first note.
//   XT_CC_AT="frame,number,value[;frame,number,value…]": a controller at a frame (CC 64 Hold, …).
//   XT_MATRIX="source,dest1,sens1,dest2,sens2[;…]": the Matrix Controls, as the patch has them.
//   XT_ASSIGN="a0,…,a11": the system's controller assigns (Beam 2 L…Knob 2, Time Trip X, Y), a CC number or 0.
//   XT_PRESS="channel[,key,poly]": the pressures before the first note.
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
//   XT_ROBOT=1: Robot Voice.  XT_VARI=0, XT_VARI2=0: Vari Sw off on OSC 1, OSC 2.
//   XT_START=n, XT_START2=n: OSC 1's, OSC 2's Start Offset, n samples into the wave.
//   XT_PLAYBACK=n, XT_PLAYBACK2=n: OSC 1's, OSC 2's Playback Mode, 0 RETRIGGER, 1 LEGATO, 2 STEP, 3 EVENT.
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
    if (const char* e = std::getenv("XT_BUS")) {
        int parallel = 1;
        std::sscanf(e, "%d,%d,%d,%d,%d", &c.chorusSend, &c.reverbSend, &c.outputAssign, &parallel, &c.outGainTg);
        c.parallel = parallel != 0;
    }
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
        o.vari = !(std::getenv("XT_VARI2") && std::atoi(std::getenv("XT_VARI2")) == 0);
        if (const char* e = std::getenv("XT_START2")) o.startOffset = std::atoi(e);
        if (const char* e = std::getenv("XT_PLAYBACK2")) o.playbackMode = std::clamp(std::atoi(e), 0, 3);
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
    c.vari = !(std::getenv("XT_VARI") && std::atoi(std::getenv("XT_VARI")) == 0);
    if (const char* e = std::getenv("XT_START")) c.startOffset = std::atoi(e);
    if (const char* e = std::getenv("XT_PLAYBACK")) c.playbackMode = std::clamp(std::atoi(e), 0, 3);
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
    if (const char* e = std::getenv("XT_MATRIX")) {
        std::string t = e;
        for (int i = 0; i < 8 && !t.empty(); ++i) {
            XtMatrixControl& x = c.matrix[i];
            std::sscanf(t.c_str(), "%d,%d,%d,%d,%d", &x.source, &x.dest[0], &x.sens[0], &x.dest[1], &x.sens[1]);
            const size_t semi = t.find(';');
            t = semi == std::string::npos ? std::string() : t.substr(semi + 1);
        }
    }
    if (const char* e = std::getenv("XT_ASSIGN")) {
        int* a = c.assign;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", a, a + 1, a + 2, a + 3, a + 4, a + 5, a + 6, a + 7, a + 8, a + 9, a + 10, a + 11);
    }
    syn.setControls(c);
    if (const char* e = std::getenv("XT_PRESS")) {
        int ch = 0, key = -1, poly = 0;
        std::sscanf(e, "%d,%d,%d", &ch, &key, &poly);
        syn.channelPressure(ch);
        if (key >= 0) syn.polyPressure(key, poly);
    }
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
    struct CcEv { long at; int number, value; };
    std::vector<CcEv> ccEv;
    if (const char* e = std::getenv("XT_CC_AT"))
        for (std::string t = e; !t.empty(); ) {
            CcEv c1{};
            if (std::sscanf(t.c_str(), "%ld,%d,%d", &c1.at, &c1.number, &c1.value) == 3) ccEv.push_back(c1);
            const size_t semi = t.find(';');
            t = semi == std::string::npos ? std::string() : t.substr(semi + 1);
        }
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
        for (const CcEv& e : ccEv) if (e.at >= done && e.at < next) next = e.at;
        for (const CcEv& e : ccEv) if (e.at == done) syn.controller(e.number, e.value);
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
