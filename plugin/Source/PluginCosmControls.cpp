// COSM 1's and COSM 2's controls from and to the parameters.  Moved as they stood from xtControls and setControls (v190).
#include "PluginProcessor.h"
#include "PluginParamIds.h"

void VSynthEmuProcessor::readCosmControls(XtSynth::Controls& c) const {
    for (int k = 0; k < 2; ++k) {
        using Params::cosmId;
        XtSynth::Controls::Cosm& m = c.cosm[k];
        m.on        = apvts.getRawParameterValue(cosmId(k, "Switch"))->load() >= 0.5f;
        m.type      = knob(cosmId(k, "Type"));
        m.resoOwn   = knob(cosmId(k, "ResoOwn"));
        m.cutoffOwn = knob(cosmId(k, "CutoffOwn"));
        m.filter    = knob(cosmId(k, "Filter"));
        m.cutoff    = knob(cosmId(k, "Cutoff"));
        m.reso      = knob(cosmId(k, "Reso"));
        m.kf        = kf(cosmId(k, "CutoffKF"));
        m.cutoffEnv = knob(cosmId(k, "CutoffEnv"));
        m.cutoffLfo = knob(cosmId(k, "CutoffLfo"));
        m.resoEnv   = knob(cosmId(k, "ResoEnv"));
        m.resoLfo   = knob(cosmId(k, "ResoLfo"));
        m.dualType  = knob(cosmId(k, "DualType"));
        m.freq1 = knob(cosmId(k, "Dual1Freq")); m.kf1 = kf(cosmId(k, "Dual1KF")); m.q1 = knob(cosmId(k, "Dual1Q"));
        m.lfo1  = knob(cosmId(k, "Dual1Lfo"));  m.env1 = knob(cosmId(k, "Dual1Env"));
        m.freq2 = knob(cosmId(k, "Dual2Freq")); m.kf2 = kf(cosmId(k, "Dual2KF")); m.q2 = knob(cosmId(k, "Dual2Q"));
        m.lfo2  = knob(cosmId(k, "Dual2Lfo"));  m.env2 = knob(cosmId(k, "Dual2Env"));
        m.tbCutoff = knob(cosmId(k, "TbCutoff")); m.tbKf = kf(cosmId(k, "TbKF")); m.tbLfo = knob(cosmId(k, "TbLfo"));
        m.tbEnv = knob(cosmId(k, "TbEnv")); m.tbQ = knob(cosmId(k, "TbQ")); m.tbQFollow = knob(cosmId(k, "TbQFollow"));
        m.tbHpf = knob(cosmId(k, "TbHpf")); m.tbHpfKf = kf(cosmId(k, "TbHpfKF")); m.tbHpfLfo = knob(cosmId(k, "TbHpfLfo"));
        m.tbHpfEnv = knob(cosmId(k, "TbHpfEnv")); m.tbGain = knob(cosmId(k, "TbGain"));
        m.combOctave = knob(cosmId(k, "CombOctave")); m.combDetune = knob(cosmId(k, "CombDetune"));
        m.combDetuneLfo = knob(cosmId(k, "CombDetuneLfo")); m.combDetuneEnv = knob(cosmId(k, "CombDetuneEnv"));
        m.combTone = knob(cosmId(k, "CombTone")); m.combToneLfo = knob(cosmId(k, "CombToneLfo")); m.combToneEnv = knob(cosmId(k, "CombToneEnv"));
        m.resType = knob(cosmId(k, "ResType")); m.resSize = knob(cosmId(k, "ResSize")); m.resSizeKf = kf(cosmId(k, "ResSizeKF"));
        m.resSizeLfo = knob(cosmId(k, "ResSizeLfo")); m.resSizeEnv = knob(cosmId(k, "ResSizeEnv")); m.resBalance = knob(cosmId(k, "ResBalance"));
        m.resBalanceLfo = knob(cosmId(k, "ResBalanceLfo")); m.resBalanceEnv = knob(cosmId(k, "ResBalanceEnv"));
        m.fsDepth = knob(cosmId(k, "FsDepth")); m.fsDepthKf = kf(cosmId(k, "FsDepthKF")); m.fsDepthLfo = knob(cosmId(k, "FsDepthLfo"));
        m.fsDepthEnv = knob(cosmId(k, "FsDepthEnv")); m.fsBalance = knob(cosmId(k, "FsBalance"));
        m.fsBalanceLfo = knob(cosmId(k, "FsBalanceLfo")); m.fsBalanceEnv = knob(cosmId(k, "FsBalanceEnv"));
        m.lofiBit = knob(cosmId(k, "LofiBit")); m.lofiRate = knob(cosmId(k, "LofiRate")); m.lofiFilter = knob(cosmId(k, "LofiFilter"));
        m.lofiFilterKf = kf(cosmId(k, "LofiFilterKF")); m.lofiFilterLfo = knob(cosmId(k, "LofiFilterLfo"));
        m.lofiFilterEnv = knob(cosmId(k, "LofiFilterEnv")); m.lofiBalance = knob(cosmId(k, "LofiBalance"));
        m.lofiBalanceLfo = knob(cosmId(k, "LofiBalanceLfo")); m.lofiBalanceEnv = knob(cosmId(k, "LofiBalanceEnv"));
        m.compAttack = knob(cosmId(k, "CompAttack")); m.compSustain = knob(cosmId(k, "CompSustain")); m.compLevel = knob(cosmId(k, "CompLevel"));
        m.limThreshold = knob(cosmId(k, "LimThreshold")); m.limRatio = knob(cosmId(k, "LimRatio")); m.limAttack = knob(cosmId(k, "LimAttack"));
        m.limRelease = knob(cosmId(k, "LimRelease")); m.limLevel = knob(cosmId(k, "LimLevel"));
        m.wsType = knob(cosmId(k, "WsType")); m.wsLevel = knob(cosmId(k, "WsLevel")); m.wsDrive = knob(cosmId(k, "WsDrive"));
        m.wsDriveLfo = knob(cosmId(k, "WsDriveLfo")); m.wsDriveEnv = knob(cosmId(k, "WsDriveEnv")); m.wsTone = knob(cosmId(k, "WsTone"));
        m.wsToneKf = kf(cosmId(k, "WsToneKF")); m.wsToneLfo = knob(cosmId(k, "WsToneLfo")); m.wsToneEnv = knob(cosmId(k, "WsToneEnv"));
        m.odType = knob(cosmId(k, "OdType")); m.odLevel = knob(cosmId(k, "OdLevel")); m.odDrive = knob(cosmId(k, "OdDrive"));
        m.odDriveLfo = knob(cosmId(k, "OdDriveLfo")); m.odDriveEnv = knob(cosmId(k, "OdDriveEnv")); m.odTone = knob(cosmId(k, "OdTone"));
        m.odToneKf = kf(cosmId(k, "OdToneKF")); m.odToneLfo = knob(cosmId(k, "OdToneLfo")); m.odToneEnv = knob(cosmId(k, "OdToneEnv"));
        m.ampType = knob(cosmId(k, "AmpType")); m.ampBass = knob(cosmId(k, "AmpBass")); m.ampMiddle = knob(cosmId(k, "AmpMiddle"));
        m.ampTreble = knob(cosmId(k, "AmpTreble")); m.ampLevel = knob(cosmId(k, "AmpLevel")); m.ampGain = knob(cosmId(k, "AmpGain"));
        m.ampGainLfo = knob(cosmId(k, "AmpGainLfo")); m.ampGainEnv = knob(cosmId(k, "AmpGainEnv"));
        m.spkType = knob(cosmId(k, "SpkType"));
        for (int t = 0; t < 2; ++t) {
            XtSynth::Controls::Cosm::SideBand& b = m.sb[t];
            const juce::String id = cosmId(k, "Sb") + juce::String(t + 1);
            if (t == 0) { b.hpf = knob(id + "Hpf"); b.subDetune = knob(id + "SubDetune"); }
            else b.q = knob(id + "Q");
            b.lpf = knob(id + "Lpf"); b.lpfKf = kf(id + "LpfKF"); b.octave = knob(id + "Octave");
            b.detune = knob(id + "Detune"); b.detuneLfo = knob(id + "DetuneLfo"); b.detuneOwn = knob(id + "DetuneOwn"); b.detuneEnv = knob(id + "DetuneEnv");
            b.width = knob(id + "Width"); b.widthKf = kf(id + "WidthKF"); b.widthLfo = knob(id + "WidthLfo");
            b.widthOwn = knob(id + "WidthOwn"); b.widthEnv = knob(id + "WidthEnv");
        }
        for (int ab = 0; ab < 2; ++ab) {
            XtSynth::Controls::CosmEnv& e = ab == 0 ? m.envA : m.envB;
            const juce::String id = cosmId(k, ab == 0 ? "EnvA" : "EnvB");
            int* const f[9] = { &e.attack, &e.decay, &e.sustain, &e.release, &e.velCurve, &e.velSens, &e.aSens, &e.dSens, &e.rSens };
            for (int i = 0; i < 9; ++i) *f[i] = knob(id + Params::kCosmEnvKnobs[i]);
            e.timeKF = kf(id + "TimeKF");
        }
        m.lfoWave     = knob(cosmId(k, "LfoWave"));
        m.lfoRate     = knob(cosmId(k, "LfoRate"));
        m.lfoOffset   = knob(cosmId(k, "LfoOffset"));
        m.lfoDelay    = knob(cosmId(k, "LfoDelay"));
        m.lfoFadeMode = knob(cosmId(k, "LfoFadeMode"));
        m.lfoFade     = knob(cosmId(k, "LfoFade"));
        m.lfoKeySync  = apvts.getRawParameterValue(cosmId(k, "LfoKeySync"))->load() >= 0.5f;
    }
}

void VSynthEmuProcessor::writeCosmControls(const XtSynth::Controls& c) {
    for (int k = 0; k < 2; ++k) {
        using Params::cosmId;
        const XtSynth::Controls::Cosm& m = c.cosm[k];
        set(cosmId(k, "Switch"), m.on ? 1 : 0);
        set(cosmId(k, "Type"), m.type);
        set(cosmId(k, "ResoOwn"), m.resoOwn);
        set(cosmId(k, "CutoffOwn"), m.cutoffOwn);
        set(cosmId(k, "Filter"), m.filter);
        set(cosmId(k, "Cutoff"), m.cutoff);
        set(cosmId(k, "Reso"), m.reso);
        set(cosmId(k, "CutoffKF"), m.kf);
        set(cosmId(k, "CutoffEnv"), m.cutoffEnv);
        set(cosmId(k, "CutoffLfo"), m.cutoffLfo);
        set(cosmId(k, "ResoEnv"), m.resoEnv);
        set(cosmId(k, "ResoLfo"), m.resoLfo);
        set(cosmId(k, "DualType"), m.dualType);
        set(cosmId(k, "Dual1Freq"), m.freq1); set(cosmId(k, "Dual1KF"), m.kf1); set(cosmId(k, "Dual1Q"), m.q1);
        set(cosmId(k, "Dual1Lfo"), m.lfo1); set(cosmId(k, "Dual1Env"), m.env1);
        set(cosmId(k, "Dual2Freq"), m.freq2); set(cosmId(k, "Dual2KF"), m.kf2); set(cosmId(k, "Dual2Q"), m.q2);
        set(cosmId(k, "Dual2Lfo"), m.lfo2); set(cosmId(k, "Dual2Env"), m.env2);
        set(cosmId(k, "TbCutoff"), m.tbCutoff); set(cosmId(k, "TbKF"), m.tbKf); set(cosmId(k, "TbLfo"), m.tbLfo);
        set(cosmId(k, "TbEnv"), m.tbEnv); set(cosmId(k, "TbQ"), m.tbQ); set(cosmId(k, "TbQFollow"), m.tbQFollow);
        set(cosmId(k, "TbHpf"), m.tbHpf); set(cosmId(k, "TbHpfKF"), m.tbHpfKf); set(cosmId(k, "TbHpfLfo"), m.tbHpfLfo);
        set(cosmId(k, "TbHpfEnv"), m.tbHpfEnv); set(cosmId(k, "TbGain"), m.tbGain);
        set(cosmId(k, "CombOctave"), m.combOctave); set(cosmId(k, "CombDetune"), m.combDetune);
        set(cosmId(k, "CombDetuneLfo"), m.combDetuneLfo); set(cosmId(k, "CombDetuneEnv"), m.combDetuneEnv);
        set(cosmId(k, "CombTone"), m.combTone); set(cosmId(k, "CombToneLfo"), m.combToneLfo); set(cosmId(k, "CombToneEnv"), m.combToneEnv);
        set(cosmId(k, "ResType"), m.resType); set(cosmId(k, "ResSize"), m.resSize); set(cosmId(k, "ResSizeKF"), m.resSizeKf);
        set(cosmId(k, "ResSizeLfo"), m.resSizeLfo); set(cosmId(k, "ResSizeEnv"), m.resSizeEnv); set(cosmId(k, "ResBalance"), m.resBalance);
        set(cosmId(k, "ResBalanceLfo"), m.resBalanceLfo); set(cosmId(k, "ResBalanceEnv"), m.resBalanceEnv);
        set(cosmId(k, "FsDepth"), m.fsDepth); set(cosmId(k, "FsDepthKF"), m.fsDepthKf); set(cosmId(k, "FsDepthLfo"), m.fsDepthLfo);
        set(cosmId(k, "FsDepthEnv"), m.fsDepthEnv); set(cosmId(k, "FsBalance"), m.fsBalance);
        set(cosmId(k, "FsBalanceLfo"), m.fsBalanceLfo); set(cosmId(k, "FsBalanceEnv"), m.fsBalanceEnv);
        set(cosmId(k, "LofiBit"), m.lofiBit); set(cosmId(k, "LofiRate"), m.lofiRate); set(cosmId(k, "LofiFilter"), m.lofiFilter);
        set(cosmId(k, "LofiFilterKF"), m.lofiFilterKf); set(cosmId(k, "LofiFilterLfo"), m.lofiFilterLfo);
        set(cosmId(k, "LofiFilterEnv"), m.lofiFilterEnv); set(cosmId(k, "LofiBalance"), m.lofiBalance);
        set(cosmId(k, "LofiBalanceLfo"), m.lofiBalanceLfo); set(cosmId(k, "LofiBalanceEnv"), m.lofiBalanceEnv);
        set(cosmId(k, "CompAttack"), m.compAttack); set(cosmId(k, "CompSustain"), m.compSustain); set(cosmId(k, "CompLevel"), m.compLevel);
        set(cosmId(k, "LimThreshold"), m.limThreshold); set(cosmId(k, "LimRatio"), m.limRatio); set(cosmId(k, "LimAttack"), m.limAttack);
        set(cosmId(k, "LimRelease"), m.limRelease); set(cosmId(k, "LimLevel"), m.limLevel);
        set(cosmId(k, "WsType"), m.wsType); set(cosmId(k, "WsLevel"), m.wsLevel); set(cosmId(k, "WsDrive"), m.wsDrive);
        set(cosmId(k, "WsDriveLfo"), m.wsDriveLfo); set(cosmId(k, "WsDriveEnv"), m.wsDriveEnv); set(cosmId(k, "WsTone"), m.wsTone);
        set(cosmId(k, "WsToneKF"), m.wsToneKf); set(cosmId(k, "WsToneLfo"), m.wsToneLfo); set(cosmId(k, "WsToneEnv"), m.wsToneEnv);
        set(cosmId(k, "OdType"), m.odType); set(cosmId(k, "OdLevel"), m.odLevel); set(cosmId(k, "OdDrive"), m.odDrive);
        set(cosmId(k, "OdDriveLfo"), m.odDriveLfo); set(cosmId(k, "OdDriveEnv"), m.odDriveEnv); set(cosmId(k, "OdTone"), m.odTone);
        set(cosmId(k, "OdToneKF"), m.odToneKf); set(cosmId(k, "OdToneLfo"), m.odToneLfo); set(cosmId(k, "OdToneEnv"), m.odToneEnv);
        set(cosmId(k, "AmpType"), m.ampType); set(cosmId(k, "AmpBass"), m.ampBass); set(cosmId(k, "AmpMiddle"), m.ampMiddle);
        set(cosmId(k, "AmpTreble"), m.ampTreble); set(cosmId(k, "AmpLevel"), m.ampLevel); set(cosmId(k, "AmpGain"), m.ampGain);
        set(cosmId(k, "AmpGainLfo"), m.ampGainLfo); set(cosmId(k, "AmpGainEnv"), m.ampGainEnv);
        set(cosmId(k, "SpkType"), m.spkType);
        for (int t = 0; t < 2; ++t) {
            const XtSynth::Controls::Cosm::SideBand& b = m.sb[t];
            const juce::String id = cosmId(k, "Sb") + juce::String(t + 1);
            set(id + "SubDetune", b.subDetune);
            if (t == 0) { set(id + "Hpf", b.hpf); set(id + "SubDetune", b.subDetune); }
            else set(id + "Q", b.q);
            set(id + "Lpf", b.lpf); set(id + "LpfKF", b.lpfKf); set(id + "Octave", b.octave);
            set(id + "Detune", b.detune); set(id + "DetuneLfo", b.detuneLfo); set(id + "DetuneOwn", b.detuneOwn); set(id + "DetuneEnv", b.detuneEnv);
            set(id + "Width", b.width); set(id + "WidthKF", b.widthKf); set(id + "WidthLfo", b.widthLfo);
            set(id + "WidthOwn", b.widthOwn); set(id + "WidthEnv", b.widthEnv);
        }
        for (int ab = 0; ab < 2; ++ab) {
            const XtSynth::Controls::CosmEnv& e = ab == 0 ? m.envA : m.envB;
            const juce::String id = cosmId(k, ab == 0 ? "EnvA" : "EnvB");
            const int* const f[9] = { &e.attack, &e.decay, &e.sustain, &e.release, &e.velCurve, &e.velSens, &e.aSens, &e.dSens, &e.rSens };
            for (int i = 0; i < 9; ++i) set(id + Params::kCosmEnvKnobs[i], *f[i]);
            set(id + "TimeKF", e.timeKF);
        }
        set(cosmId(k, "LfoWave"), m.lfoWave);
        set(cosmId(k, "LfoRate"), m.lfoRate);
        set(cosmId(k, "LfoOffset"), m.lfoOffset);
        set(cosmId(k, "LfoDelay"), m.lfoDelay);
        set(cosmId(k, "LfoFadeMode"), m.lfoFadeMode);
        set(cosmId(k, "LfoFade"), m.lfoFade);
        set(cosmId(k, "LfoKeySync"), m.lfoKeySync ? 1 : 0);
    }
}
