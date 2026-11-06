// A patch's values as XtSynth's controls: see XtPatch.h.  Each line here is one of patchOf's the other way round, and xt_tone holds the two together.
#include "XtPatch.h"
#include "XtFiles.h"
#include <algorithm>

namespace {
using Ctl = XtSynth::Controls;
using Osc = XtSynth::OscControls;

constexpr int kThrough = 0, kExternal = 2;
// XtSynth's COSM type by the firmware's, 1…16 (the type's IDs: the table at 0x897cebd8)
constexpr int kCosmType[17] = { 0, 13, 12, 14, 15, 7, 5, 6, 4, 2, 0, 1, 10, 11, 8, 9, 3 };

struct Read {
    const std::vector<uint8_t>& store;
    int zone;
    int operator()(int id) const { return xtPatchValue(store, id, zone); }
};

void lfoOf(const Read& p, int l, int& wave, int& rate, int& offset, int& delay, int& fadeMode, int& fade, bool& keySync) {
    wave = p(l); rate = p(l + 2); offset = p(l + 3); delay = p(l + 4); fadeMode = p(l + 5); fade = p(l + 6); keySync = p(l + 7) != 0;
}

struct EnvIds {
    int base;
    int attack() const { return base; }
    int decay() const { return base + 2; }
    int sustain() const { return base + 3; }
    int release() const { return base + 5; }
    int curve() const { return base + 6; }
    int sens() const { return base + 7; }
    int aSens() const { return base + 8; }
    int dSens() const { return base + 9; }
    int rSens() const { return base + 10; }
    int kf() const { return base + 11; }
};
EnvIds envIds(int e) { return { 0x315 + 13 * e }; }

void oscOf(const Read& p, int n, Osc& o) {
    const int b = 0x2a4 + 0x34 * n;
    // OSC Type 2 is the XT's external input.  There's none here, so it's as an OSC switched off (xtPatchMissing says so)
    const int type = p(b + 1);
    o.on = p(b) != 0 && type != kExternal; o.oscType = type == kExternal ? 0 : type; o.waveGain = p(b + 2); o.oscPitch = p(b + 3); o.pitchKF = 10 * p(b + 4);
    o.lfoPitchDepth = p(b + 5); o.pDepth = p(b + 6); o.oscCoarse = p(b + 7); o.oscFine = p(b + 8); o.randomPitch = p(b + 9);
    o.oscLevel = p(b + 0xa); o.oscLevelKF = 10 * p(b + 0xb); o.oscLevelLfoDepth = p(b + 0xc);
    o.ana.wave = p(b + 0xd); o.ana.impact = p(b + 0xe); o.ana.pw = p(b + 0xf); o.ana.pwKf = p(b + 0x10); o.ana.pwLfo = p(b + 0x11); o.ana.pwEnv = p(b + 0x12);
    o.ana.fat = p(b + 0x13); o.ana.fatKf = p(b + 0x14); o.ana.fatLfo = p(b + 0x15); o.ana.fatEnv = p(b + 0x16);
    o.oscTime = p(b + 0x23); o.oscTimeKF = 10 * p(b + 0x24); o.lfoTimeDepth = p(b + 0x25); o.tDepth = p(b + 0x26); o.oscTimeOffset = p(b + 0x27);
    o.oscFormant = p(b + 0x28); o.oscFormantKF = 10 * p(b + 0x29); o.lfoFormantDepth = p(b + 0x2a); o.fDepth = p(b + 0x2b);
    o.vari = p(b + 0x30) != 0;
    o.startOffset = p(b + 0x22);
    o.playbackMode = p(b + 0x2e);
    o.tempoSync = p(b + 0x2c) != 0; o.robot = p(b + 0x2d) != 0; o.loop = p(b + 0x2f) != 0; o.energy = p(b + 0x31); o.timeTrip = p(b + 0x32) != 0; o.beatKeep = p(b + 0x33) != 0;
    o.ana.detune = p(0x30d + 3 * n); o.ana.subOctave = p(0x30e + 3 * n); o.ana.subLevel = p(0x30f + 3 * n);
    const int e = 4 * n;
    const EnvIds pe = envIds(e), te = envIds(e + 1), fe = envIds(e + 2), oe = envIds(e + 3);
    o.pAttack = p(pe.attack()); o.pDecay = p(pe.decay()); o.pSustain = p(pe.sustain()); o.pRelease = p(pe.release());
    o.tAttack = p(te.attack()); o.tDecay = p(te.decay()); o.tSustain = p(te.sustain()); o.tRelease = p(te.release());
    o.fAttack = p(fe.attack()); o.fDecay = p(fe.decay()); o.fSustain = p(fe.sustain()); o.fRelease = p(fe.release());
    const auto velocityOf = [&p](const EnvIds& i, Osc::EnvVelocity& v) { v.curve = p(i.curve()); v.sens = p(i.sens()); v.aSens = p(i.aSens()); v.dSens = p(i.dSens()); v.rSens = p(i.rSens()); v.timeKF = 10 * p(i.kf()); };
    velocityOf(pe, o.pVel); velocityOf(te, o.tVel); velocityOf(fe, o.fVel);
    o.oAttack = p(oe.attack()); o.oDecay = p(oe.decay()); o.oSustain = p(oe.sustain()); o.oRelease = p(oe.release());
    o.oscVelCurve = p(oe.curve()); o.oscVelSens = p(oe.sens()); o.oASens = p(oe.aSens()); o.oDSens = p(oe.dSens()); o.oRSens = p(oe.rSens()); o.oTimeKF = 10 * p(oe.kf());
    lfoOf(p, 0x3be + 8 * n, o.lfoWave, o.lfoRate, o.lfoOffset, o.lfoDelay, o.lfoFadeMode, o.lfoFade, o.lfoKeySync);
}

void cosmEnvOf(const Read& p, int e, Ctl::CosmEnv& c) {
    const EnvIds i = envIds(e);
    c.attack = p(i.attack()); c.decay = p(i.decay()); c.sustain = p(i.sustain()); c.release = p(i.release());
    c.velCurve = p(i.curve()); c.velSens = p(i.sens()); c.aSens = p(i.aSens()); c.dSens = p(i.dSens()); c.rSens = p(i.rSens()); c.timeKF = 10 * p(i.kf());
}

void cosmOf(const Read& p, int k, Ctl::Cosm& c) {
    c = Ctl::Cosm{};
    const int zone = p.zone;
    const auto& store = p.store;
    const int fw = xtPatchValue(store, 0x3e8, zone, k);
    c.on = xtPatchValue(store, 0x3e7, zone, k) != 0 && fw != kThrough;
    c.type = kCosmType[std::clamp(fw, 0, 16)];
    // In ID order, as patchOf has them; a keyfollow is stored in tens
    const auto at = [&](int id) { return xtPatchTyped(store, kXtCosmSlots, id, zone, k); };
    const auto read = [&](int id, std::initializer_list<int*> fields) { for (int* f : fields) *f = at(id++); };
    const auto kf = [](int& v) { v *= 10; };
    switch (fw) {
        case 10:
            read(0x920, { &c.filter, &c.reso, &c.resoLfo, &c.resoEnv, &c.cutoff, &c.kf, &c.cutoffLfo, &c.cutoffEnv });
            kf(c.kf);
            break;
        case 11:
            read(0x928, { &c.filter, &c.reso, &c.resoLfo, &c.resoOwn, &c.resoEnv, &c.cutoff, &c.kf, &c.cutoffLfo, &c.cutoffOwn, &c.cutoffEnv });
            kf(c.kf);
            break;
        case 9:
            read(0x915, { &c.dualType, &c.freq1, &c.kf1, &c.q1, &c.lfo1, &c.env1, &c.freq2, &c.kf2, &c.q2, &c.lfo2, &c.env2 });
            kf(c.kf1); kf(c.kf2);
            break;
        case 16:
            read(0x94a, { &c.tbCutoff, &c.tbKf, &c.tbLfo, &c.tbEnv, &c.tbQ, &c.tbQFollow, &c.tbHpf, &c.tbHpfKf, &c.tbHpfLfo, &c.tbHpfEnv, &c.tbGain });
            kf(c.tbKf); kf(c.tbHpfKf);
            break;
        case 8:
            read(0x90e, { &c.combOctave, &c.combDetune, &c.combDetuneLfo, &c.combDetuneEnv, &c.combTone, &c.combToneLfo, &c.combToneEnv });
            break;
        case 6: {
            Ctl::Cosm::SideBand& a = c.sb[0];
            read(0x8f3, { &a.hpf, &a.lpf, &a.lpfKf, &a.subDetune, &a.octave, &a.detune, &a.detuneLfo, &a.detuneOwn, &a.detuneEnv, &a.width, &a.widthKf, &a.widthLfo, &a.widthOwn, &a.widthEnv });
            kf(a.lpfKf); kf(a.widthKf);
            break;
        }
        case 7: {
            Ctl::Cosm::SideBand& b = c.sb[1];
            read(0x901, { &b.q, &b.lpf, &b.lpfKf, &b.octave, &b.detune, &b.detuneLfo, &b.detuneOwn, &b.detuneEnv, &b.width, &b.widthKf, &b.widthLfo, &b.widthOwn, &b.widthEnv });
            kf(b.lpfKf); kf(b.widthKf);
            break;
        }
        case 5:
            read(0x8eb, { &c.resType, &c.resSize, &c.resSizeKf, &c.resSizeLfo, &c.resSizeEnv, &c.resBalance, &c.resBalanceLfo, &c.resBalanceEnv });
            kf(c.resSizeKf);
            break;
        case 14:
            read(0x93a, { &c.fsDepth, &c.fsDepthKf, &c.fsDepthLfo, &c.fsDepthEnv, &c.fsBalance, &c.fsBalanceLfo, &c.fsBalanceEnv });
            kf(c.fsDepthKf);
            break;
        case 15:
            read(0x941, { &c.lofiBit, &c.lofiRate, &c.lofiFilter, &c.lofiFilterKf, &c.lofiFilterLfo, &c.lofiFilterEnv, &c.lofiBalance, &c.lofiBalanceLfo, &c.lofiBalanceEnv });
            kf(c.lofiFilterKf);
            break;
        case 12:
            read(0x932, { &c.compAttack, &c.compSustain, &c.compLevel });
            break;
        case 13:
            read(0x935, { &c.limThreshold, &c.limRatio, &c.limAttack, &c.limRelease, &c.limLevel });
            break;
        case 2:   // Drive's Env depth before its LFO depth
            read(0x8d9, { &c.wsType, &c.wsLevel, &c.wsDrive, &c.wsDriveEnv, &c.wsDriveLfo, &c.wsTone, &c.wsToneKf, &c.wsToneLfo, &c.wsToneEnv });
            kf(c.wsToneKf);
            break;
        case 1:
            read(0x8d0, { &c.odType, &c.odLevel, &c.odDrive, &c.odDriveLfo, &c.odDriveEnv, &c.odTone, &c.odToneKf, &c.odToneLfo, &c.odToneEnv });
            kf(c.odToneKf);
            break;
        case 3:
            read(0x8e2, { &c.ampType, &c.ampBass, &c.ampMiddle, &c.ampTreble, &c.ampLevel, &c.ampGain, &c.ampGainLfo, &c.ampGainEnv });
            break;
        case 4:
            c.spkType = at(0x8ea);
            break;
        default: break;
    }
    cosmEnvOf(p, 8 + 2 * k, c.envA);
    cosmEnvOf(p, 9 + 2 * k, c.envB);
    lfoOf(p, 0x3be + 8 * (2 + k), c.lfoWave, c.lfoRate, c.lfoOffset, c.lfoDelay, c.lfoFadeMode, c.lfoFade, c.lfoKeySync);
}
} // namespace

XtSynth::Controls xtPatchControls(const std::vector<uint8_t>& store, int zone, const XtSynth::Controls& base) {
    Ctl c = base;
    const Read p { store, zone };
    // What belongs to the wave, kept through the two reads of an OSC
    const auto oscKeeping = [&](int n, Osc& o) {
        const int rootKey = o.rootKey, origTempo = o.origTempo, loopMode = o.loopMode;
        const float loopStart = o.loopStart, loopEnd = o.loopEnd;
        oscOf(p, n, o);
        o.rootKey = rootKey; o.origTempo = origTempo; o.loopMode = loopMode; o.loopStart = loopStart; o.loopEnd = loopEnd;
    };
    oscKeeping(0, c);
    oscKeeping(1, c.osc2);
    cosmOf(p, 0, c.cosm[0]);
    cosmOf(p, 1, c.cosm[1]);
    c.patchTempo = p(0x11d);
    c.patchCoarse = p(0x110); c.patchFine = p(0x111); c.octave = p(0x112); c.mono = p(0x113) == 0; c.legato = p(0x114) != 0;
    c.portamento = p(0x115) != 0; c.portaMode = p(0x116); c.portaType = p(0x117); c.portaTime = p(0x118); c.portaVeloSens = p(0x119);
    c.bendUp = p(0x11e); c.bendDown = p(0x11f);
    c.scaleTune = p(0x134) != 0;
    for (int i = 0; i < 12; ++i) c.scale[i] = p(0x136 + 2 * i);
    c.structure = p(0x293);
    c.mod.on = p(0x294) != 0; c.mod.type = p(0x295); c.mod.level = p(0x296); c.mod.attack = p(0x297); c.mod.release = p(0x298);
    c.tvaOn = p(0x299) != 0;
    c.tvaLevel = p(0x29a); c.tvaLevelKF = 10 * p(0x29b); c.tvaLevelLfo = p(0x29c); c.tvaPan = p(0x29d); c.tvaPanKF = 10 * p(0x29e); c.tvaPanLfo = p(0x29f); c.mfxSend = p(0x2a0);
    c.chorusSend = p(0x2a1); c.reverbSend = p(0x2a2); c.outputAssign = p(0x2a3);
    for (int i = 0; i < 8; ++i) {
        const int b = 0x14d + 5 * i;
        c.matrix[i].source = p(b); c.matrix[i].dest[0] = p(b + 1); c.matrix[i].sens[0] = p(b + 2); c.matrix[i].dest[1] = p(b + 3); c.matrix[i].sens[1] = p(b + 4);
    }
    lfoOf(p, 0x3de, c.tvaLfoWave, c.tvaLfoRate, c.tvaLfoOffset, c.tvaLfoDelay, c.tvaLfoFadeMode, c.tvaLfoFade, c.tvaLfoKeySync);
    const EnvIds a = envIds(12);
    c.attack = p(a.attack()); c.decay = p(a.decay()); c.sustain = p(a.sustain()); c.release = p(a.release());
    c.ampVelCurve = p(a.curve()); c.ampVelSens = p(a.sens()); c.aSens = p(a.aSens()); c.dSens = p(a.dSens()); c.rSens = p(a.rSens()); c.timeKF = 10 * p(a.kf());
    return c;
}

void xtPatchZoneSet(const std::vector<uint8_t>& store, const XtSynth::Controls& base, XtSynth::Zones& z) {
    for (int i = 0; i < XtSynth::kZones; ++i) {
        z.upper[i] = xtPatchValue(store, 0x124 + i);
        if (i) z.ctl[i] = xtPatchControls(store, i, base);
        if (i) for (int k = 0; k < 2; ++k) z.number[i][k] = xtPatchWaveNumber(store, i, k);   // STEP's count starts again on another

    }
}

int xtPatchWaveNumber(const std::vector<uint8_t>& store, int zone, int osc) {
    return xtPatchValue(store, osc ? 0x2f2 : 0x2be, zone);
}

void xtWaveOntoControls(const XtWaveFile& w, XtSynth::OscControls& o) {
    o.rootKey = std::clamp(w.originalKey, 0, 127);
    o.origTempo = w.tempo;
    const double last = (double)std::max(w.frames - 1, 1L);
    o.loopStart = (float)((double)w.loopStart / last);
    o.loopEnd = (float)((double)w.loopEnd / last);
    o.loopMode = std::clamp(w.fwdMode, 0, 2);
    if (!w.loopSwitchDecides) o.loop = true;
}

void xtWaveOntoEncoded(const XtWaveFile& w, XtEncoded& e) {
    e.tune = xtWaveTune(w.headerCents, w.tuneFine, w.tune);
    e.loopRate[0] = xtWaveLoopRate(w.loopRateFwd);
    e.loopRate[1] = xtWaveLoopRate(w.loopRateBwd);
    if (w.vpnoCount >= 0) { e.vpno = w.vpno; e.vpnoCount = w.vpnoCount; }   // its own list in place of the encode's
    if (w.bwdStart < 0 || w.bwdStart > w.bwdEnd || w.bwdEnd >= w.frames) { e.bwdMode = -1; return; }   // a region that isn't in the wave: the forward loop's
    e.bwdMode = std::clamp(w.bwdMode, 0, 2);
    e.bwdStart = (int)w.bwdStart;
    e.bwdEnd = (int)w.bwdEnd;
}

std::vector<std::string> xtPatchMissing(const std::vector<uint8_t>& store, int zone) {
    std::vector<std::string> m;
    const Read p { store, zone };
    for (int n = 0; n < 2; ++n) {
        const int b = 0x34 * n;
        const std::string osc = "OSC " + std::to_string(n + 1) + " ";
        if (p(0x2a4 + b) != 0 && p(0x2a5 + b) == kExternal) m.push_back(osc + "external input");
    }   // Playback Mode is done on every kind of wave (v156); a LITE wave's converter doesn't read it, on the XT either
    if (p(0x42e)) m.push_back("arpeggio");
    if (p(0x48d)) m.push_back("Multi Step Modulator");
    return m;
}

std::vector<XtPatchFx> xtPatchEffects(const std::vector<uint8_t>& store) {
    std::vector<XtPatchFx> out;
    for (int id : { 0x120, 0x121, 0x122, 0x177, 0x178, 0x179, 0x17a, 0x1fc, 0x1fd, 0x1fe, 0x230, 0x231 }) out.push_back({ id, xtPatchValue(store, id) });
    static constexpr struct { int typeId, slots; } kBlocks[3] = { { 0x177, kXtMfxSlots }, { 0x1fc, kXtChorusSlots }, { 0x230, kXtReverbSlots } };
    for (int b = 0; b < 3; ++b) {
        const int current = xtPatchValue(store, kBlocks[b].typeId);
        for (int t = 0; t < xtEffectTypes(b); ++t) {
            int first = 0, last = -1;
            if (!xtEffectTypeIds(b, t, first, last)) continue;
            // Its own type's from the patch; the others', which it doesn't hold, back to their defaults
            for (int id = first; id <= last; ++id) {
                const XtTypedDesc* d = xtTypedDesc(id);
                if (d) out.push_back({ id, t == current ? xtPatchTyped(store, kBlocks[b].slots, id) : d->def });
            }
        }
    }
    return out;
}
