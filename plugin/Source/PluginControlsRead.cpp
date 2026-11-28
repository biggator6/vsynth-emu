// The parameters into XtSynth's controls: OSC 1's knobs, the tuning, tempo, loop, portamento, keys and MOD; the patch TVA,
// LFO 5, the Matrix, the assigns, the envelopes and the LFO; the OSCs' envelopes' velocity.  Moved as they stood from
// xtControls (v190), which calls them in its order; knob and kf were its lambdas.
#include "PluginProcessor.h"
#include "PluginParamIds.h"
#include <array>

// A parameter's value as the synth takes it: a MIDI edit's while the message thread hasn't handed it on (v462)
float VSynthEmuProcessor::raw(const juce::String& id) const {
    return midi_.edits.read(apvts.getRawParameterValue(id));
}

int VSynthEmuProcessor::knob(const juce::String& id) const {
    return (int)std::lround(raw(id));
}

int VSynthEmuProcessor::kf(const juce::String& id) const {
    return 10 * (int)std::lround(raw(id) / 10.0f);
}

void VSynthEmuProcessor::readOscControls(XtSynth::Controls& c) const {
    // Snapped to the parameters' steps: the ranges' float conversions leave
    // the defaults at e.g. 0.99999994, and the firmware's fixed-point steps
    // would play that, not 1 (a centred XT knob is exact).
    auto snap = [](float v, float stepSize) { return std::round(v / stepSize) * stepSize; };
    c.pitchSemis   = snap(raw(Params::kPitchShift), 0.01f);
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
    c.scaleTune    = raw(Params::kScaleTune) >= 0.5f;
    for (int i = 0; i < 12; ++i) c.scale[i] = knob(Params::kScaleTunes[i]);
    c.patchTempo   = (int)std::lround(snap(raw(Params::kPatchTempo), 0.1f) * 10.0f);
    c.clockSource  = knob(Params::kClockSource);
    c.tempoSync    = raw(Params::kTempoSync) >= 0.5f;
    c.origTempo    = (int)std::lround(raw(Params::kOrigTempo) * 10000.0f);
    c.timeTrip     = raw(Params::kTripSw) >= 0.5f;
    c.beatKeep     = raw(Params::kBeatKeep) >= 0.5f;
    c.loop         = raw(Params::kLoop) >= 0.5f;
    c.loopMode     = knob(Params::kLoopMode);
    c.loopStart    = raw(Params::kLoopStart) / 100.0f;   // not to a step: a wave's own loop is to the sample
    c.loopEnd      = raw(Params::kLoopEnd) / 100.0f;
    c.startOffset  = knob(Params::kStartOffset);
    c.playbackMode = knob(Params::kPlaybackMode);
    c.portamento   = raw(Params::kPorta) >= 0.5f;
    c.portaMode    = knob(Params::kPortaMode);
    c.portaType    = knob(Params::kPortaType);
    c.portaTime    = knob(Params::kPortaTime);
    c.portaVeloSens = knob(Params::kPortaVeloSens);
    c.mono         = knob(Params::kMonoPoly) == 0;
    c.legato       = raw(Params::kLegato) >= 0.5f;
    c.octave       = knob(Params::kOctave);
    c.mod.on       = raw(Params::kModSwitch) >= 0.5f;
    c.mod.type     = knob(Params::kModType);
    c.mod.level    = knob(Params::kModLevel);
    c.mod.attack   = knob(Params::kModAttack);
    c.mod.release  = knob(Params::kModRelease);
    c.structure    = knob(Params::kStructure);
}

void VSynthEmuProcessor::readToneControls(XtSynth::Controls& c) const {
    c.energy       = knob(Params::kEnergy);
    c.rootKey      = (int)raw(Params::kRootKey);
    c.tvaOn        = raw(Params::kTvaSwitch) >= 0.5f;
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
    c.tvaLfoKeySync = raw(Params::kTvaLfoSync) >= 0.5f;
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
    c.attack       = (int)raw(Params::kAttack);
    c.decay        = (int)raw(Params::kDecay);
    c.sustain      = (int)raw(Params::kSustain);
    c.release      = (int)raw(Params::kRelease);
    c.aSens        = (int)raw(Params::kAttackSens);
    c.dSens        = (int)raw(Params::kDecaySens);
    c.rSens        = (int)raw(Params::kReleaseSens);
    c.timeKF       = 10 * (int)std::lround(raw(Params::kTimeKF) / 10.0f);
    c.pAttack      = (int)raw(Params::kPAttack);
    c.pDecay       = (int)raw(Params::kPDecay);
    c.pSustain     = (int)raw(Params::kPSustain);
    c.pRelease     = (int)raw(Params::kPRelease);
    c.pDepth       = (int)raw(Params::kPDepth);
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
    c.robot        = raw(Params::kRobot) > 0.5f;   // Robot Voice (SOLO)
    c.vari         = raw(Params::kVari) > 0.5f;
    c.lfoWave      = (int)raw(Params::kLfoWave);
    c.lfoRate      = (int)raw(Params::kLfoRate);
    c.lfoPitchDepth = (int)raw(Params::kLfoDepth);
    c.lfoDelay     = (int)raw(Params::kLfoDelay);
    c.lfoFadeMode  = (int)raw(Params::kLfoFadeMode);
    c.lfoFade      = (int)raw(Params::kLfoFade);
    c.lfoOffset    = (int)raw(Params::kLfoOffset);
    c.lfoKeySync   = raw(Params::kLfoKeySync) > 0.5f;
    c.on           = raw(Params::kOscSwitch) >= 0.5f;
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
