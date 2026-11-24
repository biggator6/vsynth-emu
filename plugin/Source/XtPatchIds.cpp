// A patch by ID from XtSynth's controls: see XtPatchIds.h.  Moved as it stood out of xt_tone's XtTonePatchIds.cpp
// (v602), its lines broken where they read and each run of consecutive IDs one call.
#include "XtPatchIds.h"

namespace {
using Ids = std::unordered_map<int, int>;
using OscC = XtSynth::OscControls;
using Ctl = XtSynth::Controls;

void envParams(Ids& p, int e, int a, int d, int s, int r, int curve, int sens, int aS, int dS, int rS, int kf) {
    const int b = 0x315 + 13 * e;
    p[b] = a;
    p[b + 2] = d;
    p[b + 3] = s;
    p[b + 5] = r;
    p[b + 6] = curve;
    p[b + 7] = sens;
    p[b + 8] = aS;
    p[b + 9] = dS;
    p[b + 10] = rS;
    p[b + 11] = kf;
}

// `values` at `first` and the IDs after it, in order
void idsFrom(Ids& p, int first, std::initializer_list<int> values) {
    for (int v : values) p[first++] = v;
}

void oscParams(Ids& p, int n, const OscC& o) {
    const int b = 0x2a4 + 0x34 * n;
    idsFrom(p, b, { o.on, o.oscType, o.waveGain, o.oscPitch, o.pitchKF / 10, o.lfoPitchDepth, o.pDepth, o.oscCoarse,
                    o.oscFine, o.randomPitch, o.oscLevel, o.oscLevelKF / 10, o.oscLevelLfoDepth });
    idsFrom(p, b + 0xd, { o.ana.wave, o.ana.impact, o.ana.pw, o.ana.pwKf, o.ana.pwLfo, o.ana.pwEnv, o.ana.fat,
                          o.ana.fatKf, o.ana.fatLfo, o.ana.fatEnv });
    p[b + 0x22] = o.startOffset;
    p[b + 0x2e] = o.playbackMode;
    idsFrom(p, b + 0x23, { o.oscTime, o.oscTimeKF / 10, o.lfoTimeDepth, o.tDepth, o.oscTimeOffset });
    idsFrom(p, b + 0x28, { o.oscFormant, o.oscFormantKF / 10, o.lfoFormantDepth, o.fDepth });
    p[b + 0x30] = o.vari;
    idsFrom(p, b + 0x2c, { o.tempoSync, o.robot });
    p[b + 0x2f] = o.loop;
    idsFrom(p, b + 0x31, { o.energy, o.timeTrip, o.beatKeep });
    idsFrom(p, 0x30d + 3 * n, { o.ana.detune, o.ana.subOctave, o.ana.subLevel });
    const int e = 4 * n;
    envParams(p, e, o.pAttack, o.pDecay, o.pSustain, o.pRelease, o.pVel.curve, o.pVel.sens, o.pVel.aSens,
              o.pVel.dSens, o.pVel.rSens, o.pVel.timeKF / 10);
    envParams(p, e + 1, o.tAttack, o.tDecay, o.tSustain, o.tRelease, o.tVel.curve, o.tVel.sens, o.tVel.aSens,
              o.tVel.dSens, o.tVel.rSens, o.tVel.timeKF / 10);
    envParams(p, e + 2, o.fAttack, o.fDecay, o.fSustain, o.fRelease, o.fVel.curve, o.fVel.sens, o.fVel.aSens,
              o.fVel.dSens, o.fVel.rSens, o.fVel.timeKF / 10);
    envParams(p, e + 3, o.oAttack, o.oDecay, o.oSustain, o.oRelease, o.oscVelCurve, o.oscVelSens, o.oASens, o.oDSens,
              o.oRSens, o.oTimeKF / 10);
    const int l = 0x3be + 8 * n;
    p[l] = o.lfoWave;
    idsFrom(p, l + 2, { o.lfoRate, o.lfoOffset, o.lfoDelay, o.lfoFadeMode, o.lfoFade, o.lfoKeySync });
}

void cosmParams(Ids& p, int k, const Ctl::Cosm& c) {
    const int add = k ? kXtCosm2Ids : 0;
    // COSM Type by XtSynth's number
    static constexpr int kFwType[16] = { 10, 11, 9, 16, 8, 6, 7, 5, 14, 15, 12, 13, 2, 1, 3, 4 };
    p[0x3e7 + add] = c.on;
    p[0x3e8 + add] = kFwType[c.type];
    const auto set = [&p, add](int id, std::initializer_list<int> v) { idsFrom(p, add + id, v); };
    set(0x920, { c.filter, c.reso, c.resoLfo, c.resoEnv, c.cutoff, c.kf / 10, c.cutoffLfo, c.cutoffEnv });
    set(0x928, { c.filter, c.reso, c.resoLfo, c.resoOwn, c.resoEnv, c.cutoff, c.kf / 10, c.cutoffLfo, c.cutoffOwn,
                 c.cutoffEnv });
    set(0x915, { c.dualType, c.freq1, c.kf1 / 10, c.q1, c.lfo1, c.env1, c.freq2, c.kf2 / 10, c.q2, c.lfo2, c.env2 });
    set(0x94a, { c.tbCutoff, c.tbKf / 10, c.tbLfo, c.tbEnv, c.tbQ, c.tbQFollow, c.tbHpf, c.tbHpfKf / 10, c.tbHpfLfo,
                 c.tbHpfEnv, c.tbGain });
    set(0x90e, { c.combOctave, c.combDetune, c.combDetuneLfo, c.combDetuneEnv, c.combTone, c.combToneLfo,
                 c.combToneEnv });
    const Ctl::Cosm::SideBand& a = c.sb[0];
    set(0x8f3, { a.hpf, a.lpf, a.lpfKf / 10, a.subDetune, a.octave, a.detune, a.detuneLfo, a.detuneOwn, a.detuneEnv,
                 a.width, a.widthKf / 10, a.widthLfo, a.widthOwn, a.widthEnv });
    const Ctl::Cosm::SideBand& b = c.sb[1];
    set(0x901, { b.q, b.lpf, b.lpfKf / 10, b.octave, b.detune, b.detuneLfo, b.detuneOwn, b.detuneEnv, b.width,
                 b.widthKf / 10, b.widthLfo, b.widthOwn, b.widthEnv });
    set(0x8eb, { c.resType, c.resSize, c.resSizeKf / 10, c.resSizeLfo, c.resSizeEnv, c.resBalance, c.resBalanceLfo,
                 c.resBalanceEnv });
    set(0x93a, { c.fsDepth, c.fsDepthKf / 10, c.fsDepthLfo, c.fsDepthEnv, c.fsBalance, c.fsBalanceLfo,
                 c.fsBalanceEnv });
    set(0x941, { c.lofiBit, c.lofiRate, c.lofiFilter, c.lofiFilterKf / 10, c.lofiFilterLfo, c.lofiFilterEnv,
                 c.lofiBalance, c.lofiBalanceLfo, c.lofiBalanceEnv });
    set(0x932, { c.compAttack, c.compSustain, c.compLevel });
    set(0x935, { c.limThreshold, c.limRatio, c.limAttack, c.limRelease, c.limLevel });
    // Drive's Env depth before its LFO depth
    set(0x8d9, { c.wsType, c.wsLevel, c.wsDrive, c.wsDriveEnv, c.wsDriveLfo, c.wsTone, c.wsToneKf / 10, c.wsToneLfo,
                 c.wsToneEnv });
    set(0x8d0, { c.odType, c.odLevel, c.odDrive, c.odDriveLfo, c.odDriveEnv, c.odTone, c.odToneKf / 10, c.odToneLfo,
                 c.odToneEnv });
    set(0x8e2, { c.ampType, c.ampBass, c.ampMiddle, c.ampTreble, c.ampLevel, c.ampGain, c.ampGainLfo, c.ampGainEnv });
    set(0x8ea, { c.spkType });
    const Ctl::CosmEnv* e[2] = { &c.envA, &c.envB };
    for (int i = 0; i < 2; ++i)
        envParams(p, 8 + 2 * k + i, e[i]->attack, e[i]->decay, e[i]->sustain, e[i]->release, e[i]->velCurve,
                  e[i]->velSens, e[i]->aSens, e[i]->dSens, e[i]->rSens, e[i]->timeKF / 10);
    const int l = 0x3be + 8 * (2 + k);
    p[l] = c.lfoWave;
    idsFrom(p, l + 2, { c.lfoRate, c.lfoOffset, c.lfoDelay, c.lfoFadeMode, c.lfoFade, c.lfoKeySync });
}

// The patch's common: its tempo, tuning, keys, portamento, bend and scale, and the zones' Range Upper keys (v195)
void commonParams(Ids& p, const Ctl& c) {
    p[0x11d] = c.patchTempo;
    p[0x41] = c.clockSource;
    idsFrom(p, 0x110, { c.patchCoarse, c.patchFine, c.octave, !c.mono, c.legato });   // Mono/Poly: 0 MONO
    idsFrom(p, 0x115, { c.portamento, c.portaMode, c.portaType, c.portaTime, c.portaVeloSens });
    idsFrom(p, 0x11e, { c.bendUp, c.bendDown });
    p[0x134] = c.scaleTune;
    for (int i = 0; i < 12; ++i) p[0x136 + 2 * i] = c.scale[i];
    for (int i = 0; i < kXtZones; ++i) p[0x124 + i] = c.zoneUpper[i];
}
} // namespace

std::unordered_map<int, int> xtWithMsmIds(std::unordered_map<int, int> p, const XtMsmSettings& s) {
    for (int id = kXtMsmFirstId; id <= kXtMsmLastId; ++id) p[id] = xtMsmValue(s, id);
    return p;
}

std::unordered_map<int, int> xtPatchIds(const XtSynth::Controls& c) {
    Ids p;
    oscParams(p, 0, c);
    oscParams(p, 1, c.osc2);
    cosmParams(p, 0, c.cosm[0]);
    cosmParams(p, 1, c.cosm[1]);
    commonParams(p, c);
    idsFrom(p, 0x293, { c.structure, c.mod.on, c.mod.type, c.mod.level, c.mod.attack, c.mod.release, c.tvaOn });
    idsFrom(p, 0x29a, { c.tvaLevel, c.tvaLevelKF / 10, c.tvaLevelLfo, c.tvaPan, c.tvaPanKF / 10, c.tvaPanLfo,
                        c.mfxSend, c.chorusSend, c.reverbSend, c.outputAssign });
    p[0x526] = c.outGainTg;
    p[0x40] = c.parallel;
    for (int i = 0; i < 8; ++i)
        idsFrom(p, 0x14d + 5 * i, { c.matrix[i].source, c.matrix[i].dest[0], c.matrix[i].sens[0],
                                    c.matrix[i].dest[1], c.matrix[i].sens[1] });
    static constexpr int kAssignId[12] = { 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x93, 0x94, 0x91, 0x92 };
    for (int i = 0; i < 12; ++i) p[kAssignId[i]] = c.assign[i];
    p[0x3de] = c.tvaLfoWave;
    idsFrom(p, 0x3e0, { c.tvaLfoRate, c.tvaLfoOffset, c.tvaLfoDelay, c.tvaLfoFadeMode, c.tvaLfoFade,
                        c.tvaLfoKeySync });
    envParams(p, 12, c.attack, c.decay, c.sustain, c.release, c.ampVelCurve, c.ampVelSens, c.aSens, c.dSens, c.rSens,
              c.timeKF / 10);
    return p;
}
