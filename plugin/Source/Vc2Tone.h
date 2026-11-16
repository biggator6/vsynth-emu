#pragma once
// The VC-2's tone object's frame (v228): rom.4's tone, vtable 0x898cd568, 0x261c bytes, from its pool of 24 at
// 0x8999f524.  Its own words word for word; the constructor 0x893b2db0 with the pool's take, the destructor 0x893b2fc0
// with its return; the methods: the update 0x893b4940, the whole path 0x893b4b10 and the render 0x893b4bb0, the LFOs'
// clock 0x893b4c20, the OSC voices' two counts 0x893b4c70 and 0x893b4ca0, the level test 0x893b4c00, held 0x893b4040,
// start 0x893b4050; and the install pass 0x893b4060.  Its parts (two OSC voices, two COSMs, the mix, the TVA, 14
// envelopes, 8 LFOs) through Vc2ToneParts, called in rom.4's order.  The note-on is v229's.  Bit for bit: xt_vc2tone
// against rom.4's routines in Sh4Interp, every part's routine hooked.  HARDWARE_FIRMWARE.md, "The VC-2's tone object".
// Audio thread.
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
    uint32_t unread2 = 0;        // +0x1b18
    uint32_t zeroed[2] {};       // +0x1b1c, +0x1b20: 0 at construction
    uint32_t unread3[4] {};      // +0x1b24…+0x1b33
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
    virtual void countOscMatched(int osc, uint32_t x) = 0; // 0x893ae720: the same by bit 2
    virtual float tvaLevel() = 0;                          // its +0x40
    virtual bool tvaHeld() = 0;                            // 0x893b25c0
    virtual uint32_t installOsc(int osc) = 0;              // 0x893ae0f0
    virtual int32_t oscKind(int osc) = 0;                  // its +0x54
    virtual uint32_t oscFlag(int osc) = 0;                 // its +0xa40
    virtual void installMix(bool kind3) = 0;               // 0x893ade90
    virtual uint32_t installCosm(int cosm, uint32_t flag) = 0;   // 0x893aafe0
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

    uint32_t head = 0;      // 0x8993fb48: the free list, 0 until the first take
    int32_t serial = 0;     // 0x8993fb4c
    std::array<Vc2Tone, kTones> tones {};
};
