#pragma once
// The plugin's parameter IDs, which the parameters' layout, the controls each way and the patch's load name: moved here
// as they stood from PluginProcessor.cpp (v190).
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>

// ─── Parameter IDs ───────────────────────────────────────────────────────────
// Start Offset's parameter holds 0 … 2^24 − 1 samples, exact in a float.  The converter compares it unsigned (0x894e4448), so a word with its top bit set
// is past any wave, as the largest is.
constexpr int kStartOffsetMax = (1 << 24) - 1;
inline int startOffsetParam(int v) { return v < 0 ? kStartOffsetMax : std::min(v, kStartOffsetMax); }

namespace Params {
    static const juce::String kPitchShift   = "pitchShift";    // added to the patch's Coarse/Fine Tune
    // Time Stretch and Formant Shift were the legacy engine's, removed in v191: kept, unread, so a saved session or
    // automation that names them still loads
    static const juce::String kTimeStretch  = "timeStretch";
    static const juce::String kFormantShift = "formantShift";
    static const juce::String kRobot        = "robot";         // Robot Voice (SOLO)
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
    inline juce::String oscId(int k, const juce::String& id) {
        return k ? "osc2" + id.substring(0, 1).toUpperCase() + id.substring(1) : id;
    }
    // Both OSCs' encode settings: a change re-encodes that OSC's wave (the processor listens to these)
    inline std::array<juce::String, 4> encodeParamIds() {
        return { kEncodeType, kEncodeDepth, oscId(1, kEncodeType), oscId(1, kEncodeDepth) };
    }
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
    inline juce::String cosmId(int k, const char* name) { return juce::String(k ? "cosm2" : "cosm") + name; }
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
