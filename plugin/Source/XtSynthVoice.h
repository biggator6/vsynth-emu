#pragma once
// What a voice of XtSynth keeps, and each OSC on it: moved from XtSynth.h as it stood (v189); XtSynth::Voice and OscVoice name them.
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

// What each OSC keeps on the voice (the firmware's objects at tone +0x20
// and +0x9d4, with their envelopes and LFO)
struct XtOscVoice {
    bool on = false;               // the OSC sounds this note: its Switch, and something to play
    int kfKey = -1;                // the key-follow key (+4): a change restarts the glide
    bool robot = false;            // Robot Voice, as at note-on (the player latches it)
    bool robotKnob = false;        // and its Switch alone: the pitch takes it with Vari Sw as that stands
    bool linear = false;           // Vari off on a SOLO or LITE wave, as at note-on: XtLinearPlayer plays
    // A LITE wave (type 0) is the voice's kind 1 (the converter at 0x894e2ec0): its own note-on (0x8950b5b0) and update (0x8950bbb0),
    // which take the converters' values as they are, no 70% steps.  Vari off, the linear player; on, XtLitePlayer (mode 2)
    bool lite = false;
    int32_t liteStart = 0;         // kind 1's +0xc4, the Start Offset as the note's block had it: each knob update gives it to the player again (0x8950bcc0)
    float root = 440.0f;           // the voice's root frequency (+0x9a4), as its setup leaves it at note-on (0x894f21c0)
    bool variBit = true;           // Vari Sw as the voice has it (+0xb4 bit 0x20), what its walk reads: from note-on, then each knob update's end
    bool syncBit = false;          // and Tempo Sync, as the player's byte 0 has it (bit 1), the same way
    bool analog = false;           // OSC Type ANALOG at note-on: the analog oscillator plays
    const XtEncoded* wave = nullptr;   // the OSC's wave at note-on (a new wave stops the voices)
    int rnd = 0;                   // the note's random pitch offset (+0x6c), 1/10 cent
    XtEnvelope penv, tenv, fenv;   // pitch, time and formant envelopes (curve 0)
    XtEnvelope oenv;               // the OSC amp envelope (curve 2)
    XtLfo lfo;
    XtPitchRamp porta;             // the pitch ramp (+0x880)
    bool chase = false;            // a released Time Trip fading out to go back to the beat (+0x9b0)
    int steps = 0;                 // the zone's and OSC's count of notes at this one's start (the input's +0xe0): STEP's
    // A LITE player's byte +4, the end flag the SOLO and band players keep (1 at the wave's end, −1 at its start): LITE's note-ons
    // (0x8950b8d0, 0x8950b712) neither clear nor set it, so on the XT it is what the tone slot's last SOLO or band player, or a LEGATO
    // copy, left there, and LEGATO takes no earlier player whose flag is 1 (0x894f2d24).  XtSynth's voices are not the XT's tone slots,
    // so the plugin takes 0; the check sets the firmware's own.
    int8_t staleEnd = 0;
    // the player's pitch ratio, formant ratio, walk and Energy as the
    // voice update leaves them (0x8950c1b0: 70% toward the targets per update)
    float curA = 1, curF = 1, curWalk = 1, curG = 0, curGain = 1;
    XtMbPlayer mb;
    XtSoloVoice solo;
    XtLinearPlayer lin;
    XtLitePlayer xfg;              // a LITE wave with Vari on (mode 2)
    XtAnalogOsc ana;               // kept across notes: its phase runs on
};
struct XtVoice : XtOscVoice {                  // OSC 1's in the base
    XtOscVoice osc2;
    int zone = 0;                  // the patch's zone the note plays, 0 for zone 1 (the block's +0x6c6; the tone's +0x1c, part and zone)
    XtMod mod;                     // kept across notes, as the tone's
    int structure = 0;             // the Structure Type as at note-on (the tone's +0x1770)
    bool active = false, releasing = false;
    int note = -1;                 // the key, with the Octave Shift
    int raw = -1;                  // the key as played (the tone's +0x42), for note-off
    int chainKey = -1;             // the block's +0x6c4: the key the notes it follows began on, its own if none (the allocator's, 0x894d4c04)
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
