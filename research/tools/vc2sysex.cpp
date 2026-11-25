// vc2run sysex's probe (vc2sysex.h): rom.4's SysEx receive object, its parser, and the setters a DT1 ends in, hooked.
// The object is built by its own static initializer; the parser 0x89310640(object, message) is what the OS MIDI
// handler calls on its message 0x01.  Model 00 00 0D's DT1 (0x8931c1b0) reads Patch Category (0x12c) to choose a
// table's entries, then hands each value to a setter by its group: those are hooked, and so are the reads, the read
// objects and the screen's message, so nothing past the setters runs.
#include "vc2sysex.h"
#include <cstdio>

namespace vc2::sx {

namespace {

constexpr uint32_t kObject = 0x8995dd0c;         // [FW] the SysEx receive object (.bss); its class 0x898c8360
constexpr uint32_t kMakeObject = 0x89315bc0;     // [FW] its static initializer (the init table's word 0x898c5ae4)
constexpr uint32_t kMakeMap = 0x89316a40;        // [FW] (its + 0x938): the tables' counts, as the SysEx task's
constexpr uint32_t kMap = 0x8995e644;            // [FW] start (0x89314ce0) makes them before its loop
constexpr uint32_t kParse = 0x89310640;          // [FW] (object, message: a word of length, then F0 … F7)
constexpr uint32_t kReadObject = 0x893207e0;     // [FW] (object, 0): a read object on the temporary patch
constexpr uint32_t kStoredObject = 0x8931a9d0;   // [FW] (object, patch): a read object on a stored patch
constexpr uint32_t kRead = 0x893206d0;           // [FW] (object, out, ID, count): an ID's bytes into out
constexpr uint32_t kScreenMessage = 0x89315480;  // [FW] (code): a message on the screen
constexpr uint32_t kModel24 = 0x89319250;        // [FW] model 00 24's DT1 (the object + 0x274, a 5-byte address)
constexpr uint32_t kNames = 0x898b5798;          // [FW] rom.4's names: a pointer an ID, to a leading space
constexpr uint32_t kInfoBlock = 0, kInfoData = 4, kInfoValue = 8;   // [FW] a setter's info (0x8931c4cc fills it)
constexpr uint32_t kInfoNamed = 0x10;            // [FW] the named ID, the value's last byte's (0x8931b158)
constexpr uint32_t kOutPort = 0x89958870;        // [FW] MIDI out A's port object, where an answer goes (.bss)
constexpr uint32_t kUsbOut = 0x8995bb10;         // [FW] USB out's, where it goes as well
constexpr uint32_t kMakePort = 0x89312740;       // [FW] (port, ring, words): its ring of words, as row 14's
constexpr uint32_t kRingWords = 4096, kPortSlots = 8;
constexpr uint32_t kRingStart = 0x1c, kRingEnd = 0x20, kRingRead = 0x24, kRingWritten = 0x28;   // [FW] the port's
constexpr uint32_t kPortClass = 0x2c;            // [FW] its class (12 bytes a slot, the method at +8)

// The setters, by group: (this, ID, info), or zoned (this, zone, ID, info); a stored patch's (0x8931ba70) first
// test Exclusive Protect (0x8992e3d2)
struct Leaf {
    uint32_t at;
    int group;
    const char* what;
    bool zoned, stored;
};
constexpr Leaf kLeaves[] = {
    {0x8931b410, 1, "setup", false, false},     {0x8931b460, 2, "system", false, false},  // [FW] 0x8931b110's
    {0x8931b540, 3, "system", false, false},    {0x8931b590, 4, "MIC", false, false},     // [FW]
    {0x8931b5e0, 5, "common", false, false},    {0x8931b6f0, 6, "common", false, false},  // [FW]
    {0x8931b870, 10, "env", true, false},       {0x8931b8c0, 11, "OSC", true, false},     // [FW] zone 1; 13, 14 zone 2
    {0x8931bbc0, 5, "common", false, true},     {0x8931bd40, 6, "common", false, true},   // [FW] 0x8931ba70's
    {0x8931bdc0, 7, "MFX", false, true},        {0x8931be40, 8, "Chorus", false, true},   // [FW]
    {0x8931bec0, 9, "Reverb", false, true},     {0x8931bf40, 10, "env", true, true},      // [FW]
    {0x8931bff0, 11, "OSC", true, true},        {0x8931c0e0, 12, "COSM", true, true}};    // [FW]

// The temporary patch's effect and COSM objects: made, a check (object, data byte, ID, bytes) that skips the set when
// it answers the bytes, the set (object, value, ID), unmade
struct FxObject {
    uint32_t make, check, set, unmake;
    int group;
    const char* what;
};
constexpr FxObject kFx[] = {{0x89321c10, 0x89321d50, 0x89321d10, 0x89321c90, 7, "MFX"},      // [FW]
                            {0x89321de0, 0x89321f20, 0x89321eb0, 0x89321e30, 8, "Chorus"},   // [FW]
                            {0x89321fb0, 0x893220c0, 0x89322080, 0x89322000, 9, "Reverb"},   // [FW]
                            {0x89322790, 0x89322980, 0x89322930, 0x89322830, 12, "COSM"}};   // [FW] (obj, part, zone)

void leafHooks(Probe& p) {
    for (const Leaf& l : kLeaves) {
        p.m.cpu.hook(l.at, [&p, l](Sh4& c) {
            const uint32_t info = c.r[l.zoned ? 7 : 6];
            const int zone = l.zoned ? (int)c.r[5] : -1;
            const uint32_t at = c.rd32(info + kInfoData) - p.data;
            p.sets.push_back({l.group, l.what, zone, c.r[l.zoned ? 6 : 5], c.rd32(info + kInfoNamed),
                              c.rd32(info + kInfoValue), at, c.rd32(info + kInfoBlock), l.stored});
            c.ret(0);
        });
    }
}

void fxHooks(Probe& p) {
    for (const FxObject& o : kFx) {
        p.m.cpu.hook(o.make, [&p](Sh4& c) {
            p.zone = (int)c.r[6];
            c.ret(0);
        });
        p.m.cpu.hook(o.check, [&p](Sh4& c) {
            p.info = c.r[12];
            c.ret(c.r[7] + 1);
        });
        p.m.cpu.hook(o.set, [&p, o](Sh4& c) {
            const int zone = o.group == 12 ? p.zone : -1;
            const uint32_t at = c.rd32(p.info + kInfoData) - p.data;
            p.sets.push_back({o.group, o.what, zone, c.r[6], c.rd32(p.info + kInfoNamed), c.r[5], at,
                              c.rd32(p.info + kInfoBlock), false});
            c.ret(0);
        });
        p.m.cpu.hook(o.unmake, [](Sh4& c) { c.ret(0); });
    }
}

void otherHooks(Probe& p) {
    Sh4& cpu = p.m.cpu;
    cpu.hook(kReadObject, [](Sh4& c) { c.ret(0); });
    cpu.hook(kStoredObject, [](Sh4& c) { c.ret(0); });
    cpu.hook(kRead, [&p](Sh4& c) {
        c.wr8(c.r[5], (uint8_t)p.category);
        c.ret(1);
    });
    cpu.hook(kScreenMessage, [&p](Sh4& c) {
        p.screen.push_back(c.r[4]);
        c.ret(0);
    });
    cpu.hook(kModel24, [&p](Sh4& c) {
        ++p.model24;
        c.ret(0);
    });
}

uint8_t checksum(const Dt1& d) {
    uint32_t sum = 0;
    for (const uint8_t a : d.address) sum += a;
    for (const uint8_t v : d.data) sum += v;
    const uint32_t good = (128 - sum % 128) % 128;
    return (uint8_t)(d.badSum ? (good + 1) % 128 : good);
}

std::vector<uint8_t> bytesOf(const Dt1& d) {
    std::vector<uint8_t> b{0xf0, 0x41, d.device};
    b.insert(b.end(), d.model.begin(), d.model.end());
    b.push_back(d.command);
    b.insert(b.end(), std::begin(d.address), std::end(d.address));
    b.insert(b.end(), d.data.begin(), d.data.end());
    b.push_back(checksum(d));
    b.push_back(0xf7);
    return b;
}

} // namespace

// An out port with a ring of its own, as rows 14 and 15 make them, but a stand-in class: the base class (0x898c8168)
// the constructor gives it has pure methods, and its +0x30 (the transmitter's start, after every word) is one; here
// every method returns at once, and the words stay in the ring for drain()
bool makePort(Machine& m, uint32_t port) {
    const uint32_t ring = m.alloc(4 * kRingWords), cls = m.alloc(kPortSlots * 12 + 4);
    if (ring == 0 || cls == 0 || !m.run(kMakePort, {port, ring, kRingWords})) return false;
    const uint32_t method = cls + kPortSlots * 12;
    for (uint32_t k = 0; k < kPortSlots; ++k) m.cpu.wr32(cls + 12 * k + 8, method);
    m.cpu.hook(method, [](Sh4& c) { c.ret(0); });
    m.cpu.wr32(port + kPortClass, cls);
    return true;
}

// The words on a port's ring since the last read, its read place moved up to them
std::vector<uint8_t> takeRing(Machine& m, uint32_t port) {
    std::vector<uint8_t> out;
    const uint32_t start = m.cpu.rd32(port + kRingStart), end = m.cpu.rd32(port + kRingEnd);
    const uint32_t written = m.cpu.rd32(port + kRingWritten);
    uint32_t at = m.cpu.rd32(port + kRingRead);
    while (at != written) {
        at += 4;
        if (at >= end) at = start;
        for (uint32_t k = 0; k < 4; ++k) out.push_back(m.cpu.rd8(at + k));
    }
    m.cpu.wr32(port + kRingRead, written);
    return out;
}

bool Probe::start() {
    if (answerKernel(m) == 0 || !m.run(kMakeObject, {}) || !m.run(kMakeMap, {kMap})) return false;
    if (!makePort(m, kOutPort) || !makePort(m, kUsbOut)) return false;
    leafHooks(*this);
    fxHooks(*this);
    otherHooks(*this);
    return true;
}

std::vector<Set> Probe::send(const Dt1& d) {
    const std::vector<uint8_t> b = bytesOf(d);
    m.cpu.wr32(kScratch, (uint32_t)b.size());
    m.cpu.write(kScratch + 4, b.data(), b.size());
    data = kScratch + 4 + (uint32_t)(b.size() - d.data.size() - 2);
    sets.clear();
    if (!m.run(kParse, {kObject, kScratch})) failed = true;
    return sets;
}

std::vector<uint8_t> Probe::drain() {
    usb = takeRing(m, kUsbOut);
    return takeRing(m, kOutPort);
}

void printAddress(uint32_t a) {
    std::printf("%02x %02x %02x %02x", byteOf(a, 0), byteOf(a, 1), byteOf(a, 2), byteOf(a, 3));
}

const char* idName(Machine& m, uint32_t id) {
    static char name[64];
    uint32_t p = m.cpu.rd32(kNames + 4 * id);
    while (m.cpu.rd8(p) == ' ') ++p;
    size_t n = 0;
    for (uint8_t ch; n + 1 < sizeof name && (ch = m.cpu.rd8(p + (uint32_t)n)) != 0; ++n) name[n] = (char)ch;
    name[n] = 0;
    return name;
}

} // namespace vc2::sx
