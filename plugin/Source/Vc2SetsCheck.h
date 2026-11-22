#pragma once
// xt_vc2sets' shared parts (v280): the calls each side logs (C's children's routines, each with the arguments rom.4's is
// given, a call struct read through its pointer, the constructor's stacked words), what they answer (an allocator an
// object or none, a refresh a word), drawn by a generator per side from one seed; the port's parts, which read the
// voices, key tables, channels, parts and the part's bytes by kind where the draws wrote them in rom.4's memory; and the
// two sides.  The children's parts' shared steps in Vc2SetsChildCheck.h (v283).  XT_VC2SETS_MAIN only.
#include "Vc2CheckMain.h"
#include "Vc2RomShared.h"
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
inline constexpr uint32_t kSetsCommonsAt = 0x89997090, kSetsCommonBytes = 6;   // [FW] the parts' words for the common
inline constexpr uint32_t kSetsClockAt = 0x8995cb40;                // [FW] the clock object (v282)
inline constexpr uint32_t kSetsSumsAt = 0x8C035000;                  // in Vc2Fw's RAM: the Matrix's sums drawn

// [FW] the parts' words for a child, by part, kind and index (an index's the words' count apart): where they start, a
// part's and a kind's stride, how many a kind has; the envelopes' (v282), the LFOs' (v283) and the mix's (v284)
struct SetPartTable {
    uint32_t at, part, kind, perKind;
};
inline constexpr SetPartTable kSetsEnvelopeTable { 0x89e2765c, 2080, 130, 13 };
inline constexpr SetPartTable kSetsLfoTable { 0x89e2f85c, 800, 50, 5 };
inline constexpr SetPartTable kSetsMixTable { 0x89e32a60, 352, 22, 1 };
inline constexpr SetPartTable kSetsTvaTable { 0x89e4987c, 384, 24, 1 };
// [FW] the TVA's globals (v285): the part's controllers' flag, the part table's trailer; in Vc2Fw's RAM, the Ms its
// draws aim at (their controllers at +0x178)
inline constexpr uint32_t kSetsTvaFlagAt = 0x89997fcc, kSetsTrailerAt = 0x89e4b07c, kSetsControlsAt = 0x178;
inline constexpr uint32_t kSetsTvaMatricesAt = 0x8C036000, kSetsTvaMatrixBytes = 0x200;

// The calls logged: C's children's methods, and a Matrix group's reader (v282, the envelopes' part)
enum SetMethod : int32_t { kSetAllocate, kSetConstruct, kSetEnd, kSetConvert, kSetUpdate, kSetCopy, kSetRelease,
                           kSetRefresh, kSetMatrixSum, kSetMethods };

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
    SetPortParts(Vc2Fw& fw, std::mt19937& answers, std::vector<SetCall>& log)
        : fw_(fw), answers_(answers), log_(log), shared_(fw) {}
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
    std::array<uint16_t, 3> commonOf(uint32_t part) override;
    std::array<uint16_t, 5> envelopeOf(uint32_t part, uint32_t kind, uint32_t index) override;
    std::array<uint16_t, 5> lfoOf(uint32_t part, uint32_t kind, uint32_t index) override;
    std::array<uint16_t, 11> mixOf(uint32_t part, uint32_t kind) override;
    // The TVA's (v285, Vc2SetTvaCheck.cpp): the velocity curve rom.4's own in the interpreter
    std::array<uint16_t, 12> tvaOf(uint32_t part, uint32_t kind) override;
    int velocityCurve(int velocity, int sens, int curve) override;
    uint8_t matrixControl(uint32_t matrix, uint32_t n) override;
    Vc2SetTvaGlobals tvaGlobals() override;
    Vc2SetClock clock() override;
    int16_t matrixSum(uint32_t reader, uint32_t matrix, uint32_t offset) override;

private:
    void logged(int32_t kind, uint32_t routine, std::array<uint32_t, 8> args);
    // The part's N words for a child by `t`, where the draws wrote them
    template <size_t N>
    std::array<uint16_t, N> partWords(const SetPartTable& t, uint32_t part, uint32_t kind, uint32_t index) {
        std::array<uint16_t, N> words;
        fw_.cpu.read(t.at + t.part * part + t.kind * kind + sizeof words * index, words.data(), sizeof words);
        return words;
    }
    Vc2Fw& fw_;
    std::mt19937& answers_;
    std::vector<SetCall>& log_;
    RomShared shared_;
};

// Every byte of `t` drawn
template <class T>
void setsNoise(T& t, std::mt19937& rng) {
    auto* b = reinterpret_cast<uint8_t*>(&t);
    for (std::size_t i = 0; i < sizeof t; ++i) b[i] = (uint8_t)rng();
}

// One of `values`, drawn (any draws in them made first, left to right)
template <class T, size_t N>
T setsOneOf(const std::array<T, N>& values, std::mt19937& rng) {
    return values[rng() % N];
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

// The further parts, each on the seed after the last's, saying its counts; how many of its cases differ: the common's
// (v281, Vc2SetCommonCheck.cpp)
long vc2SetCommonPart(SetSides& s, const Vc2Run& run);
// The envelopes' (v282, Vc2SetEnvelopeCheck.cpp), the LFOs' (v283, Vc2SetLfoCheck.cpp)
long vc2SetEnvelopePart(SetSides& s, const Vc2Run& run);
long vc2SetLfoPart(SetSides& s, const Vc2Run& run);
// The mix's (v284, Vc2SetMixCheck.cpp)
long vc2SetMixPart(SetSides& s, const Vc2Run& run);
// The TVA's (v285, Vc2SetTvaCheck.cpp)
long vc2SetTvaPart(SetSides& s, const Vc2Run& run);
// The further parts in turn
long vc2SetsFurther(SetSides& s, const Vc2Run& run);
