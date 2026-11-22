#pragma once
// xt_vc2tone's shared parts (v228, v229): the calls each side logs, the port's parts and their owner, and rom.4's side:
// the routines the tone calls but its parts' hooked to log and answer.  From v319 every part of a tone is real, the
// host's (Vc2ToneHost); what its OSC voices call beyond the tone is xt_vc2osc's hooks and fake parts (Vc2OscCheck.h),
// logged into their own calls, each on its voice, tone × 2 + its OSC.  XT_VC2TONE_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2FreeLfos.h"
#include "Vc2OscCheck.h"
#include "Vc2RomShared.h"
#include "Vc2Tone.h"
#include "Vc2ToneHost.h"
#include "Vc2ToneLists.h"
#include "Vc2ToneMix.h"
#include "Vc2ToneModulators.h"
#include <array>
#include <cstring>
#include <deque>
#include <span>
#include <vector>

inline constexpr uint32_t kToneOutAt = 0x8C036000;     // the renders' output, in Vc2Fw's RAM
inline constexpr uint32_t kToneInputAt = 0x8C050000;   // the note-on's input, in Vc2Fw's RAM
inline constexpr uint32_t kToneHooksAt = 0x8C042000;   // the hooked routines the tones' words point at, in Vc2Fw's RAM
inline constexpr uint32_t kToneEndedAt = kToneHooksAt + 0x20;   // the end routine, the tail's word on both sides
inline constexpr uint32_t kToneTruncatedAt = kToneHooksAt + 0x30;   // the truncation's routine, the tail's +0x1b18 (v273)
inline constexpr uint32_t kToneBufferAt = 0x89998b20;  // [FW] the renders' three scratch buffers, 0x100 bytes apart
inline constexpr int kToneNone = -1, kToneUnknown = -2;   // a call's part: the tone itself, or no part known
inline constexpr int kToneOut = 4;

// What the tone calls that isn't a part's (v319: every part the host's): the tail's end and truncation routines, the
// truncation's wait (v273) and the OS's event 0x89301830, which a told truncation's fade end sets (v317)
enum ToneKind : int32_t { kEnded, kWaitFade, kTruncated, kOsEvent };

// A call as either side made it: what, on which tone of the pool (−1 none) and which of its parts (Vc2TonePiece × 100
// + an envelope's or LFO's number), and its arguments
struct ToneCall {
    int32_t kind, tone, part;
    uint32_t a, b, c, d;
    bool operator==(const ToneCall&) const = default;
};

// The truncation's waits (v273): the tone whose TVA fades, and how many waits each side's hook lets pass before its fade
// is at 0, as the block task's renders would leave it
struct ToneFadeWait {
    uint32_t tone = 0;
    int romLeft = 0, portLeft = 0;
};

int32_t tonePartCode(Vc2TonePiece piece, int index);   // a part as a call names it
uint32_t toneEnvelopeAt(uint32_t tone, int envelope);  // rom.4's address of a tone's envelope, LFO
uint32_t toneLfoAt(uint32_t tone, int lfo);
uint32_t toneTvaAt(uint32_t tone);                     // and of its TVA (v317)
uint32_t toneCosmAt(uint32_t tone, int cosm);          // and of its COSM 1 (0) or 2 (1) (v318)
uint32_t toneOscAt(uint32_t tone, int osc);            // and of its OSC voice 1 (0) or 2 (1) (v319)
// v319: a voice as xt_vc2osc's hooks name it, tone × 2 + its OSC, from its rom.4 address; −1 for none
int toneVoiceNumber(uint32_t at);
inline constexpr uint32_t kToneFadeStepAt = 0x8993eb08;   // [FW] the TVAs' fade step, in .data (v317)

// v319: the routines the OSC voices share (Vc2ToneShared) as rom.4's own (RomShared), the interpreter's registers kept
// around each: the port calls them inside an event whose answer rom.4's routine has left in r0 or fr0
class ToneMath final : public Vc2ToneShared {
public:
    explicit ToneMath(Vc2Fw& fw) : fw_(fw), rom_(fw) {}
    int velocity(int velocity, int sens, int curve) const override {
        const Kept kept(fw_);
        return rom_.velocity(velocity, sens, curve);
    }
    float exp2(float x) const override {
        const Kept kept(fw_);
        return rom_.exp2(x);
    }
    float log(float x) const override {
        const Kept kept(fw_);
        return rom_.log(x);
    }
    float pow(float x, float y) const override {
        const Kept kept(fw_);
        return rom_.pow(x, y);
    }
    bool faulted() const { return rom_.faulted; }

private:
    // The interpreter's registers, put back as they were when it goes
    struct Kept {
        explicit Kept(Vc2Fw& fw) : fw_(fw) {
            std::memcpy(r_, fw.cpu.r, sizeof r_);
            std::memcpy(fb_, fw.cpu.fb, sizeof fb_);
        }
        ~Kept() {
            std::memcpy(fw_.cpu.r, r_, sizeof r_);
            std::memcpy(fw_.cpu.fb, fb_, sizeof fb_);
        }
        Vc2Fw& fw_;
        uint32_t r_[16] {}, fb_[2][16] {};
    };
    Vc2Fw& fw_;
    RomShared rom_;
};

class TonePortParts;

// What the port's parts share: the buffers and output, the note-on's input, the calls, the pool, every tone's parts,
// the lists' heads
struct ToneShared {
    Vc2ToneBuffers buffers {};
    std::array<float, Vc2ToneBuffers::kFloats> out {};
    Vc2ToneInput input {};
    std::vector<ToneCall> calls {};
    Vc2TonePool* pool = nullptr;
    std::deque<TonePortParts>* parts = nullptr;   // a deque: a host stays where it is made (v318)
    Vc2ToneLists lists {};                   // the part's two lists of tones (v273)
    ToneFadeWait fade {};
    float chaos = Vc2ToneLfo::kChaosStart;   // the LFOs' constructors' generator, rom.4's word 0x89999668
    Vc2ToneMixRings rings {};                // the mixes' delay lines (v269)
    Vc2FreeLfos frees {};                    // the free LFOs' manager (v271)
    float fadeStep = 0.0f;                   // the TVAs' fade step, rom.4's word 0x8993eb08 (v317)
    Vc2CosmPools pools {};                   // the COSMs' buffer pools, rom.4's 0x89999290 (v318)
    std::array<float, 3> past {};            // the part mixer's frames' first words, rom.4's 0x89998e20 (v318)
    // v319: what the OSC voices read beyond the tone, xt_vc2osc's: the hooks' answers, the records, the settings'
    // block, the mic ring and the unvoiced decision, the calls; and the shared routines
    OscShared osc {};
    const ToneMath* math = nullptr;
    long follows = 0, takes = 0;             // the LFOs that followed another tone's, took a free object
    long players = 0;                        // v319: the players a legato took from another tone
};

// A tone's port parts: the host's (v316), every part from v319, with the tail's two routines and the truncation's wait
// logged as its tone's, and what its OSC voices read beyond the tone xt_vc2osc's fake parts, each logged on its voice
class TonePortParts final : public Vc2ToneHost {
public:
    TonePortParts(int tone, ToneShared& shared);
    void followLfo(int lfo, int tone, const uint32_t* block) override;   // counted, then the host's
    uint32_t freeLfo(uint32_t id, int lfo) override;                     // the same
    void takePlayer(int osc, int tone) override;                         // the same (v319)
    void ended() override;   // logged with the tone's state as the end routine finds it
    void waitFade() override;
    void truncated() override;

protected:
    const Vc2ToneHost& hostOf(int tone) override;
    void osEvent(uint32_t event, uint32_t flags) override;   // logged as rom.4's hook reads it (v317)
    Vc2OscParts& oscOutside(int osc) override { return outside_[(size_t)osc]; }

private:
    void log(ToneKind kind, int32_t part, std::array<uint32_t, 4> args = {}, bool onTone = true) {
        s_.calls.push_back({ kind, onTone ? tone() : kToneNone, part, args[0], args[1], args[2], args[3] });
    }
    ToneShared& s_;
    std::array<OscPortParts, 2> outside_;
};

// The port's owner: the shared lists' heads and each tone's parts
class TonePortOwner final : public Vc2ToneOwner {
public:
    TonePortOwner(ToneShared& shared, std::deque<TonePortParts>& parts) : s_(shared), parts_(parts) {}
    std::array<uint32_t, 2> lists() override { return s_.lists.marks(); }
    Vc2ToneParts& partsOf(int tone) override { return parts_[(size_t)tone]; }

private:
    ToneShared& s_;
    std::deque<TonePortParts>& parts_;
};

// rom.4's side: the tail's routines and the OS's event hooked to log into `log`, the truncation's waits by `fade`, and
// (v319) the routines the OSC voices call xt_vc2osc's hooks, logging into `oscLog` and answering from `oscDraws`
void toneInstallRom(Vc2Fw& fw, std::vector<ToneCall>& log, ToneFadeWait& fade, const OscDraws& oscDraws,
                    std::vector<OscCall>& oscLog);
// The first call the two logs differ at; empty when they don't
std::string toneCallsDifference(const std::vector<ToneCall>& rom, const std::vector<ToneCall>& port);
