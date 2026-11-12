#pragma once
// XtSynth's controls, a tone's and each OSC's: moved from XtSynth.h as they stood (v189); XtSynth::Controls and OscControls name them.
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
#include "XtLinearPlayer.h"
#include "XtLitePlayer.h"
#include "XtArpeggio.h"
#include "XtMsm.h"

// What each of the two OSCs has of its own (the tone's OSC blocks at +0x434
// and +0x508, and their envelopes and LFO: OSC 1 Env 1..4 and LFO 1, OSC 2
// Env 5..8 and LFO 2).
struct XtOscControls {
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
    // Vari Sw (IDs 0x2d4, 0x308): off, a SOLO wave plays on XtLinearPlayer, its time following its pitch, and Robot Voice and the wave's
    // tuning on the walk go.  A band wave's player doesn't change (0x894f3660).
    bool vari = true;
    // pitch envelope knobs 0..127 and its depth −63..+63 (0x894e0ce0 cents)
    int pAttack = 0, pDecay = 64, pSustain = 0, pRelease = 64, pDepth = 0;
    // time and formant envelopes, the same, with their depths −63..+63
    // (time: ±3 at ±60, ±5 at ±63; formant: d/63 octaves)
    int tAttack = 0, tDecay = 64, tSustain = 0, tRelease = 64, tDepth = 0;
    int fAttack = 0, fDecay = 64, fSustain = 0, fRelease = 64, fDepth = 0;
    // Each of those three's velocity and key: Curve 0 (off), 1..7 and Sens −63..+63, whose factor scales its depth, and
    // the velocity → time sensitivities and Time KF −200..+200, as the OSC amp envelope has them.  The XT's defaults.
    struct EnvVelocity {
        int curve = 0, sens = 32, aSens = 0, dSens = 0, rSens = 0, timeKF = 0;
        bool operator==(const EnvVelocity&) const = default;
    };
    EnvVelocity pVel, tVel, fVel;
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
    // Start Offset (IDs 0x2c6, 0x2fa): where a note walking forward starts in the wave, in samples, as a patch stores it; a walk
    // backward stops there.  The converter takes it unsigned and makes it the last sample at most (0x894e4448); a note walking
    // backward still starts at the end.  Samples, as Playback Mode RETRIGGER has them (the block's +0xb4 bit 0x80).
    int startOffset = 0;
    // Playback Mode (IDs 0x2d2, 0x306): 0 RETRIGGER, 1 LEGATO, 2 STEP, 3 EVENT.  LEGATO: a note played while an earlier note of its
    // part and zone is held takes that note's player, so the wave goes on from where the other was (0x894f2d00).  STEP and EVENT
    // pick the wave's events.
    int playbackMode = 0;
    // Tempo Sync (IDs 0x2d0, 0x304): a PCM OSC's walk follows the tempo clock, at the speed of
    // the Patch Tempo over the wave's Original Tempo (its header's +0x1c, BPM × 10000)
    bool tempoSync = false;
    int origTempo = 1200000;
    // Time Trip Sw and Beat Keep (IDs 0x2d6, 0x2d7; 0x30a, 0x30b): the pad (XtSynth::timeTrip)
    // takes this OSC's walk; when it lets go, a fade and a jump to where the wave would
    // have got to, or with Beat Keep off on from where it stands.  On by default, as on the XT
    bool timeTrip = true, beatKeep = true;
    bool operator==(const XtOscControls&) const = default;
};
struct XtControls : XtOscControls {                 // OSC 1's in the base
    XtOscControls osc2 = [] { XtOscControls o; o.on = false; o.oscType = 0; return o; }();   // off until switched on, as the plugin has always played
    // the patch's tuning (XtOsc XtTuning): Coarse Tune −48..+48 and Fine
    // Tune −50..+50 (cents), with Pitch Shift (semitones, to the cent)
    // added to them; Scale Tune (the switch, and C…B −100..+100 cents);
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
    // 2 DIR.  The system's: Output Mode, true for Parallel, and Out Gain for TG in dB.  The descriptor says −24 for that.  It stays 0 here, and the
    // sixteenth is applied after the tones instead, in the frame's way into the chip and in busOut: inside them, switching the effects under a note would be a step.
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
    // (IDs 0x8d, 0x8e, 0x8f, 0x90, 0x93, 0x94).  Last, Time Trip Assign X and Y (IDs 0x91, 0x92), the controllers that
    // move the pad (0x894d8360).  The pad's two pairs, one per Pad Mode, are on Roland's chart's numbers: X-Y mode's
    // PAD-X and -Y on CC80 and CC81, Time Trip's on CC82 and CC83.  Pad Mode only picks which pair the XT's own pad sends:
    // no tone generator code reads it, so both are always live here.  The descriptors say OFF for all of these, but nothing
    // else sets the pad, so the XT's own would not work; the others stay OFF as they say.
    int assign[12] = { 0, 0, 0, 0, 0, 0, 80, 81, 0, 0, 82, 83 };
    // The patch's arpeggio (IDs 0x42e…0x437) and its pattern (0x449…0x48c, by line), which XtSynthArp.cpp gives
    // XtArpeggio when they change: not a tone's, so no list of setControls' has them (v189)
    XtArpSettings arp;
    XtArpPattern arpPattern;
    // The Multi Step Modulator (IDs 0x48d…0x4ee), part 0's, which XtSynthMsm.cpp gives XtMsm when it changes (v193)
    XtMsmSettings msm;
    bool operator==(const XtControls&) const = default;
};
