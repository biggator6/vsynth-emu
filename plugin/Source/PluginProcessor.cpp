#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─── Parameter IDs ───────────────────────────────────────────────────────────
namespace Params {
    static const juce::String kPitchShift   = "pitchShift";
    static const juce::String kTimeStretch  = "timeStretch";
    static const juce::String kFormantShift = "formantShift";
    static const juce::String kRobot        = "robot";         // the engine's robot mode; with the XT, Robot Voice
    static const juce::String kEncodeType   = "encodeType";    // Auto, SOLO, BACKING, ENSEMBLE
    static const juce::String kEncodeDepth  = "encodeDepth";   // multi-band attack detection
    static const juce::String kRootKey      = "rootKey";       // note that plays the sample unshifted
    static const juce::String kEnergy       = "energy";        // SOLO Energy, OFF/1..127 (g = (k/127)²)
    // The VariPhrase OSC (XtOsc): Time and Formant −63..+63 (the XT path's
    // TIME and FORMANT knobs), Time Offset, Time and Formant KF
    static const juce::String kOscTime       = "oscTime";
    static const juce::String kOscTimeOffset = "oscTimeOffset";   // BWD, ZERO, FWD
    static const juce::String kOscTimeKF     = "oscTimeKF";       // −200..+200 % per octave
    static const juce::String kOscFormant    = "oscFormant";
    static const juce::String kOscFormantKF  = "oscFormantKF";
    // The OSC's pitch (XtOsc): Coarse and Fine, the Pitch knob −63..+63
    // (±6000 cents at 63), Pitch KF and Random Pitch Depth
    static const juce::String kOscCoarse     = "oscCoarse";       // −48..+48 semitones
    static const juce::String kOscFine       = "oscFine";         // −50..+50 cents
    static const juce::String kOscPitch      = "oscPitch";
    static const juce::String kPitchKF       = "pitchKF";         // −200..+200 % per octave
    static const juce::String kRandomPitch   = "randomPitch";     // 0..1200 cents in the XT's 31 steps
    // The patch's tuning: Coarse and Fine Tune (Pitch Shift adds to them),
    // Bend Range Up/Down, Scale Tune (switch, C…B)
    static const juce::String kPatchCoarse   = "patchCoarse";     // −48..+48 semitones
    static const juce::String kPatchFine     = "patchFine";       // −50..+50 cents
    static const juce::String kBendUp        = "bendRangeUp";     // 0..48 semitones
    static const juce::String kBendDown      = "bendRangeDown";
    static const juce::String kScaleTune     = "scaleTune";
    static const juce::String kScaleTunes[12] = { "scaleTuneC", "scaleTuneCs", "scaleTuneD", "scaleTuneDs", "scaleTuneE", "scaleTuneF",
                                                  "scaleTuneFs", "scaleTuneG", "scaleTuneGs", "scaleTuneA", "scaleTuneAs", "scaleTuneB" };
    // Loop: the OSC's Loop switch, the wave's Loop Mode (One
    // Shot, One Way, Alternate) and its loop region in % of the wave
    static const juce::String kLoop          = "loop";
    static const juce::String kLoopMode      = "loopMode";
    static const juce::String kLoopStart     = "loopStart";
    static const juce::String kLoopEnd       = "loopEnd";
    // Portamento: switch, Mode (NORMAL, LEGATO), Type (RATE, TIME), Time
    // 0..127, Time Velo Sens −63..+63
    static const juce::String kPorta         = "portamento";
    static const juce::String kPortaMode     = "portaMode";
    static const juce::String kPortaType     = "portaType";
    static const juce::String kPortaTime     = "portaTime";
    static const juce::String kPortaVeloSens = "portaVeloSens";
    // Key assign: Mono/Poly (MONO, POLY), Legato Switch, Octave Shift −3..+3
    static const juce::String kMonoPoly      = "monoPoly";
    static const juce::String kLegato        = "legatoSwitch";
    static const juce::String kOctave        = "octaveShift";
    // The COSM sections as TVFs, COSM 1 ("cosm…") and COSM 2 ("cosm2…"):
    // Switch, Type (TVF, Dynamic TVF, Dual Filter, TB Filter, Comb Filter), ResoOwn/CutoffOwn (the Dynamic TVF's
    // Own Env depths), Filter, Cutoff, Reso, CutoffKF, CutoffEnv/Lfo, ResoEnv/Lfo; the
    // Dual Filter's DualType and Dual1…/Dual2… Freq, KF, Q, Lfo, Env; the TB
    // Filter's TbCutoff, TbKF, TbLfo, TbEnv, TbQ, TbQFollow, TbHpf, TbHpfKF,
    // TbHpfLfo, TbHpfEnv, TbGain; the Comb Filter's CombOctave, CombDetune,
    // CombDetuneLfo, CombDetuneEnv, CombTone, CombToneLfo, CombToneEnv; the
    // envelopes EnvA (the cutoff's) and EnvB (the resonance's), each
    // Attack…TimeKF; the LFO, LfoWave…LfoKeySync
    static juce::String cosmId(int k, const char* name) { return juce::String(k ? "cosm2" : "cosm") + name; }
    static const char* const kCosmEnvKnobs[10] = { "Attack", "Decay", "Sustain", "Release", "VelCurve", "VelSens",
                                                   "AttackSens", "DecaySens", "ReleaseSens", "TimeKF" };
    // Amp envelope, the XT's 0..127 knobs (XtEnvelope: times from the firmware's table)
    static const juce::String kAttack       = "ampAttack";
    static const juce::String kDecay        = "ampDecay";
    static const juce::String kSustain      = "ampSustain";
    static const juce::String kRelease      = "ampRelease";
    static const juce::String kAttackSens   = "ampAttackSens";   // velocity → time, −63..+63
    static const juce::String kDecaySens    = "ampDecaySens";
    static const juce::String kReleaseSens  = "ampReleaseSens";  // note-off velocity
    static const juce::String kTimeKF       = "ampTimeKF";       // −200..+200, decay and release
    // Pitch envelope: 0..127 knobs, depth −63..+63 (±6000 cents at 63)
    static const juce::String kPAttack      = "pitchEnvAttack";
    static const juce::String kPDecay       = "pitchEnvDecay";
    static const juce::String kPSustain     = "pitchEnvSustain";
    static const juce::String kPRelease     = "pitchEnvRelease";
    static const juce::String kPDepth       = "pitchEnvDepth";
    // Time and formant envelopes, the same knobs; time depth ±3 at ±60 and
    // ±5 at ±63 (walk speed), formant depth d/63 octaves
    static const juce::String kTAttack      = "timeEnvAttack";
    static const juce::String kTDecay       = "timeEnvDecay";
    static const juce::String kTSustain     = "timeEnvSustain";
    static const juce::String kTRelease     = "timeEnvRelease";
    static const juce::String kTDepth       = "timeEnvDepth";
    static const juce::String kFAttack      = "formantEnvAttack";
    static const juce::String kFDecay       = "formantEnvDecay";
    static const juce::String kFSustain     = "formantEnvSustain";
    static const juce::String kFRelease     = "formantEnvRelease";
    static const juce::String kFDepth       = "formantEnvDepth";
    // LFO (XtLfo) and its pitch depth
    static const juce::String kLfoWave      = "lfoWave";
    static const juce::String kLfoRate      = "lfoRate";
    static const juce::String kLfoDepth     = "lfoPitchDepth";
    static const juce::String kLfoTimeDepth = "lfoTimeDepth";
    static const juce::String kLfoFmtDepth  = "lfoFormantDepth";
    // The OSC TVA (XtOsc): level, key follow, tremolo depth, wave gain; its
    // amp envelope (in series with the patch TVA); velocity to level
    static const juce::String kOscLevel     = "oscLevel";
    static const juce::String kOscLevelKF   = "oscLevelKF";
    static const juce::String kOscLevelLfo  = "oscLevelLfoDepth";
    static const juce::String kWaveGain     = "waveGain";          // −12..+12 dB
    static const juce::String kOAttack      = "oscEnvAttack";
    static const juce::String kODecay       = "oscEnvDecay";
    static const juce::String kOSustain     = "oscEnvSustain";
    static const juce::String kORelease     = "oscEnvRelease";
    static const juce::String kOASens       = "oscEnvAttackSens";
    static const juce::String kODSens       = "oscEnvDecaySens";
    static const juce::String kORSens       = "oscEnvReleaseSens";
    static const juce::String kOTimeKF      = "oscEnvTimeKF";
    static const juce::String kOscVelSens   = "oscVelSens";        // −63..+63
    static const juce::String kOscVelCurve  = "oscVelCurve";       // OFF, 1..7
    static const juce::String kAmpVelSens   = "ampVelSens";        // the patch TVA
    static const juce::String kAmpVelCurve  = "ampVelCurve";
    static const juce::String kLfoDelay     = "lfoDelay";
    static const juce::String kLfoFadeMode  = "lfoFadeMode";
    static const juce::String kLfoFade      = "lfoFade";
    static const juce::String kLfoOffset    = "lfoOffset";
    static const juce::String kLfoKeySync   = "lfoKeySync";
}

// ─── Parameter Layout ─────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
VSynthEmuProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

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
        juce::StringArray { "Auto", "SOLO", "BACKING", "ENSEMBLE" }, 0
    ));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        Params::kEncodeDepth, "Encode Depth", 0, 127, 64
    ));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        Params::kRootKey, "Original Key", 0, 127, 60
    ));

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
            params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kScaleTunes[i], juce::String("Scale Tune ") + names[i], -64, 63, 0));
    }
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kLoop, "Loop", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kLoopMode, "Loop Mode",
        juce::StringArray { "One Shot", "One Way", "Alternate" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kLoopStart, "Loop Start", juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f, "%"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kLoopEnd, "Loop End", juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 100.0f, "%"));
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
    for (int k = 0; k < 2; ++k) {
        using Params::cosmId;
        const juce::String n = k ? "COSM 2 " : "COSM 1 ";
        params.push_back(std::make_unique<juce::AudioParameterBool>(cosmId(k, "Switch"), n + "Switch", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "Type"), n + "Type",
            juce::StringArray { "TVF", "Dynamic TVF", "Dual Filter", "TB Filter", "Comb Filter" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResoOwn"), n + "Resonance Own Env Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CutoffOwn"), n + "Cutoff Own Env Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "Filter"), n + "Filter Type",
            juce::StringArray { "LP24", "BP24", "HP24", "NOTCH24", "PEAK24", "LP12", "BP12", "HP12", "NOTCH12", "PEAK12", "LP6", "HP6" }, 5));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "Cutoff"), n + "Cutoff", 0, 127, 127));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "Reso"), n + "Resonance", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            cosmId(k, "CutoffKF"), n + "Cutoff Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CutoffEnv"), n + "Cutoff Env Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CutoffLfo"), n + "Cutoff LFO Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResoEnv"), n + "Resonance Env Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResoLfo"), n + "Resonance LFO Depth", -63, 63, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "DualType"), n + "Dual Filter Type",
            juce::StringArray { "LPF/HPF", "LPF->HPF", "BPF/BPF" }, 0));
        for (int f = 1; f <= 2; ++f) {               // the XT's defaults: Frequency 90 and 80, Keyfollow +100%, Q 108 and 120
            const juce::String id = cosmId(k, "Dual") + juce::String(f), name = n + "Filter " + juce::String(f) + " ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Freq", name + "Frequency", 0, 127, f == 1 ? 90 : 80));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                id + "KF", name + "Frequency Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 100.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Q", name + "Q", 0, 127, f == 1 ? 108 : 120));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Lfo", name + "LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Env", name + "Env Depth", -63, 63, 0));
        }
        {                                            // the TB Filter, the XT's defaults
            const juce::String tn = n + "TB ";
            auto kfParam = [&](const char* id, const juce::String& name) {
                params.push_back(std::make_unique<juce::AudioParameterFloat>(
                    cosmId(k, id), tn + name, juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            };
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbCutoff"), tn + "LPF Cutoff", 0, 127, 90));
            kfParam("TbKF", "LPF Cutoff Keyfollow");
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbLfo"), tn + "LPF Cutoff LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbEnv"), tn + "LPF Cutoff Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbQ"), tn + "LPF Q", 0, 127, 100));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbQFollow"), tn + "LPF Q Follow", -63, 63, 25));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbHpf"), tn + "HPF Cutoff", 0, 127, 45));
            kfParam("TbHpfKF", "HPF Cutoff Keyfollow");
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbHpfLfo"), tn + "HPF Cutoff LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbHpfEnv"), tn + "HPF Cutoff Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "TbGain"), tn + "Gain", 0, 127, 127));
        }
        {                                            // the Comb Filter, the XT's defaults (Octave +1)
            const juce::String cn = n + "Comb ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "CombOctave"), cn + "Octave",
                juce::StringArray { "-2", "-1", "0", "+1", "+2" }, 3));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombDetune"), cn + "Detune", -100, 100, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombDetuneLfo"), cn + "Detune LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombDetuneEnv"), cn + "Detune Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombTone"), cn + "Tone", 0, 127, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombToneLfo"), cn + "Tone LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CombToneEnv"), cn + "Tone Env Depth", -63, 63, 0));
        }
        for (const char* ab : { "A", "B" }) {
            const juce::String id = cosmId(k, "Env") + ab, name = n + "Env " + ab + " ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Attack", name + "Attack", 0, 127, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Decay", name + "Decay", 0, 127, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Sustain", name + "Sustain", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Release", name + "Release", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "VelCurve", name + "Velocity Curve", 0, 7, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "VelSens", name + "Velocity Sens", -63, 63, 32));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "AttackSens", name + "Attack Velocity Sens", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "DecaySens", name + "Decay Velocity Sens", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "ReleaseSens", name + "Release Velocity Sens", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                id + "TimeKF", name + "Time Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
        }
        const juce::String ln = k ? "LFO 4 " : "LFO 3 ";
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LfoWave"), ln + "Waveform",
            juce::StringArray { "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 1));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoRate"), ln + "Rate", 0, 127, 98));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LfoOffset"), ln + "Offset",
            juce::StringArray { "-100", "-50", "0", "+50", "+100" }, 2));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoDelay"), ln + "Delay", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LfoFadeMode"), ln + "Fade Mode",
            juce::StringArray { "ON <<", "ON >>", "OFF <<", "OFF >>" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoFade"), ln + "Fade", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(cosmId(k, "LfoKeySync"), ln + "Key Sync", false));
    }

    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAttack, "Attack", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kDecay, "Decay", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kSustain, "Sustain", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kRelease, "Release", 0, 127, 16));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kAttackSens, "Attack Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kDecaySens, "Decay Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kReleaseSens, "Release Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kTimeKF, "Env Time KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPAttack, "Pitch Env Attack", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPDecay, "Pitch Env Decay", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPSustain, "Pitch Env Sustain", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPRelease, "Pitch Env Release", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kPDepth, "Pitch Env Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTAttack, "Time Env Attack", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTDecay, "Time Env Decay", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTSustain, "Time Env Sustain", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTRelease, "Time Env Release", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTDepth, "Time Env Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFAttack, "Formant Env Attack", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFDecay, "Formant Env Decay", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFSustain, "Formant Env Sustain", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFRelease, "Formant Env Release", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kFDepth, "Formant Env Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kLfoWave, "LFO Waveform",
        juce::StringArray { "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoRate, "LFO Rate", 0, 127, 64));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoDepth, "LFO Pitch Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoTimeDepth, "LFO Time Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kLfoFmtDepth, "LFO Formant Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscLevel, "OSC Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kOscLevelKF, "OSC Level KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOscLevelLfo, "OSC Level LFO Depth", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kWaveGain, "Wave Gain", -12, 12, 0,
        juce::AudioParameterIntAttributes().withLabel("dB")));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOAttack, "OSC Env Attack", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kODecay, "OSC Env Decay", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOSustain, "OSC Env Sustain", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kORelease, "OSC Env Release", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kOASens, "OSC Env Attack Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kODSens, "OSC Env Decay Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kORSens, "OSC Env Release Velocity Sens", -63, 63, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        Params::kOTimeKF, "OSC Env Time KF", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
    auto curveName = juce::AudioParameterIntAttributes().withStringFromValueFunction(
        [](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v); });
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

    return { params.begin(), params.end() };
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────

VSynthEmuProcessor::VSynthEmuProcessor()
    // Instrument: stereo output only, no sidechain input
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VSynthEmu", createParameterLayout())
{
    formatManager_.registerBasicFormats();   // WAV, AIFF, OGG, FLAC

    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth })
        apvts.addParameterListener(id, this);
    findRom();
}

VSynthEmuProcessor::~VSynthEmuProcessor() {
    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth })
        apvts.removeParameterListener(id, this);
    encodePool_.removeAllJobs(true, 10000);
}

// ─── The firmware image ───────────────────────────────────────────────────────

// VSE_XT_ROM, else ~/Library/Application Support/V-Synth Emu/V-SynthXT.rom.2.bin
// (rom.2 as research/ghidra_scripts/unpack_firmware.py writes it).
void VSynthEmuProcessor::findRom() {
    juce::StringArray cand;
    if (const char* p = std::getenv("VSE_XT_ROM")) cand.add(p);
    cand.add(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("Application Support/V-Synth Emu/V-SynthXT.rom.2.bin").getFullPathName());
    for (const auto& c : cand)
        if (juce::File(c).existsAsFile() && xtSynthSetRom(c.toStdString())) {
            apvts.state.setProperty("romPath", c, nullptr);
            setStatus("ROM ready");
            return;
        }
    setStatus("No ROM: choose V-SynthXT.rom.2.bin to play through the XT's firmware");
}

bool VSynthEmuProcessor::setRomFile(const juce::File& file) {
    if (!file.existsAsFile() || !xtSynthSetRom(file.getFullPathName().toStdString())) {
        setStatus("Not a usable rom.2 image: " + file.getFileName());
        return false;
    }
    apvts.state.setProperty("romPath", file.getFullPathName(), nullptr);
    setStatus("ROM ready");
    reencode();
    return true;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock);
    engine_.setAlgorithm(VSE::Algorithm::Passthrough);
    syncParamsToEngine();
    setLatencySamples(engine_.getLatencySamples());
    synth_.prepare(sampleRate);
    scratch_.assign((size_t)std::max(samplesPerBlock, 1) * 2, 0.0f);
    playbackPos_  = -1;
    activeVoices_ = 0;
}

void VSynthEmuProcessor::releaseResources() {
    engine_.reset();
}

// ─── Process ──────────────────────────────────────────────────────────────────

XtSynth::Controls VSynthEmuProcessor::xtControls() const {
    // Snapped to the parameters' steps: the ranges' float conversions leave
    // the defaults at e.g. 0.99999994, and the firmware's fixed-point steps
    // would play that, not 1 (a centred XT knob is exact).
    auto snap = [](float v, float stepSize) { return std::round(v / stepSize) * stepSize; };
    XtSynth::Controls c;
    auto knob = [this](const juce::String& id) { return (int)std::lround(apvts.getRawParameterValue(id)->load()); };
    auto kf = [this](const juce::String& id) { return 10 * (int)std::lround(apvts.getRawParameterValue(id)->load() / 10.0f); };
    c.pitchSemis   = snap(apvts.getRawParameterValue(Params::kPitchShift)->load(), 0.01f);
    c.oscTime      = knob(Params::kOscTime);
    c.oscTimeOffset = knob(Params::kOscTimeOffset);
    c.oscTimeKF    = kf(Params::kOscTimeKF);
    c.oscFormant   = knob(Params::kOscFormant);
    c.oscFormantKF = kf(Params::kOscFormantKF);
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
    c.loop         = apvts.getRawParameterValue(Params::kLoop)->load() >= 0.5f;
    c.loopMode     = knob(Params::kLoopMode);
    c.loopStart    = snap(apvts.getRawParameterValue(Params::kLoopStart)->load(), 0.01f) / 100.0f;
    c.loopEnd      = snap(apvts.getRawParameterValue(Params::kLoopEnd)->load(), 0.01f) / 100.0f;
    c.portamento   = apvts.getRawParameterValue(Params::kPorta)->load() >= 0.5f;
    c.portaMode    = knob(Params::kPortaMode);
    c.portaType    = knob(Params::kPortaType);
    c.portaTime    = knob(Params::kPortaTime);
    c.portaVeloSens = knob(Params::kPortaVeloSens);
    c.mono         = knob(Params::kMonoPoly) == 0;
    c.legato       = apvts.getRawParameterValue(Params::kLegato)->load() >= 0.5f;
    c.octave       = knob(Params::kOctave);
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
    c.energy       = knob(Params::kEnergy);
    c.rootKey      = (int)apvts.getRawParameterValue(Params::kRootKey)->load();
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
    c.lfoWave      = (int)apvts.getRawParameterValue(Params::kLfoWave)->load();
    c.lfoRate      = (int)apvts.getRawParameterValue(Params::kLfoRate)->load();
    c.lfoPitchDepth = (int)apvts.getRawParameterValue(Params::kLfoDepth)->load();
    c.lfoDelay     = (int)apvts.getRawParameterValue(Params::kLfoDelay)->load();
    c.lfoFadeMode  = (int)apvts.getRawParameterValue(Params::kLfoFadeMode)->load();
    c.lfoFade      = (int)apvts.getRawParameterValue(Params::kLfoFade)->load();
    c.lfoOffset    = (int)apvts.getRawParameterValue(Params::kLfoOffset)->load();
    c.lfoKeySync   = apvts.getRawParameterValue(Params::kLfoKeySync)->load() > 0.5f;
    return c;
}

void VSynthEmuProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numOut     = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // ── The XT's own players: one voice per note, sample-accurate MIDI ───────
    if (xtActive_.load() && numOut > 0) {
        synth_.setControls(xtControls());
        float* L = buffer.getWritePointer(0);
        float* R = numOut > 1 ? buffer.getWritePointer(1) : nullptr;
        if (!R) {
            if ((int)scratch_.size() < numSamples) scratch_.resize((size_t)numSamples);   // normally sized in prepareToPlay
            R = scratch_.data();
        }
        int pos = 0;
        for (const auto meta : midi) {
            const int at = std::clamp(meta.samplePosition, pos, numSamples);
            if (at > pos) { synth_.render(L + pos, R + pos, at - pos); pos = at; }
            const auto msg = meta.getMessage();
            if (msg.isNoteOn()) synth_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff()) synth_.noteOff(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isPitchWheel()) synth_.pitchBend(msg.getPitchWheelValue());
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) synth_.allNotesOff();
        }
        if (pos < numSamples) synth_.render(L + pos, R + pos, numSamples - pos);
        if (numOut == 1) for (int i = 0; i < numSamples; ++i) L[i] = 0.5f * (L[i] + R[i]);
        return;
    }

    // ── Without the firmware: the loaded file through the engine ─────────────
    // MIDI: note-on starts playback, note-off decrements voice count
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            ++activeVoices_;
            playbackPos_ = 0;   // restart from top on any note-on
            engine_.reset();
        }
        else if (msg.isNoteOff())
        {
            activeVoices_ = std::max(0, activeVoices_ - 1);
            if (activeVoices_ == 0)
                playbackPos_ = -1;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            activeVoices_ = 0;
            playbackPos_  = -1;
        }
    }

    if (playbackPos_ < 0 || !fileLoaded_.load())
        return;   // nothing to play — buffer is already silent

    // ── Grab the file buffer (non-blocking) ───────────────────────────────────
    juce::GenericScopedTryLock<juce::CriticalSection> lock(bufferLock_);
    if (!lock.isLocked())
        return;   // file is being swapped out — skip this block silently

    const int fileSamples = fileBuffer_.getNumSamples();
    const int fileChans   = fileBuffer_.getNumChannels();

    if (fileSamples == 0 || fileChans == 0)
        return;

    // ── Fill a mono source block from the file buffer ─────────────────────────
    // Mix file channels down to mono for the engine, then fan back out to stereo.
    juce::AudioBuffer<float> monoIn(1, numSamples);
    monoIn.clear();

    int remaining = numSamples;
    int written   = 0;

    while (remaining > 0 && playbackPos_ >= 0)
    {
        const int avail = fileSamples - playbackPos_;
        const int toCopy = std::min(avail, remaining);

        for (int ch = 0; ch < fileChans; ++ch)
            monoIn.addFrom(0, written,
                           fileBuffer_, ch,
                           playbackPos_, toCopy,
                           1.0f / float(fileChans));

        playbackPos_ += toCopy;
        written       += toCopy;
        remaining     -= toCopy;

        if (playbackPos_ >= fileSamples)
        {
            playbackPos_  = -1;   // reached end — stop
            activeVoices_ = 0;
            break;
        }
    }

    // ── Process mono through the engine ──────────────────────────────────────
    juce::AudioBuffer<float> monoOut(1, numSamples);
    monoOut.clear();

    const float* inPtr  = monoIn.getReadPointer(0);
    float*       outPtr = monoOut.getWritePointer(0);
    const float* inArr[1]  = { inPtr };
    float*       outArr[1] = { outPtr };
    engine_.process(inArr, outArr, 1, numSamples);

    // ── Fan mono output to all output channels ───────────────────────────────
    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom(ch, 0, monoOut, 0, 0, numSamples);
}

// ─── File Loading ─────────────────────────────────────────────────────────────

void VSynthEmuProcessor::loadFile(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager_.createReaderFor(file));

    if (!reader)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Load Error",
            "Could not read: " + file.getFileName());
        return;
    }

    const int totalSamples = static_cast<int>(reader->lengthInSamples);
    const int numChans     = static_cast<int>(reader->numChannels);

    juce::AudioBuffer<float> newBuffer(numChans, totalSamples);
    reader->read(&newBuffer, 0, totalSamples, 0, true, true);

    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        fileBuffer_ = std::move(newBuffer);
        fileRate_ = reader->sampleRate;
        fileBits_ = reader->usesFloatingPointData ? 16 : (int)reader->bitsPerSample;   // float WAVs: rounded
        fileLoaded_.store(true);
    }

    playbackPos_  = -1;
    activeVoices_ = 0;
    loadedFileName_ = file.getFileName();

    // Persist the file path so it survives DAW project reload
    apvts.state.setProperty("loadedFilePath", file.getFullPathName(), nullptr);
    reencode();
}

// ─── Encoding ─────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::setStatus(const juce::String& s) {
    const juce::ScopedLock l(statusLock_);
    status_ = s;
}

juce::String VSynthEmuProcessor::getStatusText() const {
    const juce::ScopedLock l(statusLock_);
    return status_;
}

// Encodes the loaded file as the XT would (XtSynth.h) on the pool thread;
// a newer request discards an older result.
void VSynthEmuProcessor::reencode() {
    if (!fileLoaded_.load()) return;
    if (!xtSynthRomReady()) { xtActive_.store(false); return; }
    std::vector<std::vector<float>> chans;
    double rate;
    int bits;
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        for (int c = 0; c < std::min(2, fileBuffer_.getNumChannels()); ++c)
            chans.emplace_back(fileBuffer_.getReadPointer(c), fileBuffer_.getReadPointer(c) + fileBuffer_.getNumSamples());
        rate = fileRate_;
        bits = fileBits_;
    }
    const int choice = (int)apvts.getRawParameterValue(Params::kEncodeType)->load();
    const int depth  = (int)apvts.getRawParameterValue(Params::kEncodeDepth)->load();
    const int gen = ++encodeGen_;
    setStatus("Encoding...");
    encodePool_.addJob([this, chans = std::move(chans), rate, bits, choice, depth, gen] {
        int type = choice == 1 ? 1 : choice == 2 ? 3 : choice == 3 ? 5 : 0;
        if (type == 0) {   // Auto: the engine's content classifier
            std::vector<float> mono(chans[0].size());
            for (size_t i = 0; i < mono.size(); ++i) {
                float s = 0; for (const auto& c : chans) s += c[i];
                mono[i] = s / (float)chans.size();
            }
            using CT = VSE::VariphraseAnalysis::ContentType;
            const auto a = VSE::VariphraseEngine::analyzeContent(mono.data(), (int)mono.size(), rate);
            type = a.contentType == CT::BACKING ? 3 : a.contentType == CT::ENSEMBLE ? 5 : 1;   // LITE plays on the SOLO player
        }
        std::string err;
        auto e = xtSynthEncode(chans, rate, type, depth, &err, bits);
        if (gen != encodeGen_.load()) return;
        if (!e) { setStatus("Encode failed: " + juce::String(err)); return; }
        synth_.setSample(e);
        xtActive_.store(true);
        static const char* names[] = { "", "SOLO", "", "BACKING", "", "ENSEMBLE" };
        setStatus(juce::String("Encoded as ") + names[type] + (choice == 0 ? " (auto)" : ""));
    });
}

void VSynthEmuProcessor::handleAsyncUpdate() { reencode(); }

// ─── Parameters ───────────────────────────────────────────────────────────────

void VSynthEmuProcessor::parameterChanged(const juce::String& paramID, float /*newValue*/) {
    if (paramID == Params::kEncodeType || paramID == Params::kEncodeDepth) { triggerAsyncUpdate(); return; }
    syncParamsToEngine();
}

void VSynthEmuProcessor::syncParamsToEngine() {
    VSE::VariphraseParams p;
    p.pitchShiftSemitones   = apvts.getRawParameterValue(Params::kPitchShift)->load();
    p.timeStretchRatio      = apvts.getRawParameterValue(Params::kTimeStretch)->load();
    p.formantShiftSemitones = apvts.getRawParameterValue(Params::kFormantShift)->load();
    p.robotMode             = apvts.getRawParameterValue(Params::kRobot)->load() > 0.5f;
    engine_.setParams(p);
}

void VSynthEmuProcessor::setAlgorithm(VSE::Algorithm algo) {
    engine_.setAlgorithm(algo);
}

VSE::Algorithm VSynthEmuProcessor::getAlgorithm() const {
    return engine_.getAlgorithm();
}

// ─── State ────────────────────────────────────────────────────────────────────

void VSynthEmuProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VSynthEmuProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;

    apvts.replaceState(juce::ValueTree::fromXml(*xml));

    // The firmware image and the file, if their paths were persisted
    const juce::String rom = apvts.state.getProperty("romPath").toString();
    if (rom.isNotEmpty() && !xtSynthRomReady() && juce::File(rom).existsAsFile())
        setRomFile(juce::File(rom));
    juce::String path = apvts.state.getProperty("loadedFilePath").toString();
    if (path.isNotEmpty())
    {
        juce::File f(path);
        if (f.existsAsFile())
            loadFile(f);
    }
}

// ─── Editor / Entry Point ─────────────────────────────────────────────────────

juce::AudioProcessorEditor* VSynthEmuProcessor::createEditor() {
    return new VSynthEmuEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new VSynthEmuProcessor();
}
