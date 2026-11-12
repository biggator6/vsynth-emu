// The tone's other parameters: the patch TVA, the effects, LFO 5, the Matrix and the assigns, the envelopes, the LFO and
// the OSC TVA; then MOD, the Structure and the OSCs' envelopes' velocity.  Moved as they stood from createParameterLayout
// (v190), which calls them in its order.
#include "PluginProcessor.h"
#include "PluginParamIds.h"

void VSynthEmuProcessor::addToneParams(ParamList& params, const IntText& noteValue, const IntText& curveName) {
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kTvaSwitch, "TVA Switch", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaLevel, "TVA Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kTvaLevelKF, "TVA Level KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaLevelLfo, "TVA Level LFO Depth", -63, 63, 0));
    // shown as the XT does: L64 … 0 … 63R
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaPan, "TVA Pan", 0, 127, 64, juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([](int v, int) { return v == 64 ? juce::String("0") : v < 64 ? "L" + juce::String(64 - v) : juce::String(v - 64) + "R"; })
        .withValueFromStringFunction([](const juce::String& s) {
            const juce::String t = s.trim();
            return t.startsWithIgnoreCase("L") ? 64 - t.getTrailingIntValue() : t.getIntValue() + 64; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kTvaPanKF, "TVA Pan KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaPanLfo, "TVA Pan LFO Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kMfxSend, "MFX Send Level", 0, 127, 127));
    // The effect chip in the path.  Off by default: it hasn't been compared with a real XT yet, and it takes a share of a core
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kFxOn, "Effects", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaChoSend, "Chorus Send Level", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaRevSend, "Reverb Send Level", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kOutputAssign, "Output Assign", juce::StringArray{ "MULTI", "MAIN", "DIR" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kOutputMode, "Output Mode", juce::StringArray{ "Mix", "Parallel" }, 1));
    // The chip's blocks.  Defaults as the firmware's descriptors have them.
    params.push_back(std::make_unique<juce::AudioParameterBool>("fxMfxOn", "MFX Switch", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxMfxType", "MFX Type", 0, 41, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxMfxMaster", "MFX Master Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxMfxCho", "MFX Chorus Send Level", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxMfxRev", "MFX Reverb Send Level", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("fxChoOn", "Chorus Switch", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxChoType", "Chorus Type", 0, 8, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxChoMaster", "Chorus Master Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxChoRev", "Chorus Reverb Send Level", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("fxRevOn", "Reverb Switch", false));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxRevType", "Reverb Type", 0, 10, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxRevMaster", "Reverb Master Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxMaster", "Master Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>("fxGain", "Output Gain", -12, 12, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kTvaLfoWave, "LFO 5 Waveform",
        juce::StringArray { "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaLfoRate, "LFO 5 Rate", 0, 144, 98, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kTvaLfoOffset, "LFO 5 Offset",
        juce::StringArray { "-100", "-50", "0", "+50", "+100" }, 2));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaLfoDelay, "LFO 5 Delay", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kTvaLfoMode, "LFO 5 Fade Mode",
        juce::StringArray { "ON <<", "ON >>", "OFF <<", "OFF >>" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTvaLfoFade, "LFO 5 Fade", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kTvaLfoSync, "LFO 5 Key Sync", false));
    // The Matrix Controls (IDs 0x14d…0x174), all off as on the XT, and the system's controller assigns
    for (int m = 0; m < 8; ++m) {
        const juce::String n = "Matrix " + juce::String(m + 1) + " ";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(matrixParamId(m, "Source"), n + "Source", matrixSourceNames(), 0));
        for (int j = 1; j <= 2; ++j) {
            const juce::String sj(j);
            params.push_back(std::make_unique<juce::AudioParameterChoice>(matrixParamId(m, ("Dest" + sj).toRawUTF8()), n + "Dest " + sj, matrixDestNames(), 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(matrixParamId(m, ("Sens" + sj).toRawUTF8()), n + "Sens " + sj, -63, 63, 0));
        }
    }
    for (int i = 0; i < kAssigns; ++i)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(assignParamId(i), juce::String(assignName(i)) + " Assign", assignValueNames(), assignDefault(i)));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAttack, "Attack", 0, 144, 0, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kDecay, "Decay", 0, 144, 0, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kSustain, "Sustain", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kRelease, "Release", 0, 144, 16, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAttackSens, "Attack Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kDecaySens, "Decay Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kReleaseSens, "Release Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kTimeKF, "Env Time KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPAttack, "Pitch Env Attack", 0, 144, 0, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPDecay, "Pitch Env Decay", 0, 144, 64, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPSustain, "Pitch Env Sustain", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPRelease, "Pitch Env Release", 0, 144, 64, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPDepth, "Pitch Env Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTAttack, "Time Env Attack", 0, 144, 0, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTDecay, "Time Env Decay", 0, 144, 64, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTSustain, "Time Env Sustain", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTRelease, "Time Env Release", 0, 144, 64, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTDepth, "Time Env Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFAttack, "Formant Env Attack", 0, 144, 0, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFDecay, "Formant Env Decay", 0, 144, 64, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFSustain, "Formant Env Sustain", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFRelease, "Formant Env Release", 0, 144, 64, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFDepth, "Formant Env Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kLfoWave, "LFO Waveform",
        juce::StringArray { "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoRate, "LFO Rate", 0, 144, 64, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoDepth, "LFO Pitch Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoTimeDepth, "LFO Time Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoFmtDepth, "LFO Formant Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscLevel, "OSC Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kOscLevelKF, "OSC Level KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscLevelLfo, "OSC Level LFO Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kWaveGain, "Wave Gain", -12, 12, 0,
        juce::AudioParameterIntAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOAttack, "OSC Env Attack", 0, 144, 0, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kODecay, "OSC Env Decay", 0, 144, 0, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOSustain, "OSC Env Sustain", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kORelease, "OSC Env Release", 0, 144, 127, noteValue));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOASens, "OSC Env Attack Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kODSens, "OSC Env Decay Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kORSens, "OSC Env Release Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kOTimeKF, "OSC Env Time KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscVelSens, "OSC Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscVelCurve, "OSC Velocity Curve", 0, 7, 1, curveName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAmpVelSens, "Velocity Sens", -63, 63, 16));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAmpVelCurve, "Velocity Curve", 0, 7, 1, curveName));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoDelay, "LFO Delay", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kLfoFadeMode, "LFO Fade Mode",
        juce::StringArray { "ON <<", "ON >>", "OFF <<", "OFF >>" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoFade, "LFO Fade", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kLfoOffset, "LFO Offset",
        juce::StringArray { "-100", "-50", "0", "+50", "+100" }, 2));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kLfoKeySync, "LFO Key Sync", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kOscSwitch, "OSC 1 Switch", true));
}

void VSynthEmuProcessor::addModParams(ParamList& params) {
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kModSwitch, "MOD Switch", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kModType, "MOD Type",
        juce::StringArray { "MIX", "RING", "FM", "ENV RING", "OSC SYNC" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kModLevel, "MOD Original Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kModAttack, "MOD Attack Time", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kModRelease, "MOD Release Time", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kStructure, "Structure Type",
        juce::StringArray { "Type1", "Type2", "Type3" }, 0));
    // Each OSC's pitch, time and formant envelopes' velocity and key.  A host's and a patch's: no control on the editor yet (UI_DESIGN.md)
    for (int k = 0; k < 2; ++k)
        for (int e = 0; e < 3; ++e) {
            static const char* const kEnvs[3] = { "pitchEnv", "timeEnv", "formantEnv" };
            static const char* const kNames[3] = { "Pitch Env ", "Time Env ", "Formant Env " };
            const juce::String id = kEnvs[e], name = juce::String(k ? "OSC 2 " : "") + kNames[e];
            const auto intP = [&](const char* sfx, const char* label, int lo, int hi, int def) { params.push_back(std::make_unique<juce::AudioParameterInt>(Params::oscId(k, id + sfx), name + label, lo, hi, def)); };
            intP("VelCurve", "Velocity Curve", 0, 7, 0);
            intP("VelSens", "Velocity Sens", -63, 63, 32);
            intP("AttackSens", "Attack Velocity Sens", -63, 63, 0);
            intP("DecaySens", "Decay Velocity Sens", -63, 63, 0);
            intP("ReleaseSens", "Release Velocity Sens", -63, 63, 0);
            params.push_back(std::make_unique<juce::AudioParameterFloat>(Params::oscId(k, id + "TimeKF"), name + "Time KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
        }
}
