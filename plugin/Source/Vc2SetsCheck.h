#pragma once
// xt_vc2sets' shared parts (v280): the calls each side logs (C's children's routines, each with the arguments rom.4's is
// given, a call struct read through its pointer, the constructor's stacked words), what they answer (an allocator an
// object or none, a refresh a word), drawn by a generator per side from one seed; the port's parts, which read the
// voices, key tables, channels, parts and the part's bytes by kind where the draws wrote them in rom.4's memory; and the
// two sides.  XT_VC2SETS_MAIN only.
#include "Vc2CheckMain.h"
#include "Vc2Sets.h"
#include <span>
#include <vector>

// In the RAM Vc2Fw::start maps: the voices' 16 bytes, the key tables, the channels, the parts and the children
inline constexpr uint32_t kSetsVoicesAt = 0x8C030000, kSetsKeysAt = 0x8C031000, kSetsChannelsAt = 0x8C032000;
inline constexpr uint32_t kSetsPartsAt = 0x8C033000, kSetsChildrenAt = 0x8C034000;
inline constexpr uint32_t kSetsVoiceBytes = 0x60, kSetsKeyBytes = 0x40, kSetsChannelBytes = 0x100;
inline constexpr uint32_t kSetsPartBytes = 0x10, kSetsChildBytes = 0x40;
inline constexpr int kSetsObjects = 4, kSetsChildren = 64;
inline constexpr uint32_t kSetsVoiceWords = 0x34, kSetsByteAt = 0x53;   // [FW] the voice's words a call points at
inline constexpr uint32_t kSetsModesAt = 0x89998a14;                // [FW] the part's bytes by kind
inline constexpr uint32_t kSetsMatrixAt = 0x89997108;               // [FW] the part's own M
inline constexpr uint32_t kSetsControls = 0x84;                     // [FW] a channel's controllers
inline constexpr uint32_t kSetsSettingsAt = 0x89997398;             // [FW] the Matrix's settings G

enum SetMethod : int32_t { kSetAllocate, kSetConstruct, kSetEnd, kSetConvert, kSetUpdate, kSetCopy, kSetRelease,
                           kSetRefresh, kSetMethods };

// A call as either side made it: which method, the routine it reached, its arguments (r4…, a struct's three words where
// it takes one, the stacked words)
struct SetCall {
    int32_t kind;
    uint32_t routine;
    std::array<uint32_t, 8> args;
    bool operator==(const SetCall&) const = default;
};

// The port's parts: rom.4's memory read where the draws wrote it, the children's calls logged and answered
class SetPortParts final : public Vc2SetParts {
public:
    SetPortParts(Vc2Fw& fw, std::mt19937& answers, std::vector<SetCall>& log) : fw_(fw), answers_(answers), log_(log) {}
    uint32_t allocate(const Vc2SetChild& c, uint32_t number, uint32_t index) override;
    void construct(const Vc2SetChild& c, const Vc2SetChildCall& call) override;
    void end(const Vc2SetChild& c, uint32_t object) override;
    void convert(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call, uint32_t noteAt) override;
    void update(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) override;
    void copy(const Vc2SetChild& c, uint32_t object, uint32_t noteAt) override;
    void release(const Vc2SetChild& c, uint32_t object, uint32_t velocity, uint32_t noteAt) override;
    uint32_t refresh(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) override;
    Vc2SetVoice voiceAt(uint32_t channelAt) override;
    uint8_t firstByte(uint32_t object) override { return fw_.cpu.rd8(object); }
    uint32_t channelOf(uint32_t keys) override { return fw_.cpu.rd32(keys); }
    int16_t bendOf(uint32_t channel) override { return static_cast<int16_t>(fw_.cpu.rd16(channel)); }
    uint8_t controlOf(uint32_t channel, uint32_t number) override;
    uint8_t modeOf(uint32_t part, uint32_t kind) override;

private:
    void logged(int32_t kind, uint32_t routine, std::array<uint32_t, 8> args);
    Vc2Fw& fw_;
    std::mt19937& answers_;
    std::vector<SetCall>& log_;
};

// Every byte of `t` drawn
template <class T>
void setsNoise(T& t, std::mt19937& rng) {
    auto* b = reinterpret_cast<uint8_t*>(&t);
    for (std::size_t i = 0; i < sizeof t; ++i) b[i] = (uint8_t)rng();
}

// An answer of a hooked call: an allocator's object (none one time in eight), a refresh's word
uint32_t setAnswer(int32_t kind, std::mt19937& rng);

struct SetCounts {
    long events = 0;
    std::array<long, 11> byEvent {};
    std::array<long, kSetMethods> calls {};
    long noCommon = 0;
};

// The two sides: rom.4's interpreter, its hooks and log; the port's sets and M, notes, G, the part's M, answers and log
struct SetSides {
    explicit SetSides(Vc2Fw& rig);
    Vc2Fw& fw;
    Vc2Sets port {};
    Vc2Notes notes {};
    Vc2MatrixSettings g {};
    Vc2MatrixWords partM {};
    Vc2MatrixLinks partLinks {};
    std::mt19937 romAnswers {}, portAnswers {};
    std::vector<SetCall> rom {}, mine {};
    SetPortParts parts;
    SetCounts n {};
    uint32_t seed = 0;   // the answers' for the next event
    Vc2SetMatrixIn in() { return { g, partM, partLinks }; }
};

// rom.4's routine on `args` after the port's words are written into its memory, the logs cleared and the answers seeded
// alike, then the port's `mine()` giving its answer; what differs in the sets, M, the part's M, the notes, the calls or
// the answer
std::string setsOnBoth(SetSides& s, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                       const std::function<uint32_t()>& mine);

using SetEvent = std::string (*)(SetSides&, std::mt19937&);
std::span<const SetEvent> setEvents();
// The draws before an event (all at a case's first): what rom.4 reads besides the port's words, and those words
void setsDraw(SetSides& s, std::mt19937& rng, bool all);
// A set, a note, a call struct (written at kVc2Args, its pointer the answer), a voice number drawn
uint32_t setsSet(std::mt19937& rng);
uint32_t setsNote(std::mt19937& rng);
uint32_t setsCall(SetSides& s, std::mt19937& rng);
