// The parameters into XtSynth's controls: OSC 1's knobs, the tuning, tempo, loop, portamento, keys and MOD; the patch TVA,
// LFO 5, the Matrix, the assigns, the envelopes and the LFO; the OSCs' envelopes' velocity.  Moved as they stood from
// xtControls (v190), which calls them in its order; knob and kf were its lambdas.
#include "PluginProcessor.h"
#include "PluginParamIds.h"
#include <array>

int VSynthEmuProcessor::knob(const juce::String& id) const {
    return (int)std::lround(apvts.getRawParameterValue(id)->load());
}

int VSynthEmuProcessor::kf(const juce::String& id) const {
    return 10 * (int)std::lround(apvts.getRawParameterValue(id)->load() / 10.0f);
}

void VSynthEmuProcessor::readOscControls(XtSynth::Controls& c) const {
    // Snapped to the parameters' steps: the ranges' float conversions leave
    // the defaults at e.g. 0.99999994, and the firmware's fixed-point steps
    // would play that, not 1 (a centred XT knob is exact).
    auto snap = [](float v, float stepSize) { return std::round(v / stepSize) * stepSize; };
    c.pitchSemis   = snap(apvts.getRawParameterValue(Params::kPitchShift)->load(), 0.01f);
    c.oscTime      = knob(Params::kOscTime);
    c.oscTimeOffset = knob(Params::kOscTimeOffset);
    c.oscTimeKF    = kf(Params::kOscTimeKF);
    c.oscFormant   = knob(Params::kOscFormant);
    c.oscFormantKF = kf(Params::kOscFormantKF);
    c.oscType      = knob(Params::kOscType);
    c.ana.wave     = knob(Params::kAnaWave);
    c.ana.impact   = knob(Params::kAnaImpact);
    c.ana.pw       = knob(Params::kAnaPw);
    c.ana.pwKf     = kf(Params::kAnaPwKF) / 10;             // as the patch stores it
    c.ana.pwLfo    = knob(Params::kAnaPwLfo);
    c.ana.pwEnv    = knob(Params::kAnaPwEnv);
    c.ana.fat      = knob(Params::kAnaFat);
    c.ana.fatKf    = kf(Params::kAnaFatKF) / 10;
    c.ana.fatLfo   = knob(Params::kAnaFatLfo);
    c.ana.fatEnv   = knob(Params::kAnaFatEnv);
    c.ana.detune   = knob(Params::kAnaDetune);
    c.ana.subOctave = knob(Params::kAnaSubOct);
    c.ana.subLevel = knob(Params::kAnaSubLevel);
    c.oscCoarse    = knob(Params::kOscCoarse);
    c.oscFine      = knob(Params::kOscFine);
    c.oscPitch     = knob(Params::kOscPitch);
    c.pitchKF      = kf(Params::kPitchKF);
    c.randomPitch  = knob(Params::kRandomPitch);
    c.patchCoarse  = knob(Params::kPatchCoarse);
    c.patchFine    = knob(Params::kPatchFine);
    c.bendUp       = knob(Params::kBendUp);
    c.bendDown     = knob(Params::kBendDown);
    c.scaleTune    = apvts.getRawParameterValue(Params::kScaleTune)->load() >= 0.5f;
    for (int i = 0; i < 12; ++i) c.scale[i] = knob(Params::kScaleTunes[i]);
    c.patchTempo   = (int)std::lround(snap(apvts.getRawParameterValue(Params::kPatchTempo)->load(), 0.1f) * 10.0f);
    c.clockSource  = knob(Params::kClockSource);
    c.tempoSync    = apvts.getRawParameterValue(Params::kTempoSync)->load() >= 0.5f;
    c.origTempo    = (int)std::lround(apvts.getRawParameterValue(Params::kOrigTempo)->load() * 10000.0f);
    c.timeTrip     = apvts.getRawParameterValue(Params::kTripSw)->load() >= 0.5f;
    c.beatKeep     = apvts.getRawParameterValue(Params::kBeatKeep)->load() >= 0.5f;
    c.loop         = apvts.getRawParameterValue(Params::kLoop)->load() >= 0.5f;
    c.loopMode     = knob(Params::kLoopMode);
    c.loopStart    = apvts.getRawParameterValue(Params::kLoopStart)->load() / 100.0f;   // not to a step: a wave's own loop is to the sample
    c.loopEnd      = apvts.getRawParameterValue(Params::kLoopEnd)->load() / 100.0f;
    c.startOffset  = knob(Params::kStartOffset);
    c.playbackMode = knob(Params::kPlaybackMode);
    c.portamento   = apvts.getRawParameterValue(Params::kPorta)->load() >= 0.5f;
    c.portaMode    = knob(Params::kPortaMode);
    c.portaType    = knob(Params::kPortaType);
    c.portaTime    = knob(Params::kPortaTime);
    c.portaVeloSens = knob(Params::kPortaVeloSens);
    c.mono         = knob(Params::kMonoPoly) == 0;
    c.legato       = apvts.getRawParameterValue(Params::kLegato)->load() >= 0.5f;
    c.octave       = knob(Params::kOctave);
    c.mod.on       = apvts.getRawParameterValue(Params::kModSwitch)->load() >= 0.5f;
    c.mod.type     = knob(Params::kModType);
    c.mod.level    = knob(Params::kModLevel);
    c.mod.attack   = knob(Params::kModAttack);
    c.mod.release  = knob(Params::kModRelease);
    c.structure    = knob(Params::kStructure);
}

void VSynthEmuProcessor::readToneControls(XtSynth::Controls& c) const {
    c.energy       = knob(Params::kEnergy);
    c.rootKey      = (int)apvts.getRawParameterValue(Params::kRootKey)->load();
    c.tvaOn        = apvts.getRawParameterValue(Params::kTvaSwitch)->load() >= 0.5f;
    c.chorusSend   = knob(Params::kTvaChoSend);
    c.reverbSend   = knob(Params::kTvaRevSend);
    c.outputAssign = knob(Params::kOutputAssign);
    c.parallel     = knob(Params::kOutputMode) != 0;
    c.tvaLevel     = knob(Params::kTvaLevel);
    c.tvaLevelKF   = kf(Params::kTvaLevelKF);
    c.tvaLevelLfo  = knob(Params::kTvaLevelLfo);
    c.tvaPan       = knob(Params::kTvaPan);
    c.tvaPanKF     = kf(Params::kTvaPanKF);
    c.tvaPanLfo    = knob(Params::kTvaPanLfo);
    c.mfxSend      = knob(Params::kMfxSend);
    c.tvaLfoWave   = knob(Params::kTvaLfoWave);
    c.tvaLfoRate   = knob(Params::kTvaLfoRate);
    c.tvaLfoOffset = knob(Params::kTvaLfoOffset);
    c.tvaLfoDelay  = knob(Params::kTvaLfoDelay);
    c.tvaLfoFadeMode = knob(Params::kTvaLfoMode);
    c.tvaLfoFade   = knob(Params::kTvaLfoFade);
    c.tvaLfoKeySync = apvts.getRawParameterValue(Params::kTvaLfoSync)->load() >= 0.5f;
    {
        static const std::array<std::array<juce::String, 5>, 8> mxIds = [] {
            std::array<std::array<juce::String, 5>, 8> a;
            for (int m = 0; m < 8; ++m)
                a[(size_t)m] = { matrixParamId(m, "Source"), matrixParamId(m, "Dest1"), matrixParamId(m, "Sens1"), matrixParamId(m, "Dest2"), matrixParamId(m, "Sens2") };
            return a;
        }();
        for (size_t m = 0; m < 8; ++m) {
            XtMatrixControl& x = c.matrix[m];
            x.source = knob(mxIds[m][0]);
            x.dest[0] = knob(mxIds[m][1]); x.sens[0] = knob(mxIds[m][2]);
            x.dest[1] = knob(mxIds[m][3]); x.sens[1] = knob(mxIds[m][4]);
        }
        for (int i = 0; i < kAssigns; ++i) c.assign[i] = knob(assignParamId(i));
    }
    c.attack       = (int)apvts.getRawParameterValue(Params::kAttack)->load();
    c.decay        = (int)apvts.getRawParameterValue(Params::kDecay)->load();
    c.sustain      = (int)apvts.getRawParameterValue(Params::kSustain)->load();
    c.release      = (int)apvts.getRawParameterValue(Params::kRelease)->load();
    c.aSens        = (int)apvts.getRawParameterValue(Params::kAttackSens)->load();
    c.dSens        = (int)apvts.getRawParameterValue(Params::kDecaySens)->load();
    c.rSens        = (int)apvts.getRawParameterValue(Params::kReleaseSens)->load();
    c.timeKF       = 10 * (int)std::lround(apvts.getRawParameterValue(Params::kTimeKF)->load() / 10.0f);
    c.pAttack      = (int)apvts.getRawParameterValue(Params::kPAttack)->load();
    c.pDecay       = (int)apvts.getRawParameterValue(Params::kPDecay)->load();
    c.pSustain     = (int)apvts.getRawParameterValue(Params::kPSustain)->load();
    c.pRelease     = (int)apvts.getRawParameterValue(Params::kPRelease)->load();
    c.pDepth       = (int)apvts.getRawParameterValue(Params::kPDepth)->load();
    c.tAttack      = knob(Params::kTAttack);
    c.tDecay       = knob(Params::kTDecay);
    c.tSustain     = knob(Params::kTSustain);
    c.tRelease     = knob(Params::kTRelease);
    c.tDepth       = knob(Params::kTDepth);
    c.fAttack      = knob(Params::kFAttack);
    c.fDecay       = knob(Params::kFDecay);
    c.fSustain     = knob(Params::kFSustain);
    c.fRelease     = knob(Params::kFRelease);
    c.fDepth       = knob(Params::kFDepth);
    c.lfoTimeDepth = knob(Params::kLfoTimeDepth);
    c.lfoFormantDepth = knob(Params::kLfoFmtDepth);
    c.oscLevel     = knob(Params::kOscLevel);
    c.oscLevelKF   = kf(Params::kOscLevelKF);
    c.oscLevelLfoDepth = knob(Params::kOscLevelLfo);
    c.waveGain     = knob(Params::kWaveGain);
    c.oAttack      = knob(Params::kOAttack);
    c.oDecay       = knob(Params::kODecay);
    c.oSustain     = knob(Params::kOSustain);
    c.oRelease     = knob(Params::kORelease);
    c.oASens       = knob(Params::kOASens);
    c.oDSens       = knob(Params::kODSens);
    c.oRSens       = knob(Params::kORSens);
    c.oTimeKF      = kf(Params::kOTimeKF);
    c.oscVelSens   = knob(Params::kOscVelSens);
    c.oscVelCurve  = knob(Params::kOscVelCurve);
    c.ampVelSens   = knob(Params::kAmpVelSens);
    c.ampVelCurve  = knob(Params::kAmpVelCurve);
    c.robot        = apvts.getRawParameterValue(Params::kRobot)->load() > 0.5f;   // Robot Voice (SOLO)
    c.vari         = apvts.getRawParameterValue(Params::kVari)->load() > 0.5f;
    c.lfoWave      = (int)apvts.getRawParameterValue(Params::kLfoWave)->load();
    c.lfoRate      = (int)apvts.getRawParameterValue(Params::kLfoRate)->load();
    c.lfoPitchDepth = (int)apvts.getRawParameterValue(Params::kLfoDepth)->load();
    c.lfoDelay     = (int)apvts.getRawParameterValue(Params::kLfoDelay)->load();
    c.lfoFadeMode  = (int)apvts.getRawParameterValue(Params::kLfoFadeMode)->load();
    c.lfoFade      = (int)apvts.getRawParameterValue(Params::kLfoFade)->load();
    c.lfoOffset    = (int)apvts.getRawParameterValue(Params::kLfoOffset)->load();
    c.lfoKeySync   = apvts.getRawParameterValue(Params::kLfoKeySync)->load() > 0.5f;
    c.on           = apvts.getRawParameterValue(Params::kOscSwitch)->load() >= 0.5f;
}

void VSynthEmuProcessor::readEnvVelocity(XtSynth::Controls& c) const {
    for (int k = 0; k < 2; ++k) {   // each OSC's pitch, time and formant envelopes' velocity and key
        XtSynth::OscControls& o = k ? c.osc2 : c;
        XtSynth::OscControls::EnvVelocity* v[3] = { &o.pVel, &o.tVel, &o.fVel };
        static const juce::String kEnvs[3] = { "pitchEnv", "timeEnv", "formantEnv" };
        for (int e = 0; e < 3; ++e) {
            v[e]->curve = knob(Params::oscId(k, kEnvs[e] + "VelCurve"));
            v[e]->sens = knob(Params::oscId(k, kEnvs[e] + "VelSens"));
            v[e]->aSens = knob(Params::oscId(k, kEnvs[e] + "AttackSens"));
            v[e]->dSens = knob(Params::oscId(k, kEnvs[e] + "DecaySens"));
            v[e]->rSens = knob(Params::oscId(k, kEnvs[e] + "ReleaseSens"));
            v[e]->timeKF = kf(Params::oscId(k, kEnvs[e] + "TimeKF"));
        }
    }
}

XtSynth::Controls VSynthEmuProcessor::xtControls() const {
    XtSynth::Controls c;
    // The parts (PluginControlsRead.cpp, PluginCosmControls.cpp, PluginOsc2Controls.cpp, v190)
    readOscControls(c);
    readCosmControls(c);
    readToneControls(c);
    readOsc2Controls(c);
    readEnvVelocity(c);
    readArpControls(c);     // PluginArp.cpp, v190
    readMsmControls(c);     // PluginMsm.cpp, v193
    readZoneControls(c);    // PluginZones.cpp, v195
    return c;
}
