// xt_msm's two sides (XtMsmCheck.h): the firmware's Multi Step Modulator in the interpreter, driven as the tone task, the
// note allocator and the arpeggio drive it, and the port beside it.  After every event: the object's bytes from +0x1c (the
// time its timer was set for) to the last track's end, its timer's +8 and +0xc, the tempo ticks, the tone updates and the
// timer's messages; and the terms.  HARDWARE_FIRMWARE.md, "The Multi Step Modulator".
#ifdef XT_MSM_MAIN
#include "XtMsmCheck.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
constexpr uint32_t kObj = XtMsmFw::kObj;
constexpr uint32_t kObjBytes = 0xb0;                        // to the second vtable at +0xac [FW 0x894d9820]
constexpr uint32_t kOwner = 0x89cf76e4;                     // the loaders' r4, which they don't read [FW 0x894d44d0]
// The OS's tempo timer: its list, its count, its next [FW 0x8930eb00]
constexpr uint32_t kTimerGlobals[] = { 0x898586ec, XtMsmFw::kNow, 0x898586f4 };
constexpr uint32_t kQueue = 0x8987cda8, kFlag = 0x20;      // the tone task's queue, the MSM's flag [FW 0x894d4008]
constexpr uint32_t kPartList = 0x89cf5e84;                  // part 0's notes, which an update goes to [FW 0x894d96b0]
constexpr uint32_t kCtor = 0x894d9820, kInit = 0x894d9580, kLoad = 0x894d9950, kEdit = 0x894d9b20;
constexpr uint32_t kNoteOnAt = 0x894d4b08, kNoteOnEnd = 0x894d4bd4;   // the allocator's head
constexpr uint32_t kTick = 0x8930eb00, kClocks = 0x894d4240, kArpOn = 0x894d9920, kArpOff = 0x894d9930;
constexpr uint32_t kTerm = 0x894d96d0, kTermPart = 0x894d9750, kTermAll = 0x894d97b0, kShown = 0x894d9870;
constexpr uint32_t kPoint = 0x89479b70, kGet = 0x8947d310;  // the MSM block's read object and the getter
constexpr uint32_t kParts = 0x897a0fc8;                     // the parts' objects: +0 which store, +4 and +8 the two
constexpr uint32_t kPost = 0x893018a0, kUpdate = 0x894d72f0, kHeld = 0x894d5550, kSounding = 0x894d55a0;
constexpr uint32_t kPartByte = xtfw::kL;                    // the note's part, which the head reads through r5
constexpr uint32_t kFrom = 0x1c, kTo = 0x2c + 30 * XtMsm::kTracks;   // the object's bytes compared
constexpr int kSaid = 20;                                   // differences said with XT_MSM_FIRST
constexpr int kDests = 72, kZones = 16;

int asInt(uint32_t r0) {
    return (int)(int32_t)r0;
}

// How the firmware is given each kind of event: its routine, called as its caller calls it
void giveSettings(XtMsmFw& f, const XtMsmEvent& e, uint32_t routine) {
    f.given = e.settings;
    f.call(routine, { kOwner, (uint32_t)e.part });
}

void noteOnHead(XtMsmFw& f, const XtMsmEvent& e, uint32_t routine) {
    f.keyHeld = e.flag;
    f.m.cpu.wr8(kPartByte, (uint8_t)e.part);
    f.call(routine, { 0, kPartByte, 0, 0 });
}

void tempoTicks(XtMsmFw& f, const XtMsmEvent& e, uint32_t routine) {
    f.call(routine, { (uint32_t)e.ticks });
}

void toneTask(XtMsmFw& f, const XtMsmEvent& e, uint32_t routine) {
    f.sounding = e.flag;
    f.call(routine, { kObj });
}

void arpeggioFlag(XtMsmFw& f, const XtMsmEvent& e, uint32_t routine) {
    f.call(e.flag ? routine : kArpOff, {});
}

// Each kind of event: the firmware's routine, how it's called, and the port's
struct Kind {
    uint32_t routine;
    void (*fw)(XtMsmFw&, const XtMsmEvent&, uint32_t);
    void (*port)(XtMsm&, const XtMsmEvent&);
};
// one-off: a table's rows, one a kind of event, alike by design: each names its own firmware routine and port call
constexpr Kind kKinds[XtMsmEvent::kKinds] = {
    { kLoad, giveSettings, [](XtMsm& p, const XtMsmEvent& e) { p.load(e.settings, e.part); } },
    { kEdit, giveSettings, [](XtMsm& p, const XtMsmEvent& e) { p.edit(e.settings, e.part); } },
    { kNoteOnAt, noteOnHead, [](XtMsm& p, const XtMsmEvent& e) { p.noteOn(e.part, e.flag); } },
    { kTick, tempoTicks, [](XtMsm& p, const XtMsmEvent& e) { p.tick(e.ticks); } },
    { kClocks, toneTask, [](XtMsm& p, const XtMsmEvent& e) { p.clocks(e.flag); } },
    { kArpOn, arpeggioFlag, [](XtMsm& p, const XtMsmEvent& e) { p.arpeggio(e.flag); } },
};
} // namespace

XtMsmFw::XtMsmFw(const std::string& rom) : m(rom) {
    if (!m.ok) return;
    m.cpu.hook(kPost, [this](Sh4& c) {
        ++posts;
        c.ret(0);
    });
    m.cpu.hook(kUpdate, [this](Sh4& c) {
        ++updates;
        c.ret(0);
    });
    m.cpu.hook(kHeld, [this](Sh4& c) { c.ret(keyHeld ? 1 : 0); });
    m.cpu.hook(kSounding, [this](Sh4& c) { c.ret(sounding ? 1 : 0); });
    m.cpu.hook(kPoint, [this](Sh4& c) {
        if (fromStore) return;                              // the firmware's own, on the patch's store
        c.wr32(c.r[4] + 4, 1);                              // a block there: the getter is asked, not the defaults
        c.ret(c.r[4]);
    });
    m.cpu.hook(kGet, [this](Sh4& c) {
        if (fromStore) return;
        const int* v = xtMsmField(given, (int)c.r[5]);
        if (!v) failed = "the getter asked for ID " + std::to_string(c.r[5]);
        c.ret(v ? (uint32_t)*v : 0);
    });
    m.cpu.hook(kNoteOnAt, [this](Sh4& c) { entryPr = c.pr; });
    m.cpu.hook(kNoteOnEnd, [this](Sh4& c) { c.pc = entryPr; });
    built = true;
}

uint32_t XtMsmFw::call(uint32_t addr, std::initializer_list<uint32_t> args) {
    uint32_t r0 = 0;
    if (failed.empty() && !m.cpu.call(addr, args, xtfw::kStack, &r0)) failed = m.cpu.error();
    return r0;
}

void XtMsmFw::begin() {
    for (const uint32_t a : kTimerGlobals) m.cpu.wr32(a, 0);
    const std::vector<uint8_t> zero(kObjBytes, 0);
    m.cpu.write(kObj, zero.data(), zero.size());
    m.cpu.wr32(kPartList, 1);
    updates = 0;
    posts = 0;
    failed.clear();
    given = XtMsmSettings();
    sounding = false;
    keyHeld = false;
    fromStore = false;
    call(kCtor, {});
    call(kInit, { kObj, kQueue, kFlag });
}

void XtMsmFw::readStore(const std::vector<uint8_t>* store) {
    fromStore = store != nullptr;
    if (!store) return;
    const uint32_t part = m.cpu.rd32(kParts);               // part 0, both its stores the patch's [FW 0x897a0fc8]
    m.cpu.wr8(part, 1);
    m.cpu.wr32(part + 4, xtfw::kSamp);
    m.cpu.wr32(part + 8, xtfw::kSamp);
    m.cpu.write(xtfw::kSamp, store->data(), store->size());
}

int XtMsmFw::term(int dest, int part, int zone) {
    return asInt(call(kTerm, { kObj, (uint32_t)dest, (uint32_t)part, (uint32_t)zone }));
}

int XtMsmFw::termAnyZone(int dest, int part) {
    return asInt(call(kTermPart, { kObj, (uint32_t)dest, (uint32_t)part }));
}

int XtMsmFw::termAll(int dest) {
    return asInt(call(kTermAll, { kObj, (uint32_t)dest }));
}

int XtMsmFw::shownStep(int n) {
    return asInt(call(kShown, { (uint32_t)n }));
}

// ── The two together ─────────────────────────────────────────────────────────

void XtMsmCheck::begin() {
    fw.begin();
    port.reset();
    fires = 0;
    compare();
}

void XtMsmCheck::run(const XtMsmEvent& e) {
    const Kind& k = kKinds[e.kind];
    k.fw(fw, e, k.routine);
    const int32_t before = port.pending;
    k.port(port, e);
    if (port.pending > before) fires += port.pending - before;
    ++events;
    compare();
}

void XtMsmCheck::same(const char* what, long fwValue, long portValue) {
    if (fwValue == portValue) return;
    ++different;
    if (first && said < kSaid) {
        ++said;
        std::printf("  %s, event %ld: %s: firmware %ld, port %ld\n", where.c_str(), events, what, fwValue, portValue);
    }
}

void XtMsmCheck::compare() {
    ++states;
    if (!fw.failed.empty()) {
        same(("a firmware call: " + fw.failed).c_str(), 1, 0);
        fw.failed.clear();
    }
    uint8_t mine[kTo - kFrom] = {};
    const int32_t words[2] = { port.pos, port.pending };
    for (uint32_t i = 0; i < 8; ++i) mine[i] = (uint8_t)((uint32_t)words[i / 4] >> (8 * (i % 4)));
    const uint8_t flags[4] = { port.running, port.arp, port.master, port.keySync };
    std::memcpy(mine + (0x24 - kFrom), flags, sizeof flags);
    std::memcpy(mine + (0x28 - kFrom), port.loops, sizeof port.loops);
    std::memcpy(mine + (0x2c - kFrom), port.track, sizeof port.track);
    const long before = different;
    for (uint32_t off = kFrom; off < kTo; ++off) {
        char what[48];
        if (off < 0x2c) std::snprintf(what, sizeof what, "object +0x%02x", off);
        else std::snprintf(what, sizeof what, "track %u +0x%02x", (off - 0x2c) / 30 + 1, (off - 0x2c) % 30);
        same(what, fw.rd8(off), mine[off - kFrom]);
    }
    if (first && before == 0 && different > 0) {           // the first: both objects' bytes, a track a line
        for (uint32_t off = kFrom; off < kTo; off += 30) {
            std::printf("    +0x%02x", off);
            for (uint32_t i = off; i < off + 30 && i < kTo; ++i) std::printf(" %02x/%02x", fw.rd8(i), mine[i - kFrom]);
            std::printf("\n");
        }
    }
    same("the timer's time", (int32_t)fw.rd32(8), port.due);
    same("the timer's last", (int32_t)fw.rd32(0xc), port.fired);
    same("the tempo ticks", fw.now(), port.now);
    same("the tone updates", fw.updates, port.updates);
    same("the timer's messages", fw.posts, fires);
}

void XtMsmCheck::termsAlike(std::mt19937& r) {
    for (int n = 0; n < XtMsm::kTracks; ++n) {
        const XtMsmTrack& t = port.track[n];
        same("a track's term", fw.term(t.dest, 0, t.zone), port.term(t.dest, 0, t.zone));
        same("a track's term, any zone", fw.termAll(t.dest), port.termAll(t.dest));
        same("the step shown", fw.shownStep(n), port.shownStep(n));
        terms += 3;
    }
    const int dest = (int)(r() % (kDests + 8)), part = (int)(r() % 2), zone = (int)(r() % kZones);
    same("a drawn term", fw.term(dest, part, zone), port.term(dest, part, zone));
    same("a drawn term, any zone", fw.termAnyZone(dest, part), port.termAnyZone(dest, part));
    terms += 2;
}

void XtMsmCheck::sweep() {
    for (int dest = 0; dest < kDests; ++dest)
        for (int zone = 0; zone < kZones; ++zone) {
            same("a term of the sweep", fw.term(dest, 0, zone), port.term(dest, 0, zone));
            ++terms;
        }
}
#endif
