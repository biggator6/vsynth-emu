#include "PluginProcessor.h"
#include <array>
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
    // OSC Type and the analog oscillator (XtAnalog): Waveform in the XT's
    // order (SAW so far), Impact 0..40 (0.0–4.0), Pulse Width and Fat with
    // their Keyfollows, LFO and Env depths (PW on the time envelope, Fat on
    // the formant envelope)
    static const juce::String kOscType       = "oscType";        // ANALOG, PCM
    // the OSC Switch, and OSC 2: each of OSC 1's own knobs with "osc2" before
    // it (oscSwitch → osc2OscSwitch, anaWave → osc2AnaWave, lfoRate →
    // osc2LfoRate, encodeType → osc2EncodeType); off by default
    static const juce::String kOscSwitch     = "oscSwitch";
    static juce::String oscId(int k, const juce::String& id) { return k ? "osc2" + id.substring(0, 1).toUpperCase() + id.substring(1) : id; }
    static const juce::String kAnaWave       = "anaWave";
    static const juce::String kAnaImpact     = "anaImpact";
    static const juce::String kAnaPw         = "anaPw";
    static const juce::String kAnaPwKF       = "anaPwKF";
    static const juce::String kAnaPwLfo      = "anaPwLfo";
    static const juce::String kAnaPwEnv      = "anaPwEnv";
    static const juce::String kAnaFat        = "anaFat";
    static const juce::String kAnaFatKF      = "anaFatKF";
    static const juce::String kAnaFatLfo     = "anaFatLfo";
    static const juce::String kAnaFatEnv     = "anaFatEnv";
    static const juce::String kAnaDetune     = "anaDetune";
    static const juce::String kAnaSubOct     = "anaSubOct";
    static const juce::String kAnaSubLevel   = "anaSubLevel";
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
    // The MOD section (XtMod): Switch, Type, Original Level, Attack and Release
    // Times, the XT's defaults
    static const juce::String kModSwitch     = "modSwitch";
    static const juce::String kModType       = "modType";        // MIX, RING, FM, ENV RING, OSC SYNC
    static const juce::String kModLevel      = "modLevel";
    static const juce::String kModAttack     = "modAttack";
    static const juce::String kStructure     = "structure";      // Structure Type, Type1…Type3
    static const juce::String kModRelease    = "modRelease";
    // Key assign: Mono/Poly (MONO, POLY), Legato Switch, Octave Shift −3..+3
    static const juce::String kMonoPoly      = "monoPoly";
    static const juce::String kLegato        = "legatoSwitch";
    static const juce::String kOctave        = "octaveShift";
    // The COSM sections as TVFs, COSM 1 ("cosm…") and COSM 2 ("cosm2…"):
    // Switch, Type (TVF, Dynamic TVF, Dual Filter, TB Filter, Comb Filter, 1st and 2nd Side Band, Resonator, Frequency Shifter, Lo-Fi, Compressor, Limiter, Wave Shape, OD/Dist, Amp Simulator, Speaker Simulator), ResoOwn/CutoffOwn (the Dynamic TVF's
    // Own Env depths), Filter, Cutoff, Reso, CutoffKF, CutoffEnv/Lfo, ResoEnv/Lfo; the
    // Dual Filter's DualType and Dual1…/Dual2… Freq, KF, Q, Lfo, Env; the TB
    // Filter's TbCutoff, TbKF, TbLfo, TbEnv, TbQ, TbQFollow, TbHpf, TbHpfKF,
    // TbHpfLfo, TbHpfEnv, TbGain; the Comb Filter's CombOctave, CombDetune,
    // CombDetuneLfo, CombDetuneEnv, CombTone, CombToneLfo, CombToneEnv; the
    // Side Band Filters' Sb1…/Sb2… (the 1st's Hpf and SubDetune, the 2nd's Q)
    // Lpf, LpfKF, Octave, Detune, DetuneLfo/Own/Env, Width, WidthKF,
    // WidthLfo/Own/Env; the Resonator's ResType, ResSize, ResSizeKF,
    // ResSizeLfo, ResSizeEnv, ResBalance, ResBalanceLfo, ResBalanceEnv; the
    // Frequency Shifter's FsDepth, FsDepthKF, FsDepthLfo, FsDepthEnv,
    // FsBalance, FsBalanceLfo, FsBalanceEnv; Lo-Fi's LofiBit, LofiRate,
    // LofiFilter, LofiFilterKF, LofiFilterLfo, LofiFilterEnv, LofiBalance,
    // LofiBalanceLfo, LofiBalanceEnv; the Compressor's CompAttack,
    // CompSustain, CompLevel; the Limiter's LimThreshold, LimRatio,
    // LimAttack, LimRelease, LimLevel; Wave Shape's WsType, WsLevel, WsDrive,
    // WsDriveLfo, WsDriveEnv, WsTone, WsToneKF, WsToneLfo, WsToneEnv; OD/Dist's
    // the same with Od…; the Amp Simulator's AmpType, AmpBass, AmpMiddle,
    // AmpTreble, AmpLevel, AmpGain, AmpGainLfo, AmpGainEnv; the Speaker
    // Simulator's SpkType; the envelopes EnvA (the cutoff's) and EnvB (the resonance's), each
    // Attack…TimeKF; the LFO, LfoWave…LfoKeySync
    static juce::String cosmId(int k, const char* name) { return juce::String(k ? "cosm2" : "cosm") + name; }
    static const char* const kCosmEnvKnobs[10] = { "Attack", "Decay", "Sustain", "Release", "VelCurve", "VelSens",
                                                   "AttackSens", "DecaySens", "ReleaseSens", "TimeKF" };
    // Amp envelope, the XT's 0..127 knobs (XtEnvelope: times from the firmware's table)
    static const juce::String kTvaSwitch    = "tvaSwitch";
    static const juce::String kTvaLevel     = "tvaLevel";          // the patch TVA's level and pan
    static const juce::String kTvaLevelKF   = "tvaLevelKF";
    static const juce::String kTvaLevelLfo  = "tvaLevelLfo";
    static const juce::String kTvaPan       = "tvaPan";
    static const juce::String kTvaPanKF     = "tvaPanKF";
    static const juce::String kTvaPanLfo    = "tvaPanLfo";
    static const juce::String kMfxSend      = "mfxSend";
    static const juce::String kTvaLfoWave   = "tvaLfoWave";        // LFO 5
    static const juce::String kTvaLfoRate   = "tvaLfoRate";
    static const juce::String kTvaLfoOffset = "tvaLfoOffset";
    static const juce::String kTvaLfoDelay  = "tvaLfoDelay";
    static const juce::String kTvaLfoMode   = "tvaLfoFadeMode";
    static const juce::String kTvaLfoFade   = "tvaLfoFade";
    static const juce::String kTvaLfoSync   = "tvaLfoKeySync";
    // The tempo: the Patch Tempo (BPM, ID 0x11d) and the Time Trip pad as one knob, 0 lifted, 1..127 a
    // finger going clockwise round it (XtSynth::timeTripKnob).  Each OSC's Tempo Sync (IDs 0x2d0, 0x304),
    // its wave's Original Tempo (BPM), Time Trip Sw and Beat Keep (0x2d6, 0x2d7; 0x30a, 0x30b)
    static const juce::String kPatchTempo   = "patchTempo";
    static const juce::String kClockSource  = "clockSource";
    static const juce::String kTripPad      = "timeTrip";
    static const juce::String kTempoSync    = "tempoSync";
    static const juce::String kOrigTempo    = "origTempo";
    static const juce::String kTripSw       = "timeTripSw";
    static const juce::String kBeatKeep     = "beatKeep";
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

juce::String VSynthEmuProcessor::oscParamId(int k, const juce::String& id) { return Params::oscId(k, id); }

// ─── Parameter Layout ─────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
VSynthEmuProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // An envelope's time or an LFO's rate from 128 on is a note value, as the XT's knobs go on past 127: 3 to 192 clocks of 24 to the quarter
    static const char* const kNotes[17] = { "1/32", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/1T", "1/2.", "1/1", "2/1T", "1/1.", "2/1" };
    const auto noteValue = juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([](int v, int) { return v >= 128 ? juce::String(kNotes[std::min(v - 128, 16)]) : juce::String(v); })
        .withValueFromStringFunction([](const juce::String& t) {
            for (int i = 0; i < 17; ++i) if (t.trim() == kNotes[i]) return 128 + i;
            return t.getIntValue();
        });
    params.push_back(std::make_unique<juce::AudioParameterFloat>(Params::kPatchTempo, "Patch Tempo", juce::NormalisableRange<float>(20.0f, 250.0f, 0.1f), 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kClockSource, "Clock Source", juce::StringArray { "INTERNAL", "HOST" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kTripPad, "Time Trip", 0, 127, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v); })));
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kTempoSync, "Tempo Sync", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(Params::kOrigTempo, "Original Tempo", juce::NormalisableRange<float>(20.0f, 300.0f, 0.01f), 120.0f));
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
            juce::StringArray { "TVF", "Dynamic TVF", "Dual Filter", "TB Filter", "Comb Filter", "1st Side Band", "2nd Side Band", "Resonator", "Frequency Shifter", "Lo-Fi", "Compressor", "Limiter", "Wave Shape", "OD/Dist", "Amp Simulator", "Speaker Simulator" }, 0));
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
        for (int t = 1; t <= 2; ++t) {               // the Side Band Filters, the XT's defaults (Octave −1, Width 16 and 32)
            const juce::String id = cosmId(k, "Sb") + juce::String(t), sn = n + (t == 1 ? "1st" : "2nd") + " Side Band ";
            auto num = [&](const char* sfx, const char* name, int lo, int hi, int def) {
                params.push_back(std::make_unique<juce::AudioParameterInt>(id + sfx, sn + name, lo, hi, def));
            };
            auto kfParam = [&](const char* sfx, const char* name, float def) {
                params.push_back(std::make_unique<juce::AudioParameterFloat>(
                    id + sfx, sn + name, juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), def));
            };
            if (t == 1) num("Hpf", "HPF Cutoff", 0, 127, 48);
            else num("Q", "Q", 0, 127, 64);
            num("Lpf", "LPF Cutoff", 0, 127, 64);
            kfParam("LpfKF", "LPF Cutoff Keyfollow", 100.0f);
            if (t == 1) num("SubDetune", "Sub Delay Line Detune", -100, 100, 0);
            params.push_back(std::make_unique<juce::AudioParameterChoice>(id + "Octave", sn + "Octave Select", juce::StringArray { "-2", "-1", "0" }, 1));
            num("Detune", "Detune", -100, 100, 0);
            num("DetuneLfo", "Detune LFO Depth", -63, 63, 0);
            num("DetuneOwn", "Detune Own Env Depth", -63, 63, 0);
            num("DetuneEnv", "Detune Env Depth", -63, 63, 0);
            num("Width", "Side Band Width", 0, 127, t == 1 ? 16 : 32);
            kfParam("WidthKF", "Side Band Width Keyfollow", 0.0f);
            num("WidthLfo", "Side Band Width LFO Depth", -63, 63, 0);
            num("WidthOwn", "Side Band Width Own Env Depth", -63, 63, 0);
            num("WidthEnv", "Side Band Width Env Depth", -63, 63, 0);
        }
        {                                            // the Resonator, the XT's defaults
            const juce::String rn = n + "Resonator ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "ResType"), rn + "Filter Type", juce::StringArray { "1", "2", "3" }, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResSize"), rn + "Size", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "ResSizeKF"), rn + "Size Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResSizeLfo"), rn + "Size LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResSizeEnv"), rn + "Size Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResBalance"), rn + "Balance", 0, 100, 50));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResBalanceLfo"), rn + "Balance LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "ResBalanceEnv"), rn + "Balance Env Depth", -63, 63, 0));
        }
        {                                            // the Frequency Shifter, the XT's defaults
            const juce::String fn = n + "Frequency Shifter ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsDepth"), fn + "Effect Depth", 0, 127, 80));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "FsDepthKF"), fn + "Effect Depth Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsDepthLfo"), fn + "Effect LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsDepthEnv"), fn + "Effect Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsBalance"), fn + "Dry/Effect Balance", 0, 100, 50));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsBalanceLfo"), fn + "Balance LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "FsBalanceEnv"), fn + "Balance Env Depth", -63, 63, 0));
        }
        {                                            // Lo-Fi, the XT's defaults
            const juce::String ln = n + "Lo-Fi ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiBit"), ln + "Sample Bit Down", 0, 15, 10));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiRate"), ln + "Sample Rate Down", 0, 5, 4));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiFilter"), ln + "Filter", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "LofiFilterKF"), ln + "Filter Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiFilterLfo"), ln + "Filter LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiFilterEnv"), ln + "Filter Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiBalance"), ln + "Dry/Effect Balance", 0, 100, 50));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiBalanceLfo"), ln + "Balance LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LofiBalanceEnv"), ln + "Balance Env Depth", -63, 63, 0));
        }
        {                                            // the Compressor, the XT's defaults
            const juce::String cn = n + "Compressor ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CompAttack"), cn + "Attack", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CompSustain"), cn + "Sustain", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "CompLevel"), cn + "Output Level", 0, 127, 115));
        }
        {                                            // the Limiter, the XT's defaults
            const juce::String mn = n + "Limiter ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LimThreshold"), mn + "Threshold Level", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LimRatio"), mn + "Ratio",
                                                                          juce::StringArray { "1.5:1", "2:1", "4:1", "100:1" }, 1));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LimAttack"), mn + "Attack", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LimRelease"), mn + "Release", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LimLevel"), mn + "Output Level", 0, 127, 115));
        }
        {                                            // Wave Shape, the XT's defaults
            const juce::String wn = n + "Wave Shape ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "WsType"), wn + "Type",
                                                                          juce::StringArray { "1", "2", "3", "4", "5", "6" }, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsLevel"), wn + "Output Level", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsDrive"), wn + "Drive", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsDriveLfo"), wn + "Drive LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsDriveEnv"), wn + "Drive Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsTone"), wn + "Tone", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "WsToneKF"), wn + "Tone Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsToneLfo"), wn + "Tone LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "WsToneEnv"), wn + "Tone Env Depth", -63, 63, 0));
        }
        {                                            // OD/Dist, the XT's defaults
            const juce::String on = n + "OD/Dist ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "OdType"), on + "Type", juce::StringArray { "OD", "DS" }, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdLevel"), on + "Output Level", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdDrive"), on + "Drive", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdDriveLfo"), on + "Drive LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdDriveEnv"), on + "Drive Env Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdTone"), on + "Tone", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                cosmId(k, "OdToneKF"), on + "Tone Keyfollow", juce::NormalisableRange<float>(-200.0f, 200.0f, 10.0f), 0.0f));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdToneLfo"), on + "Tone LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "OdToneEnv"), on + "Tone Env Depth", -63, 63, 0));
        }
        {                                            // the Amp Simulator, the XT's defaults
            const juce::String an = n + "Amp Simulator ";
            params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "AmpType"), an + "Amp Type", juce::StringArray { "1", "2", "3" }, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpBass"), an + "Bass", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpMiddle"), an + "Middle", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpTreble"), an + "Treble", 0, 127, 64));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpLevel"), an + "Output Level", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpGain"), an + "Gain", 0, 127, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpGainLfo"), an + "Gain LFO Depth", -63, 63, 0));
            params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "AmpGainEnv"), an + "Gain Env Depth", -63, 63, 0));
        }
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "SpkType"), n + "Speaker Simulator Speaker Type",
            juce::StringArray { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12" }, 0));
        for (const char* ab : { "A", "B" }) {
            const juce::String id = cosmId(k, "Env") + ab, name = n + "Env " + ab + " ";
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Attack", name + "Attack", 0, 144, 0, noteValue));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Decay", name + "Decay", 0, 144, 0, noteValue));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Sustain", name + "Sustain", 0, 127, 127));
            params.push_back(std::make_unique<juce::AudioParameterInt>(id + "Release", name + "Release", 0, 144, 127, noteValue));
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
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoRate"), ln + "Rate", 0, 144, 98, noteValue));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LfoOffset"), ln + "Offset",
            juce::StringArray { "-100", "-50", "0", "+50", "+100" }, 2));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoDelay"), ln + "Delay", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(cosmId(k, "LfoFadeMode"), ln + "Fade Mode",
            juce::StringArray { "ON <<", "ON >>", "OFF <<", "OFF >>" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterInt>(cosmId(k, "LfoFade"), ln + "Fade", 0, 127, 0));
        params.push_back(std::make_unique<juce::AudioParameterBool>(cosmId(k, "LfoKeySync"), ln + "Key Sync", false));
    }

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
    for (int i = 0; i < 10; ++i)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(assignParamId(i), juce::String(assignName(i)) + " Assign", assignValueNames(), 0));
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
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kOscSwitch, "OSC 1 Switch", true));
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
        params.push_back(std::make_unique<juce::AudioParameterFloat>(oscId(1, Params::kOrigTempo), n + "Original Tempo", juce::NormalisableRange<float>(20.0f, 300.0f, 0.01f), 120.0f));
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
            juce::StringArray { "Auto", "SOLO", "BACKING", "ENSEMBLE" }, 0));
        intP(Params::kEncodeDepth, "Encode Depth", 0, 127, 64);
        intP(Params::kRootKey, "Original Key", 0, 127, 60);
        params.push_back(std::make_unique<juce::AudioParameterInt>(oscId(1, Params::kEnergy), n + "Energy", 0, 127, 0,
            juce::AudioParameterIntAttributes().withStringFromValueFunction(
                [](int v, int) { return v == 0 ? juce::String("OFF") : juce::String(v); })));
        params.push_back(std::make_unique<juce::AudioParameterBool>(oscId(1, Params::kRobot), n + "Robot Voice", false));
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
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f, "%"));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(oscId(1, Params::kLoopEnd), n + "Loop End",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 100.0f, "%"));
    }
    params.push_back(std::make_unique<juce::AudioParameterBool>(Params::kModSwitch, "MOD Switch", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kModType, "MOD Type",
        juce::StringArray { "MIX", "RING", "FM", "ENV RING", "OSC SYNC" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kModLevel, "MOD Original Level", 0, 127, 127));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kModAttack, "MOD Attack Time", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(Params::kModRelease, "MOD Release Time", 0, 127, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(Params::kStructure, "Structure Type",
        juce::StringArray { "Type1", "Type2", "Type3" }, 0));

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
                            Params::kEncodeType, Params::kEncodeDepth, Params::oscId(1, Params::kEncodeType), Params::oscId(1, Params::kEncodeDepth) })
        apvts.addParameterListener(id, this);
    findRom();
}

VSynthEmuProcessor::~VSynthEmuProcessor() {
    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth, Params::oscId(1, Params::kEncodeType), Params::oscId(1, Params::kEncodeDepth) })
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
    reencode(0);
    reencode(1);
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
    c.origTempo    = (int)std::lround(snap(apvts.getRawParameterValue(Params::kOrigTempo)->load(), 0.01f) * 100.0f) * 100;
    c.timeTrip     = apvts.getRawParameterValue(Params::kTripSw)->load() >= 0.5f;
    c.beatKeep     = apvts.getRawParameterValue(Params::kBeatKeep)->load() >= 0.5f;
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
    c.mod.on       = apvts.getRawParameterValue(Params::kModSwitch)->load() >= 0.5f;
    c.mod.type     = knob(Params::kModType);
    c.mod.level    = knob(Params::kModLevel);
    c.mod.attack   = knob(Params::kModAttack);
    c.mod.release  = knob(Params::kModRelease);
    c.structure    = knob(Params::kStructure);
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
    c.energy       = knob(Params::kEnergy);
    c.rootKey      = (int)apvts.getRawParameterValue(Params::kRootKey)->load();
    c.tvaOn        = apvts.getRawParameterValue(Params::kTvaSwitch)->load() >= 0.5f;
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
        for (int i = 0; i < 10; ++i) c.assign[i] = knob(assignParamId(i));
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
    c.lfoWave      = (int)apvts.getRawParameterValue(Params::kLfoWave)->load();
    c.lfoRate      = (int)apvts.getRawParameterValue(Params::kLfoRate)->load();
    c.lfoPitchDepth = (int)apvts.getRawParameterValue(Params::kLfoDepth)->load();
    c.lfoDelay     = (int)apvts.getRawParameterValue(Params::kLfoDelay)->load();
    c.lfoFadeMode  = (int)apvts.getRawParameterValue(Params::kLfoFadeMode)->load();
    c.lfoFade      = (int)apvts.getRawParameterValue(Params::kLfoFade)->load();
    c.lfoOffset    = (int)apvts.getRawParameterValue(Params::kLfoOffset)->load();
    c.lfoKeySync   = apvts.getRawParameterValue(Params::kLfoKeySync)->load() > 0.5f;
    c.on           = apvts.getRawParameterValue(Params::kOscSwitch)->load() >= 0.5f;
    {   // OSC 2's own knobs
        XtSynth::OscControls& o = c.osc2;
        using Params::oscId;
        auto knob2 = [&](const juce::String& id) { return knob(oscId(1, id)); };
        auto kf2 = [&](const juce::String& id) { return kf(oscId(1, id)); };
        o.on = apvts.getRawParameterValue(oscId(1, Params::kOscSwitch))->load() >= 0.5f;
        o.oscType = knob2(Params::kOscType);
        o.ana.wave = knob2(Params::kAnaWave);
        o.ana.impact = knob2(Params::kAnaImpact);
        o.ana.pw = knob2(Params::kAnaPw);
        o.ana.pwKf = kf2(Params::kAnaPwKF) / 10;
        o.ana.pwLfo = knob2(Params::kAnaPwLfo);
        o.ana.pwEnv = knob2(Params::kAnaPwEnv);
        o.ana.fat = knob2(Params::kAnaFat);
        o.ana.fatKf = kf2(Params::kAnaFatKF) / 10;
        o.ana.fatLfo = knob2(Params::kAnaFatLfo);
        o.ana.fatEnv = knob2(Params::kAnaFatEnv);
        o.ana.detune = knob2(Params::kAnaDetune);
        o.ana.subOctave = knob2(Params::kAnaSubOct);
        o.ana.subLevel = knob2(Params::kAnaSubLevel);
        o.oscCoarse = knob2(Params::kOscCoarse);
        o.oscFine = knob2(Params::kOscFine);
        o.oscPitch = knob2(Params::kOscPitch);
        o.pitchKF = kf2(Params::kPitchKF);
        o.randomPitch = knob2(Params::kRandomPitch);
        o.pAttack = knob2(Params::kPAttack);
        o.pDecay = knob2(Params::kPDecay);
        o.pSustain = knob2(Params::kPSustain);
        o.pRelease = knob2(Params::kPRelease);
        o.pDepth = knob2(Params::kPDepth);
        o.tAttack = knob2(Params::kTAttack);
        o.tDecay = knob2(Params::kTDecay);
        o.tSustain = knob2(Params::kTSustain);
        o.tRelease = knob2(Params::kTRelease);
        o.tDepth = knob2(Params::kTDepth);
        o.fAttack = knob2(Params::kFAttack);
        o.fDecay = knob2(Params::kFDecay);
        o.fSustain = knob2(Params::kFSustain);
        o.fRelease = knob2(Params::kFRelease);
        o.fDepth = knob2(Params::kFDepth);
        o.lfoWave = knob2(Params::kLfoWave);
        o.lfoRate = knob2(Params::kLfoRate);
        o.lfoPitchDepth = knob2(Params::kLfoDepth);
        o.lfoTimeDepth = knob2(Params::kLfoTimeDepth);
        o.lfoFormantDepth = knob2(Params::kLfoFmtDepth);
        o.lfoDelay = knob2(Params::kLfoDelay);
        o.lfoFadeMode = knob2(Params::kLfoFadeMode);
        o.lfoFade = knob2(Params::kLfoFade);
        o.lfoOffset = knob2(Params::kLfoOffset);
        o.lfoKeySync = apvts.getRawParameterValue(oscId(1, Params::kLfoKeySync))->load() > 0.5f;
        o.oscLevel = knob2(Params::kOscLevel);
        o.oscLevelKF = kf2(Params::kOscLevelKF);
        o.oscLevelLfoDepth = knob2(Params::kOscLevelLfo);
        o.waveGain = knob2(Params::kWaveGain);
        o.oAttack = knob2(Params::kOAttack);
        o.oDecay = knob2(Params::kODecay);
        o.oSustain = knob2(Params::kOSustain);
        o.oRelease = knob2(Params::kORelease);
        o.oASens = knob2(Params::kOASens);
        o.oDSens = knob2(Params::kODSens);
        o.oRSens = knob2(Params::kORSens);
        o.oTimeKF = kf2(Params::kOTimeKF);
        o.oscVelSens = knob2(Params::kOscVelSens);
        o.oscVelCurve = knob2(Params::kOscVelCurve);
        o.energy = knob2(Params::kEnergy);
        o.rootKey = knob2(Params::kRootKey);
        o.robot = apvts.getRawParameterValue(oscId(1, Params::kRobot))->load() > 0.5f;
        o.oscTime = knob2(Params::kOscTime);
        o.oscTimeOffset = knob2(Params::kOscTimeOffset);
        o.oscTimeKF = kf2(Params::kOscTimeKF);
        o.oscFormant = knob2(Params::kOscFormant);
        o.oscFormantKF = kf2(Params::kOscFormantKF);
        o.tempoSync = apvts.getRawParameterValue(oscId(1, Params::kTempoSync))->load() >= 0.5f;
        o.origTempo = (int)std::lround(snap(apvts.getRawParameterValue(oscId(1, Params::kOrigTempo))->load(), 0.01f) * 100.0f) * 100;
        o.timeTrip = apvts.getRawParameterValue(oscId(1, Params::kTripSw))->load() >= 0.5f;
        o.beatKeep = apvts.getRawParameterValue(oscId(1, Params::kBeatKeep))->load() >= 0.5f;
        o.loop = apvts.getRawParameterValue(oscId(1, Params::kLoop))->load() >= 0.5f;
        o.loopMode = knob2(Params::kLoopMode);
        o.loopStart = snap(apvts.getRawParameterValue(oscId(1, Params::kLoopStart))->load(), 0.01f) / 100.0f;
        o.loopEnd = snap(apvts.getRawParameterValue(oscId(1, Params::kLoopEnd))->load(), 0.01f) / 100.0f;
    }
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
    // (an analog OSC needs the firmware image but no sample)
    const bool analog = xtSynthRomReady() && (apvts.getRawParameterValue(Params::kOscType)->load() < 0.5f
                                              || apvts.getRawParameterValue(Params::oscId(1, Params::kOscSwitch))->load() >= 0.5f);
    if ((xtActive_.load() || analog) && numOut > 0) {
        synth_.setControls(xtControls());
        // The host's transport, for Clock Source HOST (the Standalone has none: the Patch Tempo stands)
        double hostBpm = 0.0, hostPpq = 0.0;
        bool hostMoving = false;
        if (auto* head = getPlayHead())
            if (const auto info = head->getPosition()) {
                hostBpm = info->getBpm().orFallback(0.0);
                if (const auto ppq = info->getPpqPosition()) { hostPpq = *ppq; hostMoving = info->getIsPlaying(); }
            }
        synth_.hostClock(hostBpm, hostMoving, hostPpq);
        const int trip = (int)apvts.getRawParameterValue(Params::kTripPad)->load();
        if (trip != tripKnob_) { tripKnob_ = trip; synth_.timeTripKnob(trip); }
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
            else if (msg.isChannelPressure()) synth_.channelPressure(msg.getChannelPressureValue());
            else if (msg.isAftertouch()) synth_.polyPressure(msg.getNoteNumber(), msg.getAfterTouchValue());
            else if (msg.isController()) synth_.controller(msg.getControllerNumber(), msg.getControllerValue());   // the mode messages too
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

    if (playbackPos_ < 0 || !wave_[0].loaded.load())
        return;   // nothing to play — buffer is already silent

    // ── Grab the file buffer (non-blocking) ───────────────────────────────────
    juce::GenericScopedTryLock<juce::CriticalSection> lock(bufferLock_);
    if (!lock.isLocked())
        return;   // file is being swapped out — skip this block silently

    const int fileSamples = wave_[0].buffer.getNumSamples();
    const int fileChans   = wave_[0].buffer.getNumChannels();

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
                           wave_[0].buffer, ch,
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

void VSynthEmuProcessor::loadFile(const juce::File& file, int osc)
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
        LoadedWave& w = wave_[osc];
        w.buffer = std::move(newBuffer);
        w.rate = reader->sampleRate;
        w.bits = reader->usesFloatingPointData ? 16 : (int)reader->bitsPerSample;   // float WAVs: rounded
        w.loaded.store(true);
    }

    if (osc == 0) {
        playbackPos_  = -1;
        activeVoices_ = 0;
    }
    wave_[osc].name = file.getFileName();

    // Persist the file path so it survives DAW project reload
    apvts.state.setProperty(osc ? "loadedFilePath2" : "loadedFilePath", file.getFullPathName(), nullptr);
    reencode(osc);
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
void VSynthEmuProcessor::reencode(int osc) {
    LoadedWave& w = wave_[osc];
    if (!w.loaded.load()) return;
    if (!xtSynthRomReady()) { xtActive_.store(false); return; }
    std::vector<std::vector<float>> chans;
    double rate;
    int bits;
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        for (int c = 0; c < std::min(2, w.buffer.getNumChannels()); ++c)
            chans.emplace_back(w.buffer.getReadPointer(c), w.buffer.getReadPointer(c) + w.buffer.getNumSamples());
        rate = w.rate;
        bits = w.bits;
    }
    const int choice = (int)apvts.getRawParameterValue(Params::oscId(osc, Params::kEncodeType))->load();
    const int depth  = (int)apvts.getRawParameterValue(Params::oscId(osc, Params::kEncodeDepth))->load();
    const int gen = ++w.gen;
    const juce::String who = osc ? "OSC 2: " : "";
    setStatus(who + "Encoding...");
    encodePool_.addJob([this, chans = std::move(chans), rate, bits, choice, depth, gen, osc, who] {
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
        if (gen != wave_[osc].gen.load()) return;
        if (!e) { setStatus(who + "Encode failed: " + juce::String(err)); return; }
        synth_.setSample(e, osc);
        xtActive_.store(true);
        static const char* names[] = { "", "SOLO", "", "BACKING", "", "ENSEMBLE" };
        setStatus(who + "Encoded as " + names[type] + (choice == 0 ? " (auto)" : ""));
    });
}

void VSynthEmuProcessor::handleAsyncUpdate() {
    for (int k = 0; k < 2; ++k)
        if (wave_[k].reencode.exchange(false)) reencode(k);
}

// ─── Parameters ───────────────────────────────────────────────────────────────

void VSynthEmuProcessor::parameterChanged(const juce::String& paramID, float /*newValue*/) {
    for (int k = 0; k < 2; ++k)
        if (paramID == Params::oscId(k, Params::kEncodeType) || paramID == Params::oscId(k, Params::kEncodeDepth)) {
            wave_[k].reencode.store(true);
            triggerAsyncUpdate();
            return;
        }
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
    for (int k = 0; k < 2; ++k)
    {
        juce::String path = apvts.state.getProperty(k ? "loadedFilePath2" : "loadedFilePath").toString();
        if (path.isNotEmpty())
        {
            juce::File f(path);
            if (f.existsAsFile())
                loadFile(f, k);
        }
    }
}

// ─── Editor / Entry Point ─────────────────────────────────────────────────────

juce::AudioProcessorEditor* VSynthEmuProcessor::createEditor() {
    return new VSynthEmuEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new VSynthEmuProcessor();
}

// ── The Matrix's lists ──────────────────────────────────────────────────
juce::StringArray VSynthEmuProcessor::matrixSourceNames() {
    juce::StringArray a { "OFF" };
    for (int n = 1; n < 96; ++n) a.add(n == 32 ? juce::String("(OFF)") : "CC" + juce::String(n).paddedLeft('0', 2));
    a.addArray(juce::StringArray { "BEND", "AFT", "+PAD-X", "+PAD-Y", "PAD-X", "PAD-Y", "TRIP-R", "BEAM-L", "BEAM-R", "KNOB1", "KNOB2", "VELO", "KEYF" });
    return a;
}

juce::StringArray VSynthEmuProcessor::matrixDestNames() {
    juce::StringArray a { "OFF" };
    for (const char* o : { "OSC1", "OSC2" }) {
        for (const char* d : { "PITCH", "TIME/PW", "FORMA/FAT", "LVL", "PENV-ATK", "PENV-DCY", "PENV-REL", "TENV-ATK", "TENV-DCY", "TENV-REL",
                               "FENV-ATK", "FENV-DCY", "FENV-REL", "AENV-ATK", "AENV-DCY", "AENV-REL", "LFO-RATE", "LFO-PCH", "LFO-TM/PW", "LFO-FR/FT", "LFO-LVL" })
            a.add(juce::String(o) + "-" + d);
    }
    for (const char* o : { "CSM1", "CSM2" })
        for (const char* d : { "PRM1", "PRM2", "ENV1-ATK", "ENV1-DCY", "ENV1-REL", "ENV2-ATK", "ENV2-DCY", "ENV2-REL", "LFO-RATE", "LFO-PRM1", "LFO-PRM2" })
            a.add(juce::String(o) + "-" + d);
    a.addArray(juce::StringArray { "TVA-LVL", "TVA-ENV-ATK", "TVA-ENV-DCY", "TVA-ENV-REL", "TVA-LFO-RATE", "TVA-LFO-LVL", "TVA-LFO-PAN", "MFX-SEND", "CHO-SEND", "REV-SEND",
                                   "MFX-PRM1", "MFX-PRM2", "MFX-PRM3", "TVA-PAN" });
    return a;
}

juce::StringArray VSynthEmuProcessor::assignValueNames() {
    juce::StringArray a { "OFF" };
    for (int n = 1; n < 96; ++n) a.add(n == 32 ? juce::String("(OFF)") : "CC" + juce::String(n).paddedLeft('0', 2));
    return a;
}

const char* VSynthEmuProcessor::assignParamId(int i) {
    static const char* const k[10] = { "assignBeam2L", "assignBeam2R", "assignBeam3L", "assignBeam3R", "assignBeam4L", "assignBeam4R", "assignPadX", "assignPadY", "assignKnob1", "assignKnob2" };
    return k[i];
}

const char* VSynthEmuProcessor::assignName(int i) {
    static const char* const k[10] = { "Beam 2 L", "Beam 2 R", "Beam 3 L", "Beam 3 R", "Beam-L", "Beam-R", "Pad X", "Pad Y", "Knob 1", "Knob 2" };
    return k[i];
}
