#pragma once
// xt_vc2tone's shared parts (v228, v229): the calls each side logs, what the hooked routines answer (drawn once for both
// sides), each tone's part words (the port's fake parts' mirror, rom.4's memory), the port's fake parts and their owner,
// and rom.4's side: every part routine hooked to log and answer.  XT_VC2TONE_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2FreeLfos.h"
#include "Vc2Tone.h"
#include "Vc2ToneHost.h"
#include "Vc2ToneLists.h"
#include "Vc2ToneMix.h"
#include "Vc2ToneModulators.h"
#include <array>
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

enum ToneKind : int32_t {
    kBuild, kUpdateOsc,
    kEnded, kRenderOsc, kCountOsc,
    kCountOscMatched, kInstallOsc,
    kNoteOnOsc, kTakePlayer, kSetOsc,
    // v273: what the tone's remaining methods call (the envelopes' and LFOs' settings and release real from v315, the
    // TVA's from v317, the COSMs' from v318)
    kSettingsOsc, kLegatoOsc, kKeyOffOsc, kWaitFade, kTruncated,
    kOscWord168, kOscPosition,
    kOsEvent,   // v317: the OS's event 0x89301830, which a told truncation's fade end sets
};

// A call as either side made it: what, on which tone of the pool (−1 none) and which of its parts (Vc2TonePiece × 100
// + an envelope's or LFO's number), and its arguments: counts, floats' bits, buffers by number (a 1, b 2, c 3, out 4),
// input blocks by their offset in the input, numbers
struct ToneCall {
    int32_t kind, tone, part;
    uint32_t a, b, c, d;
    bool operator==(const ToneCall&) const = default;
};

// What the hooked routines answer, drawn once for both sides
struct ToneDraws {
    std::array<uint32_t, 2> installOsc {};
    std::array<uint32_t, 2> word168 {}, position {};   // v273: each OSC voice's 0x893aed40 and 0x893aed60
};

// [FW] an OSC voice's words the tone reads or writes: +0x54, +0xa40, +0x1cc, +0xa80, the byte +0x58, +0x104, the
// addresses of its four envelopes and four LFOs at +0xa4c…+0xa68, and its cost +0xa88 (v273)
struct ToneOscWords {
    uint32_t kind = 0, flag = 0, value = 0, other = 0, byte = 0, wave = 0;
    std::array<uint32_t, 8> binds {};
    uint32_t cost = 0;
    bool operator==(const ToneOscWords&) const = default;
};

// [FW] a tone's hooked parts' words the tone reads or writes: the OSC voices' (the envelopes and LFOs are the real ones
// from v268, Vc2ToneModulators, the TVA from v317 and the COSMs from v318, the host's)
struct ToneWords {
    std::array<ToneOscWords, 2> osc {};
    bool operator==(const ToneWords&) const = default;
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
inline constexpr uint32_t kToneFadeStepAt = 0x8993eb08;   // [FW] the TVAs' fade step, in .data (v317)

class TonePortParts;

// What the port's parts share: the answers, the buffers and output, the note-on's input, the calls, the pool, every
// tone's parts, the lists' heads
struct ToneShared {
    ToneDraws draws {};
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
    long follows = 0, takes = 0;             // the LFOs that followed another tone's, took a free object
};

// A tone's port parts: the host's (v316) for the parts the port has whole, fakes for the rest, answering from the shared
// draws and its words, the calls logged as its tone's
class TonePortParts final : public Vc2ToneHost {
public:
    TonePortParts(int tone, ToneShared& shared);
    void followLfo(int lfo, int tone, const uint32_t* block) override;   // counted, then the host's
    uint32_t freeLfo(uint32_t id, int lfo) override;                     // the same
    void updateOsc(int osc, int32_t n) override;
    float oscValue(int osc) override { return std::bit_cast<float>(words.osc[(size_t)osc].value); }
    void ended() override;   // logged with the tone's state as the end routine finds it
    void renderOsc(int osc, float* buffer) override;
    void countOsc(int osc, uint32_t x) override;
    void countOscMatched(int osc, uint32_t x) override;
    uint32_t installOsc(int osc) override;
    int32_t oscKind(int osc) override { return (int32_t)words.osc[(size_t)osc].kind; }
    uint32_t oscFlag(int osc) override { return words.osc[(size_t)osc].flag; }
    void bindOsc(int osc, const Vc2ToneBinding<4>& b) override;
    uint32_t oscWave(int osc) override { return words.osc[(size_t)osc].wave; }
    void setOscWave(int osc, uint32_t wave) override { words.osc[(size_t)osc].wave = wave; }
    void noteOnOsc(int osc, const uint32_t* block, bool legato) override;
    void takePlayer(int osc, int tone) override;
    void setOsc(int osc, const uint32_t* block) override;
    Vc2ToneCosmSetUp cosmSetUp(int osc) override;
    void settingsOsc(int osc, const uint32_t* block) override;
    void legatoOsc(int osc, const uint32_t* ramp) override;
    void keyOffOsc(int osc) override;
    void waitFade() override;
    void truncated() override;
    float oscCost(int osc) override { return std::bit_cast<float>(words.osc[(size_t)osc].cost); }
    int32_t oscWord168(int osc) override;
    uint32_t oscPosition(int osc) override;

    ToneWords words {};

protected:
    void buildPart(Vc2TonePiece piece) override;     // logged
    const Vc2ToneHost& hostOf(int tone) override;
    void osEvent(uint32_t event, uint32_t flags) override;   // logged as rom.4's hook reads it (v317)

private:
    void log(ToneKind kind, int32_t part, std::array<uint32_t, 4> args = {}, bool onTone = true) {
        s_.calls.push_back({ kind, onTone ? tone() : kToneNone, part, args[0], args[1], args[2], args[3] });
    }
    // A call given an input block, logged by the block's offset (setOsc, settingsOsc, legatoOsc)
    void logBlock(ToneKind kind, int32_t part, const uint32_t* block) { log(kind, part, { blockOf(block) }); }
    uint32_t bufferOf(const float* p) const;
    uint32_t blockOf(const uint32_t* p) const;
    ToneShared& s_;
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

// rom.4's side: every part routine hooked to log into `log` and answer from `draws`, the truncation's waits by `fade`
void toneInstallRom(Vc2Fw& fw, const ToneDraws& draws, std::vector<ToneCall>& log, ToneFadeWait& fade);
// A tone's part words written to rom.4's memory, and read back
void toneWriteRom(Vc2Fw& fw, uint32_t address, const ToneWords& w);
ToneWords toneReadRom(Vc2Fw& fw, uint32_t address);
// The first call the two logs differ at; empty when they don't
std::string toneCallsDifference(const std::vector<ToneCall>& rom, const std::vector<ToneCall>& port);
