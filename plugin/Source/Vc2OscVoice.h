#pragma once
// The VC-2's OSC voice (v230, v232): rom.4's OSC voice, 0xb84 bytes, two in a tone (+0x20, +0xba4).  Its own words
// word for word: the block's copy (+0x54…+0x15c), the OSC object the players and the analog OSC keep (+0x15c…+0x908,
// which a take moves whole), the legato word (+0x908) and the tail (+0xa38…+0xa90).  The constructor 0x893ae070, the
// install 0x893ae0f0, the update 0x893ae1c0 with the re-seek 0x893aeed0, the note-on 0x893ae370 with the kind's
// 0x893af0b0 (v232, Vc2OscNoteOn.cpp), the ramp's start 0x893ae610, the counts 0x893ae6c0 and 0x893ae720, the settings
// 0x893ae780, the legato 0x893ae9b0, the render 0x893ae9f0, the getters 0x893aead0…0x893aed60 and the take 0x893aee20;
// the targets 0x893af9f0…0x893aff60 the kinds read (v232, Vc2OscTargets.h).  What it calls (the vibrato at +0, the three
// pitch ramps, each kind's install, update, settings, render and note-on, the players' seeks) through Vc2OscParts; each
// kind's own routines are later versions'.  Bit for bit: xt_vc2osc against rom.4's routines in Sh4Interp, every callee
// hooked.  HARDWARE_FIRMWARE.md, "The VC-2's OSC voice".  Audio thread.
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
};

// What it reads of the wave's loop record the block's +0xdc points at (rom.2's voice setup reads the same two)
struct Vc2OscLoopRecord {
    int32_t tune = 0;      // +0x00: 1/100 cent, the detune and the factor 2^(−tune/120000)
    int8_t fine = 0;       // +0x1b: 1/100 cent onto the offset
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

// What the voice calls on its parts and reads from them; a kind's routine by its rom.4 address
class Vc2OscParts {
public:
    virtual ~Vc2OscParts() = default;
    virtual void buildVibrato() = 0;                        // 0x893b29b0 on the voice's +0
    virtual void updateVibrato(int32_t n) = 0;              // 0x893b2c10
    virtual void setVibrato(const uint32_t* settings) = 0;  // 0x893b2b70, given the settings' +0x80
    virtual void noteOnVibrato(const uint8_t* block) = 0;   // 0x893b29e0, given the voice's +0xd4 (v232)
    virtual float vibratoPitch() = 0;                       // its +0x2c, onto the pitch by +0xfc
    virtual float vibratoLevel() = 0;                       // its +0x30, onto the gains by +0x100 (v232)
    virtual void buildRamp(int ramp, int32_t type) = 0;     // 0x893b0dc0
    virtual void tickRamp(int ramp, int32_t n) = 0;         // 0x893b0f30
    virtual void startRamp(int ramp, float a, float b) = 0;   // 0x893b0e10: fr4, fr5
    virtual void setRamp(int ramp, float a, float b) = 0;     // 0x893b0f20: the same
    virtual void legatoRamp(int ramp, uint32_t x) = 0;        // 0x893b0e30
    virtual float rampValue(int ramp) = 0;                    // 0x893b1160
    virtual float rampFactor(int ramp) = 0;                   // its +0x54, read in place by the gains (v232)
    virtual void kindCall(uint32_t routine) = 0;              // routine(voice): an install, an update, a note-on
    virtual void kindSettings(uint32_t routine, const uint32_t* from) = 0;   // routine(voice, the settings' +0xb0)
    virtual void setCarrier(const uint32_t* p) = 0;           // 0x893ec450(voice + 0xa90, the settings' +0xb0)
    virtual void initCarrier(const uint8_t* p) = 0;           // 0x893ebed0(voice + 0xa90, the voice's +0x104) (v232)
    virtual void startKind6(const uint8_t* p) = 0;            // 0x893ec700(voice + 0xafc, the voice's +0x104) (v232)
    virtual uint32_t renderBy(uint32_t routine, float* out, uint32_t third) = 0;   // routine(out, the object, third)
    virtual void renderCarrier(float* out) = 0;               // 0x893ebfa0(voice + 0xa90, out)
    virtual void renderKind6(float* out) = 0;                 // 0x893ec730(voice + 0xafc, out)
    virtual void seek(uint32_t routine, int32_t at) = 0;      // a player's, routine(the object, at)
    virtual float boundFloat(uint32_t part, uint32_t offset) = 0;   // a bound envelope's or LFO's word, by address
    virtual Vc2OscWaveHeader waveHeader(uint32_t at) = 0;
    virtual Vc2OscLoopRecord loopRecord(uint32_t at) = 0;
};

class Vc2OscVoice {
public:
    static constexpr uint32_t kSize = 0xb84, kOwn = 0x54, kOwnEnd = 0x90c, kBlockBytes = 0x108;   // [FW]
    static constexpr uint32_t kObject = 0x15c, kObjectBytes = 0x7ac, kLegato = 0x908, kKind = 0x54;   // [FW]
    static constexpr uint32_t kWave = 0x104;   // [FW] the block's +0xb0, the wave's type
    static constexpr int kRamps = 3, kSettingsWords = 0xb0 / 4, kOut = 64;

    void construct(Vc2OscParts& parts);                    // 0x893ae070 on a voice given
    void install(Vc2OscParts& parts);                      // 0x893ae0f0
    void update(int32_t n, Vc2OscParts& parts);            // 0x893ae1c0
    void reseek(Vc2OscParts& parts);                       // 0x893aeed0
    // 0x893ae370, then the kind's note-on
    void noteOn(const uint32_t* block, uint32_t legato, Vc2OscParts& parts, const Vc2ToneShared& shared);
    void kindNoteOn(Vc2OscParts& parts, const Vc2ToneShared& shared);   // 0x893af0b0 (Vc2OscNoteOn.cpp)
    void start(Vc2OscParts& parts);                        // 0x893ae610: its argument isn't read
    void count(uint32_t x);                                // 0x893ae6c0
    void countMatched(uint32_t x);                         // 0x893ae720
    void settings(const uint32_t* s, Vc2OscParts& parts);  // 0x893ae780 (Vc2OscVoiceGet.cpp)
    void legato(uint32_t x, Vc2OscParts& parts);           // 0x893ae9b0
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
    uint32_t kind() const { return u32(kKind); }

    std::array<uint8_t, kOwnEnd - kOwn> own {};   // +0x54…+0x90c: the block's copy, the object, the legato word
    Vc2OscVoiceTail tail {};                       // +0xa38…+0xa90

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
