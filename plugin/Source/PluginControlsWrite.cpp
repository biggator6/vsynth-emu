// The parameters set from XtSynth's controls, as a host's automation would, and only those that have moved: the tempo,
// the loops, the OSCs' envelopes' velocity, OSC 1's knobs, the tuning, portamento, keys and MOD; the patch TVA, LFO 5, the
// Matrix, the assigns, the envelopes and the LFO.  Moved as they stood from setControls (v190), which calls them in its
// order; set was its lambda.
#include "PluginProcessor.h"
#include "PluginParamIds.h"

void VSynthEmuProcessor::set(const juce::String& id, float v) {
    auto* p = apvts.getParameter(id);
    if (!p) { jassertfalse; return; }
    const float n = p->convertTo0to1(v);
    if (p->getValue() != n) p->setValueNotifyingHost(n);
}

void VSynthEmuProcessor::writeOscControls(const XtSynth::Controls& c) {
    set(Params::kPitchShift, c.pitchSemis);
    set(Params::kPatchTempo, (float)c.patchTempo / 10.0f);
    set(Params::kOrigTempo, (float)c.origTempo / 10000.0f);
    set(Params::kLoopStart, c.loopStart * 100.0f);
    set(Params::kLoopEnd, c.loopEnd * 100.0f);
    set(Params::kStartOffset, (float)startOffsetParam(c.startOffset));
    set(Params::kMonoPoly, c.mono ? 0 : 1);
    set(Params::kOutputMode, c.parallel ? 1 : 0);
    set(Params::kTimeKF, c.timeKF);
    {
        using Params::oscId;
        set(oscId(1, Params::kOrigTempo), (float)c.osc2.origTempo / 10000.0f);
        set(oscId(1, Params::kLoopStart), c.osc2.loopStart * 100.0f);
        set(oscId(1, Params::kLoopEnd), c.osc2.loopEnd * 100.0f);
        set(oscId(1, Params::kStartOffset), (float)startOffsetParam(c.osc2.startOffset));
        for (int k = 0; k < 2; ++k) {
            const XtSynth::OscControls& o = k ? c.osc2 : c;
            const XtSynth::OscControls::EnvVelocity* v[3] = { &o.pVel, &o.tVel, &o.fVel };
            static const juce::String kEnvs[3] = { "pitchEnv", "timeEnv", "formantEnv" };
            for (int e = 0; e < 3; ++e) {
                set(oscId(k, kEnvs[e] + "VelCurve"), v[e]->curve);
                set(oscId(k, kEnvs[e] + "VelSens"), v[e]->sens);
                set(oscId(k, kEnvs[e] + "AttackSens"), v[e]->aSens);
                set(oscId(k, kEnvs[e] + "DecaySens"), v[e]->dSens);
                set(oscId(k, kEnvs[e] + "ReleaseSens"), v[e]->rSens);
                set(oscId(k, kEnvs[e] + "TimeKF"), v[e]->timeKF);
            }
        }
    }
    
    set(Params::kOscTime, c.oscTime);
    set(Params::kOscTimeOffset, c.oscTimeOffset);
    set(Params::kOscTimeKF, c.oscTimeKF);
    set(Params::kOscFormant, c.oscFormant);
    set(Params::kOscFormantKF, c.oscFormantKF);
    set(Params::kOscType, c.oscType);
    set(Params::kAnaWave, c.ana.wave);
    set(Params::kAnaImpact, c.ana.impact);
    set(Params::kAnaPw, c.ana.pw);
    set(Params::kAnaPwKF, c.ana.pwKf * 10);
    set(Params::kAnaPwLfo, c.ana.pwLfo);
    set(Params::kAnaPwEnv, c.ana.pwEnv);
    set(Params::kAnaFat, c.ana.fat);
    set(Params::kAnaFatKF, c.ana.fatKf * 10);
    set(Params::kAnaFatLfo, c.ana.fatLfo);
    set(Params::kAnaFatEnv, c.ana.fatEnv);
    set(Params::kAnaDetune, c.ana.detune);
    set(Params::kAnaSubOct, c.ana.subOctave);
    set(Params::kAnaSubLevel, c.ana.subLevel);
    set(Params::kOscCoarse, c.oscCoarse);
    set(Params::kOscFine, c.oscFine);
    set(Params::kOscPitch, c.oscPitch);
    set(Params::kPitchKF, c.pitchKF);
    set(Params::kRandomPitch, c.randomPitch);
    set(Params::kPatchCoarse, c.patchCoarse);
    set(Params::kPatchFine, c.patchFine);
    set(Params::kBendUp, c.bendUp);
    set(Params::kBendDown, c.bendDown);
    set(Params::kScaleTune, c.scaleTune ? 1 : 0);
    for (int i = 0; i < 12; ++i) set(Params::kScaleTunes[i], c.scale[i]);
    set(Params::kClockSource, c.clockSource);
    set(Params::kTempoSync, c.tempoSync ? 1 : 0);
    
    set(Params::kTripSw, c.timeTrip ? 1 : 0);
    set(Params::kBeatKeep, c.beatKeep ? 1 : 0);
    set(Params::kLoop, c.loop ? 1 : 0);
    set(Params::kLoopMode, c.loopMode);
    set(Params::kPlaybackMode, c.playbackMode);
    
    
    set(Params::kPorta, c.portamento ? 1 : 0);
    set(Params::kPortaMode, c.portaMode);
    set(Params::kPortaType, c.portaType);
    set(Params::kPortaTime, c.portaTime);
    set(Params::kPortaVeloSens, c.portaVeloSens);
    
    set(Params::kLegato, c.legato ? 1 : 0);
    set(Params::kOctave, c.octave);
    set(Params::kModSwitch, c.mod.on ? 1 : 0);
    set(Params::kModType, c.mod.type);
    set(Params::kModLevel, c.mod.level);
    set(Params::kModAttack, c.mod.attack);
    set(Params::kModRelease, c.mod.release);
    set(Params::kStructure, c.structure);
}

void VSynthEmuProcessor::writeToneControls(const XtSynth::Controls& c) {
    set(Params::kEnergy, c.energy);
    set(Params::kRootKey, c.rootKey);
    set(Params::kTvaSwitch, c.tvaOn ? 1 : 0);
    set(Params::kTvaChoSend, c.chorusSend);
    set(Params::kTvaRevSend, c.reverbSend);
    set(Params::kOutputAssign, c.outputAssign);
    
    set(Params::kTvaLevel, c.tvaLevel);
    set(Params::kTvaLevelKF, c.tvaLevelKF);
    set(Params::kTvaLevelLfo, c.tvaLevelLfo);
    set(Params::kTvaPan, c.tvaPan);
    set(Params::kTvaPanKF, c.tvaPanKF);
    set(Params::kTvaPanLfo, c.tvaPanLfo);
    set(Params::kMfxSend, c.mfxSend);
    set(Params::kTvaLfoWave, c.tvaLfoWave);
    set(Params::kTvaLfoRate, c.tvaLfoRate);
    set(Params::kTvaLfoOffset, c.tvaLfoOffset);
    set(Params::kTvaLfoDelay, c.tvaLfoDelay);
    set(Params::kTvaLfoMode, c.tvaLfoFadeMode);
    set(Params::kTvaLfoFade, c.tvaLfoFade);
    set(Params::kTvaLfoSync, c.tvaLfoKeySync ? 1 : 0);
    for (int m = 0; m < 8; ++m) {
        const XtMatrixControl& x = c.matrix[m];
        set(matrixParamId(m, "Source"), x.source);
        set(matrixParamId(m, "Dest1"), x.dest[0]); set(matrixParamId(m, "Sens1"), x.sens[0]);
        set(matrixParamId(m, "Dest2"), x.dest[1]); set(matrixParamId(m, "Sens2"), x.sens[1]);
    }
    for (int i = 0; i < kAssigns; ++i) set(assignParamId(i), c.assign[i]);
    set(Params::kAttack, c.attack);
    set(Params::kDecay, c.decay);
    set(Params::kSustain, c.sustain);
    set(Params::kRelease, c.release);
    set(Params::kAttackSens, c.aSens);
    set(Params::kDecaySens, c.dSens);
    set(Params::kReleaseSens, c.rSens);
    
    set(Params::kPAttack, c.pAttack);
    set(Params::kPDecay, c.pDecay);
    set(Params::kPSustain, c.pSustain);
    set(Params::kPRelease, c.pRelease);
    set(Params::kPDepth, c.pDepth);
    set(Params::kTAttack, c.tAttack);
    set(Params::kTDecay, c.tDecay);
    set(Params::kTSustain, c.tSustain);
    set(Params::kTRelease, c.tRelease);
    set(Params::kTDepth, c.tDepth);
    set(Params::kFAttack, c.fAttack);
    set(Params::kFDecay, c.fDecay);
    set(Params::kFSustain, c.fSustain);
    set(Params::kFRelease, c.fRelease);
    set(Params::kFDepth, c.fDepth);
    set(Params::kLfoTimeDepth, c.lfoTimeDepth);
    set(Params::kLfoFmtDepth, c.lfoFormantDepth);
    set(Params::kOscLevel, c.oscLevel);
    set(Params::kOscLevelKF, c.oscLevelKF);
    set(Params::kOscLevelLfo, c.oscLevelLfoDepth);
    set(Params::kWaveGain, c.waveGain);
    set(Params::kOAttack, c.oAttack);
    set(Params::kODecay, c.oDecay);
    set(Params::kOSustain, c.oSustain);
    set(Params::kORelease, c.oRelease);
    set(Params::kOASens, c.oASens);
    set(Params::kODSens, c.oDSens);
    set(Params::kORSens, c.oRSens);
    set(Params::kOTimeKF, c.oTimeKF);
    set(Params::kOscVelSens, c.oscVelSens);
    set(Params::kOscVelCurve, c.oscVelCurve);
    set(Params::kAmpVelSens, c.ampVelSens);
    set(Params::kAmpVelCurve, c.ampVelCurve);
    set(Params::kRobot, c.robot ? 1 : 0);
    set(Params::kVari, c.vari ? 1 : 0);
    set(Params::kLfoWave, c.lfoWave);
    set(Params::kLfoRate, c.lfoRate);
    set(Params::kLfoDepth, c.lfoPitchDepth);
    set(Params::kLfoDelay, c.lfoDelay);
    set(Params::kLfoFadeMode, c.lfoFadeMode);
    set(Params::kLfoFade, c.lfoFade);
    set(Params::kLfoOffset, c.lfoOffset);
    set(Params::kLfoKeySync, c.lfoKeySync ? 1 : 0);
    set(Params::kOscSwitch, c.on ? 1 : 0);
}

// The other way round from xtControls: each parameter to what these controls hold, as a host's automation would, and only those that have moved.
void VSynthEmuProcessor::setControls(const XtSynth::Controls& c) {
    // The parts (PluginControlsWrite.cpp, PluginCosmControls.cpp, PluginOsc2Controls.cpp, v190)
    writeOscControls(c);
    writeCosmControls(c);
    writeToneControls(c);
    writeOsc2Controls(c);
    writeArpControls(c);    // PluginArp.cpp, v190
}
