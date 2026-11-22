#pragma once
// A VC-2 tone's parts' host (v316, step 4.3's forty-fourth version, item 10's twenty-first part): Vc2ToneParts for the
// parts the port has whole, joined as rom.4's tone reaches them: the tone's envelopes and LFOs (v268, their settings and
// release v315), its OSC mix (v269), the free LFOs (v271) and the part's two lists of tones (v273), with the flags a
// binding leaves on them.  The code xt_vc2tone's port side joined them with until v316.  From v317 its TVA (v233), its
// envelope and LFOs the tone's own, its fade's step the part's word.  From v318 its two COSMs (v249…v264): their objects,
// the note-on frame and each one's sound, the host their bound.  From v319 its two OSC voices (v230…v267), every part
// the tone has: what a voice reads beyond the tone (a kind's routines no port holds yet, by rom.4's address; the OS's
// clock; the wave's records; the mic ring and the unvoiced decision), another tone's host, the tail's two routines and
// the OS's wait and event are a deriving class's: xt_vc2tone's fakes now, then Vc2Synth's 24.  Bit for bit: xt_vc2tone
// against rom.4's own routines.  HARDWARE_FIRMWARE.md, "The VC-2 tone's parts' host", "The TVA in the tone's parts'
// host", "The COSMs in the tone's parts' host", "The OSC voices in the tone's parts' host".  The notes' task and the
// audio thread, as the tone.
#include "Vc2CosmDsp.h"
#include "Vc2CosmObject.h"
#include "Vc2CosmPools.h"
#include "Vc2FreeLfos.h"
#include "Vc2OscVoice.h"
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
    Vc2CosmPools& pools;      // 0x89999290: the COSMs' buffer pools (v318)
    // 0x89998e20: the three words after the renders' buffer c, the part mixer's frames', which a COSM's render reads
    // past its frames (a stereo Side Band Filter's follower; v318)
    const float* past;
    // The routines the OSC voices share (v319): rom.4's velocity curve 0x893a7fb0, exp2 0x893a8340, logf and powf
    const Vc2ToneShared& shared;
};

class Vc2ToneHost;

// An OSC voice's parts in the tone (v319): its address and the tone's own envelopes' and LFOs' words the host's; every
// other read and call the deriving class's `oscOutside` (Vc2ToneHost)
class Vc2ToneOscLink final : public Vc2OscParts {
public:
    Vc2ToneOscLink(Vc2ToneHost& host, int osc) : host_(host), osc_(osc) {}
    uint32_t renderBy(uint32_t routine, float* out, uint32_t third) override;
    void seek(uint32_t routine, int32_t at) override;
    int32_t osClock() override;
    Vc2MicRing& micRing() override;
    const float* unvoiced() override;
    float inputPeak() override;
    const int32_t* inputFrames() override;   // v322
    Vc2OscSamples waveSamples() override;    // v323
    float boundFloat(uint32_t part, uint32_t offset) override;   // the tone's envelope's or LFO's; another's outside
    uint32_t address() override;                                 // the voice's, at the tone's +0x20 or +0xba4
    int32_t eventPosition(uint32_t event) override;
    void objectCall(uint32_t routine) override;   // v330: VariPhrase's players' set-ups, outside
    Vc2OscWaveHeader waveHeader(uint32_t at) override;
    Vc2OscLoopRecord loopRecord(uint32_t at) override;

private:
    Vc2OscParts& outside() const;
    Vc2ToneHost& host_;
    int osc_;
};

class Vc2ToneHost : public Vc2ToneParts, private Vc2TvaParts, private Vc2CosmBound {
public:
    Vc2ToneHost(int tone, const Vc2ToneCommon& common);
    Vc2ToneHost(const Vc2ToneHost&) = delete;   // its note-on frame and its voices' links hold the host
    Vc2ToneHost& operator=(const Vc2ToneHost&) = delete;
    void build(Vc2TonePiece piece) override;     // every part (v319)
    void destroy(Vc2TonePiece piece) override;   // the LFOs and (v318) the COSMs, all a tone takes down
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
    // v318: the COSMs
    void updateCosm(int cosm, float x) override;
    void renderCosm(int cosm, float* buffer) override;
    uint32_t installCosm(int cosm, uint32_t flag) override;
    void bindCosm(int cosm, const Vc2ToneBinding<2>& b) override;
    void setUpCosm(int cosm, const Vc2ToneCosmSetUp& s, uint32_t flag) override;
    void startCosm(int cosm, const uint32_t* block, int which) override;
    void newBlockCosm(int cosm, const uint32_t* block) override;
    float cosmCost() override;
    // v319: the OSC voices (Vc2ToneHostOsc.cpp)
    void updateOsc(int osc, int32_t n) override;
    float oscValue(int osc) override;
    void renderOsc(int osc, float* buffer) override;
    void countOsc(int osc, uint32_t x) override;
    void countOscMatched(int osc, uint32_t x) override;
    void installOsc(int osc) override;
    int32_t oscKind(int osc) override;
    uint32_t oscFlag(int osc) override;
    void bindOsc(int osc, const Vc2ToneBinding<4>& b) override;
    uint32_t oscWave(int osc) override;
    void setOscWave(int osc, uint32_t wave) override;
    void noteOnOsc(int osc, const uint32_t* block, bool legato) override;
    void takePlayer(int osc, int tone) override;
    void setOsc(int osc, const uint32_t* block) override;
    Vc2ToneCosmSetUp cosmSetUp(int osc) override;
    void settingsOsc(int osc, const uint32_t* block) override;
    void legatoOsc(int osc, const uint32_t* ramp) override;
    void keyOffOsc(int osc) override;
    float oscCost(int osc) override;
    int32_t oscWord168(int osc) override;
    uint32_t oscPosition(int osc) override;

    Vc2ToneModulators mods {};   // the tone's 14 envelopes and 8 LFOs, at its +0x1b38 and +0x2158
    Vc2ToneMix mix {};           // its OSC mix, at its +0x1ab8
    Vc2Tva tva {};               // its TVA, at its +0x19f0 (v317)
    std::array<Vc2CosmObjectWords, 2> cosms {};   // its COSMs, at its +0x1728 and +0x188c (v318)
    std::array<Vc2CosmDsp, 2> sounds;             // each COSM's sound, which rom.4 keeps in the object's D and a slot
    std::array<Vc2OscVoice, 2> oscs {};           // its OSC voices, at its +0x20 and +0xba4 (v319)

protected:
    virtual const Vc2ToneHost& hostOf(int tone) = 0;     // another tone's: a key-unsynced LFO follows its LFO
    virtual void osEvent(uint32_t event, uint32_t flags) = 0;   // 0x89301830, the OS's: a told truncation's fade ended
    // What OSC voice `osc` reads and calls beyond the tone (v319): every call of its Vc2OscParts but its address and the
    // tone's own envelopes' and LFOs' words
    virtual Vc2OscParts& oscOutside(int osc) = 0;
    // A binding's flags, as the note-on leaves them after each: the bound envelopes on (+0x68 1), the LFOs not (+0x80 0)
    void bindFlags(std::span<const int32_t> envelopes, std::span<const int32_t> lfos);
    int tone() const { return tone_; }
    uint32_t self() const { return c_.pool.addressOf(tone_); }   // the tone's rom.4 address
    Vc2ToneTail& tail() { return c_.pool.tones[(size_t)tone_].tail; }

private:
    friend class Vc2ToneOscLink;
    // What the TVA reads beyond itself (v317): its envelope and LFOs, the tone's own by rom.4's addresses, the step
    Vc2TvaEnvelope envelope(uint32_t at) override;
    float lfoOut(uint32_t at) override;
    float fadeStep() override;
    void setEvent(uint32_t event, uint32_t flags) override;
    // What a COSM's frame and sound read beyond the object (v318): a float of the tone's envelopes or LFOs by address
    float word(uint32_t at) override;
    uint32_t envelopeAt(int envelope) const;   // a bound part's rom.4 address, as the tone's note-on writes it
    uint32_t lfoAt(int lfo) const;
    Vc2OscVoice& voice(int osc) { return oscs[(size_t)osc]; }
    Vc2OscParts& link(int osc) { return links_[(size_t)osc]; }
    int tone_;
    Vc2ToneCommon c_;
    Vc2CosmNoteOn cosmFrame_;   // the COSMs' note-on frame, on the object given each call
    // A COSM's note-on's block, its sound's note-on waiting for the install the tone's note-on ends with: the sound
    // takes the install's choice of mono or stereo (rom.4's frame keeps that state in the object, chosen there)
    std::array<Vc2CosmBlockWords, 2> started_ {};
    std::array<bool, 2> pending_ {};
    std::array<Vc2ToneOscLink, 2> links_;   // each voice's parts (v319)
};
