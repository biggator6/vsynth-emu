// xt_arp's two sides (XtArpeggioCheck.h): the firmware's arpeggio object in the interpreter, driven as the realtime layer
// drives it, and the port beside it; their messages compared one by one, and every field of the object after each key,
// setting and step.  HARDWARE_FIRMWARE.md, "The arpeggiator".
#ifdef XT_ARP_MAIN
#include "XtArpeggioCheck.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
constexpr uint32_t kObj = 0x8987cef8;          // the main arpeggio object [FW 0x893060a2]
constexpr uint32_t kObjBytes = 0xd48;          // [FW 0x89304960]
constexpr uint32_t kPatternBuf = 0x89880f83;   // 0x8930ca30's pattern: the 16 lines, then 32 × 16 cells
constexpr uint32_t kTask = 0x8987cd40;         // the arpeggio's task, which a fired timer wakes [FW 0x893049e0]
constexpr uint32_t kStepFlag = 0x200;
// The OS's tempo timer: its list, its count, its next, and the arpeggio objects' tempo group [FW 0x8930eb00, 0x89304360]
constexpr uint32_t kTimerGlobals[] = { 0x898586ec, 0x898586f0, 0x898586f4, 0x898586cc };
constexpr uint32_t kTimerNow = 0x898586f0;
constexpr uint32_t kCtor = 0x89304960, kEnable = 0x89304920, kShuffle = 0x89304430, kPedalCheck = 0x89303380;
constexpr uint32_t kSettled = 0x893033d0, kStop = 0x89303ab0, kNoteOn = 0x89303830, kNoteOff = 0x893039c0;
constexpr uint32_t kPedal = 0x89303a80, kTick = 0x8930eb00, kStep = 0x89302570;
constexpr uint32_t kPost = 0x89472e70, kEventSet = 0x893018a0, kEventClear = 0x893018b0;
// Answered with nothing: the Multi Step Modulator's beat and flag, the OS's ms clock (+0xd28), new (never reached)
constexpr uint32_t kQuiet[] = { 0x894da9f0, 0x894d9920, 0x894d9930, 0x89301940, 0x8952ff3c };
constexpr uint32_t kKeyNodes = 0x108, kOffNodes = 0x728, kNodeBytes = 12;
constexpr uint32_t kSettings = 0xac, kSettingBytes = 9;
constexpr int kSaid = 20;                      // differences said with XT_ARP_FIRST
} // namespace

XtArpFw::XtArpFw(const std::string& rom) : m(rom) {
    if (!m.ok) return;
    m.cpu.hook(kPost, [this](Sh4& c) {
        uint8_t b[4] = {};
        c.read(c.r[5], b, sizeof b);
        sent.push_back({ b[0], b[1], b[2] });
        c.ret(0);
    });
    m.cpu.hook(kEventSet, [this](Sh4& c) {
        if (c.r[4] == kTask && (c.r[5] & kStepFlag)) stepFlag = true;
        c.ret(0);
    });
    m.cpu.hook(kEventClear, [](Sh4& c) { c.ret(0); });
    for (const uint32_t a : kQuiet) m.cpu.hook(a, [](Sh4& c) { c.ret(0); });
    built = true;
}

void XtArpFw::call(uint32_t addr, std::initializer_list<uint32_t> args) {
    if (!failed.empty()) return;
    if (!m.cpu.call(addr, args, xtfw::kStack)) failed = m.cpu.error();
}

uint32_t XtArpFw::rd(uint32_t off, int bytes) {
    if (bytes == 1) return m.cpu.rd8(kObj + off);
    if (bytes == 2) return m.cpu.rd16(kObj + off);
    return m.cpu.rd32(kObj + off);
}

void XtArpFw::begin() {
    for (const uint32_t a : kTimerGlobals) m.cpu.wr32(a, 0);
    const std::vector<uint8_t> zero(kObjBytes, 0);
    m.cpu.write(kObj, zero.data(), zero.size());
    sent.clear();
    failed.clear();
    stepFlag = false;
    call(kCtor, { kObj });
}

void XtArpFw::channel(int ch) {
    m.cpu.wr8(kObj + 0xd2d, (uint8_t)ch);
    call(kEnable, { kObj, 1 });
}

void XtArpFw::settings(const XtArpSettings& s) {
    if ((rd(0xd2c, 1) != 0) != s.on) {
        m.cpu.wr8(kObj + 0xd2c, s.on ? 1 : 0);
        call(kStop, { kObj });
    }
    const int v[kSettingBytes] = { s.hold, s.motif, s.shuffleRate, s.shuffleRes, s.duration,
                                   s.grid, s.endPoint, s.octave, s.keyVelocity };
    for (uint32_t i = 0; i < kSettingBytes; ++i) m.cpu.wr8(kObj + kSettings + i, (uint8_t)v[i]);
    call(kShuffle, { kObj, rd(0xae, 1), rd(0xaf, 1) });
    call(kPedalCheck, { kObj });
    call(kSettled, { kObj });
}

void XtArpFw::pattern(const XtArpPattern& p) {
    m.cpu.write(kPatternBuf, p.line, sizeof p.line);
    m.cpu.write(kPatternBuf + sizeof p.line, p.cell, sizeof p.cell);
    m.cpu.wr32(kObj + 0xb8, kPatternBuf);
}

void XtArpFw::noteOn(int key, int vel) {
    call(kNoteOn, { kObj, (uint32_t)key, (uint32_t)vel });
}

void XtArpFw::noteOff(int key) {
    call(kNoteOff, { kObj, (uint32_t)key });
}

void XtArpFw::pedal(int value) {
    call(kPedal, { kObj, (uint32_t)value });
}

void XtArpFw::stop() {
    call(kStop, { kObj });
}

void XtArpFw::tick(int32_t ticks) {
    call(kTick, { (uint32_t)ticks });
    if (!stepFlag) return;
    stepFlag = false;
    if ((int32_t)rd(0xfc, 4) < XtArpeggio::kNever) call(kStep, { kObj });
}

// ── The two together ─────────────────────────────────────────────────────────

void XtArpCheck::begin(int ch, const XtArpSettings& s, const XtArpPattern& p) {
    fw.begin();
    port.reset();
    fw.channel(ch);
    port.setChannel(ch);
    pattern(p);
    settings(s);
}

void XtArpCheck::settings(const XtArpSettings& s) {
    fw.settings(s);
    port.setSettings(s);
    compare(true);
}

void XtArpCheck::pattern(const XtArpPattern& p) {
    fw.pattern(p);
    port.setPattern(p);
}

void XtArpCheck::noteOn(int key, int vel) {
    fw.noteOn(key, vel);
    port.noteOn(key, vel);
    compare(true);
}

void XtArpCheck::noteOff(int key) {
    fw.noteOff(key);
    port.noteOff(key);
    compare(true);
}

void XtArpCheck::pedal(int value) {
    fw.pedal(value);
    port.pedal(value);
    compare(true);
}

void XtArpCheck::stop() {
    fw.stop();
    port.stop();
    compare(true);
}

// The fields are compared after a tick only when the timer was due on either side
void XtArpCheck::tick(int32_t ticks) {
    const bool due = port.due <= port.now + ticks || (int32_t)fw.rd(8, 4) <= port.now + ticks;
    fw.tick(ticks);
    port.tick(ticks);
    compare(due);
}

void XtArpCheck::differs(const char* what, long fwValue, long portValue) {
    if (first && different < kSaid)
        std::printf("%s, op %ld: %s: firmware %ld, port %ld\n", where.c_str(), ops, what, fwValue, portValue);
    ++different;
}

void XtArpCheck::compare(bool fields) {
    ++ops;
    const int n = port.eventCount();
    const size_t most = std::max((size_t)n, fw.sent.size());
    const auto word = [](const XtArpEvent* e) { return e ? (long)(e->status << 16 | e->d1 << 8 | e->d2) : -1L; };
    for (size_t i = 0; i < most; ++i) {
        ++events;
        const XtArpEvent* p = (int)i < n ? port.events() + i : nullptr;
        const XtArpEvent* f = i < fw.sent.size() ? &fw.sent[i] : nullptr;
        if (p && word(p) == word(f))
            ++byMotif[port.motif & 7];
        else
            differs("a message", word(f), word(p));
    }
    fw.sent.clear();
    port.clearEvents();
    if (!fw.failed.empty()) {
        differs(("the firmware's call: " + fw.failed).c_str(), 0, 0);
        fw.failed.clear();
    }
    const auto fwNow = (int32_t)fw.global(kTimerNow), fwDue = (int32_t)fw.rd(8, 4);
    if (fwNow != port.now) differs("the tempo timer", fwNow, port.now);
    if (fwDue != port.due) differs("the timer's time", fwDue, port.due);
    if (!fields) return;
    ++states;
    scalarsAlike();
    nodesAlike();
}

void XtArpCheck::same(const char* name, uint32_t off, int bytes, long want) {
    const uint32_t mask = bytes == 4 ? 0xffffffffu : (1u << (8 * bytes)) - 1;
    const uint32_t got = fw.rd(off, bytes);
    if (got != ((uint32_t)want & mask)) differs(name, (long)got, want);
}

void XtArpCheck::link(const char* name, uint32_t off, uint32_t nodes, long want) {
    const uint32_t p = fw.rd(off, 4);
    const long got = p == 0 ? -1 : (long)(p - (kObj + nodes)) / (long)kNodeBytes;
    if (got != want) differs(name, got, want);
}

// The object's own fields, as a table: name, offset, bytes, the port's value
void XtArpCheck::scalarsAlike() {
    const XtArpeggio& a = port;
    struct Field {
        const char* name;
        uint32_t off;
        int bytes;
        long want;
    };
    const Field fields[] = {
        { "pos", 0x1c, 4, a.pos },
        { "base", 0x20, 4, a.base },
        { "shuffle rate", 0x30, 1, a.shuffleRate },
        { "shuffle resolution", 0x31, 1, a.shuffleRes },
        { "timer mode", 0xaa, 1, a.timerMode },
        { "channel", 0xbc, 1, a.channel },
        { "last note", 0xbe, 1, a.lastNote },
        { "last velocity", 0xbf, 1, a.lastVelocity },
        { "octave", 0xc0, 1, a.octave },
        { "direction", 0xc1, 1, a.direction },
        { "octave seed", 0xc6, 2, a.octaveSeed },
        { "pedal", 0xce, 1, a.pedalValue },
        { "step", 0xe0, 1, a.step },
        { "line", 0xe1, 1, a.line },
        { "started", 0xe2, 1, a.started },
        { "CC 64 sent", 0xf4, 1, a.cc64 },
        { "CC 66 sent", 0xf5, 1, a.cc66 },
        { "octave pending", 0xf6, 1, a.octavePending },
        { "next step", 0xfc, 4, a.next },
        { "the step's time", 0x100, 4, a.nominal },
        { "current key", 0x718, 1, a.curKey },
        { "notes sounding, counted", 0x719, 1, a.soundingCount },
        { "RANDOM's seed", 0x720, 2, a.keySeed },
        { "timer fired at", 0xc, 4, a.fired },
    };
    for (const Field& f : fields) same(f.name, f.off, f.bytes, f.want);
    const long set[kSettingBytes] = { a.hold, a.motif, a.rate, a.res, a.duration,
                                      a.grid, a.endPoint, a.octaveRange, a.keyVelocity };
    sameAll("a setting", kSettings, 1, set, (int)kSettingBytes);
    sameAll("shuffle", 0x32, 1, a.shuffle, XtArpeggio::kShuffle);
    sameAll("keys down", 0xd0, 2, a.held, 8);
    sameAll("a line's controller", 0xe4, 1, a.lineCc, 16);
    sameAll("notes sounding", 0x708, 2, a.sounding, 8);
}

// The keys' and the notes' lists, node by node, links as the nodes they point at
void XtArpCheck::nodesAlike() {
    const XtArpeggio& a = port;
    link("first key", 0x104, kKeyNodes, a.keyHead);
    link("current key's node", 0x71c, kKeyNodes, a.curNode);
    link("first note to end", 0x724, kOffNodes, a.offHead);
    for (int k = 0; k < XtArpeggio::kKeys; ++k) {
        const uint32_t at = kKeyNodes + kNodeBytes * (uint32_t)k;
        same("a key's velocity", at + 1, 1, a.keys[k].vel);
        link("a key's prev", at + 4, kKeyNodes, a.keys[k].prev);
        link("a key's next", at + 8, kKeyNodes, a.keys[k].next);
        const uint32_t off = kOffNodes + kNodeBytes * (uint32_t)k;
        same("a note's channel", off, 1, a.offs[k].ch);
        same("a note's end", off + 4, 4, a.offs[k].time);
        link("a note's next", off + 8, kOffNodes, a.offs[k].next);
    }
}
#endif
