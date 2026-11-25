// xt_vc2synth's whole part (v355): the part on its OS model (Vc2Synth::runOnOs, Vc2SynthOs on Vc2Kernel, the MIDI
// input path Vc2SynthOsMidi) against rom.4's part on rom.1's kernel in one interpreter (Vc2MachineRig). A fresh pair,
// each started as the check's main starts its own (rom.4's initializers and the system's start; the part's start and
// the system's defaults), then each OS: the tasks made, each running its prologue to its first wait, and the front's
// MIDI on. Then [cases] cases (16 at the least, 40 at the most), each a patch given both sides from the idle task (the
// load's steps, vc2SynthPatchGiven, with the notes part's steer) and 120 events drawn: a block two times in three (its
// ms ticks, then the input's DMA interrupt with frames drawn, then the output's), else a MIDI message as the notes part
// draws them (a channel message on MIDI IN or USB as its bytes, any other the front's). The port's model takes the
// events from one list, a wait inside a pass taking the next; the machine is then given each it took. Inside such a
// wait, one time in four, a burst of channel messages, more than the notes' queue holds, so its sender waits for room;
// a pair's first a flood, more than MIDI IN's receive ring holds. A wait longer than kLongestWait events is cut there,
// both compared inside it, and the next case is a fresh pair's (the notes' task truncating a tone that never renders
// waits for good, on rom.4 as here). After the start, each patch and each event: every region of the part (the MIDI
// path's with it), each tone's words, the kernel's words (Vc2KernelWords.h: the seven tasks' and the idle's TCBs,
// readyQueue, the system's words, the cyclic records, the watchdog), the services the port's model holds as nothing
// having changed nothing on rom.1, and the senders' waits counted on both (Vc2MachineRig's counts). v360, each from a
// generator of its own (the run's seed + 15), so no other draw moves: a fresh pair's lead-in before its first patch
// (Vc2SynthWholeEventsCheck.cpp); one case in two on a pitch, its patch the vocoder's on an EXTIN with Auto Note; and a
// compare inside a wait where MIDI IN's ring has just filled. The pair's events in Vc2SynthWholeEventsCheck.cpp (v360).
// XT_VC2SYNTH_FIRST=1 says what differs. XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2KernelWords.h"
#include "Vc2PartSettingsTables.h"   // v360
#include "Vc2PatchStore.h"
#include "Vc2SynthWholeCheck.h"   // v360
#include "Vc2SystemStore.h"
#include <algorithm>
#include <memory>

namespace {
using Os = Vc2SynthOs;
using Rig = Vc2MachineRig;
using Whole = Vc2SynthWhole;
using WholeCounts = Vc2SynthWholeCounts;
// [FW] the cyclic handlers by the port's number: the update task's, the sensing check, MIDI out A's flush
constexpr uint32_t kHandlers[] = { 0x893952a0u, 0x89311330u, 0x893115e0u };
constexpr int16_t kUsbThru = 0x7d;   // [FW] System B's USB-MIDI Thru, off at the start
// A case's events; the cases at the most (a case of the whole machine is a tenth of a second, so --synth's 400 would
// add 40 s to every plant's run)
constexpr int kEvents = 120, kMostCases = 40;
constexpr int kPatchTempo = 0x13a;                    // [FW] Patch Tempo, 200…2500 (0.1 BPM)
constexpr uint32_t kTempoFrom = 200, kTempos = 2301;
// [FW] the machine's OS rows, in the table's order: main's (rows 20, 21, 26…28) with the MIDI library's (14, 15, 18:
// the receive objects of USB's second port, MIDI IN and USB; 16, 19: the MIDI out objects; 17, the MIDI IN ring; 22…25:
// the OS MIDI task's listener and queue, its SysEx and state objects; 29, 30: the hub)
constexpr uint32_t kRigRows[] = { 0x8930ee90, 0x89311520, 0x893118f0, 0x89311e10, 0x89312d40, 0x89312ea0, 0x893133d0,
                                  0x89313e70, 0x893150c0, 0x89315440, 0x89315bc0, 0x89317180, 0x893179d0, 0x89317db0,
                                  0x89318140, 0x89318690, 0x8931a5d0 };
// v360: a pitch's case one time in two, its period 40…189 frames; its patch's IDs [FW]: OSC 1's Type (kind 0) EXTIN,
// Auto Note On Switch, Vocoder Sw on, Vocoder Type LEGACY or HIRESO, Formant Template one whose shifts aren't both 0
// (the glue reads the tracked period only with an offset, which Auto Note's template gives); the lead-in's ticks, past
// the sensing check's first 512
constexpr uint32_t kPitchOneIn = 2, kPeriodFrom = 40, kPeriods = 150, kLeadTicks = 600;
constexpr int kOscTypeId = 0x2e5, kAutoNoteId = 0x19c, kVocoderSwId = 0x193, kVocoderTypeId = 0x194;
constexpr int kTemplateId = 0x1a0;
constexpr uint32_t kExtin = 2, kOn = 1, kVocoderTypes = 2, kTemplates = 16;

// The machine's TCBs as the port's tasks: an address as one (-1 for none, -2 for any not the part's or its idle), a
// task's TCB, and the cyclic handlers as the port's numbers
int64_t machineTask(uint32_t a) {
    if (a == 0) return -1;
    if (a == Rig::kIdle) return Os::kIdle;
    for (size_t t = 0; t < Rig::kTasks.size(); ++t) {
        if (Rig::kTasks[t] == a) return (int64_t)t;
    }
    return -2;
}
uint32_t machineTcb(int t) { return t == Os::kIdle ? Rig::kIdle : Rig::kTasks[(size_t)t]; }
int64_t machineHandler(uint32_t a) {   // its number, -1 for none, -2 for any not the port's
    const auto at = std::ranges::find(kHandlers, a);
    const int64_t notOurs = a == 0 ? -1 : -2;
    return at == std::end(kHandlers) ? notOurs : (int64_t)(at - std::begin(kHandlers));
}
constexpr Vc2KernelNames kNames { machineTcb, machineTask, machineHandler };

// The services the port's model holds as nothing, where rom.1's changed something
std::string servicesDifference(const Rig& rig) {
    if (rig.suspendedSets != 0) return "rom.1 set an event on a suspended task";
    if (rig.heldWaits != 0) return "rom.1 waited on a semaphore another task held";
    return rig.restarts != 0 ? "rom.1 restarted a task" : "";
}

// The senders' waits (0x89310e70's, for room in a queue and for the OS MIDI task's to empty), counted on both, and the
// front's stand-in's queue never full
std::string waitsDifference(const Rig& rig, const Vc2SynthOsMidi& m) {
    if (m.frontDrops != 0) return "the front's stand-in dropped a message";
    if (m.roomWaits == rig.roomWaits && m.emptyWaits == rig.emptyWaits) return "";
    return "the waits for room " + std::to_string(m.roomWaits) + " against " + std::to_string(rig.roomWaits) +
           ", for empty " + std::to_string(m.emptyWaits) + " against " + std::to_string(rig.emptyWaits);
}

// v360: a pitch's case's patch, the vocoder's on an EXTIN OSC 1 with Auto Note (`own` its draws)
void vocoderOnPitch(std::vector<uint8_t>& bytes, std::mt19937& own) {
    vc2SynthRawPut(bytes, kOscTypeId, 0, kExtin);
    vc2SynthRawPut(bytes, kAutoNoteId, 0, kOn);
    vc2SynthRawPut(bytes, kVocoderSwId, 0, kOn);
    vc2SynthRawPut(bytes, kVocoderTypeId, 0, own() % kVocoderTypes);
    const auto& shifts = vc2PartSettingsTables().templateShifts;
    uint32_t t = own() % kTemplates;
    for (uint32_t k = 0; k < kTemplates && shifts[t] == 0.0f && shifts[t + kTemplates] == 0.0f; ++k) {
        t = own() % kTemplates;
    }
    vc2SynthRawPut(bytes, kTemplateId, 0, t);
}
} // namespace

std::string vc2SynthWholeDifference(Rig& rig, Vc2Synth& s) {
    std::string d = vc2SynthDifference(rig.fw, s);
    for (int tone = 0; tone < Vc2TonePool::kTones && d.empty(); ++tone) d = vc2SynthToneDifference(rig.fw, s, tone);
    if (d.empty() && s.osTasks) {
        const Vc2Kernel& k = s.osTasks->os.kernel;
        const std::string w = vc2KernelDifference(vc2KernelTheirs(rig.fw.cpu, Os::kTasks, kNames, rig.watchdog),
                                                  vc2KernelOurs(k));
        if (!w.empty()) d = "the kernel's " + w;
    }
    if (d.empty()) d = servicesDifference(rig);
    if (d.empty() && s.osTasks && s.osTasks->os.stuck()) d = "the port's OS model is stuck";
    if (d.empty() && s.osTasks) d = waitsDifference(rig, s.osTasks->midi);
    return d;
}

void Vc2SynthWholeCounts::tally(const Rig& rig) {
    loads += rig.loads;
    suspends += rig.suspends;
    semaphoreWaits += rig.semaphoreWaits;
    roomWaits += rig.roomWaits;
    emptyWaits += rig.emptyWaits;
    for (size_t k = 0; k < markers.size(); ++k) markers[k] += rig.markers[k];
}

namespace {
// A patch's steer: the notes part's, then half the time Patch Tempo within its range (noise falls outside it almost
// always, and the default's 120 BPM is the clock's own, so the clock task's tempo would never move); v360: a pitch's
// case's patch the vocoder's on an EXTIN
std::string wholeCase(Whole& w, std::vector<uint8_t>& store) {
    w.pitched = w.own() % kPitchOneIn == 0;
    w.period = kPeriodFrom + w.own() % kPeriods;
    w.n.pitched += w.pitched ? 1 : 0;
    const auto steer = [&w](std::vector<uint8_t>& bytes) {
        const bool kept = vc2SynthSteered(w.s, bytes, w.rng, w.n.under, w.n.kept);
        if (w.rng() % 2 == 0) vc2SynthRawPut(bytes, kPatchTempo, 0, kTempoFrom + w.rng() % kTempos);
        if (w.pitched) vocoderOnPitch(bytes, w.own);
        return kept;
    };
    std::string d = vc2SynthPatchGiven(w.rig.fw, w.s, store, w.rng, steer, kUsbThru);
    if (d.empty()) d = vc2SynthWholeDifference(w.rig, w.s);
    if (!d.empty()) return "the patch: " + d;
    for (int e = 0; e < kEvents && d.empty() && !w.cutting; ++e) {
        ++w.n.events;
        d = w.step();
        const char* what = w.events[w.taken - 1].block ? ", a block: " : ", a message: ";
        if (!d.empty()) d = "event " + std::to_string(e) + what + d;
    }
    return d;
}

struct Pair {
    std::unique_ptr<Rig> rig;
    std::unique_ptr<Vc2Synth> s;
    std::unique_ptr<Whole> w;
};

// A fresh pair (the last one's counts tallied), each started as the check's main starts its own, then each on its OS;
// v360: then its lead-in
std::string pairMade(const char* rom4, Pair& p, std::mt19937& rng, std::mt19937& own, WholeCounts& n) {
    if (p.rig) n.tally(*p.rig);
    p = Pair {};
    p.rig = std::make_unique<Rig>();
    std::string why, d;
    if (!p.rig->load(rom4, why)) d = "the machine's load: " + why;
    if (d.empty()) d = p.rig->prepare();
    int rows = 0;
    if (d.empty()) d = vc2SynthRomStart(p.rig->fw, rows, kRigRows);
    p.s = std::make_unique<Vc2Synth>();
    p.s->start();
    if (vc2SystemDefaults() != nullptr) p.s->giveSystem(*vc2SystemDefaults());
    if (d.empty()) d = p.rig->begin();
    p.w.reset(new Whole { *p.rig, *p.s, rng, n, own });
    Whole* w = p.w.get();
    p.s->runOnOs([w](Vc2PartEvent& e) { return w->next(e); });
    if (d.empty() && p.s->systemBValue(kUsbThru) != 0) d = "USB-MIDI Thru on";
    if (d.empty()) d = vc2SynthWholeDifference(*p.rig, *p.s);
    if (d.empty()) d = w->leadIn(kLeadTicks);
    ++n.pairs;
    return d.empty() ? "" : "the start: " + d;
}
} // namespace

long vc2SynthWholePart(const char* rom4, const Vc2Run& run) {
    std::mt19937 rng((uint32_t)run.seed + 5);
    std::mt19937 own((uint32_t)run.seed + 15);   // v360
    WholeCounts n;
    Pair p;
    std::string d = pairMade(rom4, p, rng, own, n);
    if (!d.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the whole part: %s\n", d.c_str());
    std::vector<uint8_t> store((size_t)kVc2StoreBytes);
    int c = 0;
    for (; c < std::min(vc2SynthCaseCount(run), kMostCases) && d.empty(); ++c) {
        d = wholeCase(*p.w, store);
        if (d.empty() && p.w->cutting) d = pairMade(rom4, p, rng, own, n);   // the cut pair can't go on
        if (!d.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the whole part: case %d, %s\n", c, d.c_str());
    }
    vc2SynthPatchDropped(p.rig->fw, *p.s);
    n.tally(*p.rig);
    std::printf("synth whole: the start on both OSes; %d cases on %ld pairs, %ld events: %ld blocks (%ld ticks, %ld "
                "sounding), %ld messages (%ld bursts, %ld floods); %ld waits in a pass, %ld cut; %ld patches under a "
                "VOCODER's tone, %ld Types kept; on rom.1 %ld contexts loaded, %ld suspends, %ld semaphore waits; the "
                "senders' %ld waits for room, %ld for empty; markers %ld %ld %ld %ld %ld; v360: %ld lead-ins' events, "
                "%ld compares at MIDI IN's full ring, %ld cases on a pitch\n",
                c, n.pairs, n.events, n.blocks, n.ticks, n.sounding, n.messages, n.bursts, n.floods, n.nested, n.cuts,
                n.under, n.kept, n.loads, n.suspends, n.semaphoreWaits, n.roomWaits, n.emptyWaits, n.markers[0],
                n.markers[1], n.markers[2], n.markers[3], n.markers[4], n.leads, n.fulls, n.pitched);
    return d.empty() ? 0 : 1;
}
#endif
