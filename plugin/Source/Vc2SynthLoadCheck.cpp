// xt_vc2synth's load part (v345): the part's start and settings against rom.4's own, after the tones' makes and before
// the reads part, on the seed two after the run's.  First the notes' task's start on both sides (the pieces of
// 0x8938fc00 the part has: the notes' init 0x89396960, the hook word, the start-up 0x89392ab0, the note-on gate); then
// [cases] cases (16 at the least), each a store or none (one time in four): its bytes noise (an ID out of its range
// reads its default, as rom.4's getter has it), on rom.4's side at kStoreAt with part 0's entry (0x899612a8: +0 1, +8
// the store) aimed at it, the part given the same bytes; then 20 events, before each half the time up to 512 bytes of
// the store drawn again (so a read has something to change): the part's read 0x8939b170, the system's, assignments' and
// calibration's dispatchers (0x8939b0d0, 0x8939b110, 0x8939b140), a parameters' message 0x89392890 (a kind 0…13, its
// byte 0 half the time) and the part's 0x89392830 (kind 0 and part 0 half the time each).  After the start and each
// event every region of the part, the vocoder and the flags posted the notes' task's event object.  Hooked: the OS's
// event post 0x89301830 (the flags kept by object), its timer queue's post and remove (0x893004a0, 0x89300640), its
// locks, semaphores and clock (0x893018a0…0x893018f0), each answering 0.  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2PatchStore.h"
#include "Vc2SynthCheck.h"
#include "Vc2SynthSettings.h"
#include <algorithm>

namespace {
constexpr uint32_t kNotesInit = 0x89396960, kStartUp = 0x89392ab0, kGateAt = 0x89981904;   // [FW]
constexpr uint32_t kReadPart = 0x8939b170, kMessages = 0x89392890, kPartMessages = 0x89392830;   // [FW]
constexpr uint32_t kOsEvent = 0x89301830;   // [FW] the OS's event post
// [FW] the OS's timer queue's post and remove, its locks, semaphores and clock, each answered 0
constexpr uint32_t kAnswered[] = { 0x893004a0, 0x89300640, 0x893018a0, 0x893018b0, 0x893018c0, 0x893018d0,
                                   0x893018e0, 0x893018f0 };
constexpr uint32_t kEntryAt = 0x899612a8, kEntryStore = 8;   // [FW] part 0's store entry: +0 1 with a store, +8 it
constexpr uint32_t kStoreAt = 0x8C150000, kMessageAt = 0x8C160000;   // in Vc2Fw's RAM: a drawn store, a message
constexpr int kEvents = 20, kKinds = 14, kLeastCases = 16;
constexpr size_t kWindow = 0x200;   // the most bytes of the store drawn again before an event

// [FW] the system's, the assignments' and the calibration's dispatchers, each the part's
struct Dispatcher {
    uint32_t routine;
    void (Vc2SynthSettings::*mine)();
};
constexpr Dispatcher kDispatchers[] = { { 0x8939b0d0, &Vc2SynthSettings::readSystem },
                                        { 0x8939b110, &Vc2SynthSettings::readAssigns },
                                        { 0x8939b140, &Vc2SynthSettings::readCalibration } };

struct LoadCounts {
    long stores = 0, events = 0;
    std::array<long, 4> byEvent {};
};

// What rom.4 posted, by event object: the notes' task's flags, and whether any other object was posted
struct Posts {
    uint32_t notes = 0;
    bool others = false;
};

void installRom(Vc2Fw& fw, Posts& posts) {
    fw.cpu.hook(kOsEvent, [&posts](Sh4& c) {
        if (c.r[4] == kVc2SynthNotesTask) {
            posts.notes |= c.r[5];
        } else {
            posts.others = true;
        }
        c.ret(0);
    });
    for (const uint32_t at : kAnswered) {
        fw.cpu.hook(at, [](Sh4& c) { c.ret(0); });
    }
}

// After a step: the part's regions and vocoder, then what each side posted
std::string loadDifference(Vc2Fw& fw, Vc2Synth& s, const Posts& posts) {
    std::string d = vc2SynthDifference(fw, s);
    if (d.empty() && (posts.notes != s.notesEvents || posts.others)) d = "the notes' task's events";
    return d;
}

std::string taskStart(Vc2Fw& fw, Vc2Synth& s, const Posts& posts) {
    if (!fw.call(kNotesInit, {})) return "the notes' init faults: " + fw.cpu.error();
    fw.cpu.wr32(Vc2Notes::kHookAt, kVc2SynthEndedHook);
    if (!fw.call(kStartUp, {})) return "the start-up faults: " + fw.cpu.error();
    fw.cpu.wr32(kGateAt, 0);
    s.taskStart();
    const std::string d = loadDifference(fw, s, posts);
    return d.empty() ? "" : "the task's start: " + d;
}

// A store's bytes drawn for both sides, or none
void storeDrawn(Vc2Fw& fw, Vc2Synth& s, std::vector<uint8_t>& store, std::mt19937& rng, LoadCounts& n) {
    const bool none = rng() % 4 == 0;
    for (uint8_t& b : store) b = (uint8_t)rng();
    fw.cpu.write(kStoreAt, store.data(), store.size());
    fw.cpu.wr8(kEntryAt, none ? 0 : 1);
    fw.cpu.wr32(kEntryAt + kEntryStore, kStoreAt);
    s.givePatch(none ? nullptr : &store);
    n.stores += none ? 0 : 1;
}

// A window of the store drawn again on both sides, so that a read after it has something to change
void storeMoved(Vc2Fw& fw, std::vector<uint8_t>& store, std::mt19937& rng) {
    const size_t bytes = 1 + rng() % kWindow, from = rng() % (store.size() - bytes);
    for (size_t k = from; k < from + bytes; ++k) store[k] = (uint8_t)rng();
    fw.cpu.write(kStoreAt + (uint32_t)from, store.data() + from, bytes);
}

// A message's two bytes on rom.4's side, then the routine on both: the part's (kind 0 half the time) or any kind's
std::string loadMessage(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, bool part) {
    const uint8_t kind = part && rng() % 2 ? 0 : (uint8_t)(rng() % kKinds);
    const uint8_t second = (uint8_t)(rng() % 2 ? 0 : rng());
    fw.cpu.wr8(kMessageAt, kind);
    fw.cpu.wr8(kMessageAt + 1, second);
    if (!fw.call(part ? kPartMessages : kMessages, { 0, kMessageAt })) return "a message faults: " + fw.cpu.error();
    if (part) {
        s.settings->partParameters(kind, second);
    } else {
        s.settings->parameters(kind, second);
    }
    return "";
}

std::string loadEvent(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, LoadCounts& n) {
    ++n.events;
    const uint32_t pick = rng() % 4;
    ++n.byEvent[pick];
    if (pick == 0) {
        if (!fw.call(kReadPart, { 0 })) return "the part's read faults: " + fw.cpu.error();
        s.settings->readPart(0);
        return "";
    }
    if (pick == 1) {
        const Dispatcher& d = kDispatchers[rng() % std::size(kDispatchers)];
        if (!fw.call(d.routine, {})) return "a dispatcher faults: " + fw.cpu.error();
        (s.settings.get()->*d.mine)();
        return "";
    }
    return loadMessage(fw, s, rng, pick == 3);
}
} // namespace

long vc2SynthLoadPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run) {
    LoadCounts n;
    Posts posts;
    installRom(fw, posts);
    std::string first = taskStart(fw, s, posts);
    long different = first.empty() ? 0 : 1;
    std::mt19937 rng((uint32_t)run.seed + 2);
    std::vector<uint8_t> store((size_t)kVc2StoreBytes);
    const int cases = std::max(run.cases, kLeastCases);
    for (int c = 0; c < cases && first.empty(); ++c) {
        storeDrawn(fw, s, store, rng, n);
        std::string d;
        for (int e = 0; e < kEvents && d.empty(); ++e) {
            if (rng() % 2) storeMoved(fw, store, rng);
            d = loadEvent(fw, s, rng, n);
            if (d.empty()) d = loadDifference(fw, s, posts);
        }
        if (d.empty()) continue;
        if (first.empty()) first = "case " + std::to_string(c) + ": " + d;
        ++different;
    }
    s.givePatch(nullptr);
    if (!first.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the load: %s\n", first.c_str());
    std::printf("synth load: the task's start, %d cases (%ld with a store), %ld events: %ld part reads, %ld "
                "dispatchers, %ld messages, %ld part messages\n",
                cases, n.stores, n.events, n.byEvent[0], n.byEvent[1], n.byEvent[2], n.byEvent[3]);
    return different;
}
#endif
