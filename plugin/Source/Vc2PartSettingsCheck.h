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
#include "Vc2KeysTables.h"
#include "Vc2MatrixTables.h"
#include "Vc2PartControllers.h"
#include "Vc2PartNumbers.h"
#include "Vc2PartChannel.h"
#include "Vc2MidiDispatch.h"
#include "Vc2PartSettingsCalls.h"
#include "Vc2PartStartUp.h"
#include "Vc2PartVoicesTables.h"
#include <utility>
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
// v296's: Time Trip Assign X and Y's bytes, Patch Remain's, the voices' counts' last reads
inline constexpr uint32_t kTripAssignsAt = 0x89989404, kPatchRemainAt = 0x899826fd, kVoiceCountsAt = 0x8993d02c;
// v299's: ChordMemory Switch's last read, the channels' Hold-press bytes
inline constexpr uint32_t kChordMemoryAt = 0x8993d03c, kHoldPressesAt = 0x899893e0;
// The held flags (Vc2PartSettingsCheck.cpp's until v300, when the reload objects' part reads them too)
inline constexpr uint32_t kHeldAt = 0x8993d0c0;
// A read object's handle the set-ups' hooks leave: this, the block << 16, part << 8, kind << 2, COSM; and (v291) the
// one rom.4's stack is given where Auto Note's switch reads through a read object never set up, a block past the
// OSC block's call
inline constexpr uint32_t kSettingsHandle = 0x01000000;
inline constexpr int32_t kLeftoverCall = 9;
inline constexpr uint32_t kLeftoverHandle = kSettingsHandle | (uint32_t)kLeftoverCall << 16;
// A block's code in the log and the handle: its own but for the leftover's and (v298) the system's fourth's
inline int32_t settingsBlockCall(Vc2ParamBlock block) {
    if (block == Vc2ParamBlock::kLeftover) return kLeftoverCall;
    return block == Vc2ParamBlock::kSystemD ? kSystemDCall : (int32_t)block;
}
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
        return reads_.read({ settingsBlockCall(block), part, kind, 0, id });
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
    long systemEvents = 0, systemCalls = 0;                                     // the tenth's (v296)
    long messageEvents = 0, messageCalls = 0;                                   // the eleventh's (v297)
    long receiveEvents = 0, receiveCalls = 0;                                   // the twelfth's (v298)
    long routingEvents = 0, routingCalls = 0;                                   // the thirteenth's (v299)
    long reloadEvents = 0, reloadCalls = 0;                                     // the fourteenth's (v300)
    long controllerEvents = 0, controllerCalls = 0;                             // the fifteenth's (v301)
    long numberEvents = 0, numberCalls = 0;                                     // the sixteenth's (v302)
    long handlerEvents = 0, handlerCalls = 0;                                   // the seventeenth's (v303)
    long startEvents = 0, startCalls = 0;                                       // the eighteenth's (v304)
    long tripEvents = 0, tripCalls = 0;                                         // the nineteenth's (v306)
    long channelEvents = 0, channelCalls = 0;                                   // the twentieth's (v307)
    long midiEvents = 0, midiCalls = 0;                                         // the twenty-first's (v308)
    // from v298 the twelfth's controllers and held notes heard at 50 and 51, from v299 the thirteenth's all notes off
    // at 54, from v300 the fourteenth's removes made held at 62, from v301 the fifteenth's at 63…66, v302's at 67…69,
    // v303's at 70…72, v304's at 73, v306's at 74…81, v307's at 82…86, v308's at 87…90
    std::array<long, 91> byEvent {};
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
    SettingsPortOuts outs { mine.log, mine.flags };   // v290; the flags the reload objects hold, v300
    Vc2PartNodes nodes {};            // v291
    Vc2MatrixSettings g {};           // v292: G, and v247's set-up on the same reads
    SettingsMatrixParams matrixParams { mine };
    Vc2MatrixSetup setup { matrixParams, outs };
    Vc2StaticMatrix staticM {};       // v295
    Vc2Keys keys {};                  // v297: v276's keys, which a node's class install resets
    Vc2PartReloads reloads { mine.flags, outs };   // v300
    std::array<Vc2TripFront, kSettingsParts> trips {};   // v301: the parts' trip objects, the port's
    uint8_t tripAnswer = 0;                              // v301: the trip's move's answer, drawn for both sides
    Vc2PartNumbers numbers {};                           // v302
    Vc2PartVoices voices {};                             // v304: v272's voices, which the start-up inits
    Vc2PartStartWords startWords {};                     // v304
    Vc2TripRecords tripRecords {};                       // v306: the parts' Time Trip records and the steps' scratch
    Vc2PitchSource pitch {};                             // v307: 0x89989748
    int32_t autoNoteSwitch = 0;                          // v307: Auto Note's On Switch, 0x8993fe50
    uint32_t noteGate = 0;                               // v308: 0x89981904, no note-on while set
    SettingsCounts n {};
    // rom.4's side's read: logged and answered from the store, the flag set in its memory at the flip
    uint32_t romRead(Sh4& c, const SettingsCall& call);
};

// Each byte rom.4 holds at an address against the port's (v304; settingsTails' loop until then): the first that differs
std::string settingsBytesDifference(Sh4& cpu, std::span<const std::pair<uint32_t, uint8_t>> bytes);
// The port's words of each region written into rom.4's memory, and the first region whose words differ (v306; the
// eighteenth part's own until then)
void settingsWriteRegions(SettingsSides& s, std::span<const Vc2Region> regions);
std::string settingsRegionsDifference(SettingsSides& s, std::span<const Vc2Region> regions);
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
    uint32_t (*answer)(SettingsSides&) = nullptr;   // v297: its answer, else 0
    SettingsCall (*more)(Sh4&) = nullptr;           // v302: a second entry it logs
};
// (v298) `own`: routines an earlier part hooked that run as rom.4's own from this part on, their hooks taken off
void settingsHookCallees(SettingsSides& s, std::span<const SettingsCallee> callees, std::span<const uint32_t> own = {});
// [FW] the callees more than one part hooks (v298; the Category's and the messages' until then): a controller (its
// source, channel, number and value), all notes off, a node's voices stopped (its word); and (v299; the messages' and
// the receive channel's until then) every voice truncated, the held note's event
inline SettingsCall settingsControlled(Sh4& c) {
    return { kOutController, (int32_t)c.r[4], (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[7] };
}
inline SettingsCall settingsNotesOff(Sh4&) { return { kOutNotesOff, 0, 0, 0, 0 }; }
inline SettingsCall settingsStopped(Sh4& c) { return { kOutStopVoices, 0, 0, 0, (int32_t)c.r[4] }; }
inline SettingsCall settingsTruncatedAll(Sh4&) { return { kOutTruncateAll, 0, 0, 0, 0 }; }
inline SettingsCall settingsHeldOn(Sh4&) { return { kOutHeldNoteOn, 0, 0, 0, 0 }; }
inline SettingsCall settingsNotified(Sh4& c) { return { kOutNotify, 0, 0, 0, (int32_t)c.r[4] }; }   // v303's, v307
inline constexpr SettingsCallee kSettingsControllerCallee { 0x89392f90, settingsControlled };
inline constexpr SettingsCallee kSettingsNotesOffCallee { 0x89391e30, settingsNotesOff };
inline constexpr SettingsCallee kSettingsStopCallee { 0x89390f70, settingsStopped };
inline constexpr SettingsCallee kSettingsTruncateAllCallee { 0x89391120, settingsTruncatedAll };
inline constexpr SettingsCallee kSettingsHeldOnCallee { 0x89391e50, settingsHeldOn };
inline constexpr SettingsCallee kSettingsNotifyCallee { 0x89390ea0, settingsNotified };   // the listeners
// v291's to v293's parts' noise stops before v294's word
inline constexpr size_t kV293Rows = offsetof(Vc2PartRows, heldNote);
// v294's and v295's parts' noise stops before v296's words
inline constexpr size_t kV295Rows = offsetof(Vc2PartRows, tripAssigns);
// v296's to v298's parts' noise stops before v299's words
inline constexpr size_t kV298Rows = offsetof(Vc2PartRows, chordMemory);
// Noise in `bytes` bytes at `at` (a part's case's own objects, v291's nodes and v292's G)
inline void settingsNoise(void* at, size_t bytes, std::mt19937& rng) {
    auto* b = static_cast<uint8_t*>(at);
    for (size_t k = 0; k < bytes; ++k) b[k] = (uint8_t)rng();
}

// The nodes, their lists, the part objects (each one's index 0…15) and the keys noise (the sixteenth part's, v302;
// shared with the eighteenth from v304)
inline void settingsNodesKeysNoise(SettingsSides& s, std::mt19937& rng) {
    Vc2PartNodes& n = s.nodes;
    settingsNoise(n.nodes.data(), sizeof n.nodes, rng);
    settingsNoise(n.lists.data(), sizeof n.lists, rng);
    settingsNoise(n.objects.data(), sizeof n.objects, rng);
    for (Vc2PartObject& o : n.objects) o.index = (uint8_t)(rng() % Vc2PartNodes::kObjects);
    settingsNoise(&s.keys, sizeof s.keys, rng);
}

using SettingsEvent = std::string (*)(SettingsSides&, std::mt19937&);

// A case: noise in the tables up to `Noised` bytes in (each part's draws as it first drew them), the tables into
// rom.4's memory, then `Count` (400, or from v304 fewer) of `Events` with `Flipped` flags to set, counted in `events`
// and `calls`
template <const auto& Events, size_t Noised, uint32_t Flipped, int Count = kEventsACase>
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
    s.fw.cpu.write(kTripAssignsAt, t.tripAssigns.data(), sizeof t.tripAssigns);
    s.fw.cpu.wr8(kPatchRemainAt, t.patchRemain);
    s.fw.cpu.write(kVoiceCountsAt, t.voiceCounts.data(), sizeof t.voiceCounts);
    s.fw.cpu.wr32(kChordMemoryAt, (uint32_t)t.chordMemory);
    s.fw.cpu.write(kHoldPressesAt, t.holdPresses.data(), sizeof t.holdPresses);
    std::string what;
    for (int k = 0; k < Count && what.empty(); ++k, ++events) {
        settingsDraw(s, rng, Flipped);
        what = Events[rng() % std::size(Events)](s, rng);
        calls += (long)s.mine.log.size();
    }
    return what;
}

// A case with the nodes noise first (v291's and v294's parts), then the tables' and the events
template <const auto& Events, size_t Noised>
std::string settingsNodesCaseOf(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    settingsNoise(s.nodes.nodes.data(), sizeof s.nodes.nodes, rng);
    return settingsCaseOf<Events, Noised, 5>(s, rng, events, calls);
}

#include "Vc2PartSettingsParts.h"
