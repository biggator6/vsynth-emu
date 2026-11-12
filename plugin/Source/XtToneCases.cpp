// xt_tone's cases (XtToneCases.h): random settings, and the patch by ID from XtSynth's controls.  Moved from XtToneCheck.cpp
// as they stood (v193).
#ifdef XT_TONE_MAIN
#include "XtToneCases.h"
#include "XtToneFirmware.h"

// An envelope's time: in one case of eight a note value; an LFO's rate, in one of four
int drawTime(Rng& r) { return r.one(8) ? r.range(128, 144) : r.range(0, 90); }
int drawRate(Rng& r) { return r.one(4) ? r.range(128, 144) : r.range(0, 127); }

void drawOsc(Rng& r, OscC& o) {
    o.on = true;
    o.tempoSync = r.one(3); o.timeTrip = !r.one(4); o.beatKeep = !r.one(4);
    o.origTempo = r.one(3) ? 1200000 : r.range(40, 240) * 10000 + r.range(0, 9999);
    o.oscType = 0;
    o.oscTime = r.one(2) ? 0 : r.range(-63, 63); o.oscTimeOffset = r.range(0, 2); o.oscTimeKF = r.one(2) ? 0 : 10 * r.range(-20, 20);
    o.oscFormant = r.one(2) ? 0 : r.range(-63, 63); o.oscFormantKF = r.one(2) ? 0 : 10 * r.range(-20, 20); o.energy = r.one(2) ? 0 : r.range(0, 127);
    o.tDepth = r.one(2) ? 0 : r.range(-63, 63); o.fDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.lfoTimeDepth = r.one(2) ? 0 : r.range(-63, 63); o.lfoFormantDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.ana.wave = r.range(0, 13);
    o.ana.impact = r.range(0, 40);
    o.ana.pw = r.range(-63, 63); o.ana.pwLfo = r.range(-63, 63); o.ana.pwEnv = r.range(-63, 63);
    o.ana.fat = r.range(0, 127); o.ana.fatLfo = r.range(-63, 63); o.ana.fatEnv = r.range(-63, 63);
    if (!r.one(3)) { o.ana.pwKf = r.range(-20, 20); o.ana.fatKf = r.range(-20, 20); }
    if (r.one(3)) o.ana.detune = r.range(-63, 63);
    if (r.one(3)) { o.ana.subOctave = r.range(0, 3); o.ana.subLevel = r.range(0, 127); }
    o.oscCoarse = r.one(2) ? 0 : r.range(-24, 24);
    o.oscFine = r.one(2) ? 0 : r.range(-50, 50);
    o.oscPitch = r.one(2) ? 0 : r.range(-63, 63);
    o.pitchKF = r.one(2) ? 100 : 10 * r.range(-20, 20);
    o.randomPitch = r.one(3) ? r.range(0, 30) : 0;
    o.pAttack = drawTime(r); o.pDecay = drawTime(r); o.pSustain = r.range(0, 127); o.pRelease = drawTime(r); o.pDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.tAttack = drawTime(r); o.tDecay = drawTime(r); o.tSustain = r.range(0, 127); o.tRelease = drawTime(r);
    o.fAttack = drawTime(r); o.fDecay = drawTime(r); o.fSustain = r.range(0, 127); o.fRelease = drawTime(r);
    for (OscC::EnvVelocity* v : { &o.pVel, &o.tVel, &o.fVel })
        if (r.one(3)) { v->curve = r.range(0, 7); v->sens = r.range(-63, 63); v->aSens = r.one(2) ? 0 : r.range(-63, 63); v->dSens = r.one(2) ? 0 : r.range(-63, 63); v->rSens = r.one(2) ? 0 : r.range(-63, 63); v->timeKF = r.one(2) ? 0 : 10 * r.range(-20, 20); }
    o.lfoWave = r.range(0, 7); o.lfoRate = drawRate(r); o.lfoOffset = r.range(0, 4); o.lfoDelay = r.one(2) ? 0 : r.range(0, 60);
    o.lfoFadeMode = r.range(0, 3); o.lfoFade = r.one(2) ? 0 : r.range(0, 60); o.lfoKeySync = !r.one(4);
    o.lfoPitchDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.oscLevel = r.range(0, 127); o.oscLevelKF = 10 * r.range(-20, 20); o.oscLevelLfoDepth = r.one(2) ? 0 : r.range(-63, 63); o.waveGain = r.range(-12, 12);
    o.oAttack = drawTime(r); o.oDecay = drawTime(r); o.oSustain = r.range(0, 127); o.oRelease = drawTime(r);
    o.oASens = r.one(2) ? 0 : r.range(-63, 63); o.oDSens = r.one(2) ? 0 : r.range(-63, 63); o.oRSens = r.one(2) ? 0 : r.range(-63, 63);
    o.oTimeKF = r.one(2) ? 0 : 10 * r.range(-20, 20);
    o.oscVelSens = r.range(-63, 63); o.oscVelCurve = r.range(0, 7);
}

void drawCosm(Rng& r, Ctl::Cosm& c) {
    auto d = [&] { return r.one(2) ? 0 : r.range(-63, 63); };     // a depth
    auto u = [&] { return r.range(0, 127); };
    auto kf = [&] { return r.one(2) ? 0 : 10 * r.range(-20, 20); };
    c.type = r.range(0, 15);
    c.filter = r.range(0, 11); c.cutoff = u(); c.reso = u(); c.kf = kf(); c.cutoffEnv = d(); c.cutoffLfo = d(); c.resoEnv = d(); c.resoLfo = d(); c.resoOwn = d(); c.cutoffOwn = d();
    c.dualType = r.range(0, 2); c.freq1 = u(); c.kf1 = kf(); c.q1 = u(); c.lfo1 = d(); c.env1 = d(); c.freq2 = u(); c.kf2 = kf(); c.q2 = u(); c.lfo2 = d(); c.env2 = d();
    c.tbCutoff = u(); c.tbKf = kf(); c.tbLfo = d(); c.tbEnv = d(); c.tbQ = u(); c.tbQFollow = d(); c.tbHpf = u(); c.tbHpfKf = kf(); c.tbHpfLfo = d(); c.tbHpfEnv = d(); c.tbGain = u();
    c.combOctave = r.range(0, 4); c.combDetune = r.range(-100, 100); c.combDetuneLfo = d(); c.combDetuneEnv = d(); c.combTone = u(); c.combToneLfo = d(); c.combToneEnv = d();
    for (Ctl::Cosm::SideBand& s : c.sb) {
        s.hpf = u(); s.q = u(); s.lpf = u(); s.lpfKf = kf(); s.subDetune = r.range(-100, 100); s.octave = r.range(0, 2); s.detune = r.range(-100, 100); s.detuneLfo = d(); s.detuneOwn = d(); s.detuneEnv = d();
        s.width = u(); s.widthKf = kf(); s.widthLfo = d(); s.widthOwn = d(); s.widthEnv = d();
    }
    c.resType = r.range(0, 2); c.resSize = u(); c.resSizeKf = kf(); c.resSizeLfo = d(); c.resSizeEnv = d(); c.resBalance = r.range(0, 100); c.resBalanceLfo = d(); c.resBalanceEnv = d();
    c.fsDepth = u(); c.fsDepthKf = kf(); c.fsDepthLfo = d(); c.fsDepthEnv = d(); c.fsBalance = r.range(0, 100); c.fsBalanceLfo = d(); c.fsBalanceEnv = d();
    c.lofiBit = r.range(0, 15); c.lofiRate = r.range(0, 5); c.lofiFilter = u(); c.lofiFilterKf = kf(); c.lofiFilterLfo = d(); c.lofiFilterEnv = d(); c.lofiBalance = r.range(0, 100); c.lofiBalanceLfo = d(); c.lofiBalanceEnv = d();
    c.compAttack = u(); c.compSustain = u(); c.compLevel = u();
    c.limThreshold = u(); c.limRatio = r.range(0, 3); c.limAttack = u(); c.limRelease = u(); c.limLevel = u();
    c.wsType = r.range(0, 5); c.wsLevel = u(); c.wsDrive = u(); c.wsDriveLfo = d(); c.wsDriveEnv = d(); c.wsTone = u(); c.wsToneKf = kf(); c.wsToneLfo = d(); c.wsToneEnv = d();
    c.odType = r.range(0, 1); c.odLevel = u(); c.odDrive = u(); c.odDriveLfo = d(); c.odDriveEnv = d(); c.odTone = u(); c.odToneKf = kf(); c.odToneLfo = d(); c.odToneEnv = d();
    c.ampType = r.range(0, 2); c.ampBass = u(); c.ampMiddle = u(); c.ampTreble = u(); c.ampLevel = u(); c.ampGain = u(); c.ampGainLfo = d(); c.ampGainEnv = d();
    c.spkType = r.range(0, 11);
    for (Ctl::CosmEnv* e : { &c.envA, &c.envB }) {
        e->attack = drawTime(r); e->decay = drawTime(r); e->sustain = u(); e->release = drawTime(r); e->velCurve = r.range(0, 7); e->velSens = r.range(-63, 63);
        e->aSens = d(); e->dSens = d(); e->rSens = d(); e->timeKF = kf();
    }
    c.lfoWave = r.range(0, 7); c.lfoRate = drawRate(r); c.lfoOffset = r.range(0, 4); c.lfoDelay = r.one(2) ? 0 : r.range(0, 60); c.lfoFadeMode = r.range(0, 3); c.lfoFade = r.one(2) ? 0 : r.range(0, 60); c.lfoKeySync = !r.one(4);
}

// The Matrix in half the cases, one to eight controls, on the sources whose values the case draws
void drawMatrix(Rng& r, Ctl& c) {
    for (int i = 0; i < 10; ++i) c.assign[i] = r.one(2) ? 0 : r.one(8) ? 32 : r.range(0, 95);   // 32 means none, and it is a controller's number
    if (r.one(3)) { c.assign[6] = 80; c.assign[7] = 81; }              // Pad Assign X, Y, the chart's
    // Time Trip Assign X, Y: the chart's, none, Hold or Sostenuto as well, or another.  Not those with draws of their own (7, 10, 11, 1)
    const auto tripAssign = [&r](int chart) {
        if (r.one(3)) return chart;
        if (r.one(5)) return r.one(2) ? 0 : 32;
        if (r.one(8)) return r.one(2) ? 64 : 66;
        const int n = r.range(1, 95);
        return n == 7 || n == 10 || n == 11 || n == 1 ? 16 : n;
    };
    c.assign[10] = tripAssign(82);
    c.assign[11] = r.one(16) ? c.assign[10] : tripAssign(83);          // one controller can be both
    if (r.one(2)) return;
    static const int kOther[] = { 96, 96, 96, 97, 97, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 107, 108, 108 };
    const auto dest = [&] { return r.range(0, 78); };
    for (int i = r.range(0, 7); i < 8; ++i) {
        XtMatrixControl& m = c.matrix[i];
        m.source = r.one(2) ? (r.one(8) ? 32 : r.range(0, 95)) : kOther[r.range(0, (int)(sizeof kOther / sizeof kOther[0]) - 1)];
        for (int j = 0; j < 2; ++j) {
            m.dest[j] = r.one(4) ? 0 : dest();
            m.sens[j] = r.one(6) ? 0 : r.one(4) ? (r.one(2) ? 63 : -63) : r.range(-63, 63);
        }
    }
}

Ctl drawControls(Rng& r, int g, bool pcm, bool pcm2) {
    Ctl c;
    drawMatrix(r, c);
    drawOsc(r, c);
    drawOsc(r, c.osc2);
    const auto drawPcm = [&r](OscC& o) {
        o.oscType = 1; o.rootKey = r.range(36, 84);
        o.robot = r.one(4);
        o.vari = !r.one(3);
        // the Start Offset in one case in three, now and then at or past the waves' 30000 frames, where the converter takes the last sample
        if (r.one(3)) { o.startOffset = r.one(4) ? r.range(0, 40000) : r.range(1, 4000); if (o.startOffset > 35000) o.startOffset = 30000; }
        if (r.one(3)) { o.loop = true; o.loopMode = r.range(0, 2); o.loopStart = (float)r.range(0, 120) / 1000.0f; o.loopEnd = o.loopStart + (float)r.range(5, 40) / 1000.0f; }   // near the start: notes are short
    };
    if (pcm) drawPcm(c);
    if (pcm2) drawPcm(c.osc2);
    drawCosm(r, c.cosm[0]); drawCosm(r, c.cosm[1]);
    c.structure = r.range(0, 2);
    c.tvaOn = !r.one(4);
    c.mod.on = r.one(3); c.mod.type = r.range(0, 4); c.mod.level = r.range(0, 127); c.mod.attack = r.range(0, 127); c.mod.release = r.range(0, 127);
    c.cosm[0].on = g % 3 == 0;
    c.cosm[1].on = g % 6 == 3 || g % 4 == 1;
    if (g % 10 == 4) c.ana.wave = c.osc2.ana.wave = 11;   // both on SUPER-SAW: its counter goes OSC 1 then OSC 2
    if (r.one(8)) c.on = false;
    if (r.one(3)) c.osc2.on = false;
    if (!c.on || pcm2) c.osc2.on = true;               // both off: XtSynth starts no voice at all (see the report)
    c.patchCoarse = r.one(2) ? 0 : r.range(-24, 24);
    c.patchFine = r.one(2) ? 0 : r.range(-50, 50);
    c.bendUp = r.range(0, 48); c.bendDown = r.range(0, 48);
    // the ends too, where the time between clocks reaches its limits (0x894f0fd0)
    c.patchTempo = r.one(3) ? 1200 : r.one(6) ? (r.one(2) ? r.range(200, 209) : r.range(2450, 2500)) : r.range(200, 2500);
    c.clockSource = r.one(3);
    if (r.one(3)) { c.scaleTune = true; for (int& s : c.scale) s = r.range(-100, 100); }
    c.attack = drawTime(r); c.decay = drawTime(r); c.sustain = r.range(0, 127); c.release = drawTime(r);
    c.aSens = r.one(2) ? 0 : r.range(-63, 63); c.dSens = r.one(2) ? 0 : r.range(-63, 63); c.rSens = r.one(2) ? 0 : r.range(-63, 63);
    c.timeKF = r.one(2) ? 0 : 10 * r.range(-20, 20);
    c.ampVelSens = r.range(-63, 63); c.ampVelCurve = r.range(0, 7);
    c.tvaLevel = r.one(3) ? 127 : r.range(0, 127); c.tvaLevelKF = r.one(2) ? 0 : 10 * r.range(-20, 20); c.tvaLevelLfo = r.one(2) ? 0 : r.range(-63, 63);
    c.tvaPan = r.one(3) ? 64 : r.range(0, 127); c.tvaPanKF = r.one(2) ? 0 : 10 * r.range(-20, 20); c.tvaPanLfo = r.one(2) ? 0 : r.range(-63, 63);
    c.mfxSend = r.one(2) ? 127 : r.range(0, 127);
    c.chorusSend = r.one(2) ? 0 : r.range(0, 127); c.reverbSend = r.one(2) ? 0 : r.range(0, 127);
    c.outputAssign = r.one(2) ? 0 : r.range(0, 2); c.parallel = !r.one(3); c.outGainTg = r.one(2) ? 0 : r.range(-48, 48);
    c.tvaLfoWave = r.range(0, 7); c.tvaLfoRate = drawRate(r); c.tvaLfoOffset = r.range(0, 4); c.tvaLfoDelay = r.one(2) ? 0 : r.range(0, 60);
    c.tvaLfoFadeMode = r.range(0, 3); c.tvaLfoFade = r.one(2) ? 0 : r.range(0, 60); c.tvaLfoKeySync = !r.one(4);
    // now and then the pan LFO at full depth on a slow square from the note's start, one channel's gain 0 all through, and
    // no release, so that the other's is still up as the envelope ends: a voice's end must look at both
    if (r.one(16)) { c.tvaPanLfo = r.one(2) ? 63 : -63; c.tvaLfoWave = 3; c.tvaLfoFade = c.tvaLfoDelay = 0; c.tvaLfoOffset = 2; c.release = 0; c.rSens = 0; c.tvaLevel = 127; c.tvaOn = true; c.tvaLfoRate = 30; c.tvaLfoKeySync = true; c.tvaLfoFadeMode = 0; }
    // LFO 5 a square held at full Pan Depth, and the release whose last step before its end is largest: as the envelope
    // ends one channel's gain is at 0 and the other's is not, and the tone goes on until both are (0x894f54b0)
    if (r.one(16)) {
        c.tvaOn = true;
        c.tvaLfoWave = 3; c.tvaPanLfo = 63; c.tvaLfoRate = r.range(0, 10); c.tvaLfoOffset = 2; c.tvaLfoDelay = c.tvaLfoFade = 0; c.tvaLfoKeySync = true;
        c.sustain = r.range(70, 127); c.release = 16; c.rSens = 0; c.timeKF = 0;
    }
    c.octave = r.one(3) ? r.range(-3, 3) : 0;
    if (r.one(2)) { c.portamento = true; c.portaMode = r.range(0, 1); c.portaType = r.range(0, 1); c.portaTime = r.range(0, 127); c.portaVeloSens = r.range(-63, 63); }
    return c;
}

// ── The patch, by ID, from XtSynth's controls ───────────────────────────────
// And the Multi Step Modulator's, IDs 0x48d…0x4ee (v193)
std::unordered_map<int, int> withMsmIds(std::unordered_map<int, int> p, const XtMsmSettings& s) {
    for (int id = kXtMsmFirstId; id <= kXtMsmLastId; ++id) p[id] = xtMsmValue(s, id);
    return p;
}

Ctl withoutMsm(Ctl c) {
    c.msm.master = 0;
    return c;
}

void envParams(std::unordered_map<int, int>& p, int e, int a, int d, int s, int r, int curve, int sens, int aS, int dS, int rS, int kf) {
    const int b = 0x315 + 13 * e;
    p[b] = a; p[b + 2] = d; p[b + 3] = s; p[b + 5] = r; p[b + 6] = curve; p[b + 7] = sens; p[b + 8] = aS; p[b + 9] = dS; p[b + 10] = rS; p[b + 11] = kf;
}

void oscParams(std::unordered_map<int, int>& p, int n, const OscC& o) {
    const int b = 0x2a4 + 0x34 * n;
    p[b] = o.on; p[b + 1] = o.oscType; p[b + 2] = o.waveGain; p[b + 3] = o.oscPitch; p[b + 4] = o.pitchKF / 10;
    p[b + 5] = o.lfoPitchDepth; p[b + 6] = o.pDepth; p[b + 7] = o.oscCoarse; p[b + 8] = o.oscFine; p[b + 9] = o.randomPitch;
    p[b + 0xa] = o.oscLevel; p[b + 0xb] = o.oscLevelKF / 10; p[b + 0xc] = o.oscLevelLfoDepth;
    p[b + 0xd] = o.ana.wave; p[b + 0xe] = o.ana.impact; p[b + 0xf] = o.ana.pw; p[b + 0x10] = o.ana.pwKf; p[b + 0x11] = o.ana.pwLfo; p[b + 0x12] = o.ana.pwEnv;
    p[b + 0x13] = o.ana.fat; p[b + 0x14] = o.ana.fatKf; p[b + 0x15] = o.ana.fatLfo; p[b + 0x16] = o.ana.fatEnv;
    p[b + 0x1a] = 1 + n;                              // the Wave Number
    p[b + 0x22] = o.startOffset;
    p[b + 0x2e] = o.playbackMode;
    p[b + 0x23] = o.oscTime; p[b + 0x24] = o.oscTimeKF / 10; p[b + 0x25] = o.lfoTimeDepth; p[b + 0x26] = o.tDepth; p[b + 0x27] = o.oscTimeOffset;
    p[b + 0x28] = o.oscFormant; p[b + 0x29] = o.oscFormantKF / 10; p[b + 0x2a] = o.lfoFormantDepth; p[b + 0x2b] = o.fDepth;
    p[b + 0x30] = o.vari;
    p[b + 0x2c] = o.tempoSync; p[b + 0x2d] = o.robot; p[b + 0x2f] = o.loop; p[b + 0x31] = o.energy; p[b + 0x32] = o.timeTrip; p[b + 0x33] = o.beatKeep;
    p[0x30d + 3 * n] = o.ana.detune; p[0x30e + 3 * n] = o.ana.subOctave; p[0x30f + 3 * n] = o.ana.subLevel;
    const int e = 4 * n;
    envParams(p, e, o.pAttack, o.pDecay, o.pSustain, o.pRelease, o.pVel.curve, o.pVel.sens, o.pVel.aSens, o.pVel.dSens, o.pVel.rSens, o.pVel.timeKF / 10);
    envParams(p, e + 1, o.tAttack, o.tDecay, o.tSustain, o.tRelease, o.tVel.curve, o.tVel.sens, o.tVel.aSens, o.tVel.dSens, o.tVel.rSens, o.tVel.timeKF / 10);
    envParams(p, e + 2, o.fAttack, o.fDecay, o.fSustain, o.fRelease, o.fVel.curve, o.fVel.sens, o.fVel.aSens, o.fVel.dSens, o.fVel.rSens, o.fVel.timeKF / 10);
    envParams(p, e + 3, o.oAttack, o.oDecay, o.oSustain, o.oRelease, o.oscVelCurve, o.oscVelSens, o.oASens, o.oDSens, o.oRSens, o.oTimeKF / 10);
    const int l = 0x3be + 8 * n;
    p[l] = o.lfoWave; p[l + 2] = o.lfoRate; p[l + 3] = o.lfoOffset; p[l + 4] = o.lfoDelay; p[l + 5] = o.lfoFadeMode; p[l + 6] = o.lfoFade; p[l + 7] = o.lfoKeySync;
}

void cosmParams(std::unordered_map<int, int>& p, int k, const Ctl::Cosm& c) {
    const int add = k ? kCosm2 : 0;
    static const int fwType[16] = { 10, 11, 9, 16, 8, 6, 7, 5, 14, 15, 12, 13, 2, 1, 3, 4 };   // COSM Type by XtSynth's number
    p[0x3e7 + add] = c.on; p[0x3e8 + add] = fwType[c.type];
    auto set = [&](int id, std::initializer_list<int> v) { for (int x : v) p[add + id++] = x; };
    set(0x920, { c.filter, c.reso, c.resoLfo, c.resoEnv, c.cutoff, c.kf / 10, c.cutoffLfo, c.cutoffEnv });
    set(0x928, { c.filter, c.reso, c.resoLfo, c.resoOwn, c.resoEnv, c.cutoff, c.kf / 10, c.cutoffLfo, c.cutoffOwn, c.cutoffEnv });
    set(0x915, { c.dualType, c.freq1, c.kf1 / 10, c.q1, c.lfo1, c.env1, c.freq2, c.kf2 / 10, c.q2, c.lfo2, c.env2 });
    set(0x94a, { c.tbCutoff, c.tbKf / 10, c.tbLfo, c.tbEnv, c.tbQ, c.tbQFollow, c.tbHpf, c.tbHpfKf / 10, c.tbHpfLfo, c.tbHpfEnv, c.tbGain });
    set(0x90e, { c.combOctave, c.combDetune, c.combDetuneLfo, c.combDetuneEnv, c.combTone, c.combToneLfo, c.combToneEnv });
    const Ctl::Cosm::SideBand& a = c.sb[0];
    set(0x8f3, { a.hpf, a.lpf, a.lpfKf / 10, a.subDetune, a.octave, a.detune, a.detuneLfo, a.detuneOwn, a.detuneEnv, a.width, a.widthKf / 10, a.widthLfo, a.widthOwn, a.widthEnv });
    const Ctl::Cosm::SideBand& b = c.sb[1];
    set(0x901, { b.q, b.lpf, b.lpfKf / 10, b.octave, b.detune, b.detuneLfo, b.detuneOwn, b.detuneEnv, b.width, b.widthKf / 10, b.widthLfo, b.widthOwn, b.widthEnv });
    set(0x8eb, { c.resType, c.resSize, c.resSizeKf / 10, c.resSizeLfo, c.resSizeEnv, c.resBalance, c.resBalanceLfo, c.resBalanceEnv });
    set(0x93a, { c.fsDepth, c.fsDepthKf / 10, c.fsDepthLfo, c.fsDepthEnv, c.fsBalance, c.fsBalanceLfo, c.fsBalanceEnv });
    set(0x941, { c.lofiBit, c.lofiRate, c.lofiFilter, c.lofiFilterKf / 10, c.lofiFilterLfo, c.lofiFilterEnv, c.lofiBalance, c.lofiBalanceLfo, c.lofiBalanceEnv });
    set(0x932, { c.compAttack, c.compSustain, c.compLevel });
    set(0x935, { c.limThreshold, c.limRatio, c.limAttack, c.limRelease, c.limLevel });
    set(0x8d9, { c.wsType, c.wsLevel, c.wsDrive, c.wsDriveEnv, c.wsDriveLfo, c.wsTone, c.wsToneKf / 10, c.wsToneLfo, c.wsToneEnv });   // Drive's Env depth before its LFO depth
    set(0x8d0, { c.odType, c.odLevel, c.odDrive, c.odDriveLfo, c.odDriveEnv, c.odTone, c.odToneKf / 10, c.odToneLfo, c.odToneEnv });
    set(0x8e2, { c.ampType, c.ampBass, c.ampMiddle, c.ampTreble, c.ampLevel, c.ampGain, c.ampGainLfo, c.ampGainEnv });
    set(0x8ea, { c.spkType });
    const Ctl::CosmEnv* e[2] = { &c.envA, &c.envB };
    for (int i = 0; i < 2; ++i)
        envParams(p, 8 + 2 * k + i, e[i]->attack, e[i]->decay, e[i]->sustain, e[i]->release, e[i]->velCurve, e[i]->velSens, e[i]->aSens, e[i]->dSens, e[i]->rSens, e[i]->timeKF / 10);
    const int l = 0x3be + 8 * (2 + k);
    p[l] = c.lfoWave; p[l + 2] = c.lfoRate; p[l + 3] = c.lfoOffset; p[l + 4] = c.lfoDelay; p[l + 5] = c.lfoFadeMode; p[l + 6] = c.lfoFade; p[l + 7] = c.lfoKeySync;
}

std::unordered_map<int, int> patchOf(const Ctl& c) {
    std::unordered_map<int, int> p;
    oscParams(p, 0, c); oscParams(p, 1, c.osc2);
    cosmParams(p, 0, c.cosm[0]); cosmParams(p, 1, c.cosm[1]);
    p[0x11d] = c.patchTempo;
    p[0x41] = c.clockSource;
    p[0x110] = c.patchCoarse; p[0x111] = c.patchFine; p[0x112] = c.octave; p[0x113] = !c.mono; p[0x114] = c.legato;   // Mono/Poly: 0 MONO
    p[0x115] = c.portamento; p[0x116] = c.portaMode; p[0x117] = c.portaType; p[0x118] = c.portaTime; p[0x119] = c.portaVeloSens;
    p[0x11e] = c.bendUp; p[0x11f] = c.bendDown;
    p[0x134] = c.scaleTune;
    for (int i = 0; i < 12; ++i) p[0x136 + 2 * i] = c.scale[i];
    p[0x293] = c.structure; p[0x294] = c.mod.on; p[0x295] = c.mod.type; p[0x296] = c.mod.level; p[0x297] = c.mod.attack; p[0x298] = c.mod.release;
    p[0x299] = c.tvaOn;
    p[0x29a] = c.tvaLevel; p[0x29b] = c.tvaLevelKF / 10; p[0x29c] = c.tvaLevelLfo; p[0x29d] = c.tvaPan; p[0x29e] = c.tvaPanKF / 10; p[0x29f] = c.tvaPanLfo; p[0x2a0] = c.mfxSend;
    p[0x2a1] = c.chorusSend; p[0x2a2] = c.reverbSend; p[0x2a3] = c.outputAssign; p[0x526] = c.outGainTg; p[0x40] = c.parallel;
    for (int i = 0; i < 8; ++i) {
        const int b = 0x14d + 5 * i;
        p[b] = c.matrix[i].source; p[b + 1] = c.matrix[i].dest[0]; p[b + 2] = c.matrix[i].sens[0]; p[b + 3] = c.matrix[i].dest[1]; p[b + 4] = c.matrix[i].sens[1];
    }
    static const int kAssignId[12] = { 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x93, 0x94, 0x91, 0x92 };
    for (int i = 0; i < 12; ++i) p[kAssignId[i]] = c.assign[i];
    p[0x3de] = c.tvaLfoWave; p[0x3e0] = c.tvaLfoRate; p[0x3e1] = c.tvaLfoOffset; p[0x3e2] = c.tvaLfoDelay; p[0x3e3] = c.tvaLfoFadeMode; p[0x3e4] = c.tvaLfoFade; p[0x3e5] = c.tvaLfoKeySync;
    envParams(p, 12, c.attack, c.decay, c.sustain, c.release, c.ampVelCurve, c.ampVelSens, c.aSens, c.dSens, c.rSens, c.timeKF / 10);
    return p;
}
#endif
