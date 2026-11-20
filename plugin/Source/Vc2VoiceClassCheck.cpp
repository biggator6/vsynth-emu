// xt_vc2voices' fourth part (v279): the note voice's class (its six virtuals and the voice's words they set) and the
// update queue, Vc2VoiceClass and Vc2UpdateQueue, against rom.4's own routines on its own voices, keys and queue, after
// the release path's part on the seed after its.  The note's five routines, the conversion set's eight and the OS's
// lock, unlock, wait, signal and post are hooked and logged with their arguments (a C routine's struct read through its
// pointer), placing a set and C's refresh answering as drawn; the queue's routines run on both sides (the assigner's
// part's hook on the wait taken off).  The queue's 36 bytes are written into rom.4's memory before each event and
// compared after it, beside the voices' and keys' spans.  The draws, beyond the release path's: every voice on queue 0,
// its queue links one list (a few voices on the queue, in a drawn order, the rest linked to themselves), its set, its
// +0x58 and its bytes +0x50…+0x55; the queue's current node, flag and OS words.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2VoiceClassCheck.h"
#include <algorithm>

namespace {
using V = Vc2PartVoices;
using Q = Vc2UpdateQueue;
constexpr uint32_t kNoteAt = 0x8998a51c, kNoteBytes = 0x744;   // [FW] the notes W

// How a hooked routine's arguments are logged: the registers past r4, or C's struct through r5 (and r6 after it)
enum class Shape { regs0, regs1, regs2, set, setNote };

struct Hook {
    uint32_t routine;
    VoicesKind kind;
    Shape shape;
};

// [FW] the note's routines, the conversion set's and the OS's
constexpr Hook kHooks[] = {
    { 0x89396520, kVoicesNote, Shape::regs2 }, { 0x89396590, kVoicesNote, Shape::regs1 },
    { 0x89396650, kVoicesNote, Shape::regs0 }, { 0x893967a0, kVoicesNote, Shape::regs0 },
    { 0x893968b0, kVoicesNote, Shape::regs0 }, { 0x893a76a0, kVoicesSet, Shape::regs1 },
    { 0x893a76d0, kVoicesSet, Shape::setNote }, { 0x893a7a20, kVoicesSet, Shape::regs1 },
    { 0x893a7b20, kVoicesSet, Shape::setNote }, { 0x893a7c50, kVoicesSet, Shape::regs2 },
    { 0x893a7cc0, kVoicesSet, Shape::set },    { 0x893a7df0, kVoicesSet, Shape::regs1 },
    { 0x893a7f10, kVoicesSet, Shape::set },    { 0x893018e0, kVoicesOs, Shape::regs0 },
    { 0x893018f0, kVoicesOs, Shape::regs0 },   { 0x893018b0, kVoicesOs, Shape::regs0 },
    { 0x893018c0, kVoicesOs, Shape::regs0 },   { 0x893018a0, kVoicesOs, Shape::regs0 },
};
enum HookIndex { kConstruct, kEndNote, kBlocks, kUpdate, kRelease, kPlace, kMake, kEndSet, kConvert, kReleaseSet,
                 kUpdateSet, kCopy, kRefresh, kLock, kUnlock, kWait, kSignal, kPost };
// [FW] what the earlier parts hook and this one runs: the queue's wait and the note voice's virtuals
constexpr uint32_t kTheirsAgain[] = { 0x893951b0, 0x893970b0, 0x893971f0, 0x893973a0, 0x89397280, 0x89397130,
                                      0x893972d0 };

// A set placed, by the answer drawn: one for each note's number, or none
uint32_t setOf(const VoicesAnswer& a) {
    return a.note != 0 ? kClassSetsAt + kClassSetBytes * ((a.note - kNoteAt) / kNoteBytes) : 0;
}

// A routine's arguments by its shape: C's struct read through r5 only for C's routines that are given one
std::array<uint32_t, 7> argsOf(Sh4& c, Shape shape) {
    const std::array<std::array<uint32_t, 7>, 3> regs = { { {}, { c.r[5] }, { c.r[5], c.r[6] } } };
    if ((size_t)shape < regs.size()) return regs[(size_t)shape];
    const uint32_t s = c.r[5];
    return { c.rd32(s), c.rd32(s + 4), c.rd32(s + 8), shape == Shape::setNote ? c.r[6] : 0 };
}

} // namespace

// rom.4's side: the note's, C's and the OS's routines; the queue's wait its own again
void classInstallRom(ClassSides& c) {
    VoicesSides& s = c.r.a.s;
    for (size_t k = 0; k < std::size(kHooks); ++k)
        s.fw.cpu.hook(kHooks[k].routine, [&s, k](Sh4& cpu) {
            const Hook& h = kHooks[k];
            s.rom.push_back({ h.kind, h.routine, cpu.r[4], argsOf(cpu, h.shape) });
            const bool answers = k == kPlace || k == kRefresh;
            const VoicesAnswer* a = answers ? &s.draws.next(s.draws.rom) : nullptr;
            cpu.ret(a == nullptr ? 0 : k == kPlace ? setOf(*a) : a->ended);
        });
    for (const uint32_t at : kTheirsAgain) s.fw.cpu.unhook(at);
}

namespace {
std::string queueDiffer(ClassSides& c) {
    Vc2QueueWords theirs;
    c.r.a.s.fw.cpu.read(Q::kAt, &theirs, sizeof theirs);
    const auto* ours = reinterpret_cast<const uint32_t*>(&c.queue.q);
    const auto* rom = reinterpret_cast<const uint32_t*>(&theirs);
    for (uint32_t k = 0; k < sizeof theirs / 4; ++k) {
        if (ours[k] == rom[k]) continue;
        char t[96];
        std::snprintf(t, sizeof t, "the queue's +0x%02x: %08x, against %08x", 4 * k, ours[k], rom[k]);
        return t;
    }
    return "";
}

// The link after `node`: the queue's head's, or a voice's
uint32_t& nextAfter(ClassSides& c, uint32_t node) {
    return node == Q::kHeadAt ? c.queue.q.next : c.r.a.s.port.at(node).queueNext;
}

} // namespace

// The voices' queue links one list: a few on the queue in a drawn order, the rest linked to themselves, and one time
// in four a voice's link aimed elsewhere; every voice on queue 0; the queue's current node one of them, the head or none
void drawQueue(ClassSides& c, std::mt19937& rng) {
    V& port = c.r.a.s.port;
    std::array<uint32_t, V::kVoices> all {};
    for (int i = 0; i < V::kVoices; ++i) all[(size_t)i] = port.addressOf(i);
    std::ranges::shuffle(all, rng);
    const size_t queued = rng() % 8;
    uint32_t last = Q::kHeadAt;
    for (size_t i = 0; i < all.size(); ++i) {
        Vc2VoiceNode& n = port.at(all[i]);
        n.queue = 0;
        n.queued = rng() % 2;
        n.queueNext = all[i];
        n.queuePrev = all[i];
        if (i >= queued) continue;
        n.queuePrev = last;
        nextAfter(c, last) = all[i];
        last = all[i];
    }
    nextAfter(c, last) = Q::kHeadAt;
    c.queue.q.prev = last;
    const uint32_t odd = all[rng() % all.size()], to = rng() % 3 ? all[rng() % all.size()] : Q::kHeadAt;
    if (rng() % 4 == 0) (rng() % 2 ? port.at(odd).queueNext : port.at(odd).queuePrev) = to;
    const uint32_t currents[4] = { 0, Q::kHeadAt, all[0], all[rng() % all.size()] };
    c.queue.q.current = currents[rng() % 4];
    c.queue.q.vtable = rng() % 4 ? Q::kHeadVtable : (uint32_t)rng();
    c.queue.q.post = (uint32_t)rng();
    c.queue.q.lock = (uint32_t)rng();
}

namespace {
// A voice's words the class decides by: its set, +0x58 and the bytes +0x50…+0x55; its held bits any byte one time in
// four (the release path's draws keep them below 8)
void drawClassWords(Vc2Voice& w, std::mt19937& rng) {
    if (rng() % 4 == 0) w.node.held = (uint8_t)rng();
    w.set = rng() % 4 ? kClassSetsAt + kClassSetBytes * (uint32_t)(rng() % V::kVoices) : 0;
    w.word58 = rng() % 2;
    for (uint8_t* b : { &w.byte50, &w.byte51, &w.byte52, &w.byte53, &w.byte54, &w.byte55 })
        *b = (uint8_t)rng();
}

} // namespace

// The release path's draws, the queue's, the messages' keys (not past 127), the age counter now and then (its top bits
// set), the retune's word (0 in rom.4) half the time, and each voice's class words now and then
void classDraw(ClassSides& c, std::mt19937& rng, bool first) {
    steerRelease(c.r, rng, first);
    drawQueue(c, rng);
    if (first || rng() % 8 == 0) c.r.a.s.port.ages = (uint32_t)rng();
    c.r.a.s.fw.cpu.wr32(kClassRetuneWordAt, rng() % 2 ? 0 : (uint32_t)rng());
    for (size_t k = 0; k < (size_t)kAssignMessages; ++k) c.r.keys.glides[k].key = (int8_t)(rng() % 128);
    for (int i = 0; i < V::kVoices; ++i)
        if (first || rng() % 4 == 0) drawClassWords(c.r.a.s.port.voice(c.r.a.s.port.addressOf(i)), rng);
}

std::string classStart(ClassSides& c, std::mt19937& rng) {
    const std::string what = releaseStart(c.r, rng);
    c.queue.q.unread = (uint32_t)rng();
    return what;
}

namespace {
void classCount(ClassSides& c) {
    ++c.n.events;
    c.n.setCalls += vc2CountCalls(c.r.a.s.mine, kVoicesSet);
    c.n.osCalls += vc2CountCalls(c.r.a.s.mine, kVoicesOs);
}

void sayClassCounts(const ClassCounts& n) {
    const auto& e = n.byEvent;
    std::printf("class: %ld events: %ld updates, %ld retunes, %ld destroys, %ld key-offs, %ld prepares, %ld change "
                "tests, %ld and %ld words; queue: %ld constructors, %ld initializers, %ld links, %ld unlinks, "
                "%ld enqueues, %ld moves on, %ld passes on, %ld takes off, %ld and %ld waits, %ld moves (%ld set "
                "calls, %ld OS calls)\n", n.events, e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9],
                e[10], e[11], e[12], e[13], e[14], e[15], e[16], e[17], e[18], n.setCalls, n.osCalls);
}
} // namespace

ClassSides::ClassSides(VoicesSides& s) : r(s), parts(s, queue) {}

uint32_t ClassPortParts::logged(int32_t kind, uint32_t routine, uint32_t object, std::array<uint32_t, 7> more) {
    s_.mine.push_back({ kind, routine, object, more });
    return 0;
}

void ClassPortParts::constructNote(uint32_t note, uint32_t part, uint32_t kind) {
    logged(kVoicesNote, kHooks[kConstruct].routine, note, { part, kind });
}

void ClassPortParts::endNote(uint32_t note) {
    logged(kVoicesNote, kHooks[kEndNote].routine, note, { 3 });
}

void ClassPortParts::noteBlocks(uint32_t note) {
    logged(kVoicesNote, kHooks[kBlocks].routine, note, {});
}

void ClassPortParts::noteUpdate(uint32_t note) {
    logged(kVoicesNote, kHooks[kUpdate].routine, note, {});
}

void ClassPortParts::noteRelease(uint32_t note) {
    logged(kVoicesNote, kHooks[kRelease].routine, note, {});
}

uint32_t ClassPortParts::placeSet(uint32_t number) {
    logged(kVoicesSet, kHooks[kPlace].routine, kClassSetBytes, { number });
    return setOf(s_.draws.next(s_.draws.port));
}

void ClassPortParts::makeSet(uint32_t set, const Vc2SetCall& call, uint32_t note) {
    logged(kVoicesSet, kHooks[kMake].routine, set, { call.channelAt, call.byteAt, call.word, note });
}

void ClassPortParts::endSet(uint32_t set) {
    logged(kVoicesSet, kHooks[kEndSet].routine, set, { 3 });
}

void ClassPortParts::convertSet(uint32_t set, const Vc2SetCall& call, uint32_t note) {
    logged(kVoicesSet, kHooks[kConvert].routine, set, { call.channelAt, call.byteAt, call.word, note });
}

void ClassPortParts::releaseSet(uint32_t set, uint32_t velocity, uint32_t note) {
    logged(kVoicesSet, kHooks[kReleaseSet].routine, set, { velocity, note });
}

void ClassPortParts::updateSet(uint32_t set, const Vc2SetCall& call) {
    logged(kVoicesSet, kHooks[kUpdateSet].routine, set, { call.channelAt, call.byteAt, call.word });
}

void ClassPortParts::copySet(uint32_t set, uint32_t note) {
    logged(kVoicesSet, kHooks[kCopy].routine, set, { note });
}

uint32_t ClassPortParts::refreshSet(uint32_t set, const Vc2SetCall& call) {
    logged(kVoicesSet, kHooks[kRefresh].routine, set, { call.channelAt, call.byteAt, call.word });
    return s_.draws.next(s_.draws.port).ended;
}

void ClassPortParts::lock() {
    logged(kVoicesOs, kHooks[kLock].routine, kClassLockAt, {});
}

void ClassPortParts::unlock() {
    logged(kVoicesOs, kHooks[kUnlock].routine, kClassLockAt, {});
}

void ClassPortParts::wait(uint32_t object) {
    logged(kVoicesOs, kHooks[kWait].routine, object, {});
}

void ClassPortParts::signal(uint32_t object) {
    logged(kVoicesOs, kHooks[kSignal].routine, object, {});
}

void ClassPortParts::post(uint32_t object) {
    logged(kVoicesOs, kHooks[kPost].routine, object, {});
}

Vc2UpdateQueue& ClassPortParts::queueAt(uint32_t queue) {
    if (queue != Q::kAt) logged(kVoicesOs, 0, queue, {});
    return queue_;
}

void ClassPortParts::enqueue(uint32_t queue, uint32_t voice) {
    queueAt(queue).enqueue(voice, s_.port, *this);
}

void ClassPortParts::takeOff(uint32_t queue, uint32_t voice) {
    queueAt(queue).takeOff(voice, s_.port, *this);
}

void ClassPortParts::waitQueue(uint32_t queue, uint32_t voice) {
    queueAt(queue).wait(voice, *this);
}

std::string classOnBoth(ClassSides& c, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                        const std::function<uint32_t()>& mine) {
    VoicesSides& s = c.r.a.s;
    s.fw.cpu.write(Vc2Keys::kTablesAt, &c.r.keys, sizeof c.r.keys);
    s.fw.cpu.write(Q::kAt, &c.queue.q, sizeof c.queue.q);
    const std::string what = vc2BothAnswer(s.fw, routine, args, mine, [&] {
        std::string d = voicesDifference(s);
        if (d.empty()) d = releaseKeysDiffer(c.r);
        return d.empty() ? queueDiffer(c) : d;
    });
    return what.empty() ? what : std::string(routine.name) + ": " + what;
}

long vc2VoiceClassPart(VoicesSides& s, const Vc2Run& run) {
    return voicesPart<ClassSides, classInstallRom, classStart, classDraw, classEvents, classCount, sayClassCounts>(
        s, run, 3);
}
#endif
