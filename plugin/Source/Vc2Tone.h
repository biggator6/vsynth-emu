#pragma once
// The VC-2's tone object's frame (v228): rom.4's tone, vtable 0x898cd568, 0x261c bytes, from its pool of 24 at
// 0x8999f524.  Its own words word for word; the constructor 0x893b2db0 with the pool's take, the destructor 0x893b2fc0
// with its return; the methods: the update 0x893b4940, the whole path 0x893b4b10 and the render 0x893b4bb0, the LFOs'
// clock 0x893b4c20, the OSC voices' two counts 0x893b4c70 and 0x893b4ca0, the level test 0x893b4c00, held 0x893b4040,
// start 0x893b4050; and the install pass 0x893b4060.  Its parts (two OSC voices, two COSMs, the mix, the TVA, 14
// envelopes, 8 LFOs) through Vc2ToneParts, called in rom.4's order.  The note-on is v229's.  Bit for bit: xt_vc2tone
// against rom.4's routines in Sh4Interp, every part's routine hooked.  HARDWARE_FIRMWARE.md, "The VC-2's tone object".
// Audio thread.
#include "Vc2ToneInput.h"
#include <array>
#include <cstdint>

// The tone's own words at +0x00, as rom.4 lays them out
struct Vc2ToneHead {
    uint32_t next = 0, prev = 0;   // +0x00, +0x04: the part's voice list (0x893ad920's)
    int32_t serial = 0;            // +0x08: the pool's count when the tone was taken (its index until then)
    int32_t state = 0;             // +0x0c: 0 built, 2 sounding (start), 4 ended; the destructor tests 1
    int32_t index = 0;             // +0x10: its index in the pool
    uint32_t self = 0;             // +0x14: its own address
    uint32_t vtable = 0;           // +0x18
    uint32_t id = 0;               // +0x1c: the constructor's argument, which the matched count's mask tests
};
static_assert(sizeof(Vc2ToneHead) == 0x20, "rom.4's layout, the tone's +0x00…+0x20");

// The tone's own words from +0x1af8 to the envelopes, as rom.4 lays them out; what the constructor leaves alone keeps
// what the tone's last use left
struct Vc2ToneTail {
    int32_t index = 0;           // +0x1af8: +0x10's copy, at construction
    uint32_t syncOn = 0;         // +0x1afc: read only by the structure renders, which nothing reaches
    uint32_t sync = 0;           // +0x1b00: the same
    uint32_t unread[4] {};       // +0x1b04…+0x1b13: not touched by the frame
    uint32_t ended = 0;          // +0x1b14: the routine the update calls when the tone ends
    uint32_t truncated = 0;      // +0x1b18: the routine 0x893b3bc0 calls once the TVA's fade has ended
    uint32_t zeroed[2] {};       // +0x1b1c, +0x1b20: 0 at construction
    int32_t structure = 0;       // +0x1b24: the note-on's input's +0
    uint32_t render[3] {};       // +0x1b28…+0x1b33: the structure render's member pointer, which nothing calls
    int32_t renders = 0;         // +0x1b34: 0 at construction; the renders run only with it set
};
static_assert(sizeof(Vc2ToneTail) == 0x40, "rom.4's layout, the tone's +0x1af8…+0x1b38");

// The parts a constructor and a destructor build and take down, in the order rom.4's constructor builds them
enum class Vc2TonePiece : int32_t { osc1, osc2, cosm1, cosm2, tva, mix, envelopes, lfos };
inline constexpr int kVc2ToneOsc1 = 0, kVc2ToneOsc2 = 1, kVc2ToneCosm1 = 0, kVc2ToneCosm2 = 1;

// The renders' three scratch buffers: rom.4's at 0x89998b20, 0x89998c20 and 0x89998d20, 32 stereo frames each
struct Vc2ToneBuffers {
    static constexpr int kFloats = 64;
    float a[kFloats], b[kFloats], c[kFloats];
};

// The envelopes and LFOs a part is bound to, by number (the note-on's)
template <size_t N>
struct Vc2ToneBinding {
    std::array<int32_t, N> envelopes {}, lfos {};
};

// What a COSM's set-up is given from an OSC (0x893aa590's struct): its +0x1cc, its +0xa80 and its byte +0x58
struct Vc2ToneCosmSetUp {
    float value = 0.0f, other = 0.0f;
    uint8_t byte = 0;
};

// What the tone calls on its parts and reads from them (each read is a word of the part's rom.4 names)
class Vc2ToneParts {
public:
    virtual ~Vc2ToneParts() = default;
    virtual void build(Vc2TonePiece piece) = 0;            // its constructor (0x8950773a's array for the envelopes, LFOs)
    virtual void destroy(Vc2TonePiece piece) = 0;          // the LFOs' array (0x89507814), a COSM's (0x893aa060, 2)
    virtual bool lfoFollows(int lfo) = 0;                  // its +0x7c
    virtual void unfollow(int lfo) = 0;                    // 0x893ad720: taken off the list it follows
    virtual void unlist(bool fromOne) = 0;                 // off the part's lists: 0x893ada30 (state 1), else 0x893ad9a0
    virtual bool envelopeOn(int envelope) = 0;             // its +0x68
    virtual void tickEnvelope(int envelope, int32_t n) = 0;   // 0x893abe80
    virtual bool lfoOn(int lfo) = 0;                       // its +0x80
    virtual void tickLfo(int lfo, int32_t n) = 0;          // 0x893ad100
    virtual void clockLfo(int lfo, float x) = 0;           // 0x893ad080
    virtual void updateOsc(int osc, int32_t n) = 0;        // 0x893ae1c0
    virtual void updateMix() = 0;                          // 0x893ade40
    virtual float oscValue(int osc) = 0;                   // its +0x1cc, which COSM 1's update takes
    virtual void updateCosm(int cosm, float x) = 0;        // its vtable's +0xc
    virtual bool tvaOver() = 0;                            // 0x893b2560: its envelope done and its gains under a floor
    virtual bool tvaStopped() = 0;                         // 0x893b2610: its +0x54
    virtual void updateTva() = 0;                          // 0x893b23d0
    virtual void ended() = 0;                              // the tail's routine (+0x1b14), given the tone
    virtual void renderOsc(int osc, float* buffer) = 0;    // 0x893ae9f0
    virtual void renderMix(const float* a, const float* b, float* c) = 0;   // 0x893ade50
    virtual void renderCosm(int cosm, float* buffer) = 0;  // the DSP routine at its +0x148
    virtual void renderTva(const float* in, float* out) = 0;   // 0x893b2640
    virtual void countOsc(int osc, uint32_t x) = 0;        // 0x893ae6c0: x onto a byte (+0x19c) when its +0x15c bit 1
    virtual void countOscMatched(int osc, uint32_t x) = 0; // 0x893ae720: x into a byte (+0x19d) when its +0x15c bit 2
    virtual float tvaLevel() = 0;                          // its +0x40
    virtual bool tvaHeld() = 0;                            // 0x893b25c0
    virtual uint32_t installOsc(int osc) = 0;              // 0x893ae0f0
    virtual int32_t oscKind(int osc) = 0;                  // its +0x54
    virtual uint32_t oscFlag(int osc) = 0;                 // its +0xa40
    virtual void installMix(bool kind3) = 0;               // 0x893ade90
    virtual uint32_t installCosm(int cosm, uint32_t flag) = 0;   // 0x893aafe0
    // The note-on's (v229)
    virtual void noteOnEnvelope(int envelope, const uint32_t* block) = 0;   // 0x893ac1c0
    virtual void noteOnLfo(int lfo, const uint32_t* block, float x) = 0;     // 0x893acda0
    virtual void followLfo(int lfo, int tone, const uint32_t* block) = 0;    // 0x893ace60: that tone's LFO of its number
    virtual uint32_t freeLfo(uint32_t id, int lfo) = 0;    // 0x893ad640: a free object, by its address
    virtual void attachFree(int lfo, uint32_t free) = 0;   // 0x893acfc0
    virtual float freeValue(uint32_t free) = 0;            // the free object's +0x10
    virtual void startFree(int lfo, const uint32_t* block, float value, float x) = 0;   // 0x893acf10
    virtual bool lfoListed(int lfo) = 0;                   // its +0x60
    virtual uint32_t lfoFollow(int lfo) = 0;               // its +0x7c
    virtual void relist(uint32_t follow, uint32_t id, int lfo) = 0;   // 0x893ad780
    virtual void list(uint32_t id, int lfo) = 0;           // 0x893ad800
    virtual void markFree(uint32_t free, uint32_t id, int lfo) = 0;   // the free object's +4 and +0
    virtual void bindOsc(int osc, const Vc2ToneBinding<4>& b) = 0;    // their addresses at +0xa4c…, the flags
    virtual void bindCosm(int cosm, const Vc2ToneBinding<2>& b) = 0;  // 0x893aa7b0, then the flags
    virtual void bindTva(int envelope, int lfoA, int lfoB, int32_t serial) = 0;   // +0xa4…+0xac, +0xc0, the flags
    virtual uint32_t oscWave(int osc) = 0;                 // its +0x104
    virtual void setOscWave(int osc, uint32_t wave) = 0;
    virtual void noteOnOsc(int osc, const uint32_t* block, bool legato) = 0;   // 0x893ae370
    virtual void takePlayer(int osc, int tone) = 0;        // 0x893aee20: that tone's voice's player (+0x15c)
    virtual void setOsc(int osc, const uint32_t* block) = 0;   // 0x893ae610
    virtual void noteOnMix(const uint32_t* block, uint32_t flag1, uint32_t flag2) = 0;   // 0x893adbf0
    virtual Vc2ToneCosmSetUp cosmSetUp(int osc) = 0;       // the OSC's words a COSM's set-up is given
    virtual void setUpCosm(int cosm, const Vc2ToneCosmSetUp& s, uint32_t flag) = 0;   // 0x893aa590
    virtual void startCosm(int cosm, const uint32_t* block, int which) = 0;   // 0x893aa0e0
    virtual void noteOnTva(const uint32_t* block, uint32_t flag) = 0;   // 0x893b2100
    virtual void enlist() = 0;                             // 0x893ada10, given the tone
    // v273: what the tone's remaining methods call
    virtual void settingsOsc(int osc, const uint32_t* block) = 0;             // 0x893ae780
    virtual void settingsLfo(uint32_t lfo, const uint32_t* block) = 0;        // 0x893acff0
    virtual void settingsEnvelope(uint32_t envelope, const uint32_t* block) = 0;   // 0x893ac110
    virtual void newBlockCosm(int cosm, const uint32_t* block) = 0;           // 0x893aa5d0
    virtual void settingsMix(const uint32_t* block) = 0;                      // 0x893add60
    virtual void newBlockTva(const uint32_t* block) = 0;                      // 0x893b23b0
    virtual void legatoOsc(int osc, const uint32_t* ramp) = 0;                // 0x893ae9b0
    virtual void releaseLfo(int lfo) = 0;                  // 0x893ac8c0
    virtual void releaseEnvelope(int envelope) = 0;        // 0x893ac470
    virtual void keyOffOsc(int osc) = 0;                   // 0x893b2b60: its vibrato's +0x34 to 0
    virtual void truncateTvaTold() = 0;                    // 0x893b25a0
    virtual void truncateTva() = 0;                        // 0x893b25b0
    virtual float tvaFade() = 0;                           // its +0x64
    virtual void waitFade() = 0;                           // 0x893017f0(0x1000): the task asleep until the fade is rendered
    virtual void truncated() = 0;                          // the tail's routine (+0x1b18), given the tone
    virtual float oscCost(int osc) = 0;                    // its +0xa88
    virtual float cosmCost() = 0;                          // COSM 1's +0x14c
    virtual int32_t oscWord168(int osc) = 0;               // 0x893aed40
    virtual uint32_t oscPosition(int osc) = 0;             // 0x893aed60
};

class Vc2TonePool;

// What a note-on reads beyond its tone: the two lists it searches for a tone of the same id (0x89999678, else 0x8999967c
// when that is empty: tones linked through their +4, each node's +0x14 the tone), and another tone's parts
class Vc2ToneOwner {
public:
    virtual ~Vc2ToneOwner() = default;
    virtual std::array<uint32_t, 2> lists() = 0;   // the two lists' heads, by rom.4's addresses (0 empty)
    virtual Vc2ToneParts& partsOf(int tone) = 0;
};

class Vc2Tone {
public:
    static constexpr uint32_t kSize = 0x261c;
    static constexpr uint32_t kVtable = 0x898cd568;   // [FW] the tone's vtable, a word of its own
    static constexpr int kEnvelopes = 14, kLfos = 8;
    static constexpr int32_t kBuilt = 0, kFromOne = 1, kSounding = 2, kEnded = 4;   // [FW] the state's values

    void construct(uint32_t self, uint32_t id, Vc2ToneParts& parts);   // 0x893b2db0 on a tone given
    void destroy(Vc2ToneParts& parts);                // 0x893b2fc0 but the pool's return
    void update(int32_t n, Vc2ToneParts& parts);      // +0x0c
    void renderWhole(float* out, Vc2ToneBuffers& b, Vc2ToneParts& parts);   // +0x18
    void render(float* out, Vc2ToneBuffers& b, Vc2ToneParts& parts);        // +0x24
    void clock(float x, Vc2ToneParts& parts);         // +0x30
    void count(uint32_t x, Vc2ToneParts& parts);      // +0x3c
    void countMatched(uint32_t x, int32_t value, uint32_t mask, Vc2ToneParts& parts);   // +0x48
    bool sounds(Vc2ToneParts& parts) const;           // +0x54: the TVA's level above 0
    bool held(Vc2ToneParts& parts) const;             // +0x60
    void start();                                     // +0x6c
    void install(Vc2ToneParts& parts);                // 0x893b4060
    // 0x893b3390 (v229, Vc2ToneNoteOn.cpp): the parts started and bound by the input, the legato's take of the tone
    // of the input's serial in `pool`; it writes the input's fix-up back
    void noteOn(Vc2ToneInput& in, Vc2TonePool& pool, Vc2ToneOwner& owner, Vc2ToneParts& parts);
    // v273 (Vc2ToneMethods.cpp): the block setters, each a part's (an OSC's or a COSM's by number, 0 or 1, else none)
    void setOscBlock(uint32_t osc, const uint32_t* block, Vc2ToneParts& parts);         // 0x893b3110
    void setLfoBlock(uint32_t lfo, const uint32_t* block, Vc2ToneParts& parts);         // 0x893b31a0
    void setEnvelopeBlock(uint32_t envelope, const uint32_t* block, Vc2ToneParts& parts);   // 0x893b31f0
    void setCosmBlock(uint32_t cosm, const uint32_t* block, Vc2ToneParts& parts);       // 0x893b3240
    void setMixBlock(const uint32_t* block, Vc2ToneParts& parts);       // 0x893b32d0
    void setTvaBlock(const uint32_t* block, Vc2ToneParts& parts);       // 0x893b3310
    void setLegato(const uint32_t* ramp, Vc2ToneParts& parts);          // 0x893b3350: both OSC voices
    void keyOff(Vc2ToneParts& parts);                 // 0x893b3ad0: every LFO, envelope and OSC voice, when sounding
    void truncateAndWait(Vc2ToneParts& parts);        // 0x893b3bc0: the TVA's fade to its end, then the tail's routine
    void truncateAtOnce(Vc2ToneParts& parts);         // 0x893b3c20
    float cost(Vc2ToneParts& parts) const;            // 0x893b4020: what it costs the CPU, its OSCs' and COSM 1's
    // The position getters by an OSC's number, 0 past 1: 0x893b3f80 (as a halfword) and 0x893b3fd0; their first cases,
    // 0x893b3fc0 and 0x893b4010, are no routines of their own
    uint32_t oscWord168(uint32_t osc, Vc2ToneParts& parts) const;
    uint32_t oscPosition(uint32_t osc, Vc2ToneParts& parts) const;

    Vc2ToneHead head {};
    Vc2ToneTail tail {};
    std::array<int32_t, kEnvelopes> envelopeIndex {};   // each envelope's +4, which the constructor writes
    std::array<int32_t, kLfos> lfoIndex {};             // each LFO's +0x34, the same
    uint32_t link = 0;                                  // +0x2618: the pool's free list
};

// The pool of 24 tones the constructor takes from when given none, and the destructor gives back to
class Vc2TonePool {
public:
    static constexpr int kTones = 24;
    static constexpr uint32_t kBase = 0x8999f524;     // [FW] the pool's address, which its links hold
    Vc2Tone* make(uint32_t id, Vc2ToneParts& parts);  // 0x893b2db0(0, id): a take, then the construction; none when out
    void unmake(Vc2Tone& tone, bool back, Vc2ToneParts& parts);   // 0x893b2fc0(tone, back): back onto the free list
    uint32_t addressOf(int index) const { return kBase + Vc2Tone::kSize * (uint32_t)index; }
    uint32_t end() const { return addressOf(kTones); }   // the sentinel's word (0x8993fb44): the pool's end
    Vc2Tone& at(uint32_t address) { return tones[(address - kBase) / Vc2Tone::kSize]; }
    int indexOf(uint32_t address) const { return (int)((address - kBase) / Vc2Tone::kSize); }
    bool holds(uint32_t address) const { return address >= kBase && address < end(); }   // a tone's, not a list's head
    int bySerial(int32_t wanted) const;   // the first tone whose +8 is `wanted` (0x893b4340), −1 none

    uint32_t head = 0;      // 0x8993fb48: the free list, 0 until the first take
    int32_t serial = 0;     // 0x8993fb4c
    std::array<Vc2Tone, kTones> tones {};
};
