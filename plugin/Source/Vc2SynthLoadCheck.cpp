// xt_vc2synth's load part (v345): the part's start and settings against rom.4's own, after the tones' makes and before
// the reads part, on the seed two after the run's.  First the notes' task's start on both sides (the pieces of
// 0x8938fc00 the part has: the notes' init 0x89396960, the hook word, the start-up 0x89392ab0, the note-on gate); then
// [cases] cases (16 at the least), each (v347) a system (rom.4's defaults half the time, else its three areas noise)
// and a store or none (one time in four): its bytes noise (an ID out of its range reads its default, as rom.4's getter
// has it), on rom.4's side at kStoreAt with part 0's entry (0x899612a8: +0 1, +8 the store) aimed at it, the part given
// the same bytes; then 20 events, before each half the time up to 512 bytes of the store drawn again (so a read has
// something to change): the part's read 0x8939b170, the system's, assignments' and calibration's dispatchers
// (0x8939b0d0, 0x8939b110, 0x8939b140), a parameters' message 0x89392890 (a kind 0…13, 0 one time in four; its byte 0
// half the time) and the part's 0x89392830 (kind 0 and part 0 half the time each).  After the start and each event
// every region of the part, the vocoder, the flags posted the notes' task's event object and (v347) any read no region
// holds.  Hooked: the OS's event post 0x89301830 (the flags kept by object), its timer queue's post and remove
// (0x893004a0, 0x89300640), its locks, semaphores and clock (0x893018a0…0x893018f0), each answering 0, the OS MIDI's
// routines (v346) and the OS's event wait (v347).  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2Params.h"
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
constexpr int kEvents = 20, kKinds = 14;
constexpr int kReceiveSwitch = 0x47;   // [FW] Part 1 Receive Switch, system B's one bit
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

using Posts = Vc2SynthPosts;   // v346: the parts' shared rig (Vc2SynthCheck.h)

std::string loadDifference(Vc2Fw& fw, Vc2Synth& s, const Posts& posts) {
    return vc2SynthStepDifference(fw, s, posts);
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

// v347: a system on both sides: rom.4's defaults half the time, else its three areas noise, Part 1 Receive Switch on
// three times in four of those (v349: always where `receives`, as the update task's passes need: with it off, G's
// channel word is rom.4's 0x8999738c, 0, and M's init on the static M reads its sources at 0x84 on, an XT's boot
// flash, which the part has none of)
void systemDrawn(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, bool receives) {
    Vc2System system = *vc2SystemDefaults();
    if (rng() % 2 == 0) {
        for (uint8_t& b : system.bytes) b = (uint8_t)rng();
        const XtParamDesc* on = vc2ParamDesc(kReceiveSwitch);
        const size_t at = (size_t)(Vc2System::kRecord + on->offset);
        if (receives || rng() % 4) system.bytes[at] |= (uint8_t)(0x80 >> on->bit);
    }
    for (const Vc2System::Area& a : Vc2System::kAreas) fw.cpu.write(a.at, system.bytes.data() + a.from, a.size);
    s.giveSystem(system);
}

// A store's bytes drawn for both sides, or none, with a system drawn: whether a store was; v349: the bytes given
// `steer` before either side has them, a store where it says so
bool storeDrawn(Vc2Fw& fw, Vc2Synth& s, std::vector<uint8_t>& store, std::mt19937& rng, bool receives = false,
                const Vc2SynthSteer& steer = {}) {
    systemDrawn(fw, s, rng, receives);
    bool none = rng() % 4 == 0;
    for (uint8_t& b : store) b = (uint8_t)rng();
    if (steer && steer(store)) none = false;
    fw.cpu.write(kStoreAt, store.data(), store.size());
    fw.cpu.wr8(kEntryAt, none ? 0 : 1);
    fw.cpu.wr32(kEntryAt + kEntryStore, kStoreAt);
    s.givePatch(none ? nullptr : &store);
    return !none;
}

// The part's read and a dispatcher, each on both sides
std::string partRead(Vc2Fw& fw, Vc2Synth& s) {
    if (!fw.call(kReadPart, { 0 })) return "the part's read faults: " + fw.cpu.error();
    s.settings->readPart(0);
    return "";
}

std::string dispatched(Vc2Fw& fw, Vc2Synth& s, const Dispatcher& d) {
    if (!fw.call(d.routine, {})) return "a dispatcher faults: " + fw.cpu.error();
    (s.settings.get()->*d.mine)();
    return "";
}

// A window of the store drawn again on both sides, so that a read after it has something to change
void storeMoved(Vc2Fw& fw, std::vector<uint8_t>& store, std::mt19937& rng) {
    const size_t bytes = 1 + rng() % kWindow, from = rng() % (store.size() - bytes);
    for (size_t k = from; k < from + bytes; ++k) store[k] = (uint8_t)rng();
    fw.cpu.write(kStoreAt + (uint32_t)from, store.data() + from, bytes);
}

// A message's two bytes on rom.4's side, then the routine on both: the part's or any kind's
std::string messageBoth(Vc2Fw& fw, Vc2Synth& s, bool part, uint8_t kind, uint8_t second) {
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

// A drawn message: the part's (kind 0 half the time) or any kind's (v347: kind 0, the routing's reset, one in four), its
// second byte 0 half the time
std::string loadMessage(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, bool part) {
    const bool zero = part ? rng() % 2 != 0 : rng() % 4 == 0;
    const uint8_t kind = zero ? 0 : (uint8_t)(rng() % kKinds);
    const uint8_t second = (uint8_t)(rng() % 2 ? 0 : rng());
    return messageBoth(fw, s, part, kind, second);
}

std::string loadEvent(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, LoadCounts& n) {
    ++n.events;
    const uint32_t pick = rng() % 4;
    ++n.byEvent[pick];
    if (pick == 0) return partRead(fw, s);
    if (pick == 1) return dispatched(fw, s, kDispatchers[rng() % std::size(kDispatchers)]);
    return loadMessage(fw, s, rng, pick == 3);
}
} // namespace

void vc2SynthInstallOs(Vc2Fw& fw, Vc2SynthPosts& posts) {
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
    for (const uint32_t at : Vc2OsMidiParts::kOsRoutines) {   // v346: the OS's, as the part's osRoutine leaves them
        fw.cpu.hook(at, [](Sh4& c) { c.ret(0); });
    }
    // v347: the OS's event wait, which a truncate-and-wait (0x893b3bc0) calls on the TVA's fade with its tone in r12:
    // the fade rendered at once, as the part's waitFade has it (its TVA's +0x64, the tone's +0x1a54, 0); v349: the
    // update task's (mask 3) answered the events drawn for its pass; v350: the notes' task's (mask 0x3fe) its
    constexpr uint32_t kOsWait = 0x893017f0, kFadeEvent = 0x1000, kFadeAt = 0x19f0 + 0x64, kUpdateMask = 3;   // [FW]
    constexpr uint32_t kNotesMask = 0x3fe;   // [FW]
    fw.cpu.hook(kOsWait, [&posts](Sh4& c) {
        if (c.r[4] == kFadeEvent) c.wr32(c.r[12] + kFadeAt, 0);
        const uint32_t waited = c.r[4] == kNotesMask ? posts.notesWaited : 0;
        c.ret(c.r[4] == kUpdateMask ? posts.updateEvents : waited);
    });
}

std::string vc2SynthStepDifference(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthPosts& posts) {
    std::string d = vc2SynthDifference(fw, s);
    if (d.empty() && (posts.notes != s.notesEvents || posts.others)) d = "the notes' task's events";
    if (d.empty() && s.reads->unheld() != 0) {   // v347: a read that reaches past the part's memory
        char t[80];
        std::snprintf(t, sizeof t, "the part read bytes no region holds, from 0x%08x", s.reads->firstUnheld());
        d = t;
    }
    return d;
}

void vc2SynthPatchDropped(Vc2Fw& fw, Vc2Synth& s) {
    fw.cpu.wr8(kEntryAt, 0);
    s.givePatch(nullptr);
}

std::string vc2SynthPatchLoaded(Vc2Fw& fw, Vc2Synth& s, std::vector<uint8_t>& store, std::mt19937& rng,
                                const Vc2SynthPosts& posts, const Vc2SynthSteer& steer) {
    storeDrawn(fw, s, store, rng, true, steer);   // v349: Part 1 receiving, for the update task's passes
    std::string d = partRead(fw, s);
    for (const Dispatcher& k : kDispatchers) {
        if (d.empty()) d = dispatched(fw, s, k);
    }
    if (d.empty()) d = messageBoth(fw, s, true, 0, 0);   // v346: part 0's message, its node the held class
    return d.empty() ? vc2SynthStepDifference(fw, s, posts) : d;
}

long vc2SynthCases(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, uint32_t offset, const char* what,
                   const std::function<std::string(std::mt19937&, std::vector<uint8_t>&)>& one) {
    std::mt19937 rng((uint32_t)run.seed + offset);
    std::vector<uint8_t> store((size_t)kVc2StoreBytes);
    std::string d;
    int c = 0;
    for (; c < vc2SynthCaseCount(run) && d.empty(); ++c) d = one(rng, store);
    vc2SynthPatchDropped(fw, s);
    if (d.empty()) return 0;
    if (xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  %s: case %d: %s\n", what, c - 1, d.c_str());
    return 1;
}

long vc2SynthLoadPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, const Vc2SynthPosts& posts) {
    LoadCounts n;
    const auto oneCase = [&](std::mt19937& rng, std::vector<uint8_t>& store) {
        n.stores += storeDrawn(fw, s, store, rng) ? 1 : 0;
        std::string d;
        for (int e = 0; e < kEvents && d.empty(); ++e) {
            if (rng() % 2) storeMoved(fw, store, rng);
            d = loadEvent(fw, s, rng, n);
            if (d.empty()) d = loadDifference(fw, s, posts);
        }
        return d;
    };
    const std::string start = taskStart(fw, s, posts);
    if (!start.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the load: %s\n", start.c_str());
    const long different = !start.empty() ? 1 : vc2SynthCases(fw, s, run, 2, "the load", oneCase);
    std::printf("synth load: the task's start, %d cases (%ld with a store), %ld events: %ld part reads, %ld "
                "dispatchers, %ld messages, %ld part messages\n",
                vc2SynthCaseCount(run), n.stores, n.events, n.byEvent[0], n.byEvent[1], n.byEvent[2], n.byEvent[3]);
    return different;
}
#endif
