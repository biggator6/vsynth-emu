#pragma once
// Polyphonic playback of a sample the way the V-Synth XT plays an encoded
// wave.  The sample is encoded once, as the XT does after import:
//   SOLO              the XT's SOLO encoder (XtSoloEncoder, a bit-exact port)
//   BACKING/ENSEMBLE  the firmware's multi-band encoder and load-time band
//                     rebuild, run from the user's rom.2 (XtFirmware)
// Each note then runs its own firmware player (XtSoloVoice / XtMbPlayer) at
// the XT's 44.1 kHz under the XT's amp envelope (XtEnvelope), and the mix is
// resampled to the host rate.  No JUCE.
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
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
    struct Controls {
        // the OSC's pitch (XtOsc): the XT's Coarse −48..+48 and Fine −50..+50
        // (cents); the wave's Original Key 0..127; the Pitch knob −63..+63
        // (the depth curve: ±1200 cents at ±36, ±6000 at ±63), Pitch KF
        // −200..+200 (% per octave, steps of 10; 100 plays the keyboard in
        // semitones) and Random Pitch Depth 0..30 (0…1200 cents,
        // xtRandomPitchDepth)
        int rootKey = 60;
        int oscCoarse = 0, oscFine = 0, oscPitch = 0, pitchKF = 100, randomPitch = 0;
        // the patch's tuning (XtOsc XtTuning): Coarse Tune −48..+48 and Fine
        // Tune −50..+50 (cents), with Pitch Shift (semitones, to the cent)
        // added to them; Scale Tune (the switch, and C…B −64..+63 cents);
        // Bend Range Up and Down 0..48 semitones
        float pitchSemis = 0;
        int patchCoarse = 0, patchFine = 0;
        bool scaleTune = false;
        int scale[12] = {};
        int bendUp = 2, bendDown = 2;
        // the VariPhrase OSC (XtOsc): Time and Formant −63..+63, Time Offset
        // 0 BWD / 1 ZERO / 2 FWD, Time and Formant KF −200..+200 (% per
        // octave, steps of 10), Energy 0 (OFF)..127
        int oscTime = 0, oscTimeOffset = 2, oscTimeKF = 0, oscFormant = 0, oscFormantKF = 0, energy = 0;
        // amp envelope knobs, 0..127 as on the XT (release 16 = 30 ms);
        // velocity → time sensitivities −63..+63; Time KF −200..+200
        int attack = 0, decay = 0, sustain = 127, release = 16;
        int aSens = 0, dSens = 0, rSens = 0, timeKF = 0;
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
        // to level for the OSC and for the patch TVA: Sens −63..+63, Curve
        // 0 (off), 1..7
        int oscLevel = 127, oscLevelKF = 0, oscLevelLfoDepth = 0, waveGain = 0;
        int oAttack = 0, oDecay = 0, oSustain = 127, oRelease = 127, oASens = 0, oDSens = 0, oRSens = 0, oTimeKF = 0;
        int oscVelSens = 0, oscVelCurve = 1, ampVelSens = 16, ampVelCurve = 1;
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
        // follow the input's level by their Own Env depths −63..+63) or 2
        // Dual Filter (its own parameters below); Filter
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
            CosmEnv envA, envB;
            int lfoWave = 1, lfoRate = 98, lfoOffset = 2, lfoDelay = 0, lfoFadeMode = 0, lfoFade = 0;
            bool lfoKeySync = false;
        };
        Cosm cosm[2];
    };

    XtSynth();
    ~XtSynth();
    void prepare(double hostRate);
    // Any thread: the next render() starts using this sample (voices stop).
    void setSample(std::shared_ptr<const XtEncoded> e);
    bool hasSample() const { return cur != nullptr; }

    // Audio thread
    void setControls(const Controls& c);
    void noteOn(int note, float velocity);
    void noteOff(int note, float velocity = 0.5f);
    void allNotesOff();
    // The bender, 0..16383 (8192 centre): the part's bend (xtPitchBend),
    // in every voice's pitch from the next update on.
    void pitchBend(int value);
    void render(float* left, float* right, int n);

private:
    struct Voice {
        bool active = false, releasing = false;
        int note = -1;                 // the key, with the Octave Shift
        int raw = -1;                  // the key as played (the tone's +0x42), for note-off
        int srcKey = -1;               // the key portamento glides from (the OSC's +0x7c)
        int kfKey = -1;                // the key-follow key (+4): a change restarts the glide
        bool damping = false;          // truncated (0x894f5500): fading out over the Truncate Time
        float damp = 0;                // the TVA's fade (+0x54)
        long age = 0, tail = 0;
        float gain = 1;               // the patch TVA's velocity gain (0x894ea990)
        int vel = 64, relVel = 64;     // 0..127, for the envelope's times
        bool robot = false;            // Robot Voice, as at note-on (the player latches it)
        int rnd = 0;                   // the note's random pitch offset (+0x6c), 1/10 cent
        XtEnvelope env;
        XtEnvelope penv, tenv, fenv;   // pitch, time and formant envelopes (curve 0)
        XtEnvelope oenv;               // the OSC amp envelope (curve 2)
        XtLfo lfo;
        // the COSM sections: on as at note-on; each its TVF or Dual Filter,
        // envelopes (A: the Cutoff's or Filter 1's; B: the Q's or Filter 2's)
        // and LFO
        struct CosmVoice {
            bool on = false;
            int type = 0;                      // the type as at note-on
            XtCosmTvf tvf;
            XtCosmDual dual;
            XtEnvelope envA, envB;
            XtLfo lfo;
        };
        CosmVoice cs[2];
        XtPitchRamp porta;             // the pitch ramp (+0x880): kept across notes, as the XT's voices keep it
        // the player's pitch ratio, formant ratio, walk and Energy as the
        // voice update leaves them (0x8950c1b0: 70% toward the targets per update)
        float curA = 1, curF = 1, curWalk = 1, curG = 0, curGain = 1;
        // The TVA that applies the envelope (0x894f5360 / 0x894f5720): gain,
        // its target and per-sample step, frames since the last update.
        float cur = 0, target = 0, step = 0, elapsed = 0;
        XtMbPlayer mb;
        XtSoloVoice solo;
    };
    std::shared_ptr<const XtEncoded> cur, pending, retired;
    std::mutex swapLock;
    bool swapPending = false;
    Controls ctl;
    Voice voice[kVoices];
    long clock = 0;
    // The XT's control clock: the tone update (envelope step, TVA targets)
    // runs between blocks once per 441 frames (10 ms).
    static constexpr int kTickFrames = 441;
    int tickCount = 0;                // frames since the last update
    XtLfo freeLfo;                    // the free-running LFO a key-unsynced voice follows
    XtLfo freeClfo[2];                // the same for LFO 3 and 4
    bool freeClfoOn[2] = {};
    XtKeyState keys;                  // the channel's key state (0x894d6330 / 0x894d63c0)
    long hostFrames = 0;              // the system clock (ms) the key state reads
    XtPitchRandom pitchRandom;        // the XT's one random pitch generator (from its power-on seed)
    int bender = 8192;
    bool freeLfoOn = false;
    float chaosSeed = 0.3141f;        // the global logistic map the LFOs draw their seeds from
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
    void targets(const Voice& v, float& A, float& F, float& walk, float& g) const;
    float gainTarget(const Voice& v, float env) const;   // 0x894f3dc0 / 0x894f3e20
    XtEnvParams ampEnvParams(const Voice& v) const;      // the OSC amp envelope
    XtLoop waveLoop() const;                         // the players' loop
    float noteCents(const Voice& v, int key) const;  // voice +8 at a key (+0x68 at the previous one)
    int keyFollowKey(const Voice& v) const;          // the OSC's +4
    int shiftKey(int key) const;                     // + the Octave Shift, within 0..127
    void startVoice(int raw, int key, int vel);
    void legatoNote(int key, int vel);               // the sounding voice takes a note (0x894dc1e0)
    void truncate(Voice& v);
    void applyRates(Voice& v);
    void tvaUpdate(Voice& v);
    XtEnvParams envParams(const Voice& v) const;
    XtEnvParams oscEnvParams(const Voice& v, int a, int d, int s, int r) const;   // pitch, time, formant
    XtLfoParams lfoParams() const;
    XtLfoParams clfoParams(int k) const;             // LFO 3 (COSM 1), LFO 4 (COSM 2)
    XtEnvParams cosmEnvParams(const Voice& v, const Controls::CosmEnv& e) const;
    XtTvfBlock cosmBlock(const Voice& v, int k) const;
    XtDualBlock dualBlock(const Voice& v, int k) const;
    void cosmSetBlock(Voice& v, int k);              // a new block for a sounding section (0x894ed8f0)
    float drawSeed();
};
