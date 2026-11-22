#pragma once
// xt_vc2mixer's shared parts (v227): what a block draws for both sides, the calls each side logs, the port's fake
// parts and voices, and rom.4's voices in RAM with the hooks that answer for its calls.  XT_VC2MIXER_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2PartMixer.h"
#include <array>
#include <vector>

inline constexpr int kMixVoices = 3;
inline constexpr uint32_t kMixInputAt = 0x8C035000, kMixOutAt = 0x8C036000;   // in Vc2Fw's RAM
inline constexpr uint32_t kMixRingAt = 0x8C037000;    // where the hooked 0x893ed410 says the detector's ring is
inline constexpr uint32_t kMixVoicesAt = 0x8C040000, kMixVoiceStride = 0x40, kMixVtableAt = 0x8C041000;
inline constexpr uint32_t kMixMethodsAt = 0x8C042000;   // the voices' methods, each a hooked address
inline constexpr uint32_t kMixEngineAt = 0x89999778;    // [FW] the vocoder object's +0x20, its engine

// A voice's answers and what it renders for a block, drawn once for both sides
struct MixVoiceDraw {
    bool carrier = false, held = false;
    std::array<Vc2MixerFrame, Vc2PartMixer::kBlock> frames {};
};

// A block's draws: the voices, whether an engine plays, what the vocoder adds to the carriers
struct MixDraws {
    int voices = 0;
    std::array<MixVoiceDraw, kMixVoices> voice {};
    bool engine = false;
    std::array<Vc2MixerFrame, Vc2PartMixer::kBlock> vocoder {};
};

enum MixKind : int32_t {
    kInputStage, kMicRing, kUnvoiced, kDetectorRing, kVocoder, kResetTracker, kResetUnvoiced, kResetRing,
    kCarrier, kRenderCarrier, kHeld, kRender,
    kToneUpdate, kToneClock, kToneCount, kFreeLfosClock, kGlue,   // v311: the second part's, a float's bits as the arg
};
enum MixArg : int32_t { kNoBuffer, kSecondBuffer, kMicBuffer, kRingBuffer, kCarrierBuffer, kDirectBuffer, kOther };

// A call as either side made it: what, which voice (−1 none), which buffer
struct MixCall {
    int32_t kind, voice, arg;
    bool operator==(const MixCall&) const = default;
};

// The port's parts: answers from the draws, the calls logged, the detector's ring its own
class MixPortParts final : public Vc2MixerParts {
public:
    MixPortParts(const Vc2PartMixer& mixer, const MixDraws& draws) : mixer_(mixer), draws_(draws) {}
    void inputStage(const float* second) override { log(kInputStage, second); }
    void micRing(const float* mic) override { log(kMicRing, mic); }
    bool engine() override { return draws_.engine; }
    void unvoiced(const float* mic) override { log(kUnvoiced, mic); }
    float* detectorRing() override;
    void vocoder(const float* ring, Vc2MixerFrame* frames) override;
    void resetTracker() override { calls.push_back({ kResetTracker, -1, kNoBuffer }); }
    void resetUnvoiced() override { calls.push_back({ kResetUnvoiced, -1, kNoBuffer }); }
    void resetRing() override { calls.push_back({ kResetRing, -1, kNoBuffer }); }
    int32_t bufferOf(const void* p) const;
    std::vector<MixCall> calls;
    std::array<float, Vc2PartMixer::kBlock> ring {};

private:
    void log(MixKind kind, const void* p) { calls.push_back({ kind, -1, bufferOf(p) }); }
    const Vc2PartMixer& mixer_;
    const MixDraws& draws_;
};

// A port's voice: its answers and frames from the draws, its calls logged with the parts'
class MixPortVoice final : public Vc2MixerVoice {
public:
    MixPortVoice(int index, MixPortParts& parts, const MixDraws& draws) : index_(index), parts_(parts), draws_(draws) {}
    bool carrier() override { return answered(kCarrier, draw().carrier); }
    void renderCarrier(Vc2MixerFrame* frames) override { rendered(kRenderCarrier, frames); }
    bool held() override { return answered(kHeld, draw().held); }
    void render(Vc2MixerFrame* frames) override { rendered(kRender, frames); }

private:
    const MixVoiceDraw& draw() const { return draws_.voice[(size_t)index_]; }
    bool answered(MixKind kind, bool answer);              // a question logged and answered from the draws
    void rendered(MixKind kind, Vc2MixerFrame* frames);    // a render logged, the drawn frames added
    int index_;
    MixPortParts& parts_;
    const MixDraws& draws_;
};

// The draws: a frame's word (mostly the bus's range; now and then a NaN of either sign, an infinity, or a NaN beside
// it), a block's frames of them, an input word (any, a quieter one, now and then 0 or the most negative)
float mixDrawSample(std::mt19937& rng);
void mixDrawFrames(std::array<Vc2MixerFrame, Vc2PartMixer::kBlock>& frames, std::mt19937& rng);
int32_t mixDrawWord(std::mt19937& rng);
// The first call the two logs differ at, as kind, voice and buffer; empty when they don't
std::string mixCallsDifference(const std::vector<MixCall>& rom, const std::vector<MixCall>& port);
// What a render adds to a buffer, the same on both sides
void mixAdd(Vc2MixerFrame* frames, const std::array<Vc2MixerFrame, Vc2PartMixer::kBlock>& added);
// What the vocoder makes of the carriers, the same on both sides
void mixVocoder(Vc2MixerFrame* frames, const std::array<Vc2MixerFrame, Vc2PartMixer::kBlock>& added);
// rom.4's side: `count` voices linked in its list, their method table, and the mixer's calls hooked to answer from the
// draws and log into `log`
void mixInstallRom(Vc2Fw& fw, const MixDraws& draws, std::vector<MixCall>& log);
void mixLinkVoices(Vc2Fw& fw, int count);
// The second part (v311, Vc2MixerTasksCheck.cpp): the voices' update and schedule on the seed after the run's, on the
// mixer and rom.4's log of the first's; how many cases differ, its counts said
long vc2MixerTasksPart(Vc2Fw& fw, Vc2PartMixer& mixer, std::vector<MixCall>& rom, const Vc2Run& run);
