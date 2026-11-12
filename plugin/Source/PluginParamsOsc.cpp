// OSC 1's and OSC 2's parameters: the tempo, the VariPhrase and analog knobs, the pitch, the loop and the keys; OSC 2's
// own copies of OSC 1's.  Moved as they stood from createParameterLayout (v190), which calls them in its order.
#include "PluginProcessor.h"
#include "PluginParamIds.h"

void VSynthEmuProcessor::addOscParams(ParamList& params, const juce::AudioParameterFloatAttributes& twoPlaces) {
    params.push_back(std::make_unique<juce::AudioParameterFloat>(Params::kPatchTempo, "Patch Tempo", juce::NormalisableRange<float>(20.0f, 250.0f, 0.1f), 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kClockSource, "Clock Source", juce::StringArray { "INTERNAL", "HOST" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTripPad, "Time Trip", 0, 127, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v); })));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kTempoSync, "Tempo Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(Params::kOrigTempo, "Original Tempo", juce::NormalisableRange<float>(20.0f, 300.0f, 0.0001f), 120.0f, twoPlaces));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kTripSw, "Time Trip Switch", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kBeatKeep, "Beat Keep", true));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kPitchShift, "Pitch Shift",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f, "st"
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kTimeStretch, "Time Stretch",
        juce::NormalisableRange<float>(0.25f, 4.0f, 0.001f, 0.4f),
        1.0f, "x"
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kFormantShift, "Formant Shift",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
        0.0f, "st"
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        Params::kRobot, "Robot Mode", false
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        Params::kEncodeType, "Encode Type",
        juce::StringArray { "Auto", "SOLO", "BACKING", "ENSEMBLE", "LITE" }, 0
    ));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        Params::kEncodeDepth, "Encode Depth", 0, 127, 64
    ));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        Params::kRootKey, "Original Key", 0, 127, 60
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kVari, "Vari Sw", true));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        Params::kEnergy, "Energy", 0, 127, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction(
            [](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v); })
    ));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscTime, "Time", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kOscTimeOffset, "Time Offset",
        juce::StringArray { "BWD", "ZERO", "FWD" }, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kOscTimeKF, "Time KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscFormant, "Formant", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kOscFormantKF, "Formant KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kOscType, "OSC Type", juce::StringArray { "ANALOG", "PCM" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kAnaWave, "Analog Waveform",
        juce::StringArray { "SAW", "SQUARE", "TRIANGLE", "SINE", "RAMP", "JUNO", "HQ-SAW", "HQ-SQUARE", "NOISE", "LA-SAW", "LA-SQUARE",
                            "SUPER-SAW", "FEEDBACK-OSC", "X-MOD" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaImpact, "Impact", 0, 40, 10,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return juce::String(v / 10.0, 1); })));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaPw, "Pulse Width", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kAnaPwKF, "PW KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaPwLfo, "PW LFO Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaPwEnv, "PW Env Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaFat, "Fat", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kAnaFatKF, "Fat KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaFatLfo, "Fat LFO Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaFatEnv, "Fat Env Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaDetune, "Detune", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaSubOct, "Sub Octave", 0, 3, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v - 3); })
            .withValueFromStringFunction([](const juce::String& s) { return s.trim().equalsIgnoreCase("OFF") ? 0 : s.getIntValue() + 3; })));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAnaSubLevel, "Sub Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscCoarse, "Coarse", -48, 48, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscFine, "Fine", -50, 50, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscPitch, "OSC Pitch", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kPitchKF, "Pitch KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 100.0f));
    {
        juce::StringArray depths;
        for (int i = 0; i <= 30; ++i) depths.add(juce::String(xtRandomPitchDepth(i) / 10));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kRandomPitch, "Random Pitch Depth", depths, 0));
    }
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPatchCoarse, "Patch Coarse Tune", -48, 48, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPatchFine, "Patch Fine Tune", -50, 50, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kBendUp, "Bend Range Up", 0, 48, 2));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kBendDown, "Bend Range Down", 0, 48, 2));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kScaleTune, "Scale Tune Switch", false));
    {
        static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
            params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kScaleTunes[i], juce::String("Scale Tune ") + names[i], -100, 100, 0));
    }
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kLoop, "Loop", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kLoopMode, "Loop Mode",
        juce::StringArray { "One Shot", "One Way", "Alternate" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kLoopStart, "Loop Start", juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f, twoPlaces));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kLoopEnd, "Loop End", juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f, twoPlaces));
    // Samples, as a patch stores it; the firmware takes the wave's last sample for anything past it.  2^24 − 1 holds the longest
    // wave the XT's memory takes and stays exact in a float
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kStartOffset, "Start Offset", 0, kStartOffsetMax, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kPlaybackMode, "Playback Mode",
        juce::StringArray { "RETRIGGER", "LEGATO", "STEP", "EVENT" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kPorta, "Portamento", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kPortaMode, "Portamento Mode",
        juce::StringArray { "NORMAL", "LEGATO" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kPortaType, "Portamento Type",
        juce::StringArray { "RATE", "TIME" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPortaTime, "Portamento Time", 0, 127, 20));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPortaVeloSens, "Portamento Time Velo Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kMonoPoly, "Mono/Poly",
        juce::StringArray { "MONO", "POLY" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kLegato, "Legato Switch", true));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOctave, "Octave Shift", -3, 3, 0));
}

void VSynthEmuProcessor::addOsc2Params(ParamList& params, const IntText& noteValue,
                                       const juce::AudioParameterFloatAttributes& twoPlaces, const IntText& curveName) {
    {   // OSC 2, the ranges and defaults of OSC 1's
        using Params::oscId;
        const juce::String n = "OSC 2 ";
        auto intP = [&](const juce::String& id, const char* name, int lo, int hi, int def) {
            params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, id), n + name, lo, hi, def));
        };
        auto kfP = [&](const juce::String& id, const char* name, float def) {
            params.push_back(std::make_unique<juce::AudioParameterFloat>(oscId(1, id), n + name, juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), def));
        };
        auto noteP = [&](const juce::String& id, const char* name, int def) {
            params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, id), n + name, 0, 144, def, noteValue));
        };
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kOscSwitch), n + "Switch", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kTempoSync), n + "Tempo Sync", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(oscId(1, Params::kOrigTempo), n + "Original Tempo", juce::NormalisableRange<float>(20.0f, 300.0f, 0.0001f), 120.0f, twoPlaces));
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kTripSw), n + "Time Trip Switch", true));
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kBeatKeep), n + "Beat Keep", true));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kOscType), n + "Type", juce::StringArray { "ANALOG", "PCM" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kAnaWave), n + "Analog Waveform",
            juce::StringArray { "SAW", "SQUARE", "TRIANGLE", "SINE", "RAMP", "JUNO", "HQ-SAW", "HQ-SQUARE", "NOISE", "LA-SAW", "LA-SQUARE",
                                "SUPER-SAW", "FEEDBACK-OSC", "X-MOD" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, Params::kAnaImpact), n + "Impact", 0, 40, 10,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return juce::String(v / 10.0, 1); })));
        intP(Params::kAnaPw, "Pulse Width", -63, 63, 0);
        kfP(Params::kAnaPwKF, "PW KF", 0.0f);
        intP(Params::kAnaPwLfo, "PW LFO Depth", -63, 63, 0);
        intP(Params::kAnaPwEnv, "PW Env Depth", -63, 63, 0);
        intP(Params::kAnaFat, "Fat", 0, 127, 0);
        kfP(Params::kAnaFatKF, "Fat KF", 0.0f);
        intP(Params::kAnaFatLfo, "Fat LFO Depth", -63, 63, 0);
        intP(Params::kAnaFatEnv, "Fat Env Depth", -63, 63, 0);
        intP(Params::kAnaDetune, "Detune", -63, 63, 0);
        params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, Params::kAnaSubOct), n + "Sub Octave", 0, 3, 0,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v - 3); })
                .withValueFromStringFunction([](const juce::String& t) { return t.trim().equalsIgnoreCase("OFF") ? 0 : t.getIntValue() + 3; })));
        intP(Params::kAnaSubLevel, "Sub Level", 0, 127, 127);
        intP(Params::kOscCoarse, "Coarse", -48, 48, 0);
        intP(Params::kOscFine, "Fine", -50, 50, 0);
        intP(Params::kOscPitch, "Pitch", -63, 63, 0);
        kfP(Params::kPitchKF, "Pitch KF", 100.0f);
        {
            juce::StringArray depths;
            for (int i = 0; i <= 30; ++i) depths.add(juce::String(xtRandomPitchDepth(i) / 10));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kRandomPitch), n + "Random Pitch Depth", depths, 0));
        }
        noteP(Params::kPAttack, "Pitch Env Attack", 0);
        noteP(Params::kPDecay, "Pitch Env Decay", 64);
        intP(Params::kPSustain, "Pitch Env Sustain", 0, 127, 0);
        noteP(Params::kPRelease, "Pitch Env Release", 64);
        intP(Params::kPDepth, "Pitch Env Depth", -63, 63, 0);
        noteP(Params::kTAttack, "Time Env Attack", 0);
        noteP(Params::kTDecay, "Time Env Decay", 64);
        intP(Params::kTSustain, "Time Env Sustain", 0, 127, 0);
        noteP(Params::kTRelease, "Time Env Release", 64);
        intP(Params::kTDepth, "Time Env Depth", -63, 63, 0);
        noteP(Params::kFAttack, "Formant Env Attack", 0);
        noteP(Params::kFDecay, "Formant Env Decay", 64);
        intP(Params::kFSustain, "Formant Env Sustain", 0, 127, 0);
        noteP(Params::kFRelease, "Formant Env Release", 64);
        intP(Params::kFDepth, "Formant Env Depth", -63, 63, 0);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kLfoWave), n + "LFO Waveform",
            juce::StringArray { "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 0));
        noteP(Params::kLfoRate, "LFO Rate", 64);
        intP(Params::kLfoDepth, "LFO Pitch Depth", -63, 63, 0);
        intP(Params::kLfoTimeDepth, "LFO Time Depth", -63, 63, 0);
        intP(Params::kLfoFmtDepth, "LFO Formant Depth", -63, 63, 0);
        intP(Params::kLfoDelay, "LFO Delay", 0, 127, 0);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kLfoFadeMode), n + "LFO Fade Mode",
            juce::StringArray { "ON <<", "ON >>", "OFF <<", "OFF >>" }, 0));
        intP(Params::kLfoFade, "LFO Fade", 0, 127, 0);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kLfoOffset), n + "LFO Offset",
            juce::StringArray { "-100", "-50", "0", "+50", "+100" }, 2));
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kLfoKeySync), n + "LFO Key Sync", true));
        intP(Params::kOscLevel, "Level", 0, 127, 127);
        kfP(Params::kOscLevelKF, "Level KF", 0.0f);
        intP(Params::kOscLevelLfo, "Level LFO Depth", -63, 63, 0);
        params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, Params::kWaveGain), n + "Wave Gain", -12, 12, 0,
            juce::AudioParameterIntAttributes().withLabel("dB")));
        noteP(Params::kOAttack, "Env Attack", 0);
        noteP(Params::kODecay, "Env Decay", 0);
        intP(Params::kOSustain, "Env Sustain", 0, 127, 127);
        noteP(Params::kORelease, "Env Release", 127);
        intP(Params::kOASens, "Env Attack Velocity Sens", -63, 63, 0);
        intP(Params::kODSens, "Env Decay Velocity Sens", -63, 63, 0);
        intP(Params::kORSens, "Env Release Velocity Sens", -63, 63, 0);
        kfP(Params::kOTimeKF, "Env Time KF", 0.0f);
        intP(Params::kOscVelSens, "Velocity Sens", -63, 63, 0);
        params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, Params::kOscVelCurve), n + "Velocity Curve", 0, 7, 1, curveName));
        // its wave and the VariPhrase knobs
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kEncodeType), n + "Encode Type",
            juce::StringArray { "Auto", "SOLO", "BACKING", "ENSEMBLE", "LITE" }, 0));
        intP(Params::kEncodeDepth, "Encode Depth", 0, 127, 64);
        intP(Params::kRootKey, "Original Key", 0, 127, 60);
        params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, Params::kEnergy), n + "Energy", 0, 127, 0,
            juce::AudioParameterIntAttributes().withStringFromValueFunction(
                [](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v); })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kRobot), n + "Robot Voice", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kVari), n + "Vari Sw", true));
        intP(Params::kOscTime, "Time", -63, 63, 0);
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kOscTimeOffset), n + "Time Offset",
            juce::StringArray { "BWD", "ZERO", "FWD" }, 2));
        kfP(Params::kOscTimeKF, "Time KF", 0.0f);
        intP(Params::kOscFormant, "Formant", -63, 63, 0);
        kfP(Params::kOscFormantKF, "Formant KF", 0.0f);
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kLoop), n + "Loop", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kLoopMode), n + "Loop Mode",
            juce::StringArray { "One Shot", "One Way", "Alternate" }, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(oscId(1, Params::kLoopStart), n + "Loop Start",
            juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f, twoPlaces));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(oscId(1, Params::kLoopEnd), n + "Loop End",
            juce::NormalisableRange<float>(0.0f, 100.0f), 100.0f, twoPlaces));
        params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, Params::kStartOffset), n + "Start Offset", 0, kStartOffsetMax, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(oscId(1, Params::kPlaybackMode), n + "Playback Mode",
            juce::StringArray { "RETRIGGER", "LEGATO", "STEP", "EVENT" }, 0));
    }
}
