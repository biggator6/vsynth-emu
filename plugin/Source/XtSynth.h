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
#include "XtLinearPlayer.h"
#include "XtLitePlayer.h"
#include "XtEncoded.h"
#include "XtSynthControls.h"
#include "XtSynthVoice.h"
#include "XtSynthZones.h"
#include "XtArpeggio.h"
#include "XtTempoTicks.h"

class XtSynth {
public:
    static constexpr int kVoices = 16;
    using OscControls = XtOscControls;     // XtSynthControls.h
    using Controls = XtControls;

    static constexpr int kZones = kXtZones;
    using Zones = XtZones;                 // XtSynthZones.h: a patch's zones
    // The zone a key plays, with the Octave Shift (the part's note-on, 0x894dbcb0): the first whose upper key it doesn't pass, on
    // the table 0x894794f0 makes of the upper keys, each at least the one before and the last 127.  0 for zone 1.
    static int xtZoneOf(const int (&upper)[kZones], int key);

    XtSynth();
    ~XtSynth();
    void prepare(double hostRate);
    // Any thread: the next render() starts using this sample on OSC `osc` (0
    // OSC 1, 1 OSC 2; voices stop).  The XT binds a wave per OSC (IDs 0x2be, 0x2f2).  `number`: its Wave Number, which Playback
    // Mode STEP's count starts again from 0 on a change of (0x894e5ad4); −1, a new wave.
    void setSample(std::shared_ptr<const XtEncoded> e, int osc = 0, int number = -1);
    bool hasSample(int osc = 0) const { return cur[osc] != nullptr; }
    // Any thread, the same way: a patch's zones 2…16 (voices stop).  nullptr: zone 1 alone, every key on it.
    void setZones(std::shared_ptr<const Zones> z);
    // Audio thread: whether a note on zone 1 would sound (a sample, or an analog
    // waveform the port plays).
    bool canPlay() const { return canPlay(0); }

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
    // Any thread: a patch came (VSynthEmuProcessor::loadPatch), so the Multi Step Modulator stops and reads it at the next
    // setControls (0x894d9950), where an edit of its section reads it without stopping it (0x894d9b20)
    void msmPatch() { msmPatchDue.store(true, std::memory_order_relaxed); }
    void render(float* left, float* right, int n);

private:
    using OscVoice = XtOscVoice;           // XtSynthVoice.h
    using Voice = XtVoice;
    std::shared_ptr<const XtEncoded> cur[2], pending[2], retired[2];   // each OSC's wave
    int curNumber[2] = { -1, -1 }, pendingNumber[2] = { -1, -1 };     // and its Wave Number
    // Each zone's and OSC's count of notes started (0x89cf7210: part × 64 + zone × 4 + OSC × 2), every note-on of the zone, both
    // OSCs, whatever their mode (0x894e4940); from 0 when the OSC's Wave Number changes (0x894e5ad4).  STEP's event is it modulo the count
    uint16_t stepCount[kZones][2] = {};
    int zoneNumber[kZones][2] = {};
    std::shared_ptr<const Zones> zones, pendingZones, retiredZones;     // zones 2…16
    std::mutex swapLock;
    bool swapPending[2] = {}, zonesPending = false;
    Controls ctl;
    // Each zone's controls as its notes read them: its own blocks, and the rest zone 1's (zoneCtl[0] unused: zone 1 is ctl)
    Controls zoneCtl[kZones];
    Controls zoneWas[kZones];                        // setControls': each sounding zone's before the change
    int zoneUpper[kZones] = { 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
    const Controls& zc(int z) const { return z ? zoneCtl[z] : ctl; }
    const Controls& zc(const Voice& v) const { return zc(v.zone); }
    Controls zoneControls(int z) const;              // zone z's own blocks on ctl's
    void zonesFollow();                              // each zone's controls again, after ctl or the zones moved
    Voice voice[kVoices];
    long clock = 0;
    // The XT's control clock: the tone update (envelope step, TVA targets)
    // runs between blocks once per 441 frames (10 ms).
    static constexpr int kTickFrames = 441;
    int tickCount = 0;                // frames since the last update
    // The free-running phase a key-unsynced voice starts from, by zone (the pool's objects are by the tone's +0x1c, part and zone,
    // 0x894f1590): LFO 1, LFO 2, LFO 3 and 4, LFO 5.  A zone's first takes an object no note has used, which runs at 1 Hz from power-on.
    XtLfoFree freeLfo[kZones], freeLfo2[kZones];
    XtLfoFree freeClfo[kZones][2];
    XtLfoFree freeTlfo[kZones];
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
    // The arpeggiator (XtSynthArp.cpp): the router's key tracker (+0x80), the part's keys down as its Switch went
    // on, what it was last given of the patch, and the tempo clock's ticks it steps on
    XtArpeggio arp;
    uint16_t arpKeys[8] = {}, tgKeysAtSwitch[8] = {};
    XtArpSettings arpGiven;
    XtArpPattern patternGiven;
    XtTempoTicks arpTicks;
    // The Multi Step Modulator (XtSynthMsm.cpp, v193): part 0's, on the same tempo ticks; what it was last given
    XtMsm msm;
    XtMsmSettings msmGiven;
    std::atomic<bool> msmPatchDue { false };
    bool msmHooked = msmHook();                      // its tone update, every note of the part (0x894d96b0 → 0x894d50d0)
    bool msmHook();
    void msmSync();                                  // its settings when they changed, or a patch's
    void msmMs(int32_t ticks);                       // a ms of tempo ticks, then the tone task's flag 0x20 (0x894d4240)
    void msmNoteOn();                                // the allocator's head (0x894d4b08), before a new voice
    bool msmTerms(Voice& v, bool release) const;     // the terms a conversion pass reads; a key-off's, the releases'
    XtPortaSettings portaSettings(const Voice& v) const;    // the portamento (0x894e6510): its Time with PORTA-TIME's term
    void oscFollows(Voice& v, OscVoice& s, const OscControls& o, bool porta, bool key);   // the glide's again (0x894f2600)
    void cosmNewBlock(Voice& v, int k);              // cosmSetBlock on the COSMs' input words (XtCosmWords)
    bool matrixNoteOn(Voice& v);                     // matrixUpdate, the words held until the note has started
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
    void clockIn();                // an F8 (0x894712f0): the arpeggio's ticks, then clockCount
    void clockCount();             // its count of six
    void clockMs();                // the timer routine's count (0x894710e0, its end)
    int msDue() const;             // ms the next block starts with
    double hostStep() const;       // clocks in a block, at the host's tempo
    void oscBeat(OscVoice& s, const OscControls& o);   // a block's Tempo Sync, Time Trip and Beat Keep to the player (0x894f3ec0)
    const XtEncoded* waveOf(int zone, int osc) const { return zone ? (zones ? zones->wave[zone][osc].get() : nullptr) : cur[osc].get(); }
    bool oscPlays(int zone, int osc) const;          // switched on, with something to play
    bool canPlay(int zone) const;
    void targets(const Voice& v, const OscVoice& s, const OscControls& o, float& A, float& F, float& walk, float& g, bool noteOn = false) const;
    float gainTarget(const Voice& v, const OscVoice& s, const OscControls& o, float env) const;   // 0x894f3dc0 / 0x894f3e20
    XtMatrixInputs matrixInputs(const Voice& v) const;
    bool matrixUpdate(Voice& v) const;               // the sums, as a conversion pass reads them; true if any moved
    void matrixRelease(Voice& v) const;              // what a key-off forms
    void updateAll(Voice& v);                        // every envelope's and LFO's settings, as an update gives them
    float pitchKnob(const Voice& v, const OscVoice& s, const OscControls& o) const;   // the voice's +0xc
    XtEnvParams ampEnvParams(const Voice& v, const OscControls& o) const;   // the OSC amp envelope
    XtLoop waveLoop(const OscVoice& s, const OscControls& o) const;   // the players' loop
    static int32_t waveStart(const OscVoice& s, const OscControls& o);  // the block's +0xb8: the Start Offset, the last sample at most
    // Playback Mode STEP and EVENT on a band wave (0x894e42e0, 0x894e4070): the two event numbers it plays between, false for samples
    static bool eventPlay(const OscVoice& s, const OscControls& o);   // Playback Mode STEP or EVENT taking effect: a band wave, with a list for STEP
    bool waveEvents(const Voice& v, const OscVoice& s, const OscControls& o, int& first, int& last) const;
    float noteCents(const Voice& v, const OscVoice& s, const OscControls& o, int key) const;  // voice +8 at a key (+0x68 at the previous one)
    void noteTuning(const Voice& v, const OscVoice& s, const OscControls& o, int& coarse, int& fine) const;   // the per-note setup's +0x68 and +0x70
    XtAnalogShared anaShared;                        // the analog voices' shared state
    XtModBlock modBlk[kZones];                       // the MOD's block (the tone block's +0x5dc), by zone: what a type leaves stays for the next
    XtModShared modShared;                           // FM's delay lines; a voice's index is its tone's number
    float mixPast[3] = {};                           // the last tone's mix, its first words: they follow OSC 2's buffer on the XT
    XtAnalogBlock analogBlock(const Voice& v, const OscVoice& s, const OscControls& o) const; // the analog converter's block
    int keyFollowKey(const Voice& v, const OscControls& o) const;   // the OSC's +4
    void legatoTake(OscVoice& s, const OscVoice& from);  // Playback Mode LEGATO: the earlier note's player (0x894f2d00)
    int shiftKey(int key) const;                     // + the Octave Shift, within 0..127
    void startVoice(int raw, int key, int vel, int zone);
    void liveUpdate(Voice& v, const Controls& c, const Controls& was, bool anyChanged);   // a knob's update of a sounding voice, its zone's controls now and before
    void legatoNote(int key, int vel);               // the sounding voice takes a note (0x894dc1e0)
    void oscStart(Voice& v, OscVoice& s, const OscControls& o);   // the voice's OSC note-on
    void oscEnvs(Voice& v, OscVoice& s, const OscControls& o, bool noteOn);   // its envelopes, at note-on or an update
    void oscLfoNoteOn(const Voice& v, OscVoice& s, const OscControls& o, XtLfoFree& free, const XtLfo* other, float seed);   // its LFO
    void truncate(Voice& v);
    void keyRelease(Voice& v, int velocity);         // a key let go (0x894d58b0)
    void voiceOff(Voice& v);                         // the note's release (0x894dc270)
    void keysCleared();                              // the mode messages' reset of the key state
    // The part's own note, controller and panic, which the router (XtSynthArp.cpp) stands in front of
    void tgNoteOn(int note, float velocity);
    void tgNoteOff(int note, float velocity);
    void tgController(int number, int value);
    void tgAllNotesOff();
    void arpSync();                                  // the patch's settings and pattern, when changed
    void arpBlock();                                 // a block's ms, then the ms timer on
    void arpMs();                                    // one ms of ticks
    void arpApply();                                 // what it sent, to the part
    void arpFlag();                                  // the Multi Step Modulator's arpeggio flag
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
    friend struct XtToneMsmProbe;                    // its MSM mode (XtToneMsm.cpp)
    float tapOsc1[64] = {}, tapOsc2[64] = {}, tapMix[64] = {}, tapCosm[64] = {};
#endif
};
