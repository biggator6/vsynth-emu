#pragma once
// The VC-2's OSC voice (v230, v232): rom.4's OSC voice, 0xb84 bytes, two in a tone (+0x20, +0xba4).  Its own words
// word for word: the block's copy (+0x54…+0x15c), the OSC object the players and the analog OSC keep (+0x15c…+0x908,
// which a take moves whole), the legato word (+0x908) and the tail (+0xa38…+0xa90).  The constructor 0x893ae070, the
// install 0x893ae0f0, the update 0x893ae1c0 with the re-seek 0x893aeed0, the note-on 0x893ae370 with the kind's
// 0x893af0b0 (v232, Vc2OscNoteOn.cpp), the ramp's start 0x893ae610, the counts 0x893ae6c0 and 0x893ae720, the settings
// 0x893ae780, the legato 0x893ae9b0, the render 0x893ae9f0, the getters 0x893aead0…0x893aed60 and the take 0x893aee20;
// the targets 0x893af9f0…0x893aff60 the kinds read (v232, Vc2OscTargets.h).  Its three pitch ramps, the portamento, are
// its own (v236, Vc2PitchRamp.h, +0x90c…+0xa38), and so are the vibrato (+0, v267), kind 5's carrier (+0xa90) and kind
// 6's pitch shifter (+0xafc, v266).  What it calls (each kind's install, update, settings, render and note-on, the
// players' seeks) through Vc2OscParts; each kind's own routines are later versions'.  Bit for bit: xt_vc2osc against
// rom.4's routines in Sh4Interp, every callee but those hooked.  HARDWARE_FIRMWARE.md, "The VC-2's OSC voice".  Audio
// thread.
#include "Vc2Kind6.h"
#include "Vc2OscVibrato.h"
#include "Vc2PitchRamp.h"
#include "Vc2VocoderOsc.h"
#include <array>
#include <cstdint>
#include <cstring>

// What the note-on reads of the wave's header the block's +0xd4 points at, by its offsets
struct Vc2OscWaveHeader {
    uint8_t flag = 0;      // +0x02: 1 sets the voice's +0xa40
    uint16_t rate = 0;     // +0x08: 44000 over it is +0xa6c; it scales +0xa80
    int8_t rootKey = 0;    // +0x0a: the root key, × 100 into +0xa80's exponent
    int32_t cents = 0;     // +0x18: the header's cents, the offset's whole part
    uint32_t word1c = 0;   // +0x1c: into the object's +0x1a4 at VariPhrase's note-on (v238)
};

// What it reads of the wave's loop record (vplp) the block's +0xdc points at, by rom.2's names (XtFiles.h, its file
// reader, which reads them at the same offsets) where rom.4's VariPhrase note-on uses them as they say (v238)
struct Vc2OscLoopRecord {
    int32_t tune = 0;        // +0x00: 1/100 cent, the detune and the factor 2^(−tune/120000)
    int32_t loopStart = 0;   // +0x04: the forward loop, in frames
    int32_t loopEnd = 0;     // +0x08
    int32_t bwdEnd = 0;      // +0x0c: the backward loop
    int32_t bwdStart = 0;    // +0x10
    int16_t rateFwd = 0;     // +0x14: the loops' rate factors, in tenths of a cent: exp2(rate / 12000)
    int16_t rateBwd = 0;     // +0x16
    int8_t fwdMode = 0;      // +0x18: the loops' modes (0 One Shot, 1 One Way, 2 Alternate)
    int8_t bwdMode = 0;      // +0x19
    int8_t fine = 0;         // +0x1b: 1/100 cent onto the offset
};

// The voice's tail, as rom.4 lays it out; rom.2's names for the wave's tuning (XtWaveTune: rom.2's +0x994…+0x9a0)
struct Vc2OscVoiceTail {
    uint32_t unread = 0;           // +0xa38: nothing here touches it
    uint32_t sync = 0;             // +0xa3c: kind 3's render is given it
    uint32_t flag = 0;             // +0xa40: the flag the tone gives the COSMs, the mix and the TVA
    uint32_t render = 0;           // +0xa44: the render routine of kinds 1, 2, 4 and from 7, by rom.4's address
    uint32_t renderAnalog = 0;     // +0xa48: kind 3's, the same
    std::array<uint32_t, 8> binds {};   // +0xa4c…+0xa68: four envelopes, four LFOs by rom.4's addresses (the tone's)
    float rateRatio = 0;           // +0xa6c: 1, or 44000 over the wave header's rate
    float offset = 0;              // +0xa70: the header's cents and the loop record's fine part
    float detune = 0;              // +0xa74: the loop record's tune / 100
    float walkFactor = 0;          // +0xa78: the factor on the walk, 1 with no wave
    float formantFactor = 0;       // +0xa7c: the factor on the formant ratio, the same
    float hz = 0;                  // +0xa80
    float factor = 0;              // +0xa84: 2^(−tune/120000), 1 when that isn't above 0
    float installed = 0;           // +0xa88: kinds 4 to 7's install's
    uint32_t seekPending = 0;      // +0xa8c: set, the update waits for the object's level to fall, then re-seeks
};
static_assert(sizeof(Vc2OscVoiceTail) == 0x58, "rom.4's layout, the voice's +0xa38…+0xa90");
static_assert(sizeof(Vc2PitchRamp) == 0x64, "rom.4's layout, a ramp's 0x64 bytes from the voice's +0x90c");
static_assert(sizeof(Vc2Carrier) == 0x6c, "rom.4's layout, the carrier at the voice's +0xa90");
static_assert(sizeof(Vc2Kind6) == 0x88, "rom.4's layout, kind 6's object at the voice's +0xafc");

// A wave's halfwords, reached by the addresses rom.4's object holds: `samples` is what rom.4 has at `address`; a read
// outside them gives 0 (v261, Vari off's renders'; the voice's parts' from v323)
struct Vc2OscSamples {
    const int16_t* samples = nullptr;
    uint32_t address = 0;
    uint32_t count = 0;
    int at(uint32_t word) const {
        const uint32_t index = (word - address) / 2;
        return index < count ? samples[index] : 0;
    }
};

// What the voice calls on its parts and reads from them; a kind's routine by its rom.4 address
class Vc2OscParts {
public:
    virtual ~Vc2OscParts() = default;
    virtual void kindCall(uint32_t routine) = 0;              // routine(voice): an install, an update, a note-on
    virtual void kindSettings(uint32_t routine, const uint32_t* from) = 0;   // routine(voice, the settings' +0xb0)
    virtual uint32_t renderBy(uint32_t routine, float* out, uint32_t third) = 0;   // routine(out, the object, third)
    virtual void seek(uint32_t routine, int32_t at) = 0;      // a player's, routine(the object, at)
    // v266: what the voice's carrier and kind 6 read from outside it: the OS's millisecond word (0x893018d0), where a
    // carrier's phase starts; the mic ring (0x8a0f31cc), which the part's block mixer writes; the input stage's
    // unvoiced decision for the block, a float a sample (0x899d99dc, by 0x893ed500)
    virtual int32_t osClock() = 0;
    virtual Vc2MicRing& micRing() = 0;
    virtual const float* unvoiced() = 0;
    virtual float inputPeak() = 0;   // v321: the input stage's peak (0x899d9a5c), kind 5's update's Dynamics
    // v322: the effect chip's frames as the part reads them ([0x8993eadc], the part mixer's too: a block's 32 of 8
    // words), which the EXT IN player plays
    virtual const int32_t* inputFrames() = 0;
    // v323: the wave memory the object's samples' address (+0xf4) points into, which Vari off's renders read
    virtual Vc2OscSamples waveSamples() = 0;
    virtual float boundFloat(uint32_t part, uint32_t offset) = 0;   // a bound envelope's or LFO's word, by address
    // v238: the voice's address as rom.4's memory holds it (VariPhrase's note-on keeps its +0x108's at +0x1a0); a
    // player's routine on the object; 0x893cc0d0(the object, event, &position, &…), an event's position ×16
    virtual uint32_t address() = 0;
    virtual void objectCall(uint32_t routine) = 0;
    virtual int32_t eventPosition(uint32_t event) = 0;
    virtual Vc2OscWaveHeader waveHeader(uint32_t at) = 0;
    virtual Vc2OscLoopRecord loopRecord(uint32_t at) = 0;
};

class Vc2OscVoice {
public:
    static constexpr uint32_t kSize = 0xb84, kOwn = 0x54, kOwnEnd = 0x90c, kBlockBytes = 0x108;   // [FW]
    static constexpr uint32_t kObject = 0x15c, kObjectBytes = 0x7ac, kLegato = 0x908, kKind = 0x54;   // [FW]
    static constexpr uint32_t kWave = 0x104;   // [FW] the block's +0xb0, the wave's type
    static constexpr int kRamps = 3, kSettingsWords = 0xb0 / 4, kOut = 64;
    // [FW] the ramps by what they glide (+0x90c, +0x970, +0x9d4): the targets read them so, the start and the settings
    // start the pitch's again
    static constexpr size_t kPitchRamp = 0, kFormantRamp = 1, kWalkRamp = 2;

    void construct();                                      // 0x893ae070 on a voice given
    void install(Vc2OscParts& parts);                      // 0x893ae0f0
    void update(int32_t n, Vc2OscParts& parts, const Vc2ToneShared& shared);   // 0x893ae1c0
    void reseek(Vc2OscParts& parts);                       // 0x893aeed0
    // 0x893ae370, then the kind's note-on
    void noteOn(const uint32_t* block, uint32_t legato, Vc2OscParts& parts, const Vc2ToneShared& shared);
    void kindNoteOn(Vc2OscParts& parts, const Vc2ToneShared& shared);   // 0x893af0b0 (Vc2OscNoteOn.cpp)
    // v238 (Vc2OscPlayers.cpp): LITE's install 0x893af7a0, VariPhrase's 0x893af870 and its note-on 0x893af420
    void installLite(Vc2OscParts& parts);
    void installVari();
    void noteOnVari(Vc2OscParts& parts, const Vc2ToneShared& shared);
    void noteOnLite(Vc2OscParts& parts, const Vc2ToneShared& shared);   // 0x893c7c30 (v240, Vc2OscLite.cpp)
    void noteOnAnalog(Vc2OscParts& parts, const Vc2ToneShared& shared);   // 0x893c22e0 (v242, Vc2OscAnalog.cpp)
    // v248 (Vc2OscAnalogUpdate.cpp): the analog's install 0x893c2ad0, update 0x893c2ce0 and settings 0x893c3460
    // (given the settings' +0xb0)
    void installAnalog();
    void updateAnalog(Vc2OscParts& parts, const Vc2ToneShared& shared);
    void settingsAnalog(const uint32_t* from);
    // v321 (Vc2OscNoteOn.cpp): kind 5's update 0x893afff0 (P at +0x104 and the carrier) and kind 6's 0x893b0150 (P,
    // then kind 6's set)
    void updateCarrier(Vc2OscParts& parts, const Vc2ToneShared& shared);
    void updateKind6(Vc2OscParts& parts, const Vc2ToneShared& shared);
    // v322 (Vc2OscNoteOn.cpp): the EXT IN player's update 0x893c6260 (kinds 4 and 7, and kind 6's first) and its
    // renders 0x893c62e0 and 0x893c61e0 by the frame's words they play on the left and right
    void updateExtIn(Vc2OscParts& parts, const Vc2ToneShared& shared);
    void renderExtIn(int left, int right, float* out, Vc2OscParts& parts);
    // v323: the renders the voice has as its own, run when `routine`, the address an install writes, is one of them:
    // the EXT IN player's two and Vari off's two (v261); false, nothing done, for any other
    bool renderOwn(uint32_t routine, float* out, Vc2OscParts& parts);
    // 0x893ae610: ramp 0's start, given the ramp's six words (Vc2RampSettings; the tone's input + 0xc)
    void start(const uint32_t* ramp, Vc2OscParts& parts, const Vc2ToneShared& shared);
    void count(uint32_t x);                                // 0x893ae6c0
    void countMatched(uint32_t x);                         // 0x893ae720
    // 0x893ae780 (Vc2OscVoiceGet.cpp)
    void settings(const uint32_t* s, Vc2OscParts& parts, const Vc2ToneShared& shared);
    void legato(const uint32_t* ramp);                     // 0x893ae9b0: the ramps' legato updates, given their words
    uint32_t render(float* out, Vc2OscParts& parts);       // 0x893ae9f0: kind 3's routine's answer, else 0
    void take(const Vc2OscVoice& from);                    // 0x893aee20, given another voice's object
    // The getters (Vc2OscVoiceGet.cpp)
    float formant() const;                                 // 0x893aead0: kind 2's +0x68
    float time() const;                                    // 0x893aeb00: kind 2's +0x64
    float pitch(Vc2OscParts& parts) const;                 // 0x893aeb20: cents, within ±6000
    float kindFormant(Vc2OscParts& parts, const Vc2ToneShared& shared) const;   // 0x893aecc0
    float kindWalk(Vc2OscParts& parts, const Vc2ToneShared& shared) const;      // 0x893aecf0
    float level() const;                                   // 0x893aed10
    int32_t word168() const;                               // 0x893aed40
    uint32_t position() const;                             // 0x893aed60

    // The voice's own bytes, by its offsets
    uint32_t u32(uint32_t at) const { return get<uint32_t>(at); }
    float f32(uint32_t at) const { return get<float>(at); }
    uint8_t u8(uint32_t at) const { return own[at - kOwn]; }
    const uint8_t* at(uint32_t offset) const { return own.data() + (offset - kOwn); }
    void setU32(uint32_t at, uint32_t v) { put(at, v); }
    void setF32(uint32_t at, float v) { put(at, v); }
    void setU8(uint32_t at, uint8_t v) { own[at - kOwn] = v; }
    uint16_t u16(uint32_t at) const { return get<uint16_t>(at); }
    void setU16(uint32_t at, uint16_t v) { put(at, v); }
    uint32_t kind() const { return u32(kKind); }

    Vc2OscVibrato vibrato {};                     // +0x00…+0x54 (v267)
    std::array<uint8_t, kOwnEnd - kOwn> own {};   // +0x54…+0x90c: the block's copy, the object, the legato word
    // +0x90c, +0x970, +0x9d4: the pitch ramps (v236), built with types 0, 2 and 1: the pitch's, the formant's, the walk's
    std::array<Vc2PitchRamp, kRamps> ramps {};
    Vc2OscVoiceTail tail {};                       // +0xa38…+0xa90
    Vc2Carrier carrier {};                         // +0xa90…+0xafc: kind 5's (v266)
    Vc2Kind6 kind6 {};                             // +0xafc…+0xb84: kind 6's (v266)

private:
    template <class T>
    T get(uint32_t at) const {
        T v;
        std::memcpy(&v, own.data() + (at - kOwn), sizeof v);
        return v;
    }
    template <class T>
    void put(uint32_t at, T v) {
        std::memcpy(own.data() + (at - kOwn), &v, sizeof v);
    }
};
