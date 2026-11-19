#pragma once
// xt_vc2settings' shared parts (v286): a getter's call as either side logs it (the read object's block, part and kind,
// and the ID), the store both sides answer from, each side's reads (its store, its copy of the held flags, the read
// after which a flag is set, its log), the port's parameters on them, and the two sides; from v288 the tables'
// addresses, the events by what a routine is given and a case of them, the parts' (the third in
// Vc2PartSettingsOscCheck.cpp); from v295 the calls' log and the port's calls out in Vc2PartSettingsCalls.h.
// XT_VC2SETTINGS_MAIN only.
#include "Vc2CheckCalls.h"
#include "Vc2CheckMain.h"
#include "Vc2LibMathTables.h"
#include "Vc2MatrixTables.h"
#include "Vc2PartSettingsCalls.h"
#include "Vc2PartSettingsTables.h"
#include <span>

// [FW] the tables, rom.4's
inline constexpr uint32_t kCommonsAt = 0x89997090, kEnvelopesAt = 0x89e2765c, kLfosAt = 0x89e2f85c;
inline constexpr uint32_t kMixesAt = 0x89e32a60, kTvasAt = 0x89e4987c, kControlsAt = 0x89997fcc;
inline constexpr uint32_t kBytesAt = 0x89998a14, kCosmsAt = 0x89e2003c;   // v287's
inline constexpr uint32_t kOscsAt = 0x89e371e0;                            // v288's
inline constexpr uint32_t kStepsAt = 0x89989c84;                           // v289's: part 0's step counts
inline constexpr uint32_t kRobotAt = 0x89997fdc, kUnvoiceLevelAt = 0x899d9968;   // v290's
inline constexpr uint32_t kModesAt = 0x89995390, kNodesAt = 0x899888d0;           // v291's: the mode rows, the nodes
inline constexpr uint32_t kMatrixAt = 0x89997398;                                 // v292's: G, the Matrix's settings
inline constexpr uint32_t kHeldNoteAt = 0x899891d0;                               // v294's: the held note's word
// A read object's handle the set-ups' hooks leave: this, the block << 16, part << 8, kind << 2, COSM; and (v291) the
// one rom.4's stack is given where Auto Note's switch reads through a read object never set up, a block past the
// OSC block's call
inline constexpr uint32_t kSettingsHandle = 0x01000000;
inline constexpr int32_t kLeftoverCall = 9;
inline constexpr uint32_t kLeftoverHandle = kSettingsHandle | (uint32_t)kLeftoverCall << 16;
inline constexpr int kSettingsParts = 16, kSettingsKinds = 16, kEventsACase = 400;
// [FW] The OSC block's reader, hooked in the first three parts (v288), and its call as the log has it then, a block
// past the read objects'
inline constexpr uint32_t kOscBlock = 0x893a04e0;
inline constexpr int32_t kOscBlockCall = 8;

// The parameters' values, one draw per event: by a hash of the seed and the read, mostly a byte's or a depth's, now
// and then at the laws' edges or any word; the OSC block's reader's answer mostly 1, now and then 0 or any word; a
// part's own IDs drawn within a few values (v291's, after the run's)
// An ID drawn within `values` from `from` when the draw's pick (0…7) is below `picks`: 4 half the time, 8 always (v292)
struct SettingsNarrow {
    int16_t id;
    uint32_t values, picks;
    int32_t from = 0;
};
inline constexpr uint32_t kHalfTheTime = 4, kAlways = 8;
struct SettingsStore {
    uint32_t seed = 0;
    std::span<const SettingsNarrow> narrow {};
    int32_t value(const SettingsCall& c) const;
};

// A side's reads in an event: the getter answers from the store, logs, and after the `flipAt`-th read sets the flag
// `flipFlag` (rom.4's side in its memory too, by its hook)
struct SettingsReads {
    SettingsStore store {};
    std::array<uint8_t, 5> flags {};
    int flipAt = 0, flipFlag = 0, reads = 0;
    std::vector<SettingsCall> log {};
    int32_t read(const SettingsCall& c);
};

class SettingsPortParams final : public Vc2PartParams {
public:
    explicit SettingsPortParams(SettingsReads& reads) : reads_(reads) {}
    int32_t value(Vc2ParamBlock block, int part, int kind, int16_t id) override {
        const int32_t code = block == Vc2ParamBlock::kLeftover ? kLeftoverCall : (int32_t)block;
        return reads_.read({ code, part, kind, 0, id });
    }
    int32_t cosmValue(int part, int kind, int cosm, int16_t id) override {
        return reads_.read({ (int32_t)Vc2ParamBlock::kCosm, part, kind, cosm, id });
    }
    bool held(int flag) override { return reads_.flags[flag] != 0; }

private:
    SettingsReads& reads_;
};

// v247's Matrix set-up's parameters on the same reads (v292): the part's block, the system's, flag 0
class SettingsMatrixParams final : public Vc2MatrixParams {
public:
    explicit SettingsMatrixParams(SettingsReads& reads) : reads_(reads) {}
    int32_t partValue(int part, int16_t id) override {
        return reads_.read({ (int32_t)Vc2ParamBlock::kPart, part, 0, 0, id });
    }
    int32_t systemValue(int16_t id) override { return reads_.read({ (int32_t)Vc2ParamBlock::kSystem, 0, 0, 0, id }); }
    bool held() override { return reads_.flags[0] != 0; }

private:
    SettingsReads& reads_;
};

struct SettingsCounts {
    long events = 0, calls = 0, gaveUp = 0, flipped = 0;
    long moreEvents = 0, moreCalls = 0;   // the second part's (v287)
    long oscEvents = 0, oscCalls = 0;     // the third's (v288)
    long blockEvents = 0, blockCalls = 0; // the fourth's (v289)
    long vocoderEvents = 0, vocoderCalls = 0, extin = 0, offsets = 0;   // the fifth's (v290)
    long modeEvents = 0, modeCalls = 0, soundOffs = 0, leftovers = 0;   // the sixth's (v291)
    long dispatchEvents = 0, dispatchCalls = 0, wholeParts = 0, received = 0;   // the seventh's (v292)
    long categoryEvents = 0, categoryCalls = 0, heldNotes = 0, notesOff = 0;    // the eighth's (v294)
    long staticEvents = 0, staticCalls = 0;                                     // the ninth's (v295)
    std::array<long, 39> byEvent {};
};

// The two sides: rom.4's interpreter with the read objects' set-ups, the getter and (v288) the OSC block's reader
// hooked, the tables, each side's reads, the port
struct SettingsSides {
    explicit SettingsSides(Vc2Fw& rig);
    Vc2Fw& fw;
    Vc2PartRows rows {};
    SettingsReads rom {}, mine {};
    SettingsPortParams params { mine };
    Vc2PartSettings settings { params };
    SettingsPortOuts outs { mine.log };   // v290
    Vc2PartNodes nodes {};            // v291
    Vc2MatrixSettings g {};           // v292: G, and v247's set-up on the same reads
    SettingsMatrixParams matrixParams { mine };
    Vc2MatrixSetup setup { matrixParams, outs };
    Vc2StaticMatrix staticM {};       // v295
    SettingsCounts n {};
    // rom.4's side's read: logged and answered from the store, the flag set in its memory at the flip
    uint32_t romRead(Sh4& c, const SettingsCall& call);
};

// rom.4's routine on `args` and the port's `mine()`, both from the same flags, store and flip; what differs in the
// tables, the calls or the answer
std::string settingsOnBoth(SettingsSides& s, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                           const std::function<uint32_t()>& mine);
// The draws before an event: the store's seed, now and then a held flag, now and then one of the first `flipped`
// flags set after a read
void settingsDraw(SettingsSides& s, std::mt19937& rng, uint32_t flipped);
// A callee hooked on rom.4's side (v290's, shared from v291): its address and its call as the log has it; each answers
// 0 when hooked by settingsHookCallees
struct SettingsCallee {
    uint32_t at;
    SettingsCall (*called)(Sh4&);
};
void settingsHookCallees(SettingsSides& s, std::span<const SettingsCallee> callees);
// A further part (v289): its seed's offset from the run's, its counts, a case of its events, its line, and what it
// does first
struct SettingsPartRow {
    uint32_t offset;
    long SettingsCounts::*events;
    long SettingsCounts::*calls;
    std::string (*oneCase)(SettingsSides&, std::mt19937&, long&, long&);
    void (*say)(const SettingsCounts&);
    void (*start)(SettingsSides&) = nullptr;
};
// Its cases, its line, how many differ
long settingsRunPart(SettingsSides& s, const Vc2Run& run, const SettingsPartRow& p);
// The third part (v288): the OSC table's readers, the block reader hooked; the fourth (v289): rom.4's own
// (Vc2PartSettingsOscCheck.cpp); the fifth (v290): the vocoder reader (Vc2PartSettingsVocoderCheck.cpp)
extern const SettingsPartRow kOscPartRows[2];
extern const SettingsPartRow kVocoderPartRow;
// The sixth (v291): the mode row and Auto Note's switch (Vc2PartSettingsModesCheck.cpp)
extern const SettingsPartRow kModePartRow;
// The seventh (v292): the dispatchers and G's channel word (Vc2PartSettingsDispatchCheck.cpp)
extern const SettingsPartRow kDispatchPartRow;
// The eighth (v294): the Category and budget (Vc2PartSettingsCategoryCheck.cpp)
extern const SettingsPartRow kCategoryPartRow;
// The ninth (v295): the static M's initializers (Vc2StaticMatrixCheck.cpp)
extern const SettingsPartRow kStaticPartRow;
// v291's to v293's parts' noise stops before v294's word
inline constexpr size_t kV293Rows = offsetof(Vc2PartRows, heldNote);
// Noise in `bytes` bytes at `at` (a part's case's own objects, v291's nodes and v292's G)
inline void settingsNoise(void* at, size_t bytes, std::mt19937& rng) {
    auto* b = static_cast<uint8_t*>(at);
    for (size_t k = 0; k < bytes; ++k) b[k] = (uint8_t)rng();
}

using SettingsEvent = std::string (*)(SettingsSides&, std::mt19937&);

// A part's reader on a drawn part, the port's side `mine(part)` (v289)
template <uint32_t Routine, uint32_t TableAt, size_t Counted, class Mine>
std::string partEventWith(SettingsSides& s, std::mt19937& rng, const Mine& mine) {
    const int part = (int)(rng() % kSettingsParts);
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a part's reader", vc2R0 }, { TableAt, (uint32_t)part },
                          [&] { return (uint32_t)mine(part); });
}

template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string partEvent(SettingsSides& s, std::mt19937& rng) {
    const auto mine = [&](int part) { return (s.settings.*Member)(s.rows, part); };
    return partEventWith<Routine, TableAt, Counted>(s, rng, mine);
}

template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string kindEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts), kind = (int)(rng() % kSettingsKinds);
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a kind's row", vc2R0 }, { TableAt, (uint32_t)part, (uint32_t)kind },
                          [&] { return (uint32_t)(s.settings.*Member)(s.rows, part, kind); });
}

template <auto Member, uint32_t Routine, uint32_t TableAt, int Rows, size_t Counted>
std::string rowEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts), kind = (int)(rng() % kSettingsKinds), i = (int)(rng() % Rows);
    ++s.n.byEvent[Counted];
    const Vc2CheckRoutine r { Routine, "a row", vc2R0 };
    return settingsOnBoth(s, r, { TableAt, (uint32_t)part, (uint32_t)kind, (uint32_t)i },
                          [&] { return (uint32_t)(s.settings.*Member)(s.rows, part, kind, i); });
}

// A system's reader: rom.4's answers 1 (0x893a71d0 reads no table: the word it's given unread)
template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string systemEvent(SettingsSides& s, std::mt19937&) {
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a system's reader", vc2R0 }, { TableAt }, [&] {
        (s.settings.*Member)(s.rows);
        return 1u;
    });
}

// A case: noise in the tables up to `Noised` bytes in (each part's draws as it first drew them), the tables into
// rom.4's memory, then 400 of `Events` with `Flipped` flags to set, counted in `events` and `calls`
template <const auto& Events, size_t Noised, uint32_t Flipped>
std::string settingsCaseOf(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    auto* rows = reinterpret_cast<uint8_t*>(&s.rows);
    for (uint8_t* b = rows; b != rows + Noised; ++b) *b = (uint8_t)rng();
    const Vc2PartRows& t = s.rows;
    s.fw.cpu.write(kCommonsAt, &t.commons, sizeof t.commons);
    s.fw.cpu.write(kEnvelopesAt, &t.envelopes, sizeof t.envelopes);
    s.fw.cpu.write(kLfosAt, &t.lfos, sizeof t.lfos);
    s.fw.cpu.write(kMixesAt, &t.mixes, sizeof t.mixes);
    s.fw.cpu.write(kTvasAt, &t.tvas, sizeof t.tvas);
    s.fw.cpu.wr8(kControlsAt, t.controls);
    s.fw.cpu.write(kBytesAt, &t.bytes, sizeof t.bytes);
    s.fw.cpu.write(kCosmsAt, &t.cosms, sizeof t.cosms);
    s.fw.cpu.write(kOscsAt, &t.oscs, sizeof t.oscs);
    s.fw.cpu.write(kStepsAt, &t.steps, sizeof t.steps);
    s.fw.cpu.write(kRobotAt, &t.robot, sizeof t.robot);
    s.fw.cpu.write(kUnvoiceLevelAt, &s.outs.unvoiceLevel, sizeof s.outs.unvoiceLevel);
    s.fw.cpu.write(kModesAt, &t.modes, sizeof t.modes);
    s.fw.cpu.write(kNodesAt, s.nodes.nodes.data(), sizeof s.nodes.nodes);
    s.fw.cpu.write(kMatrixAt, &s.g, sizeof s.g);
    s.fw.cpu.wr32(kHeldNoteAt, t.heldNote);
    std::string what;
    for (int k = 0; k < kEventsACase && what.empty(); ++k, ++events) {
        settingsDraw(s, rng, Flipped);
        what = Events[rng() % std::size(Events)](s, rng);
        calls += (long)s.mine.log.size();
    }
    return what;
}
