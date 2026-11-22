// xt_vc2synth <rom.4> (v314): Vc2Synth's start against rom.4's.  On a fresh image (C startup's .data and .bss), rom.4's
// own static initializers run in the interpreter in its table's order: the whole table's two OS MIDI rows (0x893179d0,
// 0x89317db0), then the part's rows from 0x898c5cc0 to the one before 0x893f5070 (a task's that isn't the part's).
// Then Vc2Synth built and started, and every member compared with rom.4's memory at its address, byte for byte: the
// whole objects, the tables a field at a time, the tone pool's tones by the words a tone holds (its head and tail),
// the vocoder by v219's shared compares (its engines and stages) with its engine and hook words.  The start is one,
// so the run is one case whatever [cases] says.  XT_VC2SYNTH_FIRST=1 says what differs.  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2CheckRig.h"
#include "Vc2CheckWords.h"
#include "Vc2Synth.h"
#include "Vc2AutoNoteTables.h"
#include "Vc2CarrierTables.h"
#include "Vc2CosmTables.h"
#include "Vc2HiresoTables.h"
#include "Vc2Kind6Tables.h"
#include "Vc2KeysTables.h"
#include "Vc2LegacyTables.h"
#include "Vc2LibMathTables.h"
#include "Vc2MatrixTables.h"
#include "Vc2OsMidiTables.h"
#include "Vc2PartMixerTables.h"
#include "Vc2PartSettingsTables.h"
#include "Vc2PartVoicesTables.h"
#include "Vc2SetEnvelopeTables.h"
#include "Vc2ToneMixTables.h"
#include "Vc2TrackerTables.h"
#include "Vc2Unvoiced.h"
#include "Vc2VocoderTables.h"
#include <cstring>
#include <memory>
#include <vector>

namespace {
constexpr uint32_t kRowsAt = 0x898c5cc0, kRowBytes = 12, kRowRoutine = 4;   // [FW] the part's rows: {0, routine, 0}
constexpr uint32_t kFirstOutside = 0x893f5070;                              // [FW] row 95's routine, not the part's
constexpr uint32_t kMidiRows[] = { 0x893179d0, 0x89317db0 };                 // [FW] the OS MIDI's rows, earlier
constexpr uint32_t kVocoderAt = 0x89999758, kHiresoAt = kVocoderAt + 0x24, kLegacyAt = kVocoderAt + 0x2d94;   // [FW]
constexpr uint32_t kEngineWord = kVocoderAt + 0x20, kHookWord = 0x899d9d8c, kLegacyHook = 0x893f0480;          // [FW]
constexpr uint32_t kEditAt = 0x8994025c, kToneTailAt = 0x1af8;                                                 // [FW]

// A region of rom.4's memory and the port's bytes for it, any length
struct SynthRegion {
    uint32_t at;
    const void* mine;
    size_t bytes;
    const char* what;
};

std::string bytesDifference(Vc2Fw& fw, const SynthRegion& r) {
    std::vector<uint8_t> theirs(r.bytes);
    fw.cpu.read(r.at, theirs.data(), r.bytes);
    const auto* ours = static_cast<const uint8_t*>(r.mine);
    for (size_t k = 0; k < r.bytes; ++k) {
        if (theirs[k] == ours[k]) continue;
        char t[160];
        std::snprintf(t, sizeof t, "%s, byte 0x%08x: %02x, against %02x", r.what, r.at + (uint32_t)k, ours[k],
                      theirs[k]);
        return t;
    }
    return "";
}

template <class T>
SynthRegion region(uint32_t at, const T& mine, const char* what) { return { at, &mine, sizeof mine, what }; }

// [FW] every member's place: the whole objects and the tables a field at a time
std::vector<SynthRegion> synthRegions(Vc2Synth& s) {
    const Vc2PartRows& r = s.rows;
    std::vector<SynthRegion> out = {
        region(Vc2OsMidi::kPortsAt, s.midi.ports, "the OS MIDI's port objects"),
        region(Vc2OsMidi::kStatesAt, s.midi.states, "the channel states"),
        region(Vc2OsMidi::kFlagsAt, s.midi.receiveProgram, "Receive Program Change"),
        region(Vc2OsMidi::kFlagsAt + 1, s.midi.receiveBank, "Receive Bank Select"),
        region(0x89981b6c, s.voices.voices, "the voices"), region(Vc2PartVoices::kHeadsAt, s.voices.heads, "the heads"),
        region(0x899826d4, s.voices.g, "the voices' globals"), region(0x8998270c, s.voices.ages, "the next age"),
        region(Vc2Keys::kTablesAt, s.keys, "the keys"),
        region(Vc2Notes::kHookAt, s.notes.hook, "the notes' hook"), region(0x8998a518, s.notes.unread, "its float"),
        region(Vc2Notes::kNotesAt, s.notes.notes, "the notes"),
        region(Vc2UpdateQueue::kAt, s.queue.q, "the update queue"),
        region(Vc2Sets::kAt, s.sets.items, "C"), region(Vc2NoteMatrices::kAt, s.sets.matrices.items, "the notes' M"),
        region(Vc2SetCommons::kAt, s.commons.items, "C's commons"),
        region(Vc2SetEnvelopes::kAt, s.envelopes.items, "C's envelopes"),
        region(Vc2SetLfos::kAt, s.lfos.items, "C's LFOs"), region(Vc2SetMixes::kAt, s.mixes.items, "C's mixes"),
        region(Vc2SetTvas::kAt, s.tvas.items, "C's TVAs"), region(Vc2SetTvas::kStepAt, s.tvas.step, "the TVA's step"),
        region(0x89997090, r.commons, "the commons' table"), region(0x89e2765c, r.envelopes, "the envelopes' table"),
        region(0x89e2f85c, r.lfos, "the LFOs' table"), region(0x89e32a60, r.mixes, "the mixes' table"),
        region(0x89e4987c, r.tvas, "the TVAs' table"), region(0x89997fcc, r.controls, "the controls' flag"),
        region(0x89998a14, r.bytes, "the part's bytes"), region(0x89e2003c, r.cosms, "the COSM inputs' table"),
        region(0x89e371e0, r.oscs, "the OSC table"), region(0x89989c84, r.steps, "the step counts"),
        region(0x89997fdc, r.robot, "robot human depth"), region(0x89995390, r.modes, "the mode rows"),
        region(0x899891d0, r.heldNote, "the held note"), region(0x89989404, r.tripAssigns, "Time Trip's assigns"),
        region(0x899826fd, r.patchRemain, "Patch Remain"), region(0x8993d02c, r.voiceCounts, "the voices' counts"),
        region(0x8993d03c, r.chordMemory, "ChordMemory Switch"), region(0x899893e0, r.holdPresses, "the Hold bytes"),
        region(0x89997398, s.g, "G"), region(Vc2StaticMatrix::kAt, s.staticM, "the static M"),
        region(Vc2PartNodes::kNodesAt, s.nodes.nodes, "the nodes"),
        region(Vc2PartNodes::kListsAt, s.nodes.lists, "the nodes' lists"),
        region(Vc2PartNodes::kClassesAt, s.nodes.classes, "the class bytes"),
        region(Vc2PartNodes::kObjectsAt, s.nodes.objects, "the part objects"),
        region(Vc2PartNumbers::kObjectsAt, s.numbers.objects, "the number objects"),
        region(Vc2PartNumbers::kTableAt, s.numbers.table, "the numbers' table"),
        region(0x8993d0c0, s.held, "the held flags"),
        region(Vc2PartReloads::kAt, s.reloads.objects, "the reload objects"),
        region(Vc2PartReloads::kFloatsAt, s.reloads.floats, "their floats"),
        region(Vc2TripRecords::kAt, s.trips.records, "Time Trip's records"),
        region(Vc2TripRecords::kScratchAt, s.trips.scratch, "its scratch"),
        region(Vc2PartStartWords::kRoutingAt, s.startWords.routing, "the routing object's bytes"),
        region(Vc2PartStartWords::kFloatsAt, s.startWords.floats, "its floats"),
        region(Vc2PartStartWords::kAfterAt, s.startWords.after, "the bytes after the assigns"),
        region(Vc2PartStartWords::kRemainAt, s.startWords.remain, "the settings object's word"),
        region(0x89989748, s.pitch, "the pitch source"), region(0x89981904, s.noteGate, "the note-on gate"),
        region(0x89981b60, s.scheduled, "the schedule's count"),
        region(0x8993d034, s.autoNoteKeys, "Auto Note's keys"), region(0x8993d080, s.ticks, "the ticks"),
        region(0x8993d084, s.time, "the time struct"),
        region(0x8993fb48, s.tones.head, "the tone pool's head"), region(0x8993fb4c, s.tones.serial, "its serial"),
        region(Vc2ToneLists::kNewestAt, s.lists.newest, "the newest mark"),
        region(Vc2ToneLists::kNewestAt + 4, s.lists.joined, "the joined mark"),
        region(Vc2ToneLists::kSoundingAt, s.lists.sounding, "the sounding list"),
        region(Vc2ToneLists::kNewAt, s.lists.fresh, "the new list"),
        region(Vc2FreeLfos::kAt, s.freeLfos.w, "the free LFOs"), region(0x89999290, s.pools.w, "the COSM pools"),
        region(0x8993eac4, s.mixer.w, "the mixer's words"), region(0x8999f1d0, s.mixer.b, "its buffers"),
        region(0x89934550, s.mixer.bypass, "its bypass"), region(0x89997290, s.mixer.offsets, "its offsets"),
        region(kVocoderAt, s.vocoder.w, "the vocoder's words"), region(kEditAt, s.vocoder.edit, "its edit words"),
        region(0x899d9b70, s.tracker.s, "the tracker"),
        region(0x8993fe3c, s.autoNote.w, "Auto Note's words"), region(0x899d994c, s.autoNote.g, "its globals"),
        region(0x8a0f31cc, s.ring.w, "the mic ring"), region(0x899d9968, s.ring.g, "its globals"),
        region(0x8993fe54, s.ring.mode, "its mode"), region(0x89999668, s.chaos, "the chaos generator"),
    };
    Vc2ToneMixRings& rings = s.rings;
    out.push_back({ Vc2ToneMixRings::kAAt, rings.a(Vc2ToneMixRings::kAAt).data(),
                    Vc2ToneMixRings::kTones * Vc2ToneMixRings::kBytes, "the mix's rings a" });
    out.push_back({ Vc2ToneMixRings::kBAt, rings.b(Vc2ToneMixRings::kBAt).data(),
                    Vc2ToneMixRings::kTones * Vc2ToneMixRings::kBytes, "the mix's rings b" });
    for (int k = 0; k < Vc2TonePool::kTones; ++k) {
        const uint32_t at = s.tones.addressOf(k);
        out.push_back(region(at, s.tones.tones[(size_t)k].head, "a tone's head"));
        out.push_back(region(at + kToneTailAt, s.tones.tones[(size_t)k].tail, "a tone's tail"));
    }
    return out;
}

// What differs: the regions in order, then the vocoder's engines, stages, engine word and hook word
std::string synthDifference(Vc2Fw& fw, Vc2Synth& s) {
    for (const SynthRegion& r : synthRegions(s))
        if (std::string d = bytesDifference(fw, r); !d.empty()) return d;
    std::string d = vc2HiresoDifference(fw, kHiresoAt, s.vocoder.hireso);
    if (d.empty()) d = vc2LegacyDifference(fw, kLegacyAt, s.vocoder.legacy);
    if (d.empty()) d = vc2UnvoicedDifference(fw, s.vocoder.stage);
    if (d.empty() && fw.cpu.rd32(kEngineWord) != 0) d = "the vocoder's engine word";
    if (d.empty() && fw.cpu.rd32(kHookWord) != kLegacyHook) d = "the vocoder's hook word";
    return d;
}

// rom.4's start: the OS MIDI's rows, then the part's, each called in turn; how many, or what faulted
std::string runRows(Vc2Fw& fw, int& rows) {
    std::vector<uint32_t> routines(std::begin(kMidiRows), std::end(kMidiRows));
    for (uint32_t at = kRowsAt; fw.cpu.rd32(at + kRowRoutine) != kFirstOutside; at += kRowBytes)
        routines.push_back(fw.cpu.rd32(at + kRowRoutine));
    for (const uint32_t routine : routines) {
        if (!fw.call(routine, {})) return "the initializer " + std::to_string(routine) + " faults: " + fw.cpu.error();
        ++rows;
    }
    return "";
}

bool synthLoadTables(const char* path) {
    return vc2LoadAll<vc2LoadPartVoices, vc2LoadKeys, vc2LoadOsMidi, vc2LoadPartSettings, vc2LoadUnvoiced,
                      vc2LoadLegacy, vc2LoadHireso, vc2LoadVocoder, vc2LoadCarrier, vc2LoadTracker, vc2LoadAutoNote,
                      vc2LoadKind6, vc2LoadMixer, vc2LoadCosm, vc2LoadToneMix, vc2LoadSetEnvelopes, vc2LoadMatrix,
                      vc2LoadLibMath>(path);
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2synth", synthLoadTables);
    if (!run) return 1;
    int rows = 0;
    std::string what = runRows(fw, rows);
    const auto synth = std::make_unique<Vc2Synth>();
    synth->start();
    if (what.empty()) what = synthDifference(fw, *synth);
    if (!what.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the start: %s\n", what.c_str());
    std::printf("synth: the start, %d of rom.4's initializers, %zu regions and the vocoder compared\n", rows,
                synthRegions(*synth).size());
    std::printf("xt_vc2synth: %d different\n", what.empty() ? 0 : 1);
    return 0;
}
#endif
