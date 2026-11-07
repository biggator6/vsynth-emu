#include "PluginProcessor.h"
#include <array>
#include <map>
#include "PluginEditor.h"
#include "XtPatch.h"
#include "XtVpwFormat.h"

// ─── Parameter IDs ───────────────────────────────────────────────────────────
// Start Offset's parameter holds 0 … 2^24 − 1 samples, exact in a float.  The converter compares it unsigned (0x894e4448), so a word with its top bit set
// is past any wave, as the largest is.
constexpr int kStartOffsetMax = (1 << 24) - 1;
static int startOffsetParam(int v) { return v < 0 ? kStartOffsetMax : std::min(v, kStartOffsetMax); }

namespace Params {
    static const juce::String kPitchShift   = "pitchShift";
    static const juce::String kTimeStretch  = "timeStretch";
    static const juce::String kFormantShift = "formantShift";
    static const juce::String kRobot        = "robot";         // the engine's robot mode; with the XT, Robot Voice
    static const juce::String kEncodeType   = "encodeType";    // Auto, SOLO, BACKING, ENSEMBLE, LITE (last, so that saved sessions keep their choice)
    static const juce::String kEncodeDepth  = "encodeDepth";   // multi-band attack detection
    static const juce::String kRootKey      = "rootKey";       // note that plays the sample unshifted
    static const juce::String kVari         = "vari";          // Vari Sw: off, the wave's time follows its pitch (XtLinearPlayer, or a band player's walk)
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
    static const juce::String kStartOffset   = "startOffset";   // Start Offset in samples, the XT's unit: where a note starts in the wave
    static const juce::String kPlaybackMode  = "playbackMode";  // Playback Mode: RETRIGGER, LEGATO, STEP, EVENT (IDs 0x2d2, 0x306)
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
    static const juce::String kFxOn         = "fxOn";
    // The patch TVA's other sends, and where the tone goes
    static const juce::String kTvaChoSend   = "tvaChoSend";
    static const juce::String kTvaRevSend   = "tvaRevSend";
    static const juce::String kOutputAssign = "outputAssign";
    static const juce::String kOutputMode   = "outputMode";
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
    // A wave's own loop and tempo hold more than a box can show
    const auto twoPlaces = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(v, 2); });
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

    return { params.begin(), params.end() };
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────

VSynthEmuProcessor::VSynthEmuProcessor()
    // Instrument: stereo output only, no sidechain input
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "VSynthEmu", createParameterLayout())
{
    for (auto& e : fxEdits_) e.store(kFxNoEdit, std::memory_order_relaxed);
    formatManager_.registerBasicFormats();   // WAV, AIFF, OGG, FLAC
    formatManager_.registerFormat(new XtVpwFormat(), false);

    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth, Params::oscId(1, Params::kEncodeType), Params::oscId(1, Params::kEncodeDepth) })
        apvts.addParameterListener(id, this);
    findRom();
}

// The control thread: the driver is quick, a hundred times a second is plenty, and nothing it does holds up the audio.
class VSynthEmuProcessor::FxThread : public juce::Thread {
public:
    explicit FxThread(VSynthEmuProcessor& p) : juce::Thread("XT effects"), owner(p) {}
    void run() override { while (!threadShouldExit()) { owner.fxTick(); wait(10); } }
private:
    VSynthEmuProcessor& owner;
};

void VSynthEmuProcessor::fxTick() {
    const std::lock_guard<std::mutex> lock(fxLock_);
    if (apvts.getRawParameterValue(Params::kFxOn)->load() < 0.5f && !fxUnit_) return;
    if (!fxUnit_) {
        if (romFile_.isEmpty()) return;
        auto u = std::make_unique<XtFxUnit>(romFile_.toRawUTF8());
        fxApply(u->control());
        u->control().loadPatch();
        fxPatchPending_ = false;
        u->settle();
        if (!u->ok()) { DBG("effects: " << u->control().error()); romFile_.clear(); return; }
        fxUnit_ = std::move(u);
        synth_.setBusSink(fxUnit_.get());
        fxRunning_ = true;
    }
    XtFxControl& c = fxUnit_->control();
    fxApply(c);
    if (fxPatchPending_.exchange(false)) c.wantPatch();
    c.tick();
    if (apvts.getRawParameterValue(Params::kFxOn)->load() >= 0.5f) {    // nothing's taking the words otherwise
        c.setMatrix(synth_.fxMatrix(0), synth_.fxMatrix(1), synth_.fxMatrix(2));
        c.outsideClock(synth_.fxClockSix());
    }
    c.pump();
}

// The plugin's effect parameters, each the firmware's ID.
static const struct { const char* name; int id; } kFxKnobs[] = {
    { "fxMfxOn", 0x120 }, { "fxChoOn", 0x121 }, { "fxRevOn", 0x122 },
    { "fxMfxType", 0x177 }, { "fxMfxMaster", 0x178 }, { "fxMfxCho", 0x179 }, { "fxMfxRev", 0x17a },
    { "fxChoType", 0x1fc }, { "fxChoMaster", 0x1fd }, { "fxChoRev", 0x1fe },
    { "fxRevType", 0x230 }, { "fxRevMaster", 0x231 },
    { "fxMaster", 0x3e }, { "fxGain", 0x76 }
};

void VSynthEmuProcessor::fxManualTicks() {
    fxManual_ = true;
    if (fxThread_) fxThread_->stopThread(2000);
    fxThread_.reset();
}

int VSynthEmuProcessor::fxKnobId(const juce::String& parameter) {
    for (const auto& k : kFxKnobs) if (parameter == k.name) return k.id;
    return -1;
}

// All of it to the driver's store, which only takes what's changed.
void VSynthEmuProcessor::fxApply(XtFxControl& c) {
    for (const auto& k : kFxKnobs) c.set(k.id, (int)std::lround(apvts.getRawParameterValue(k.name)->load()));
    c.set(0x11d, synth_.fxPatchTempo());
    c.set(0x41, synth_.fxClockSix() ? 1 : 0);
    for (int id = 0; id < kFxIds; ++id) { const int v = fxEdits_[id].load(std::memory_order_relaxed); if (v != kFxNoEdit) c.set(id, v); }
}

void VSynthEmuProcessor::setFxParam(int id, int value) { if (id >= 0 && id < kFxIds) fxEdits_[id].store(value, std::memory_order_relaxed); }

int VSynthEmuProcessor::fxParam(int id, int fallback) const {
    const int v = id >= 0 && id < kFxIds ? fxEdits_[id].load(std::memory_order_relaxed) : kFxNoEdit;
    return v == kFxNoEdit ? fallback : v;
}

const XtFxCatalog& VSynthEmuProcessor::fxCatalog() {
    if (!fxCatalog_.ok) {
        juce::String rom;
        { const std::lock_guard<std::mutex> lock(fxLock_); rom = romFile_; }
        if (rom.isNotEmpty()) fxCatalog_ = xtFxCatalog(rom.toRawUTF8());
    }
    return fxCatalog_;
}

VSynthEmuProcessor::~VSynthEmuProcessor() {
    if (fxThread_) fxThread_->stopThread(2000);
    fxThread_.reset();
    for (const auto& id : { Params::kPitchShift, Params::kTimeStretch, Params::kFormantShift, Params::kRobot,
                            Params::kEncodeType, Params::kEncodeDepth, Params::oscId(1, Params::kEncodeType), Params::oscId(1, Params::kEncodeDepth) })
        apvts.removeParameterListener(id, this);
    encodePool_.removeAllJobs(true, 10000);
}

// ─── The firmware image ───────────────────────────────────────────────────────

static const char* const kRomName = "V-SynthXT.rom.2.bin";   // rom.2 as research/ghidra_scripts/unpack_firmware.py writes it
static const char* const kRomKey  = "romPath";
static const char* const kPatchFolderKey = "patchFolder";

// What belongs to the plugin and not to a project, whichever format is running: ~/Library/Application Support/V-Synth Emu,
// or VSE_APP_DIR, so that PluginTest leaves the user's alone.
static juce::File appDir() {
    if (const char* d = std::getenv("VSE_APP_DIR")) return juce::File(juce::String::fromUTF8(d));
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/V-Synth Emu");
}

static std::unique_ptr<juce::PropertiesFile> appSettings() {
    juce::PropertiesFile::Options o;
    o.millisecondsBeforeSaving = -1;   // only when told
    return std::make_unique<juce::PropertiesFile>(appDir().getChildFile("settings.xml"), o);
}

// The tables belong to the process and every instance plays from them, so an image already in memory isn't read again
// under a voice that's sounding, and one that won't load leaves the last in place.
static bool loadRom(const juce::String& path) {
    static std::mutex lock;
    static juce::String loaded;
    const std::lock_guard<std::mutex> hold(lock);
    if (path == loaded && xtSynthRomReady()) return true;
    if (xtSynthSetRom(path.toStdString())) { loaded = path; return true; }
    if (loaded.isNotEmpty()) xtSynthSetRom(loaded.toStdString());
    return false;
}

// A new instance's search, first to last: VSE_XT_ROM; beside the plugin, never in its bundle, where an added file breaks
// the signature; the plugin's own folder; the image last chosen by hand, which is how an AU, a VST3 and a standalone, each
// in a folder of its own, share one.
void VSynthEmuProcessor::findRom() {
    juce::StringArray cand;
    if (const char* p = std::getenv("VSE_XT_ROM")) cand.add(juce::String::fromUTF8(p));
    cand.add(juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile(kRomName).getFullPathName());
    cand.add(appDir().getChildFile(kRomName).getFullPathName());
    cand.add(appSettings()->getValue(kRomKey));
    for (const auto& c : cand)
        if (c.isNotEmpty() && juce::File(c).existsAsFile() && openRom(juce::File(c))) return;
    setStatus("No ROM: choose V-SynthXT.rom.2.bin, or put it next to the plugin");
}

bool VSynthEmuProcessor::openRom(const juce::File& file) {
    if (!file.existsAsFile() || !loadRom(file.getFullPathName())) {
        setStatus("Not a usable rom.2 image: " + file.getFileName());
        return false;
    }
    apvts.state.setProperty(kRomKey, file.getFullPathName(), nullptr);
    { const std::lock_guard<std::mutex> lock(fxLock_); romFile_ = file.getFullPathName(); }
    setStatus("ROM ready");
    reencode(0);
    reencode(1);
    return true;
}

bool VSynthEmuProcessor::setRomFile(const juce::File& file) {
    if (!openRom(file)) return false;
    appDir().createDirectory();
    auto settings = appSettings();
    settings->setValue(kRomKey, file.getFullPathName());
    settings->save();
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
    if (!fxThread_ && !fxManual_) { fxThread_ = std::make_unique<FxThread>(*this); fxThread_->startThread(); }
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
        o.vari = apvts.getRawParameterValue(oscId(1, Params::kVari))->load() > 0.5f;
        o.oscTime = knob2(Params::kOscTime);
        o.oscTimeOffset = knob2(Params::kOscTimeOffset);
        o.oscTimeKF = kf2(Params::kOscTimeKF);
        o.oscFormant = knob2(Params::kOscFormant);
        o.oscFormantKF = kf2(Params::kOscFormantKF);
        o.tempoSync = apvts.getRawParameterValue(oscId(1, Params::kTempoSync))->load() >= 0.5f;
        o.origTempo = (int)std::lround(apvts.getRawParameterValue(oscId(1, Params::kOrigTempo))->load() * 10000.0f);
        o.timeTrip = apvts.getRawParameterValue(oscId(1, Params::kTripSw))->load() >= 0.5f;
        o.beatKeep = apvts.getRawParameterValue(oscId(1, Params::kBeatKeep))->load() >= 0.5f;
        o.loop = apvts.getRawParameterValue(oscId(1, Params::kLoop))->load() >= 0.5f;
        o.loopMode = knob2(Params::kLoopMode);
        o.loopStart = apvts.getRawParameterValue(oscId(1, Params::kLoopStart))->load() / 100.0f;
        o.loopEnd = apvts.getRawParameterValue(oscId(1, Params::kLoopEnd))->load() / 100.0f;
        o.startOffset = knob2(Params::kStartOffset);
        o.playbackMode = knob2(Params::kPlaybackMode);
    }
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
    return c;
}

// The other way round from xtControls: each parameter to what these controls hold, as a host's automation would, and only those that have moved.
void VSynthEmuProcessor::setControls(const XtSynth::Controls& c) {
    const auto set = [this](const juce::String& id, float v) {
        auto* p = apvts.getParameter(id);
        if (!p) { jassertfalse; return; }
        const float n = p->convertTo0to1(v);
        if (p->getValue() != n) p->setValueNotifyingHost(n);
    };
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
    {   // OSC 2's own knobs
        const XtSynth::OscControls& o = c.osc2;
        using Params::oscId;
        set(oscId(1, Params::kOscSwitch), o.on ? 1 : 0);
        set(oscId(1, Params::kOscType), o.oscType);
        set(oscId(1, Params::kAnaWave), o.ana.wave);
        set(oscId(1, Params::kAnaImpact), o.ana.impact);
        set(oscId(1, Params::kAnaPw), o.ana.pw);
        set(oscId(1, Params::kAnaPwKF), o.ana.pwKf * 10);
        set(oscId(1, Params::kAnaPwLfo), o.ana.pwLfo);
        set(oscId(1, Params::kAnaPwEnv), o.ana.pwEnv);
        set(oscId(1, Params::kAnaFat), o.ana.fat);
        set(oscId(1, Params::kAnaFatKF), o.ana.fatKf * 10);
        set(oscId(1, Params::kAnaFatLfo), o.ana.fatLfo);
        set(oscId(1, Params::kAnaFatEnv), o.ana.fatEnv);
        set(oscId(1, Params::kAnaDetune), o.ana.detune);
        set(oscId(1, Params::kAnaSubOct), o.ana.subOctave);
        set(oscId(1, Params::kAnaSubLevel), o.ana.subLevel);
        set(oscId(1, Params::kOscCoarse), o.oscCoarse);
        set(oscId(1, Params::kOscFine), o.oscFine);
        set(oscId(1, Params::kOscPitch), o.oscPitch);
        set(oscId(1, Params::kPitchKF), o.pitchKF);
        set(oscId(1, Params::kRandomPitch), o.randomPitch);
        set(oscId(1, Params::kPAttack), o.pAttack);
        set(oscId(1, Params::kPDecay), o.pDecay);
        set(oscId(1, Params::kPSustain), o.pSustain);
        set(oscId(1, Params::kPRelease), o.pRelease);
        set(oscId(1, Params::kPDepth), o.pDepth);
        set(oscId(1, Params::kTAttack), o.tAttack);
        set(oscId(1, Params::kTDecay), o.tDecay);
        set(oscId(1, Params::kTSustain), o.tSustain);
        set(oscId(1, Params::kTRelease), o.tRelease);
        set(oscId(1, Params::kTDepth), o.tDepth);
        set(oscId(1, Params::kFAttack), o.fAttack);
        set(oscId(1, Params::kFDecay), o.fDecay);
        set(oscId(1, Params::kFSustain), o.fSustain);
        set(oscId(1, Params::kFRelease), o.fRelease);
        set(oscId(1, Params::kFDepth), o.fDepth);
        set(oscId(1, Params::kLfoWave), o.lfoWave);
        set(oscId(1, Params::kLfoRate), o.lfoRate);
        set(oscId(1, Params::kLfoDepth), o.lfoPitchDepth);
        set(oscId(1, Params::kLfoTimeDepth), o.lfoTimeDepth);
        set(oscId(1, Params::kLfoFmtDepth), o.lfoFormantDepth);
        set(oscId(1, Params::kLfoDelay), o.lfoDelay);
        set(oscId(1, Params::kLfoFadeMode), o.lfoFadeMode);
        set(oscId(1, Params::kLfoFade), o.lfoFade);
        set(oscId(1, Params::kLfoOffset), o.lfoOffset);
        set(oscId(1, Params::kLfoKeySync), o.lfoKeySync ? 1 : 0);
        set(oscId(1, Params::kOscLevel), o.oscLevel);
        set(oscId(1, Params::kOscLevelKF), o.oscLevelKF);
        set(oscId(1, Params::kOscLevelLfo), o.oscLevelLfoDepth);
        set(oscId(1, Params::kWaveGain), o.waveGain);
        set(oscId(1, Params::kOAttack), o.oAttack);
        set(oscId(1, Params::kODecay), o.oDecay);
        set(oscId(1, Params::kOSustain), o.oSustain);
        set(oscId(1, Params::kORelease), o.oRelease);
        set(oscId(1, Params::kOASens), o.oASens);
        set(oscId(1, Params::kODSens), o.oDSens);
        set(oscId(1, Params::kORSens), o.oRSens);
        set(oscId(1, Params::kOTimeKF), o.oTimeKF);
        set(oscId(1, Params::kOscVelSens), o.oscVelSens);
        set(oscId(1, Params::kOscVelCurve), o.oscVelCurve);
        set(oscId(1, Params::kEnergy), o.energy);
        set(oscId(1, Params::kRootKey), o.rootKey);
        set(oscId(1, Params::kRobot), o.robot ? 1 : 0);
        set(oscId(1, Params::kVari), o.vari ? 1 : 0);
        set(oscId(1, Params::kOscTime), o.oscTime);
        set(oscId(1, Params::kOscTimeOffset), o.oscTimeOffset);
        set(oscId(1, Params::kOscTimeKF), o.oscTimeKF);
        set(oscId(1, Params::kOscFormant), o.oscFormant);
        set(oscId(1, Params::kOscFormantKF), o.oscFormantKF);
        set(oscId(1, Params::kTempoSync), o.tempoSync ? 1 : 0);
        
        set(oscId(1, Params::kTripSw), o.timeTrip ? 1 : 0);
        set(oscId(1, Params::kBeatKeep), o.beatKeep ? 1 : 0);
        set(oscId(1, Params::kLoop), o.loop ? 1 : 0);
        set(oscId(1, Params::kLoopMode), o.loopMode);
        set(oscId(1, Params::kPlaybackMode), o.playbackMode);
    }
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
        synth_.useBusSink(apvts.getRawParameterValue(Params::kFxOn)->load() >= 0.5f);
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
    buffer.applyGain(0.125f);   // at the level the XT's voices play (XtSynth::busOut), so losing the image isn't 18 dB louder
}

// ─── File Loading ─────────────────────────────────────────────────────────────

// A V-Synth wave by its first chunks: a .vpw, or a project's .wav, the same chunks in a RIFF.  A LITE wave in a project is a RIFF with no
// vpfl, its vpsc after the fmt chunk.
static bool isVSynthWave(const juce::File& file) {
    juce::FileInputStream in(file);
    char head[64] = {};
    const int n = in.openedOk() ? in.read(head, (int)sizeof head) : 0;
    if (n >= 4 && !std::memcmp(head, "vpfl", 4)) return true;
    if (n < 12 || std::memcmp(head, "RIFF", 4) || std::memcmp(head + 8, "WAVE", 4)) return false;
    for (int i = 12; i + 4 <= n; ++i) if (!std::memcmp(head + i, "vpfl", 4) || !std::memcmp(head + i, "vpsc", 4)) return true;
    return false;
}

static bool readVSynthWave(const juce::File& file, XtWaveFile& wave, std::string* why = nullptr) {
    juce::MemoryBlock bytes;
    return file.loadFileAsData(bytes) && xtWaveRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), wave, why);
}

// The wave's own settings to OSC `osc`'s parameters.
void VSynthEmuProcessor::waveToParameters(const XtWaveFile& wave, int osc) {
    XtSynth::Controls c = xtControls();
    xtWaveOntoControls(wave, osc ? c.osc2 : static_cast<XtSynth::OscControls&>(c));
    const XtSynth::OscControls& o = osc ? c.osc2 : c;
    const auto set = [this, osc](const juce::String& id, float v) {
        auto* p = apvts.getParameter(Params::oscId(osc, id));
        if (p) p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    set(Params::kRootKey, (float)o.rootKey);
    set(Params::kOrigTempo, (float)o.origTempo / 10000.0f);
    set(Params::kLoopStart, o.loopStart * 100.0f);
    set(Params::kLoopEnd, o.loopEnd * 100.0f);
    set(Params::kLoopMode, (float)o.loopMode);
    set(Params::kLoop, o.loop ? 1.0f : 0.0f);
}

// A V-Synth wave's audio into OSC `osc`.  What the wave says of itself goes with it to each encode.
void VSynthEmuProcessor::takeWave(const XtWaveFile& wave, const juce::File& file, int osc) {
    juce::AudioBuffer<float> audio(wave.channels, (int)wave.frames);
    for (int c = 0; c < wave.channels; ++c) {
        float* d = audio.getWritePointer(c);
        for (long i = 0; i < wave.frames; ++i) d[i] = (float)wave.pcm[(size_t)(i * wave.channels + c)] / 32768.0f;
    }
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        LoadedWave& w = wave_[osc];
        w.buffer = std::move(audio);
        w.rate = 44100.0;
        w.bits = 16;
        w.ownType = wave.encodeType;
        w.own = wave;
        w.own.pcm.clear();
        w.isOwn = true;
        w.loaded.store(true);
    }
    if (osc == 0) { playbackPos_ = -1; activeVoices_ = 0; }
    wave_[osc].name = file.getFileName();
    apvts.state.setProperty(osc ? "loadedFilePath2" : "loadedFilePath", file.getFullPathName(), nullptr);
    reencode(osc);
}

void VSynthEmuProcessor::unloadWave(int osc) {
    LoadedWave& w = wave_[osc];
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        w.loaded.store(false);
        w.buffer.setSize(0, 0);
        w.isOwn = false;
        w.ownType = -1;
    }
    ++w.gen;                       // an encode under way comes to nothing
    if (osc == 0) { playbackPos_ = -1; activeVoices_ = 0; }
    w.name.clear();
    apvts.state.setProperty(osc ? "loadedFilePath2" : "loadedFilePath", juce::String(), nullptr);
    synth_.setSample(nullptr, osc);
}

void VSynthEmuProcessor::loadFile(const juce::File& file, int osc, bool waveSettings)
{
    if (xtSynthRomReady() && isVSynthWave(file)) {
        XtWaveFile wave;
        std::string why;
        if (readVSynthWave(file, wave, &why)) {
            if (waveSettings) waveToParameters(wave, osc);
            takeWave(wave, file, osc);
            return;
        }
        setStatus(file.getFileName() + ": " + juce::String(why));
    }

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager_.createReaderFor(file));

    if (!reader)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Load Error",
            (file.hasFileExtension("vpw") && !xtSynthRomReady() ? "A .vpw needs the firmware image: " : "Could not read: ") + file.getFileName());
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
        w.ownType = reader->metadataValues.getValue(XtVpwFormat::kEncodeType, "-1").getIntValue();
        w.isOwn = false;
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

// ─── A V-Synth patch ─────────────────────────────────────────────────────────

// A project keeps its waves in Wave beside Patch, each named by its number.  A copy named "… 2.wav" sorts after its original.
static juce::File findWave(const juce::File& folder, int number) {
    if (number <= 0 || !folder.isDirectory()) return {};
    juce::Array<juce::File> found;
    folder.findChildFiles(found, juce::File::findFiles, false, juce::String(number).paddedLeft('0', 3) + " *");
    juce::File best;
    for (const auto& f : found)
        if ((f.hasFileExtension("vpw") || f.hasFileExtension("wav")) && (best == juce::File() || f.getFileName().length() < best.getFileName().length()))
            best = f;
    return best;
}

bool VSynthEmuProcessor::loadPatch(const juce::File& file, int zone) {
    if (!xtSynthRomReady()) { setStatus("A patch needs the firmware image"); return false; }
    juce::MemoryBlock bytes;
    XtPatchFile patch;
    std::string why;
    if (!file.loadFileAsData(bytes) || !xtPatchRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), patch, &why)) {
        setStatus(file.getFileName() + ": " + (why.empty() ? juce::String("can't be read") : juce::String(why)));
        return false;
    }
    appDir().createDirectory();
    auto settings = appSettings();
    settings->setValue(kPatchFolderKey, file.getParentDirectory().getFullPathName());
    settings->save();
    applyPatch(patch.name, patch.store, zone, file.getParentDirectory().getSiblingFile("Wave"));
    return true;
}

juce::File VSynthEmuProcessor::lastPatchFolder() const {
    const juce::File f(appSettings()->getValue(kPatchFolderKey));
    return f.isDirectory() ? f : juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}

void VSynthEmuProcessor::applyPatch(const std::string& name, const std::vector<uint8_t>& store, int zone, const juce::File& waveFolder) {
    XtSynth::Controls c = xtPatchControls(store, zone, xtControls());
    // Its waves first, for what they say of themselves
    juce::File files[2];
    XtWaveFile waves[2];
    bool found[2] = {};
    juce::StringArray notes;
    for (int k = 0; k < 2; ++k) {
        XtSynth::OscControls& o = k ? c.osc2 : static_cast<XtSynth::OscControls&>(c);
        const int number = xtPatchWaveNumber(store, zone, k);
        if (o.oscType != 1 || number <= 0) continue;
        files[k] = findWave(waveFolder, number);
        std::string why;
        if (files[k].existsAsFile() && readVSynthWave(files[k], waves[k], &why)) { found[k] = true; xtWaveOntoControls(waves[k], o); }
        else notes.add("no wave " + juce::String(number));
    }
    setControls(c);
    for (int k = 0; k < 2; ++k)
        if (auto* p = apvts.getParameter(Params::oscId(k, Params::kEncodeType))) p->setValueNotifyingHost(p->convertTo0to1(0.0f));   // Auto: each wave's own
    for (const auto& fx : xtPatchEffects(store)) {
        const char* knob = nullptr;
        for (const auto& k : kFxKnobs) if (k.id == fx.id) knob = k.name;
        if (!knob) { setFxParam(fx.id, fx.value); continue; }
        if (auto* p = apvts.getParameter(knob)) p->setValueNotifyingHost(p->convertTo0to1((float)fx.value));
    }
    fxPatchPending_ = true;
    for (int k = 0; k < 2; ++k) {
        if (found[k]) takeWave(waves[k], files[k], k);
        else unloadWave(k);
    }
    loadZones(store, waveFolder, notes);
    for (int z = 0; z < xtPatchZones(store); ++z)
        for (const auto& m : xtPatchMissing(store, z)) notes.addIfNotAlreadyThere(m);
    patchName_ = juce::String(name);
    patchNote_ = notes.joinIntoString(", ");
    apvts.state.setProperty("patchName", patchName_, nullptr);
    apvts.state.setProperty("patchNote", patchNote_, nullptr);
    if (!found[0] && !found[1]) setStatus("Patch: " + patchName_);
}

// Zones 2…16 (the user's decision, 2026-11-05): the patch's data, not the parameters.  Their settings go to the synth at once, analog OSCs
// and all, then again with their waves once the pool has encoded them: each wave once, as its own type, its own tuning and loop on it.
void VSynthEmuProcessor::loadZones(const std::vector<uint8_t>& store, const juce::File& waveFolder, juce::StringArray& notes) {
    const int gen = ++zonesGen_;
    const int zones = xtPatchZones(store);
    patchZones_.store(zones);
    apvts.state.setProperty("patchStore", zones > 1 ? juce::Base64::toBase64(store.data(), store.size()) : juce::String(), nullptr);
    apvts.state.setProperty("patchWaves", zones > 1 ? waveFolder.getFullPathName() : juce::String(), nullptr);
    if (zones < 2) { synth_.setZones(nullptr); zonesReady_.store(true); return; }
    notes.add(juce::String(zones) + " zones, zone 1 on the panel");
    auto set = std::make_shared<XtSynth::Zones>();
    xtPatchZoneSet(store, xtControls(), *set);
    struct Want { int zone, osc, number; };
    std::vector<Want> want;
    std::map<int, XtWaveFile> files;
    for (int z = 1; z < zones; ++z)
        for (int k = 0; k < 2; ++k) {
            XtSynth::OscControls& o = k ? set->ctl[z].osc2 : static_cast<XtSynth::OscControls&>(set->ctl[z]);
            const int number = xtPatchWaveNumber(store, z, k);
            if (o.oscType != 1 || number <= 0) continue;
            if (!files.count(number)) {
                XtWaveFile w;
                const juce::File f = findWave(waveFolder, number);
                if (!(f.existsAsFile() && readVSynthWave(f, w))) { w = XtWaveFile{}; notes.addIfNotAlreadyThere("no wave " + juce::String(number)); }
                files[number] = std::move(w);
            }
            const XtWaveFile& w = files[number];
            if (w.frames <= 0) continue;
            xtWaveOntoControls(w, o);
            want.push_back({ z, k, number });
        }
    synth_.setZones(set);
    xtActive_.store(true);
    zonesReady_.store(want.empty());
    if (want.empty()) return;
    encodePool_.addJob([this, gen, set, want = std::move(want), files = std::move(files)] {
        std::map<int, std::shared_ptr<XtEncoded>> enc;
        for (const auto& [number, w] : files) {
            if (gen != zonesGen_.load()) return;
            if (w.frames <= 0) continue;
            std::vector<std::vector<float>> chans((size_t)w.channels);
            for (long i = 0; i < w.frames; ++i)
                for (int c = 0; c < w.channels; ++c) chans[(size_t)c].push_back((float)w.pcm[(size_t)(i * w.channels + c)] / 32768.0f);
            const int type = w.encodeType == 0 || w.encodeType == 1 || w.encodeType == 3 || w.encodeType == 5 ? w.encodeType : 1;
            auto e = xtSynthEncode(chans, 44100.0, type, 64, nullptr, 16, &w.events, &w.vpdt);   // on its own vpdt, as the file has it
            if (!e) continue;
            xtWaveOntoEncoded(w, *e);
            enc[number] = std::move(e);
        }
        auto withWaves = std::make_shared<XtSynth::Zones>(*set);
        for (const Want& x : want)
            if (const auto it = enc.find(x.number); it != enc.end()) withWaves->wave[x.zone][x.osc] = it->second;
        if (gen != zonesGen_.load()) return;
        synth_.setZones(withWaves);
        zonesReady_.store(true);
    });
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
    int bits, own;
    XtWaveFile ownWave;
    bool isOwn = false;
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        own = w.ownType == 0 || w.ownType == 1 || w.ownType == 3 || w.ownType == 5 ? w.ownType : -1;
        if (w.isOwn) { ownWave = w.own; isOwn = true; }
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
    encodePool_.addJob([this, chans = std::move(chans), rate, bits, choice, depth, gen, osc, who, own, ownWave = std::move(ownWave), isOwn] {
        int type = choice == 1 ? 1 : choice == 2 ? 3 : choice == 3 ? 5 : choice == 4 ? 0 : own;   // Auto: what the wave says it is, or
        if (0 > type) {   // Auto: the engine's content classifier
            std::vector<float> mono(chans[0].size());
            for (size_t i = 0; i < mono.size(); ++i) {
                float s = 0; for (const auto& c : chans) s += c[i];
                mono[i] = s / (float)chans.size();
            }
            using CT = VSE::VariphraseAnalysis::ContentType;
            const auto a = VSE::VariphraseEngine::analyzeContent(mono.data(), (int)mono.size(), rate);
            type = a.contentType == CT::BACKING ? 3 : a.contentType == CT::ENSEMBLE ? 5 : 1;   // the classifier's LITE as SOLO: Auto doesn't choose LITE
        }
        std::string err;
        // A V-Synth SOLO wave as its own type keeps its own events (as encoded, or edited in V-Producer), which its vpno names;
        // otherwise the detection at the Encode Depth finds them (xtSynthEncode)
        auto e = xtSynthEncode(chans, rate, type, depth, &err, bits, isOwn && type == own ? &ownWave.events : nullptr, isOwn && type == own ? &ownWave.vpdt : nullptr);
        if (gen != wave_[osc].gen.load()) return;
        if (!e) { setStatus(who + "Encode failed: " + juce::String(err)); return; }
        if (isOwn) xtWaveOntoEncoded(ownWave, *e);
        synth_.setSample(e, osc);
        xtActive_.store(true);
        static const char* names[] = { "LITE", "SOLO", "", "BACKING", "", "ENSEMBLE" };
        setStatus(who + "Encoded as " + names[type] + (choice != 0 ? "" : own >= 0 ? " (the wave's own)" : " (auto)"));
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
    juce::String edits;
    for (int id = 0; id < kFxIds; ++id) { const int v = fxEdits_[id].load(std::memory_order_relaxed); if (v != kFxNoEdit) edits << id << "=" << v << ","; }
    state.setProperty("fxEdits", edits, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VSynthEmuProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;

    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    for (auto& e : fxEdits_) e.store(kFxNoEdit, std::memory_order_relaxed);
    for (const auto& pair : juce::StringArray::fromTokens(apvts.state.getProperty("fxEdits").toString(), ",", ""))
        if (pair.contains("=")) setFxParam(pair.upToFirstOccurrenceOf("=", false, false).getIntValue(), pair.fromFirstOccurrenceOf("=", false, false).getIntValue());
    fxPatchPending_ = true;

    // The firmware image and the file, if their paths were persisted
    const juce::String rom = apvts.state.getProperty("romPath").toString();
    if (rom.isNotEmpty() && !xtSynthRomReady() && juce::File(rom).existsAsFile())
        openRom(juce::File(rom));
    for (int k = 0; k < 2; ++k)
    {
        juce::String path = apvts.state.getProperty(k ? "loadedFilePath2" : "loadedFilePath").toString();
        if (path.isNotEmpty())
        {
            juce::File f(path);
            if (f.existsAsFile())
                loadFile(f, k, false);
        }
    }
    patchName_ = apvts.state.getProperty("patchName").toString();
    patchNote_ = apvts.state.getProperty("patchNote").toString();
    // A patch's zones 2…16, from the store it kept
    juce::MemoryOutputStream zoneStore;
    const juce::String kept = apvts.state.getProperty("patchStore").toString();
    if (kept.isNotEmpty() && xtSynthRomReady() && juce::Base64::convertFromBase64(zoneStore, kept)) {
        const auto* b = static_cast<const uint8_t*>(zoneStore.getData());
        const std::vector<uint8_t> store(b, b + zoneStore.getDataSize());
        juce::StringArray unused;
        loadZones(store, juce::File(apvts.state.getProperty("patchWaves").toString()), unused);
    } else {
        ++zonesGen_;
        patchZones_.store(1);
        zonesReady_.store(true);
        synth_.setZones(nullptr);
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
    static const char* const k[kAssigns] = { "assignBeam2L", "assignBeam2R", "assignBeam3L", "assignBeam3R", "assignBeam4L", "assignBeam4R", "assignPadX", "assignPadY", "assignKnob1", "assignKnob2", "assignTripX", "assignTripY" };
    return k[i];
}

const char* VSynthEmuProcessor::assignName(int i) {
    static const char* const k[kAssigns] = { "Beam 2 L", "Beam 2 R", "Beam 3 L", "Beam 3 R", "Beam-L", "Beam-R", "Pad X", "Pad Y", "Knob 1", "Knob 2", "Trip X", "Trip Y" };
    return k[i];
}

int VSynthEmuProcessor::assignDefault(int i) { return i == 6 ? 80 : i == 7 ? 81 : i == 10 ? 82 : i == 11 ? 83 : 0; }
