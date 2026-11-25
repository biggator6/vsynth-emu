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
// having changed nothing on rom.1, and the senders' waits counted on both (Vc2MachineRig's counts). XT_VC2SYNTH_FIRST=1
// says what differs. XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2KernelWords.h"
#include "Vc2MachineRig.h"
#include "Vc2PatchStore.h"
#include "Vc2Synth.h"
#include "Vc2SynthCheck.h"
#include "Vc2SynthOsTasks.h"
#include "Vc2SystemStore.h"
#include <algorithm>
#include <deque>
#include <memory>

namespace {
using Os = Vc2SynthOs;
using Rig = Vc2MachineRig;
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
constexpr uint32_t kBlockFrames = 32, kFramesPerMs = 44100, kMsPerSecond = 1000, kInputShifts = 12;
// The check's own bounds, no firmware's: a wait inside a pass cut at so many events; a burst's messages (more than the
// notes' queue's 127) and a flood's (more than MIDI IN's receive ring's 2047), drawn on MIDI IN or USB
constexpr size_t kLongestWait = 6000;
constexpr uint32_t kBurstFrom = 130, kBursts = 300, kFloodFrom = 2100, kFloods = 300, kBurstChance = 4;
constexpr uint8_t kChannelFirst = 0x80, kSystemFirst = 0xf0, kMidiIn = 1, kInputs = 2;
constexpr int kTries = 64;

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

// Every region of the part, the vocoder, each tone's words, then the kernel's, the services', whether the port's model
// is stuck, and the senders' waits
std::string wholeDifference(Rig& rig, Vc2Synth& s) {
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

struct WholeCounts {
    long events = 0, blocks = 0, messages = 0, ticks = 0, nested = 0, sounding = 0, under = 0, kept = 0;
    long cuts = 0, bursts = 0, floods = 0, pairs = 0;
    // the machines', every pair's
    long loads = 0, suspends = 0, semaphoreWaits = 0, roomWaits = 0, emptyWaits = 0;
    std::array<long, 5> markers {};
    void tally(const Rig& rig) {
        loads += rig.loads;
        suspends += rig.suspends;
        semaphoreWaits += rig.semaphoreWaits;
        roomWaits += rig.roomWaits;
        emptyWaits += rig.emptyWaits;
        for (size_t k = 0; k < markers.size(); ++k) markers[k] += rig.markers[k];
    }
};

// The pair and the events: drawn as the port's model takes them, the machine given them after
struct Whole {
    Rig& rig;
    Vc2Synth& s;
    std::mt19937& rng;
    WholeCounts& n;
    std::deque<Vc2PartEvent> events;
    std::deque<Vc2SynthInput> inputs;   // the blocks' frames, where their events point
    size_t taken = 0;                   // the events the port's model has taken
    size_t given = 0;                   // and the machine been given
    size_t stepFrom = 0;                // the step's first
    uint32_t msAcc = 0;                 // the machine's ms against its frames, as the port's model counts them
    uint32_t burst = 0;                 // a burst's messages still to come
    bool flooding = false, flooded = false;
    bool cutting = false;               // a wait cut: the pair can't go on
    std::string cut;                    // what differed where it was cut

    // A burst's next: a channel message as the notes part draws them, on MIDI IN or USB (a flood's on MIDI IN)
    Vc2PartEvent burstMessage() {
        Vc2PartEvent e;
        e.block = false;
        for (int k = 0; k < kTries && (e.message.status < kChannelFirst || e.message.status >= kSystemFirst); ++k) {
            const Vc2SynthDrawn m = vc2SynthMessageDrawn(rng, s);
            e.message = m.m;
            e.word = m.word;
        }
        e.message.source = (uint8_t)(flooding ? kMidiIn : kMidiIn + rng() % kInputs);
        --burst;
        ++n.messages;
        return e;
    }
    // A block two times in three, its frames silence one time in four, else noise shifted down 0…11 bits; else a
    // message; a burst's while one goes on
    Vc2PartEvent drawn() {
        if (burst > 0) return burstMessage();
        Vc2PartEvent e;
        if (rng() % 3 != 0) {
            const bool silent = rng() % 4 == 0;
            const uint32_t shift = rng() % kInputShifts;
            Vc2SynthInput& frames = inputs.emplace_back();
            for (int32_t& word : frames) word = silent ? 0 : (int32_t)rng() >> shift;
            e.input = frames.data();
            ++n.blocks;
        } else {
            const Vc2SynthDrawn m = vc2SynthMessageDrawn(rng, s);
            e.block = false;
            e.message = m.m;
            e.word = m.word;
            ++n.messages;
        }
        return e;
    }
    // A wait's first event drawn: one time in four a burst, the pair's first a flood
    void burstDrawn() {
        if (rng() % kBurstChance != 0) return;
        flooding = !flooded;
        flooded = true;
        burst = flooding ? kFloodFrom + rng() % kFloods : kBurstFrom + rng() % kBursts;
        ++(flooding ? n.floods : n.bursts);
    }
    // The next event the port's model takes, drawn when none waits; none past the longest wait, cut there
    bool next(Vc2PartEvent& e) {
        if (taken - stepFrom >= kLongestWait) {
            if (!cutting) waitCut();
            return false;
        }
        if (taken == events.size()) {
            if (taken == stepFrom + 1 && burst == 0) burstDrawn();
            events.push_back(drawn());
        }
        e = events[taken++];
        return true;
    }
    std::string give(const Vc2PartEvent& e) {
        if (!e.block) return rig.message(e.message, e.word);
        std::string d;
        for (msAcc += kBlockFrames * kMsPerSecond; msAcc >= kFramesPerMs && d.empty(); msAcc -= kFramesPerMs) {
            ++n.ticks;
            d = rig.tick();
        }
        if (d.empty()) d = rig.input(e.input);
        return d.empty() ? rig.output() : d;
    }
    // The machine given what the port's model took, both compared there, inside the wait, before the port's unwinds
    void waitCut() {
        while (given < taken && cut.empty()) cut = give(events[given++]);
        if (cut.empty()) cut = wholeDifference(rig, s);
        if (!cut.empty()) cut = "inside a wait cut: " + cut;
        cutting = true;
        ++n.cuts;
    }
    // One event or more on the port's model (a wait inside a pass takes the next), then each on the machine
    std::string step() {
        stepFrom = taken;
        s.osTasks->deliverNext();
        n.nested += taken - stepFrom > 1 ? 1 : 0;
        if (cutting) return cut;
        std::string d;
        while (given < taken && d.empty()) d = give(events[given++]);
        n.sounding += std::ranges::any_of(s.frames, [](float x) { return x != 0.0f; }) ? 1 : 0;
        return d.empty() ? wholeDifference(rig, s) : d;
    }
};

// A patch's steer: the notes part's, then half the time Patch Tempo within its range (noise falls outside it almost
// always, and the default's 120 BPM is the clock's own, so the clock task's tempo would never move)
std::string wholeCase(Whole& w, std::vector<uint8_t>& store) {
    const auto steer = [&w](std::vector<uint8_t>& bytes) {
        const bool kept = vc2SynthSteered(w.s, bytes, w.rng, w.n.under, w.n.kept);
        if (w.rng() % 2 == 0) vc2SynthRawPut(bytes, kPatchTempo, 0, kTempoFrom + w.rng() % kTempos);
        return kept;
    };
    std::string d = vc2SynthPatchGiven(w.rig.fw, w.s, store, w.rng, steer, kUsbThru);
    if (d.empty()) d = wholeDifference(w.rig, w.s);
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

// A fresh pair (the last one's counts tallied), each started as the check's main starts its own, then each on its OS
std::string pairMade(const char* rom4, Pair& p, std::mt19937& rng, WholeCounts& n) {
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
    p.w.reset(new Whole { *p.rig, *p.s, rng, n });
    Whole* w = p.w.get();
    p.s->runOnOs([w](Vc2PartEvent& e) { return w->next(e); });
    if (d.empty() && p.s->systemBValue(kUsbThru) != 0) d = "USB-MIDI Thru on";
    if (d.empty()) d = wholeDifference(*p.rig, *p.s);
    ++n.pairs;
    return d.empty() ? "" : "the start: " + d;
}
} // namespace

long vc2SynthWholePart(const char* rom4, const Vc2Run& run) {
    std::mt19937 rng((uint32_t)run.seed + 5);
    WholeCounts n;
    Pair p;
    std::string d = pairMade(rom4, p, rng, n);
    if (!d.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the whole part: %s\n", d.c_str());
    std::vector<uint8_t> store((size_t)kVc2StoreBytes);
    int c = 0;
    for (; c < std::min(vc2SynthCaseCount(run), kMostCases) && d.empty(); ++c) {
        d = wholeCase(*p.w, store);
        if (d.empty() && p.w->cutting) d = pairMade(rom4, p, rng, n);   // the cut pair can't go on
        if (!d.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the whole part: case %d, %s\n", c, d.c_str());
    }
    vc2SynthPatchDropped(p.rig->fw, *p.s);
    n.tally(*p.rig);
    std::printf("synth whole: the start on both OSes; %d cases on %ld pairs, %ld events: %ld blocks (%ld ticks, %ld "
                "sounding), %ld messages (%ld bursts, %ld floods); %ld waits in a pass, %ld cut; %ld patches under a "
                "VOCODER's tone, %ld Types kept; on rom.1 %ld contexts loaded, %ld suspends, %ld semaphore waits; the "
                "senders' %ld waits for room, %ld for empty; markers %ld %ld %ld %ld %ld\n",
                c, n.pairs, n.events, n.blocks, n.ticks, n.sounding, n.messages, n.bursts, n.floods, n.nested, n.cuts,
                n.under, n.kept, n.loads, n.suspends, n.semaphoreWaits, n.roomWaits, n.emptyWaits, n.markers[0],
                n.markers[1], n.markers[2], n.markers[3], n.markers[4]);
    return d.empty() ? 0 : 1;
}
#endif
