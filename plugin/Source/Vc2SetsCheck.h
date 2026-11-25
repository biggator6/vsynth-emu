#pragma once
// xt_vc2sets' shared parts (v280): the calls each side logs (C's children's routines, each with the arguments rom.4's is
// given, a call struct read through its pointer, the constructor's stacked words), what they answer (an allocator an
// object or none, a refresh a word), drawn by a generator per side from one seed; the port's parts, which read the
// voices, key tables, channels, parts and the part's bytes by kind where the draws wrote them in rom.4's memory; and the
// two sides.  The children's parts' shared steps in Vc2SetsChildCheck.h (v283).  XT_VC2SETS_MAIN only.
#include "Vc2CheckMain.h"
#include "Vc2RomShared.h"
#include "Vc2SetMemoryReads.h"   // v341
#include "Vc2Sets.h"
#include "Vc2WaveSets.h"   // v398, v399
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

// In the RAM Vc2Fw::start maps: the voices' 16 bytes, the key tables, the channels, the parts and the children
inline constexpr uint32_t kSetsVoicesAt = 0x8C030000, kSetsKeysAt = 0x8C031000, kSetsChannelsAt = 0x8C032000;
inline constexpr uint32_t kSetsPartsAt = 0x8C033000, kSetsChildrenAt = 0x8C034000;
inline constexpr uint32_t kSetsVoiceBytes = 0x60, kSetsKeyBytes = 0x40, kSetsChannelBytes = 0x100;
inline constexpr uint32_t kSetsPartBytes = 0x10, kSetsChildBytes = 0x40;
inline constexpr int kSetsObjects = 4, kSetsChildren = 64;
inline constexpr uint32_t kSetsVoiceWords = 0x34, kSetsByteAt = 0x53;   // [FW] the voice's words a call points at
inline constexpr uint32_t kSetsMatrixAt = 0x89997108;               // [FW] the part's own M
inline constexpr uint32_t kSetsSettingsAt = 0x89997398;             // [FW] the Matrix's settings G
inline constexpr uint32_t kSetsSumsAt = 0x8C035000;                  // in Vc2Fw's RAM: the Matrix's sums drawn
// v341: the part's addresses C reads, the plugin's (Vc2SetMemoryReads.h), by the names the draws use
inline constexpr uint32_t kSetsModesAt = kVc2SetModesAt, kSetsControls = kVc2SetControls;
inline constexpr uint32_t kSetsCommonsAt = kVc2SetCommonsAt, kSetsCommonBytes = kVc2SetCommonBytes;
inline constexpr uint32_t kSetsClockAt = kVc2SetClockAt;
using SetPartTable = Vc2SetPartTable;
inline constexpr SetPartTable kSetsEnvelopeTable = kVc2SetEnvelopeTable, kSetsLfoTable = kVc2SetLfoTable;
inline constexpr SetPartTable kSetsMixTable = kVc2SetMixTable, kSetsTvaTable = kVc2SetTvaTable;
inline constexpr SetPartTable kSetsCosmTable = kVc2SetCosmTable;
inline constexpr uint32_t kSetsTvaFlagAt = kVc2SetTvaFlagAt, kSetsTrailerAt = kVc2SetTrailerAt;
inline constexpr uint32_t kSetsControlsAt = kVc2SetMatrixControls;
// In Vc2Fw's RAM, the Ms the TVA's draws aim at (their controllers at +0x178)
inline constexpr uint32_t kSetsTvaMatricesAt = 0x8C036000, kSetsTvaMatrixBytes = 0x200;

// The calls logged: C's children's methods, a Matrix group's reader (v282, the envelopes' part) and an OSC kind's routine
// not ported (v391)
enum SetMethod : int32_t { kSetAllocate, kSetConstruct, kSetEnd, kSetConvert, kSetUpdate, kSetCopy, kSetRelease,
                           kSetRefresh, kSetMatrixSum, kSetOscKind, kSetMethods };

// A call as either side made it: which method, the routine it reached, its arguments (r4…, a struct's three words where
// it takes one, the stacked words)
struct SetCall {
    int32_t kind;
    uint32_t routine;
    std::array<uint32_t, 8> args;
    bool operator==(const SetCall&) const = default;
};

// rom.4's shared routines for the port's side, the interpreter's registers kept around each (v390; the TVA's curve's way
// from v285): the port runs after rom.4's routine, whose answer is read from r0 after it
class SetKeptShared final : public Vc2ToneShared {
public:
    SetKeptShared(Vc2Fw& fw, const RomShared& rom) : fw_(fw), rom_(rom) {}
    int velocity(int v, int sens, int curve) const override {
        return kept([&] { return rom_.velocity(v, sens, curve); });
    }
    float exp2(float x) const override {
        return kept([&] { return rom_.exp2(x); });
    }
    float log(float x) const override {
        return kept([&] { return rom_.log(x); });
    }
    float pow(float x, float y) const override {
        return kept([&] { return rom_.pow(x, y); });
    }

private:
    template <class F>
    std::invoke_result_t<F> kept(F f) const {
        uint32_t r[16];
        std::memcpy(r, fw_.cpu.r, sizeof r);
        const auto v = f();
        std::memcpy(fw_.cpu.r, r, sizeof r);
        return v;
    }
    Vc2Fw& fw_;
    const RomShared& rom_;
};

// The port's parts: rom.4's memory read where the draws wrote it (from v341 by the plugin's Vc2SetMemoryReads, on
// rom.4's memory), the children's calls logged and answered
class SetPortParts final : public Vc2SetMemoryReads {
public:
    SetPortParts(Vc2Fw& fw, std::mt19937& answers, std::vector<SetCall>& log)
        : fw_(fw), answers_(answers), log_(log), shared_(fw), kept_(fw, shared_) {}
    uint32_t allocate(const Vc2SetChild& c, uint32_t number, uint32_t index) override;
    void construct(const Vc2SetChild& c, const Vc2SetChildCall& call) override;
    void end(const Vc2SetChild& c, uint32_t object) override;
    void convert(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call, uint32_t noteAt) override;
    void update(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) override;
    void copy(const Vc2SetChild& c, uint32_t object, uint32_t noteAt) override;
    void release(const Vc2SetChild& c, uint32_t object, uint32_t velocity, uint32_t noteAt) override;
    uint32_t refresh(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) override;
    // The TVA's (v285, Vc2SetTvaCheck.cpp): the velocity curve rom.4's own in the interpreter
    int velocityCurve(int velocity, int sens, int curve) override;
    int16_t matrixSum(uint32_t reader, uint32_t matrix, uint32_t offset) override;
    // The COSM inputs' (v390, Vc2SetCosmInputCheck.cpp): the shared routines rom.4's own, a reader's call logged and
    // answered as the hooked readers answer it
    const Vc2ToneShared& shared() override { return kept_; }
    const int16_t* matrixGroup(uint32_t reader, uint32_t matrix) override;
    // The OSC blocks' (v391, Vc2SetOscBlockCheck.cpp): the step counts the port's own (`oscSteps`), the groups and the
    // pitch source unlogged, a kind's routine not ported logged (from v397 none: the wave store's routines PCM's start
    // calls, logged, answered and filling the wave's words from the answers, as their hooks do)
    uint16_t nextOscStep(uint32_t part, uint32_t kind, uint32_t osc) override;
    Vc2MatrixGroups oscGroups(uint32_t matrix, uint32_t osc) override;
    float pitchSource() override;
    // v398: the store the port's own (`store`, written into rom.4's memory with the blocks before each event), the
    // memory rom.4's, where the draws put the records, blocks, events' tables and sets' data (v396's events' halfwords
    // too), at most 65,536 bytes read a use (a bound a planted error's endless walk meets); v399: the sets the port's
    // own (`sets`) as the store is
    const Vc2WaveStore& waveStore() override { return store; }
    const Vc2WaveMemory& waveMemory() override;
    Vc2WaveSets& waveSets() override { return sets; }
    std::array<uint16_t, 512> oscSteps {};   // the step counts at 0x89989c84, 16 parts' 64 bytes
    Vc2WaveStore store {};
    Vc2WaveSets sets {};

protected:
    void read(uint32_t at, void* to, size_t bytes) override { fw_.cpu.read(at, to, bytes); }

private:
    // rom.4's memory as the store's, its reads counted since the last use
    class FwMemory final : public Vc2WaveMemory {
    public:
        explicit FwMemory(Vc2Fw& fw) : fw_(fw) {}
        uint8_t byte(uint32_t at) const override;
        mutable long reads = 0;

    private:
        Vc2Fw& fw_;
    };
    void logged(int32_t kind, uint32_t routine, std::array<uint32_t, 8> args);
    Vc2Fw& fw_;
    std::mt19937& answers_;
    std::vector<SetCall>& log_;
    RomShared shared_;
    SetKeptShared kept_;
    std::array<int16_t, 64> group_ {};   // a reader's answer, as the hooked readers give it from kSetsSumsAt
    FwMemory memory_ { fw_ };
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
// The COSM inputs' (v390, Vc2SetCosmInputCheck.cpp)
long vc2SetCosmInputPart(SetSides& s, const Vc2Run& run);
// The OSC blocks' (v391, Vc2SetOscBlockCheck.cpp)
long vc2SetOscBlockPart(SetSides& s, const Vc2Run& run);
// The further parts in turn
long vc2SetsFurther(SetSides& s, const Vc2Run& run);
