#pragma once
// xt_vc2tone's shared parts (v228, v229): the calls each side logs, what the hooked routines answer (drawn once for both
// sides), each tone's part words (the port's fake parts' mirror, rom.4's memory), the port's fake parts and their owner,
// and rom.4's side: every part routine hooked to log and answer.  XT_VC2TONE_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2Tone.h"
#include "Vc2ToneModulators.h"
#include <array>
#include <span>
#include <vector>

inline constexpr uint32_t kToneOutAt = 0x8C036000;     // the renders' output, in Vc2Fw's RAM
inline constexpr uint32_t kToneInputAt = 0x8C050000;   // the note-on's input, in Vc2Fw's RAM
inline constexpr uint32_t kToneFreeAt = 0x8C052000;    // the free objects 0x893ad640 gives, 0x20 apart by LFO number
inline constexpr uint32_t kToneFreeStride = 0x20;
inline constexpr uint32_t kToneHooksAt = 0x8C042000;   // the hooked routines the tones' words point at, in Vc2Fw's RAM
inline constexpr uint32_t kToneEndedAt = kToneHooksAt + 0x20;   // the end routine, the tail's word on both sides
inline constexpr uint32_t kToneBufferAt = 0x89998b20;  // [FW] the renders' three scratch buffers, 0x100 bytes apart
inline constexpr uint32_t kToneListsAt = 0x89999678;   // [FW] the two +4 lists' heads the note-on searches
inline constexpr int kToneNone = -1, kToneUnknown = -2;   // a call's part: the tone itself, or no part known
inline constexpr int kToneOut = 4;

enum ToneKind : int32_t {
    kBuild, kDestroy, kUnfollow, kUnlist, kUpdateOsc, kUpdateMix, kUpdateCosm,
    kTvaOver, kTvaStopped, kUpdateTva, kEnded, kRenderOsc, kRenderMix, kRenderCosm, kRenderTva, kCountOsc,
    kCountOscMatched, kTvaHeld, kInstallOsc, kInstallMix, kInstallCosm, kFollowLfo,
    kFreeLfo, kAttachFree, kStartFree, kRelist, kList, kBindCosm, kNoteOnOsc, kTakePlayer, kSetOsc, kNoteOnMix,
    kSetUpCosm, kStartCosm, kNoteOnTva, kEnlist,
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
    bool tvaOver = false, tvaStopped = false, tvaHeld = false;
    std::array<uint32_t, 2> installOsc {}, installCosm {};
    std::array<float, Vc2Tone::kLfos> freeValue {};   // each free object's +0x10
};

// [FW] an OSC voice's words the tone reads or writes: +0x54, +0xa40, +0x1cc, +0xa80, the byte +0x58, +0x104, and the
// addresses of its four envelopes and four LFOs at +0xa4c…+0xa68
struct ToneOscWords {
    uint32_t kind = 0, flag = 0, value = 0, other = 0, byte = 0, wave = 0;
    std::array<uint32_t, 8> binds {};
    bool operator==(const ToneOscWords&) const = default;
};

// [FW] a tone's hooked parts' words the tone reads or writes: the OSC voices', the TVA's +0x40 and its +0xa4, +0xa8,
// +0xac and +0xc0 (the envelopes and LFOs are the real ones from v268, Vc2ToneModulators)
struct ToneWords {
    std::array<ToneOscWords, 2> osc {};
    uint32_t tvaLevel = 0;
    std::array<uint32_t, 4> tvaBinds {};
    bool operator==(const ToneWords&) const = default;
};

int32_t tonePartCode(Vc2TonePiece piece, int index);   // a part as a call names it
uint32_t toneEnvelopeAt(uint32_t tone, int envelope);  // rom.4's address of a tone's envelope, LFO
uint32_t toneLfoAt(uint32_t tone, int lfo);

// What the port's parts share: the answers, the buffers and output, the note-on's input, the free objects' +0 and +4,
// the calls, the pool, the lists' heads
struct ToneShared {
    ToneDraws draws {};
    Vc2ToneBuffers buffers {};
    std::array<float, Vc2ToneBuffers::kFloats> out {};
    Vc2ToneInput input {};
    std::array<std::array<uint32_t, 2>, Vc2Tone::kLfos> free {};
    std::vector<ToneCall> calls {};
    const Vc2TonePool* pool = nullptr;
    std::array<uint32_t, 2> lists {};
    float chaos = Vc2ToneLfo::kChaosStart;   // the LFOs' constructors' generator, rom.4's word 0x89999668
};

// A tone's port parts: answers from the shared draws and its words, the calls logged as its tone's
class TonePortParts final : public Vc2ToneParts {
public:
    TonePortParts(int tone, ToneShared& shared) : tone_(tone), s_(shared) {}
    void build(Vc2TonePiece piece) override;
    void destroy(Vc2TonePiece piece) override;
    bool lfoFollows(int lfo) override { return mods.lfos[(size_t)lfo].w.follow != 0; }
    void unfollow(int lfo) override;
    void unlist(bool fromOne) override;
    bool envelopeOn(int envelope) override { return mods.envelopes[(size_t)envelope].w.on != 0; }
    void tickEnvelope(int envelope, int32_t n) override;
    bool lfoOn(int lfo) override { return mods.lfos[(size_t)lfo].w.on != 0; }
    void tickLfo(int lfo, int32_t n) override;
    void clockLfo(int lfo, float x) override;
    void updateOsc(int osc, int32_t n) override;
    void updateMix() override;
    float oscValue(int osc) override { return std::bit_cast<float>(words.osc[(size_t)osc].value); }
    void updateCosm(int cosm, float x) override;
    bool tvaOver() override { return answered(kTvaOver, s_.draws.tvaOver); }
    bool tvaStopped() override { return answered(kTvaStopped, s_.draws.tvaStopped); }
    void updateTva() override;
    void ended() override;   // logged with the tone's state as the end routine finds it
    void renderOsc(int osc, float* buffer) override;
    void renderMix(const float* a, const float* b, float* c) override;
    void renderCosm(int cosm, float* buffer) override;
    void renderTva(const float* in, float* out) override;
    void countOsc(int osc, uint32_t x) override;
    void countOscMatched(int osc, uint32_t x) override;
    float tvaLevel() override { return std::bit_cast<float>(words.tvaLevel); }
    bool tvaHeld() override { return answered(kTvaHeld, s_.draws.tvaHeld); }
    uint32_t installOsc(int osc) override;
    int32_t oscKind(int osc) override { return (int32_t)words.osc[(size_t)osc].kind; }
    uint32_t oscFlag(int osc) override { return words.osc[(size_t)osc].flag; }
    void installMix(bool kind3) override;
    uint32_t installCosm(int cosm, uint32_t flag) override;
    void noteOnEnvelope(int envelope, const uint32_t* block) override;
    void noteOnLfo(int lfo, const uint32_t* block, float x) override;
    void followLfo(int lfo, int tone, const uint32_t* block) override;
    uint32_t freeLfo(uint32_t id, int lfo) override;
    void attachFree(int lfo, uint32_t free) override;
    float freeValue(uint32_t free) override;
    void startFree(int lfo, const uint32_t* block, float value, float x) override;
    bool lfoListed(int lfo) override { return mods.lfos[(size_t)lfo].w.sync != 0; }
    uint32_t lfoFollow(int lfo) override { return mods.lfos[(size_t)lfo].w.follow; }
    void relist(uint32_t follow, uint32_t id, int lfo) override;
    void list(uint32_t id, int lfo) override;
    void markFree(uint32_t free, uint32_t id, int lfo) override;
    void bindOsc(int osc, const Vc2ToneBinding<4>& b) override;
    void bindCosm(int cosm, const Vc2ToneBinding<2>& b) override;
    void bindTva(int envelope, int lfoA, int lfoB, int32_t serial) override;
    uint32_t oscWave(int osc) override { return words.osc[(size_t)osc].wave; }
    void setOscWave(int osc, uint32_t wave) override { words.osc[(size_t)osc].wave = wave; }
    void noteOnOsc(int osc, const uint32_t* block, bool legato) override;
    void takePlayer(int osc, int tone) override;
    void setOsc(int osc, const uint32_t* block) override;
    void noteOnMix(const uint32_t* block, uint32_t flag1, uint32_t flag2) override;
    Vc2ToneCosmSetUp cosmSetUp(int osc) override;
    void setUpCosm(int cosm, const Vc2ToneCosmSetUp& s, uint32_t flag) override;
    void startCosm(int cosm, const uint32_t* block, int which) override;
    void noteOnTva(const uint32_t* block, uint32_t flag) override;
    void enlist() override;

    ToneWords words {};
    Vc2ToneModulators mods {};   // the real envelopes and LFOs

private:
    void log(ToneKind kind, int32_t part, std::array<uint32_t, 4> args = {}, bool onTone = true) {
        s_.calls.push_back({ kind, onTone ? tone_ : kToneNone, part, args[0], args[1], args[2], args[3] });
    }
    bool answered(ToneKind kind, bool answer);
    uint32_t bufferOf(const float* p) const;
    uint32_t blockOf(const uint32_t* p) const;
    void setFlags(std::span<const int32_t> envelopes, std::span<const int32_t> lfos);
    int tone_;
    ToneShared& s_;
};

// The port's owner: the shared lists' heads and each tone's parts
class TonePortOwner final : public Vc2ToneOwner {
public:
    TonePortOwner(ToneShared& shared, std::vector<TonePortParts>& parts) : s_(shared), parts_(parts) {}
    std::array<uint32_t, 2> lists() override { return s_.lists; }
    Vc2ToneParts& partsOf(int tone) override { return parts_[(size_t)tone]; }

private:
    ToneShared& s_;
    std::vector<TonePortParts>& parts_;
};

// rom.4's side: every part routine hooked to log into `log` and answer from `draws`
void toneInstallRom(Vc2Fw& fw, const ToneDraws& draws, std::vector<ToneCall>& log);
// A tone's part words written to rom.4's memory, and read back; COSM 1's vtable and the DSP routines' pointers too
void toneWriteRom(Vc2Fw& fw, uint32_t address, const ToneWords& w);
ToneWords toneReadRom(Vc2Fw& fw, uint32_t address);
// The first call the two logs differ at; empty when they don't
std::string toneCallsDifference(const std::vector<ToneCall>& rom, const std::vector<ToneCall>& port);
