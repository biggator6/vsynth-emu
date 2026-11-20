#pragma once
// A VC-2 tone's parts' host (v316, step 4.3's forty-fourth version, item 10's twenty-first part): Vc2ToneParts for the
// parts the port has whole, joined as rom.4's tone reaches them: the tone's envelopes and LFOs (v268, their settings and
// release v315), its OSC mix (v269), the free LFOs (v271) and the part's two lists of tones (v273), with the flags a
// binding leaves on them.  The code xt_vc2tone's port side joined them with until v316.  From v317 its TVA (v233), its
// envelope and LFOs the tone's own, its fade's step the part's word.  The parts not joined yet (the OSC voices, the
// COSMs), the tail's two routines and the OS's wait and event are a deriving class's: xt_vc2tone's fakes now, the ports
// as each is joined, then Vc2Synth's 24.  Bit for bit: xt_vc2tone against rom.4's own routines.  HARDWARE_FIRMWARE.md,
// "The VC-2 tone's parts' host", "The TVA in the tone's parts' host".  The notes' task and the audio thread, as the tone.
#include "Vc2FreeLfos.h"
#include "Vc2Tone.h"
#include "Vc2ToneLists.h"
#include "Vc2ToneMix.h"
#include "Vc2ToneModulators.h"
#include "Vc2Tva.h"
#include <span>

// What every tone's parts share, each rom.4's at its own address
struct Vc2ToneCommon {
    Vc2TonePool& pool;        // 0x8999f524, its head and serial 0x8993fb48
    Vc2ToneLists& lists;      // 0x89999678, 0x899996d0, 0x899996ec
    Vc2ToneMixRings& rings;   // 0x8a0af984, 0x8a0bb384: the mixes' delay lines
    Vc2FreeLfos& frees;       // 0x8a0c6d84
    float& chaos;             // 0x89999668: the LFOs' constructors' generator
    float& fadeStep;          // 0x8993eb08: the TVAs' truncated fade's step a sample, C's TVA step (v285, v317)
};

class Vc2ToneHost : public Vc2ToneParts, private Vc2TvaParts {
public:
    Vc2ToneHost(int tone, const Vc2ToneCommon& common) : tone_(tone), c_(common) {}
    void build(Vc2TonePiece piece) override;     // the envelopes, LFOs and mix; the other parts buildPart's
    void destroy(Vc2TonePiece piece) override;   // the LFOs; the other parts destroyPart's
    bool lfoFollows(int lfo) override;
    void unfollow(int lfo) override;
    void unlist(bool fromOne) override;
    bool envelopeOn(int envelope) override;
    void tickEnvelope(int envelope, int32_t n) override;
    bool lfoOn(int lfo) override;
    void tickLfo(int lfo, int32_t n) override;
    void clockLfo(int lfo, float x) override;
    void updateMix() override {}   // [FW] 0x893ade40 returns at once
    void renderMix(const float* a, const float* b, float* c) override;
    void installMix(bool kind3) override;
    void noteOnEnvelope(int envelope, const uint32_t* block) override;
    void noteOnLfo(int lfo, const uint32_t* block, float x) override;
    void followLfo(int lfo, int tone, const uint32_t* block) override;
    uint32_t freeLfo(uint32_t id, int lfo) override;
    void attachFree(int lfo, uint32_t free) override;
    float freeValue(uint32_t free) override;
    void startFree(int lfo, const uint32_t* block, float value, float x) override;
    bool lfoListed(int lfo) override;
    uint32_t lfoFollow(int lfo) override;
    void relist(uint32_t follow, uint32_t id, int lfo) override;
    void list(uint32_t id, int lfo) override;
    void markFree(uint32_t free, uint32_t id, int lfo) override;
    void noteOnMix(const uint32_t* block, uint32_t flag1, uint32_t flag2) override;
    void enlist() override;
    void settingsLfo(uint32_t lfo, const uint32_t* block) override;
    void settingsEnvelope(uint32_t envelope, const uint32_t* block) override;
    void settingsMix(const uint32_t* block) override;
    void releaseLfo(int lfo) override;
    void releaseEnvelope(int envelope) override;
    // v317: the TVA
    bool tvaOver() override;
    bool tvaStopped() override;
    void updateTva() override;
    void renderTva(const float* in, float* out) override;
    float tvaLevel() override;
    bool tvaHeld() override;
    void bindTva(int envelope, int lfoA, int lfoB, int32_t serial) override;
    void noteOnTva(const uint32_t* block, uint32_t flag) override;
    void newBlockTva(const uint32_t* block) override;
    void truncateTvaTold() override;
    void truncateTva() override;
    float tvaFade() override;

    Vc2ToneModulators mods {};   // the tone's 14 envelopes and 8 LFOs, at its +0x1b38 and +0x2158
    Vc2ToneMix mix {};           // its OSC mix, at its +0x1ab8
    Vc2Tva tva {};               // its TVA, at its +0x19f0 (v317)

protected:
    virtual void buildPart(Vc2TonePiece piece) = 0;      // a part the host doesn't hold yet
    virtual void destroyPart(Vc2TonePiece piece) = 0;
    virtual const Vc2ToneHost& hostOf(int tone) = 0;     // another tone's: a key-unsynced LFO follows its LFO
    virtual void osEvent(uint32_t event, uint32_t flags) = 0;   // 0x89301830, the OS's: a told truncation's fade ended
    // A binding's flags, as the note-on leaves them after each: the bound envelopes on (+0x68 1), the LFOs not (+0x80 0)
    void bindFlags(std::span<const int32_t> envelopes, std::span<const int32_t> lfos);
    int tone() const { return tone_; }
    uint32_t self() const { return c_.pool.addressOf(tone_); }   // the tone's rom.4 address
    Vc2ToneTail& tail() { return c_.pool.tones[(size_t)tone_].tail; }

private:
    // What the TVA reads beyond itself (v317): its envelope and LFOs, the tone's own by rom.4's addresses, the step
    Vc2TvaEnvelope envelope(uint32_t at) override;
    float lfoOut(uint32_t at) override;
    float fadeStep() override;
    void setEvent(uint32_t event, uint32_t flags) override;
    uint32_t envelopeAt(int envelope) const;   // a bound part's rom.4 address, as the tone's note-on writes it
    uint32_t lfoAt(int lfo) const;
    int tone_;
    Vc2ToneCommon c_;
};
