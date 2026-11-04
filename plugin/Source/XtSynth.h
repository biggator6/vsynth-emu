#pragma once
// Polyphonic playback of a sample the way the V-Synth XT plays an encoded
// wave.  The sample is encoded once, as the XT does after import:
//   SOLO              the XT's SOLO encoder (XtSoloEncoder, a bit-exact port)
//   BACKING/ENSEMBLE  the firmware's multi-band encoder and load-time band
//                     rebuild, run from the user's rom.2 (XtFirmware)
// Each note then runs its own firmware player (XtSoloVoice / XtMbPlayer) at
// the XT's 44.1 kHz under the XT's amp envelope (XtEnvelope), and the mix is
// resampled to the host rate.  With OSC Type ANALOG a note plays the XT's
// analog oscillator instead (XtAnalog), with or without a sample.  A note has
// two OSCs, as a tone on the XT does, each with its own wave; OSC 2 runs before
// OSC 1, and the MOD section (XtMod) combines them.  No JUCE.
#include "XtBus.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "XtAnalog.h"
#include "XtCosm.h"
#include "XtEnvelope.h"
#include "XtKeys.h"
#include "XtLfo.h"
#include "XtMultiBandPlayer.h"
#include "XtOsc.h"
#include "XtSoloVoice.h"

struct XtEncoded {
    int type = 0;                  // 1 SOLO, 3 BACKING, 5 ENSEMBLE
    int channels = 1;              // the wave's channels (1 or 2)
    long frames = 0;               // at 44.1 kHz
    std::vector<uint8_t> vpdt;     // multi-band
    XtMbBands bands;
    XtMbWave wave;
    XtSoloPlan solo;               // SOLO
};

// The user's rom.2: loads the encoders' and players' tables and opens the
// firmware for the interpreter.  False if the file is not a usable image.
bool xtSynthSetRom(const std::string& path);
bool xtSynthRomReady();

// Encodes planar float channels at `sampleRate`.  type: 1 SOLO, 3 BACKING,
// 5 ENSEMBLE; depth: Encode Depth 0..127 (multi-band attack detection).
// The sample is imported as the XT imports a WAV: to int16 (sources wider
// than 16 bits keep their top 16, as the XT's 24/32-bit readers do), then
// the XT's own converter to 44.1 kHz (xtFwImport) when the rate differs.
// `bits`: the source's bits per sample.
std::shared_ptr<XtEncoded> xtSynthEncode(const std::vector<std::vector<float>>& channels, double sampleRate,
                                         int type, int depth, std::string* err = nullptr, int bits = 16);

class XtSynth {
public:
    static constexpr int kVoices = 16;
    // What each of the two OSCs has of its own (the tone's OSC blocks at +0x434
    // and +0x508, and their envelopes and LFO: OSC 1 Env 1..4 and LFO 1, OSC 2
    // Env 5..8 and LFO 2).
    struct OscControls {
        // the OSC Switch (IDs 0x2a4, 0x2d8): off, the OSC's kind is 0 and its
        // buffer silence
        bool on = true;
        // the OSC's pitch (XtOsc): the XT's Coarse −48..+48 and Fine −50..+50
        // (cents); the wave's Original Key 0..127; the Pitch knob −63..+63
        // (the depth curve: ±1200 cents at ±36, ±6000 at ±63), Pitch KF
        // −200..+200 (% per octave, steps of 10; 100 plays the keyboard in
        // semitones) and Random Pitch Depth 0..30 (0…1200 cents,
        // xtRandomPitchDepth)
        int rootKey = 60;
        int oscCoarse = 0, oscFine = 0, oscPitch = 0, pitchKF = 100, randomPitch = 0;
        // OSC Type: 0 ANALOG, the analog oscillator (XtAnalog; it needs no
        // sample, and its pitch is against A4), 1 PCM, the OSC's loaded sample
        // (OSC 1's default in the plugin).  The analog knobs as the patch
        // stores them: Waveform 0..13 in the XT's order, Impact 0..40, Pulse
        // Width −63..+63 on the time envelope, Fat 0..127 on the formant
        // envelope, their Keyfollows ±20 (×10 = the displayed ±200) and LFO
        // and Env depths −63..+63.  X-MOD on OSC 1 uses OSC 2 up: OSC 2's
        // output moves its phase and is not heard.
        int oscType = 1;
        XtAnalogParams ana;
        // the VariPhrase OSC (XtOsc): Time and Formant −63..+63, Time Offset
        // 0 BWD / 1 ZERO / 2 FWD, Time and Formant KF −200..+200 (% per
        // octave, steps of 10), Energy 0 (OFF)..127
        int oscTime = 0, oscTimeOffset = 2, oscTimeKF = 0, oscFormant = 0, oscFormantKF = 0, energy = 0;
        // pitch envelope knobs 0..127 and its depth −63..+63 (0x894e0ce0 cents)
        int pAttack = 0, pDecay = 64, pSustain = 0, pRelease = 64, pDepth = 0;
        // time and formant envelopes, the same, with their depths −63..+63
        // (time: ±3 at ±60, ±5 at ±63; formant: d/63 octaves)
        int tAttack = 0, tDecay = 64, tSustain = 0, tRelease = 64, tDepth = 0;
        int fAttack = 0, fDecay = 64, fSustain = 0, fRelease = 64, fDepth = 0;
        // LFO: waveform 0..7 in the XT's order (SIN, TRI, SAW, SQR, RND, TRP,
        // S&H, CHAOS), rate/delay/fade 0..127, offset 0..4 (−100…+100), fade
        // mode 0..3 (ON <<, ON >>, OFF <<, OFF >>); its pitch depth −63..+63
        int lfoWave = 0, lfoRate = 64, lfoOffset = 2, lfoDelay = 0, lfoFadeMode = 0, lfoFade = 0;
        bool lfoKeySync = true;
        int lfoPitchDepth = 0, lfoTimeDepth = 0, lfoFormantDepth = 0;
        // the OSC TVA (XtOsc): Level 0..127, Level KF −200..+200, Level LFO
        // depth −63..+63, Wave Gain −12..+12 dB; its amp envelope (curve 2,
        // in series with the patch TVA) with A/D/R-Sens and Time KF; velocity
        // to level: Sens −63..+63, Curve 0 (off), 1..7
        int oscLevel = 127, oscLevelKF = 0, oscLevelLfoDepth = 0, waveGain = 0;
        int oAttack = 0, oDecay = 0, oSustain = 127, oRelease = 127, oASens = 0, oDSens = 0, oRSens = 0, oTimeKF = 0;
        int oscVelSens = 0, oscVelCurve = 1;
        // Robot Voice (SOLO): voiced grains at a fixed pitch, the note's
        // pitch against A440 instead of the Original Key
        bool robot = false;
        // Loop (all three players): the OSC's Loop switch and the wave's
        // Loop Mode (0 One Shot: no loop, 1 One Way, 2 Alternate) and loop
        // region, as fractions of the wave (end 1 = the last sample, the
        // XT's default), for forward and backward play alike
        bool loop = false;
        int loopMode = 1;
        float loopStart = 0.0f, loopEnd = 1.0f;
        // Tempo Sync (IDs 0x2d0, 0x304): a PCM OSC's walk follows the tempo clock, at the speed of
        // the Patch Tempo over the wave's Original Tempo (its header's +0x1c, BPM × 10000)
        bool tempoSync = false;
        int origTempo = 1200000;
        // Time Trip Sw and Beat Keep (IDs 0x2d6, 0x2d7; 0x30a, 0x30b): the pad (XtSynth::timeTrip)
        // takes this OSC's walk; when it lets go, a fade and a jump to where the wave would
        // have got to, or with Beat Keep off on from where it stands.  On by default, as on the XT
        bool timeTrip = true, beatKeep = true;
        bool operator==(const OscControls&) const = default;
    };
    struct Controls : OscControls {                 // OSC 1's in the base
        OscControls osc2 = [] { OscControls o; o.on = false; o.oscType = 0; return o; }();   // off until switched on, as the plugin has always played
        // the patch's tuning (XtOsc XtTuning): Coarse Tune −48..+48 and Fine
        // Tune −50..+50 (cents), with Pitch Shift (semitones, to the cent)
        // added to them; Scale Tune (the switch, and C…B −64..+63 cents);
        // Bend Range Up and Down 0..48 semitones
        float pitchSemis = 0;
        int patchCoarse = 0, patchFine = 0;
        bool scaleTune = false;
        int scale[12] = {};
        int bendUp = 2, bendDown = 2;
        // Patch Tempo (ID 0x11d), BPM × 10, 200..2500: Clock Source INTERNAL.  It drives the clock
        // that the note-value LFO rates and Tempo Sync follow, and the note-value envelope times.
        // The firmware keeps whole BPM of it (XtSynth::clockPeriod).
        int patchTempo = 1200;
        // Clock Source (ID 0x41): 0 INTERNAL, the Patch Tempo; 1 EXTERNAL, whose clocks here come from
        // the host's transport (XtSynth::hostClock), the Patch Tempo standing in until the host tells a tempo.
        int clockSource = 0;
        // amp envelope knobs, 0..127 as on the XT (release 16 = 30 ms);
        // velocity → time sensitivities −63..+63; Time KF −200..+200.  Any envelope's Attack,
        // Decay and Release, and any LFO's Rate, take 128..144 too: a note value, 3 to 192
        // clocks of 24 to the quarter (0x897cfeb8)
        int attack = 0, decay = 0, sustain = 127, release = 16;
        int aSens = 0, dSens = 0, rSens = 0, timeKF = 0;
        // the TVA Switch (ID 0x299), off: no envelope, velocity, Level, Pan or LFO, only CC 7
        // and 11 and the MFX Send Level.  Off by default, as on the XT
        bool tvaOn = false;
        // velocity to level for the patch TVA: Sens −63..+63, Curve 0 (off), 1..7
        int ampVelSens = 16, ampVelCurve = 1;
        // the patch TVA (IDs 0x29a…0x2a0): Level 0..127, its Keyfollow −200..+200 (steps
        // of 10) and LFO Depth −63..+63; Pan 0..127 (64 the centre), its Keyfollow and LFO
        // Depth; MFX Send Level 0..127, the level to the output bus.  With the Switch off only
        // the MFX Send Level counts.  The XT's defaults
        int tvaLevel = 127, tvaLevelKF = 0, tvaLevelLfo = 0, tvaPan = 64, tvaPanKF = 0, tvaPanLfo = 0, mfxSend = 127;
        // The other sends and where the tone goes: Chorus and Reverb Send Level 0..127; Output Assign 0 MULTI (through the MFX), 1 MAIN,
        // 2 DIR.  The system's: Output Mode, true for Parallel, and Out Gain for TG in dB.  The descriptor says −24 for that, with the effect
        // chip's ×4 after it.  With no chip in the sound yet it stays 0 here, at the level the plugin has always had.
        int chorusSend = 0, reverbSend = 0, outputAssign = 0, outGainTg = 0;
        bool parallel = true;
        // LFO 5, the TVA's (IDs 0x3de…0x3e5), as the others.  The XT's defaults
        int tvaLfoWave = 1, tvaLfoRate = 98, tvaLfoOffset = 2, tvaLfoDelay = 0, tvaLfoFadeMode = 0, tvaLfoFade = 0;
        bool tvaLfoKeySync = false;
        // Portamento: the switch, Mode (0 NORMAL, 1 LEGATO: only while a key
        // is held), Type (0 RATE: Time per octave, 1 TIME), Time 0..127
        // (k²·10000/127² ms; the XT's default 20) and Time Velo Sens −63..+63
        bool portamento = false;
        int portaMode = 0, portaType = 0, portaTime = 20, portaVeloSens = 0;
        // the patch's key assign: Mono/Poly, Legato Switch (MONO: a note
        // played while a key is held takes over the sounding voice) and
        // Octave Shift −3..+3 (the XT's defaults: POLY, ON, 0)
        bool mono = false, legato = true;
        int octave = 0;
        // The COSM sections (XtCosm), COSM 1 then COSM 2 in series: the
        // switch; the type, 0 TVF, 1 Dynamic TVF (its Q and Cutoff also
        // follow the input's level by their Own Env depths −63..+63), 2
        // Dual Filter, 3 TB Filter, 4 Comb Filter, 5 or 6 the 1st or 2nd Side
        // Band Filter, 7 Resonator, 8 Frequency Shifter, 9 Lo-Fi, 10
        // Compressor, 11 Limiter, 12 Wave Shape, 13 OD/Dist, 14 Amp Simulator,
        // 15 Speaker Simulator (their own parameters below); Filter
        // Type 0..11 (LP24, BP24, HP24, NOTCH24, PEAK24,
        // LP12, BP12, HP12, NOTCH12, PEAK12, LP6, HP6); Cutoff and Resonance
        // (Q) 0..127; Cutoff Keyfollow −200..+200 (steps of 10); the Cutoff's
        // and Q's Env and LFO depths −63..+63.  Their envelopes (curve 2):
        // envA drives the Cutoff (Env 9, COSM 2 Env 11), envB the Q (Env 10,
        // 12), each with the Velocity Curve 0 (off), 1..7 and Sens −63..+63
        // that scale its depth; the LFO is LFO 3 (COSM 2 LFO 4).  The XT's
        // defaults.
        struct CosmEnv {
            int attack = 0, decay = 0, sustain = 127, release = 127;
            int velCurve = 0, velSens = 32, aSens = 0, dSens = 0, rSens = 0, timeKF = 0;
            bool operator==(const CosmEnv&) const = default;
        };
        struct Cosm {
            bool on = false;
            int type = 0, resoOwn = 0, cutoffOwn = 0;
            int filter = 5, cutoff = 127, reso = 0, kf = 0;
            int cutoffEnv = 0, cutoffLfo = 0, resoEnv = 0, resoLfo = 0;
            // the Dual Filter: Filter Type 0..2 (LPF/HPF, LPF->HPF, BPF/BPF);
            // per filter Frequency and Q 0..127, Keyfollow −200..+200 (steps
            // of 10), LFO and Env depths −63..+63; Filter 1 on envA, 2 on envB
            int dualType = 0, freq1 = 90, kf1 = 100, q1 = 108, lfo1 = 0, env1 = 0;
            int freq2 = 80, kf2 = 100, q2 = 120, lfo2 = 0, env2 = 0;
            // the TB Filter: LPF Cutoff 0..127, Keyfollow −200..+200 (steps
            // of 10), LFO and Env depths −63..+63, Q 0..127, Q Follow
            // −63..+63; HPF Cutoff, Keyfollow, LFO and Env depths; Gain
            // 0..127; the LPF on envA, the HPF on envB
            int tbCutoff = 90, tbKf = 0, tbLfo = 0, tbEnv = 0, tbQ = 100, tbQFollow = 25;
            int tbHpf = 45, tbHpfKf = 0, tbHpfLfo = 0, tbHpfEnv = 0, tbGain = 127;
            // the Comb Filter: Octave 0..4 (−2..+2), Detune −100..+100 with
            // its LFO and Env depths (envB), Tone 0..127 with its depths (envA)
            int combOctave = 3, combDetune = 0, combDetuneLfo = 0, combDetuneEnv = 0, combTone = 0, combToneLfo = 0, combToneEnv = 0;
            // the Side Band Filters, each its own set (sb[0] the 1st, sb[1]
            // the 2nd): the 1st's HPF Cutoff, the 2nd's Q, 0..127; LPF Cutoff
            // 0..127, Keyfollow −200..+200 (steps of 10); the 1st's Sub Delay
            // Detune −100..+100; Octave 0..2 (−2..0); Detune −100..+100 with
            // its LFO, Own Env and Env depths −63..+63 (envB); Width 0..127,
            // its Keyfollow and the same depths (envA)
            struct SideBand {
                int hpf = 48, q = 64, lpf = 64, lpfKf = 100, subDetune = 0, octave = 1;
                int detune = 0, detuneLfo = 0, detuneOwn = 0, detuneEnv = 0;
                int width = 16, widthKf = 0, widthLfo = 0, widthOwn = 0, widthEnv = 0;
                bool operator==(const SideBand&) const = default;
            };
            SideBand sb[2] = { {}, { .width = 32 } };
            // the Resonator: Filter Type 0..2, Size 0..127 with Keyfollow
            // −200..+200 (steps of 10), LFO and Env depths −63..+63 (envA);
            // Balance 0..100 with its depths (envB)
            int resType = 0, resSize = 64, resSizeKf = 0, resSizeLfo = 0, resSizeEnv = 0, resBalance = 50, resBalanceLfo = 0, resBalanceEnv = 0;
            // the Frequency Shifter: Effect Depth 0..127 with Keyfollow
            // −200..+200 (steps of 10), LFO and Env depths −63..+63 (envA);
            // Dry/Effect Balance 0..100 with its depths (envB)
            int fsDepth = 80, fsDepthKf = 0, fsDepthLfo = 0, fsDepthEnv = 0, fsBalance = 50, fsBalanceLfo = 0, fsBalanceEnv = 0;
            // Lo-Fi: Sample Bit Down 0..15, Sample Rate Down 0..5; Filter
            // 0..127 with Keyfollow −200..+200 (steps of 10), LFO and Env depths
            // −63..+63 (envA); Dry/Effect Balance 0..100 with its depths (envB)
            int lofiBit = 10, lofiRate = 4, lofiFilter = 64, lofiFilterKf = 0, lofiFilterLfo = 0, lofiFilterEnv = 0;
            int lofiBalance = 50, lofiBalanceLfo = 0, lofiBalanceEnv = 0;
            // the Compressor: Attack, Sustain and Output Level 0..127 (no
            // envelopes or LFO)
            int compAttack = 64, compSustain = 64, compLevel = 115;
            // the Limiter: Threshold Level, Attack, Release and Output Level
            // 0..127, Ratio 0..3 (1.5:1, 2:1, 4:1, 100:1; no envelopes or LFO)
            int limThreshold = 64, limRatio = 1, limAttack = 64, limRelease = 64, limLevel = 115;
            // Wave Shape: Type 0..5 (1..6), Output Level 0..127; Drive 0..127
            // with its LFO and Env depths −63..+63 (envA); Tone 0..127 with
            // Keyfollow −200..+200 (steps of 10) and its depths (envB)
            int wsType = 0, wsLevel = 127, wsDrive = 127, wsDriveLfo = 0, wsDriveEnv = 0, wsTone = 127, wsToneKf = 0, wsToneLfo = 0, wsToneEnv = 0;
            // OD/Dist: Type 0 OD, 1 DS (latched at note-on), and the rest as
            // Wave Shape's
            int odType = 0, odLevel = 127, odDrive = 127, odDriveLfo = 0, odDriveEnv = 0, odTone = 127, odToneKf = 0, odToneLfo = 0, odToneEnv = 0;
            // the Amp Simulator: Amp Type 0..2 (1..3); Bass, Middle, Treble
            // and Output Level 0..127; Gain 0..127 with its LFO and Env
            // depths −63..+63 (envA)
            int ampType = 0, ampBass = 64, ampMiddle = 64, ampTreble = 64, ampLevel = 127, ampGain = 0, ampGainLfo = 0, ampGainEnv = 0;
            // the Speaker Simulator: Speaker Type 0..11 (1..12)
            int spkType = 0;
            CosmEnv envA, envB;
            int lfoWave = 1, lfoRate = 98, lfoOffset = 2, lfoDelay = 0, lfoFadeMode = 0, lfoFade = 0;
            bool lfoKeySync = false;
            bool operator==(const Cosm&) const = default;
        };
        Cosm cosm[2];
        // The MOD section (XtMod): Switch, Type 0 MIX, 1 RING, 2 FM, 3 ENV RING, 4
        // OSC SYNC, Original Level, Attack and Release Times, between the OSCs and
        // COSM 1.  Off, or MIX, is the sum.
        XtModParams mod;
        // Structure Type (ID 0x293): 0…2, Type1…Type3 on the panel (xtToneOrder).
        int structure = 0;
        // The Matrix Controls (IDs 0x14d…0x174, xtMatrixSums).
        XtMatrixControl matrix[8];
        // The system's controller assigns, a CC number or 0 for OFF: Beam 2 L, R and Beam 3 L, R (IDs 0x89…0x8c,
        // the part's bytes at +0x180), then what the sources BEAM-L, BEAM-R, PAD-X and -Y, KNOB1, KNOB2 read
        // (IDs 0x8d, 0x8e, 0x8f, 0x90, 0x93, 0x94).  OFF by default, as the XT's descriptors have them.  Last, Time Trip
        // Assign X and Y (IDs 0x91, 0x92), the controllers that move the pad (0x894d8360): CC82 and CC83 by default, as
        // Roland's chart has them.  The descriptors say OFF, but nothing else sets the pad, so the XT's own would not work.
        int assign[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 82, 83 };
        bool operator==(const Controls&) const = default;
    };

    XtSynth();
    ~XtSynth();
    void prepare(double hostRate);
    // Any thread: the next render() starts using this sample on OSC `osc` (0
    // OSC 1, 1 OSC 2; voices stop).  The XT binds a wave per OSC (IDs 0x2be, 0x2f2).
    void setSample(std::shared_ptr<const XtEncoded> e, int osc = 0);
    bool hasSample(int osc = 0) const { return cur[osc] != nullptr; }
    // Audio thread: whether a note would sound (a sample, or an analog
    // waveform the port plays).
    bool canPlay() const;

    // Audio thread
    void setControls(const Controls& c);
    void noteOn(int note, float velocity);
    void noteOff(int note, float velocity = 0.5f);
    void allNotesOff();
    // The bender, 0..16383 (8192 centre): the part's bend (xtPitchBend),
    // in every voice's pitch from the next update on.
    void pitchBend(int value);
    // Channel and poly pressure, 0..127 (C + 2, C + 4 + key): the sources' AFT, and an update as above.
    void channelPressure(int value);
    void polyPressure(int key, int value);
    // A MIDI controller (0x89474bf4, then the handler by number, 0x894d6f10): the channel keeps
    // numbers below 96, and all but the mode messages then update the sounding voices, as any knob
    // does.  CC 7, 11 (level), 10 (pan), 64 Hold and 66 Sostenuto (the keys' release, 0x894d58b0),
    // 121 Reset All Controllers; 120 All Sound Off truncates; 123…127 release every key.
    void controller(int cc, int value);
    // The Time Trip pad, as its two controllers (XtTripPad): x and y 1..127 a finger on it, 0 lifted.
    void timeTrip(int x, int y);
    // The pad as one plain control: 0 lifted, 1..127 a finger going clockwise round its rim, one turn
    // over the range, which is forward: a turn is 48 steps, two quarter notes of the wave's Original Tempo.
    void timeTripKnob(int value);
    // The host's transport, before each host block is rendered, for Clock Source EXTERNAL.  `bpm` above
    // 0 when it tells one; `moving` when it is running and `ppq`, in quarter notes, is where this block
    // starts.  A clock falls on each 24th of a quarter it passes.  When it isn't moving, the clocks run on
    // at the tempo, so that a note played on a stopped transport has them too.
    void hostClock(double bpm, bool moving, double ppq);
    void render(float* left, float* right, int n);

private:
    // What each OSC keeps on the voice (the firmware's objects at tone +0x20
    // and +0x9d4, with their envelopes and LFO)
    struct OscVoice {
        bool on = false;               // the OSC sounds this note: its Switch, and something to play
        int kfKey = -1;                // the key-follow key (+4): a change restarts the glide
        bool robot = false;            // Robot Voice, as at note-on (the player latches it)
        bool analog = false;           // OSC Type ANALOG at note-on: the analog oscillator plays
        const XtEncoded* wave = nullptr;   // the OSC's wave at note-on (a new wave stops the voices)
        int rnd = 0;                   // the note's random pitch offset (+0x6c), 1/10 cent
        XtEnvelope penv, tenv, fenv;   // pitch, time and formant envelopes (curve 0)
        XtEnvelope oenv;               // the OSC amp envelope (curve 2)
        XtLfo lfo;
        XtPitchRamp porta;             // the pitch ramp (+0x880)
        bool chase = false;            // a released Time Trip fading out to go back to the beat (+0x9b0)
        // the player's pitch ratio, formant ratio, walk and Energy as the
        // voice update leaves them (0x8950c1b0: 70% toward the targets per update)
        float curA = 1, curF = 1, curWalk = 1, curG = 0, curGain = 1;
        XtMbPlayer mb;
        XtSoloVoice solo;
        XtAnalogOsc ana;               // kept across notes: its phase runs on
    };
    struct Voice : OscVoice {                  // OSC 1's in the base
        OscVoice osc2;
        XtMod mod;                     // kept across notes, as the tone's
        int structure = 0;             // the Structure Type as at note-on (the tone's +0x1770)
        bool active = false, releasing = false;
        int note = -1;                 // the key, with the Octave Shift
        int raw = -1;                  // the key as played (the tone's +0x42), for note-off
        int srcKey = -1;               // the key portamento glides from (the OSC's +0x7c)
        bool damping = false;          // truncated (0x894f5500): fading out over the Truncate Time
        float damp = 0;                // the TVA's fade (+0x54)
        long age = 0;
        float velGain = 1;            // the patch TVA's velocity gain, latched at note-on (0x894ea990)
        XtTvaBlock tb;                // the TVA's block (0x894f50e0, 0x894f5310), the level in it
        int vel = 64, relVel = 64;     // 0..127, for the envelope's times
        // The note's +0x48: bit 0 its key is down, 1 Hold may keep it, 2 Sostenuto may.  It stops
        // when all three are clear; +0x2f is the release velocity until then.
        int keyFlags = 7, relHeld = 64;
        // The note's object M, what matrixUpdate reads and leaves: its velocity and keys, as the note started (a
        // legato leaves them); the Matrix's sums by slot; the D-Beam's controllers (+0x180…+0x183).
        int mVel = 64, mKey = 60, mRaw = 60;
        int16_t mx[kXtMatrixSlots] = {};
        int beam[4] = {};
        int tempo10 = 1200;            // the tempo at the note's start: the note-value envelope times (input +0x18)
        float blockTempo = 0.0f;       // OSC 1's block +0x60, which the LFOs start from: left as it was with OSC 1 off
        XtEnvelope env;
        XtLfo tlfo;                    // LFO 5, on the TVA's level and pan
        // the COSM sections: on as at note-on; each its TVF, Dual, TB, Comb or
        // Side Band Filter, Resonator, Frequency Shifter, Lo-Fi, Compressor,
        // Limiter, Wave Shape, OD/Dist, Amp or Speaker Simulator, envelopes
        // (A: the Cutoff's, Filter 1's, the LPF's, Tone's, Width's, Size's,
        // Depth's, the Lo-Fi Filter's or Drive's; B: the Q's, Filter 2's, the
        // HPF's, Detune's, Balance's or Wave Shape's Tone) and LFO
        struct CosmVoice : XtCosmSection {
            bool on = false;
            XtEnvelope envA, envB;
            XtLfo lfo;
            // What a new block reads (object +0x104…+0x110): the outputs as the note-on or
            // the last update took them, not the envelope's own, which a moved Sustain moves at once
            float heldA = 0, heldB = 0, heldLfo = 0;
            void hold() { heldA = envA.level(); heldB = envB.level(); heldLfo = lfo.out(); }
        };
        CosmVoice cs[2];
        // The TVA (0x894f5360 / 0x894f5720), left and right: its gain (the level times the
        // envelope, the LFO on the level and the pan), the gain's target and per-sample
        // step; frames since the last update.  The output bus's eight levels (+0x70…+0x8c),
        // each going 3% of the way to the block's in each block.
        float cur[2] = {}, target[2] = {}, step[2] = {}, elapsed = 0;
        float bus[XtBusCount] = {};
    };
    std::shared_ptr<const XtEncoded> cur[2], pending[2], retired[2];   // each OSC's wave
    std::mutex swapLock;
    bool swapPending[2] = {};
    Controls ctl;
    Voice voice[kVoices];
    long clock = 0;
    // The XT's control clock: the tone update (envelope step, TVA targets)
    // runs between blocks once per 441 frames (10 ms).
    static constexpr int kTickFrames = 441;
    int tickCount = 0;                // frames since the last update
    XtLfoFree freeLfo, freeLfo2;      // the free-running phase a key-unsynced voice starts from (LFO 1, LFO 2)
    XtLfoFree freeClfo[2];            // the same for LFO 3 and 4
    XtLfoFree freeTlfo;               // and for LFO 5
    XtKeyState keys;                  // the channel's key state (0x894d6330 / 0x894d63c0)
    long hostFrames = 0;              // the system clock (ms) the key state reads
    XtPitchRandom pitchRandom;        // the XT's one random pitch generator (from its power-on seed)
    int bender = 8192;
    int cc[96] = {};                  // the channel's (C + 0x84 + number), as ccDefaults sets them
    int pressure = 0, polyPress[128] = {};   // its channel and poly pressure (C + 2, C + 4)
    int hold = 0, sost = 0;           // the part's Hold and Sostenuto (N + 0x11c, + 0x11d)
    uint16_t sostKeys[8] = {};        // the keys held when Sostenuto went down (N + 0x120)
    int lastRelease = 0;              // the last note-off's velocity (N + 0xa)
    XtTripPad pad;                    // the part's Time Trip pad
    // The tempo clock, 24 to the quarter (0x89471030 → 0x894d4210 → 0x894f4da0), on the audio's
    // clock here: frames × 24000 against the period × 44100.  It runs with nothing sounding.
    uint64_t clockAcc = 0;
    uint32_t clockBlocks = 0, clockLast = 0;   // blocks (0x8987c7a8), at the last clock (0x8987c7a4)
    // Clock Source EXTERNAL (the same object, +0x40…+0x48).  Each clock comes from outside, and the tempo
    // the converters read is the milliseconds over the last six of them.
    bool clockExt = false;            // +0x40
    int clockSeen = -1;               // +0x41: clocks into this count, −1 before the first
    int clockSince = 0;               // +0x46: its ms
    int clockSix = 125;               // +0x48: the ms over the last six
    uint32_t clockMsAcc = 0;          // the 1 ms timer (0x89471030), on the audio's clock: frames × 1000 against 44100
    // What stands in for the MIDI clock's F8s: the host's transport, in clocks
    double hostBpm = 0.0;             // the last it told; 0 before any
    double hostAt = 0.0;              // the first frame of the next block
    int64_t hostDone = 0;             // the last whole clock given
    float chaosSeed = 0x1.666666p-1f; // the global logistic map the LFOs draw their seeds from (0.7 at power-on, 0x894f4c80)
    double hostRate = 44100;

    // 44.1 kHz mix, produced in 32-frame blocks, read by the resampler
    std::vector<float> src;        // interleaved L/R
    size_t srcFrames = 0;
    double readPos = 0;            // in source frames
    double step = 1;               // 44100 / hostRate
    std::vector<float> kernel;     // polyphase windowed sinc
    int kHalf = 16, kPhases = 512;

    void pickUpSample();
    void mixBlock(float* lr);      // 32 frames
    // The whole output bus of the block just mixed, in the XT's units: 8 words a frame, MAIN L, R, the Reverb and Chorus sends, MULTI L, R, DIR L, R (0x894f5720).
    // Through the effects.  A sink goes in once, from any thread, and lives as long as this; useBusSink is the audio thread's, between blocks.  A tone can be switched between the two as it sounds.
public:
    void setBusSink(XtBusSink* s) { sink.store(s, std::memory_order_release); }
    void useBusSink(bool on) { sinkOn = on; }
    // What the effects' driver is told, for the control thread: the Patch Tempo; the outside clock's ms over six clocks, 0 on the patch's own; and the Matrix's MFX-PRM1…3 as a tone's last update left them.
    int fxPatchTempo() const { return fxTempo.load(std::memory_order_relaxed); }
    int fxClockSix() const { return fxSix.load(std::memory_order_relaxed); }
    int fxMatrix(int i) const { return fxPrm[i].load(std::memory_order_relaxed); }
private:
    std::atomic<XtBusSink*> sink { nullptr };
    std::atomic<int> fxTempo { 1200 }, fxSix { 0 };
    mutable std::atomic<int> fxPrm[3] { {0}, {0}, {0} };
    bool sinkOn = false;
    void busOut(float* lr);
    static constexpr int kFrameWords = 8;
    enum FrameWord : int { FrameMainL, FrameMainR, FrameReverb, FrameChorus, FrameMultiL, FrameMultiR, FrameDirL, FrameDirR };
    float frame[32 * kFrameWords] = {};
    int clockPeriod() const;       // the ms of a quarter (the clock object's +0x14)
    int tempo10Now() const;        // the tempo ×10 the converters read (0x894e2d20)
    float tempoNow() const;        // and as the block has it (+0x60)
    int clocksDue() const;         // clocks the next block starts with
    void clockTick();              // 0x894f4da0
    void padController(int number, int value);   // a controller to the pad, if it is one of its assigns (0x894d8360)
    void padSteps(int step);       // the pad's whole steps to the voices (0x894f4e10)
    void clockFollow();            // Clock Source to the clock's object (0x89471e30)
    void clockIn();                // an F8 (0x894712f0)
    void clockMs();                // the timer routine's count (0x894710e0, its end)
    int msDue() const;             // ms the next block starts with
    double hostStep() const;       // clocks in a block, at the host's tempo
    void oscBeat(OscVoice& s, const OscControls& o);   // a block's Tempo Sync, Time Trip and Beat Keep to the player (0x894f3ec0)
    const XtEncoded* waveOf(const OscControls& o) const { return cur[&o == &ctl ? 0 : 1].get(); }
    bool oscPlays(const OscControls& o) const;       // switched on, with something to play
    void targets(const Voice& v, const OscVoice& s, const OscControls& o, float& A, float& F, float& walk, float& g) const;
    float gainTarget(const Voice& v, const OscVoice& s, const OscControls& o, float env) const;   // 0x894f3dc0 / 0x894f3e20
    XtMatrixInputs matrixInputs(const Voice& v) const;
    bool matrixUpdate(Voice& v) const;               // the sums, as a conversion pass reads them; true if any moved
    void matrixRelease(Voice& v) const;              // what a key-off forms
    void updateAll(Voice& v);                        // every envelope's and LFO's settings, as an update gives them
    float pitchKnob(const Voice& v, const OscVoice& s, const OscControls& o) const;   // the voice's +0xc
    XtEnvParams ampEnvParams(const Voice& v, const OscControls& o) const;   // the OSC amp envelope
    XtLoop waveLoop(const OscVoice& s, const OscControls& o) const;   // the players' loop
    float noteCents(const Voice& v, const OscVoice& s, const OscControls& o, int key) const;  // voice +8 at a key (+0x68 at the previous one)
    void noteTuning(const Voice& v, const OscVoice& s, const OscControls& o, int& coarse, int& fine) const;   // the per-note setup's +0x68 and +0x70
    XtAnalogShared anaShared;                        // the analog voices' shared state
    XtModBlock modBlk;                               // the MOD's block (the tone block's +0x5dc)
    XtModShared modShared;                           // FM's delay lines; a voice's index is its tone's number
    float mixPast[3] = {};                           // the last tone's mix, its first words: they follow OSC 2's buffer on the XT
    XtAnalogBlock analogBlock(const Voice& v, const OscVoice& s, const OscControls& o) const; // the analog converter's block
    int keyFollowKey(const Voice& v, const OscControls& o) const;   // the OSC's +4
    int shiftKey(int key) const;                     // + the Octave Shift, within 0..127
    void startVoice(int raw, int key, int vel);
    void legatoNote(int key, int vel);               // the sounding voice takes a note (0x894dc1e0)
    void oscStart(Voice& v, OscVoice& s, const OscControls& o);   // the voice's OSC note-on
    void oscEnvNoteOn(Voice& v, OscVoice& s, const OscControls& o);   // its envelopes
    void oscLfoNoteOn(const Voice& v, OscVoice& s, const OscControls& o, XtLfoFree& free, const XtLfo* other, float seed);   // its LFO
    void truncate(Voice& v);
    void keyRelease(Voice& v, int velocity);         // a key let go (0x894d58b0)
    void voiceOff(Voice& v);                         // the note's release (0x894dc270)
    void keysCleared();                              // the mode messages' reset of the key state
    void ccDefaults();                               // the channel's controllers at power-on (0x89475290)
    void applyRates(OscVoice& s);
    void tvaUpdate(Voice& v);
    XtTvaBlock tvaBlock(const Voice& v) const;       // the converter's block, at the voice's key and latched velocity
    void tvaNewBlock(Voice& v) const;                // a sounding voice's new one (0x894f5310), which leaves DIR's pair as the note began
    void tvaGains(const Voice& v, float levelEnv, float* lr) const;   // a gain or target, left and right
    void newBlocks(Voice& v);                        // an update's blocks for a sounding voice (0x894dba00)
    void lfoUpdates(Voice& v);                       // and all its LFOs' (0x894f5c70)
    XtLfoParams tlfoParams(const Voice& v) const;    // LFO 5
    XtEnvParams envParams(const Voice& v) const;
    XtEnvParams oscEnvParams(const Voice& v, const OscVoice& s, int which, int a, int d, int su, int r) const;   // 0 pitch, 1 time, 2 formant
    XtLfoParams lfoParams(const Voice& v, const OscVoice& s, const OscControls& o) const;
    XtLfoParams clfoParams(const Voice& v, int k) const;   // LFO 3 (COSM 1), LFO 4 (COSM 2)
    XtEnvParams cosmEnvParams(const Voice& v, int k, const Controls::CosmEnv& e) const;
    XtTvfBlock cosmBlock(const Voice& v, int k) const;
    XtDualBlock dualBlock(const Voice& v, int k) const;
    XtTbBlock tbBlock(const Voice& v, int k) const;
    XtCombBlock combBlock(const Voice& v, int k) const;
    XtSbfBlock sbfBlock(const Voice& v, int k) const;
    XtResBlock resBlock(const Voice& v, int k) const;
    XtFsBlock fsBlock(const Voice& v, int k) const;
    XtLofiBlock lofiBlock(const Voice& v, int k) const;
    XtCompBlock compBlock(const Voice& v, int k) const;
    XtLimBlock limBlock(const Voice& v, int k) const;
    XtWsBlock wsBlock(const Voice& v, int k) const;
    XtOdBlock odBlock(const Voice& v, int k) const;
    XtAmpBlock ampBlock(const Voice& v, int k) const;
    void cosmSetBlock(Voice& v, int k);              // a new block for a sounding section (0x894ed8f0)
    float drawSeed();
#ifdef XT_TONE_CHECK
    // The whole-tone check (XtToneCheck.cpp): it reads a voice between blocks,
    // and the last voice's stages in the XT's units (int16 × 65536) here.
    friend struct XtToneProbe;
    float tapOsc1[64] = {}, tapOsc2[64] = {}, tapMix[64] = {}, tapCosm[64] = {};
#endif
};
