// xt_tone's patch by ID from XtSynth's controls (XtToneCases.h): what the firmware's getter answers.  Moved from
// XtToneCases.cpp as it stood (v195), which keeps the cases' draws, when the zones' splits came in with commonParams.
#ifdef XT_TONE_MAIN
#include "XtToneCases.h"
#include "XtToneFirmware.h"

// And the Multi Step Modulator's, IDs 0x48d…0x4ee (v193)
std::unordered_map<int, int> withMsmIds(std::unordered_map<int, int> p, const XtMsmSettings& s) {
    for (int id = kXtMsmFirstId; id <= kXtMsmLastId; ++id) p[id] = xtMsmValue(s, id);
    return p;
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

// The patch's common: its tempo, tuning, keys, portamento, bend and scale, and the zones' Range Upper keys (v195)
void commonParams(std::unordered_map<int, int>& p, const Ctl& c) {
    p[0x11d] = c.patchTempo;
    p[0x41] = c.clockSource;
    p[0x110] = c.patchCoarse; p[0x111] = c.patchFine; p[0x112] = c.octave; p[0x113] = !c.mono; p[0x114] = c.legato;   // Mono/Poly: 0 MONO
    p[0x115] = c.portamento; p[0x116] = c.portaMode; p[0x117] = c.portaType; p[0x118] = c.portaTime; p[0x119] = c.portaVeloSens;
    p[0x11e] = c.bendUp; p[0x11f] = c.bendDown;
    p[0x134] = c.scaleTune;
    for (int i = 0; i < 12; ++i) p[0x136 + 2 * i] = c.scale[i];
    for (int i = 0; i < kXtZones; ++i) p[0x124 + i] = c.zoneUpper[i];
}

std::unordered_map<int, int> patchOf(const Ctl& c) {
    std::unordered_map<int, int> p;
    oscParams(p, 0, c); oscParams(p, 1, c.osc2);
    cosmParams(p, 0, c.cosm[0]); cosmParams(p, 1, c.cosm[1]);
    commonParams(p, c);
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
