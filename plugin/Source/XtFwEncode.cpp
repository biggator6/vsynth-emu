// The XT's encodes run in the interpreter (XtFirmware.h's xtFwEncodeMulti and xtFwEncodeSolo): moved out of
// XtFirmware.cpp in v481, which was past the kit's line limit, their shared set-ups each made one (the sample block, the
// sample memory's chunks, the vpdt taken), so that the XT's own event edits can go between the detection and the driver.
#include "XtFirmware.h"
#include "Sh4Interp.h"
#include "XtFwAddrs.h"
#include "XtFwEvents.h"
#include "XtFwMachine.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

using namespace xtfw;       // the machine the routines run on, and its memory (XtFwMachine.h)
using namespace xtfwaddr;   // rom.2's entry points (XtFwAddrs.h)

namespace {

// The markers as the attack detection leaves them (0x895164c0, read back after it): a list head and one marker an event,
// each {position, a later position, a level} in samples with +0xd set, doubly linked (next at +0xbc, previous at
// +0xc0).  The driver's 0x895126d0 takes the events' positions from the first word alone; the later position here is
// the event's own, as at the end.
void putMarkers(Sh4& c, uint32_t& heap, const std::vector<uint32_t>& at) {
    const uint32_t head = heap;
    heap += 0x100;
    for (uint32_t a = head; a < heap; a += 4) c.wr32(a, 0);
    uint32_t prev = head;
    for (uint32_t pos : at) {
        const uint32_t m = heap;
        heap += 0x100;
        for (uint32_t a = m; a < heap; a += 4) c.wr32(a, 0);
        c.wr32(m, pos);
        c.wr32(m + 4, pos);
        c.wr8(m + 0xd, 1);
        c.wr32(prev + 0xbc, m);
        c.wr32(m + 0xc0, prev);
        prev = m;
    }
    c.wr32(kMarkHead, head);
    c.wr32(kMarkCount, (uint32_t)at.size());
}

// A start and an end marker alone (type 1, type 2), doubly linked: one segment, a sustained note
void putEnds(Sh4& c, uint32_t& heap, long frames) {
    const auto marker = [&c](uint32_t at, uint32_t pos, uint8_t kind) {
        c.wr32(at, pos);
        c.wr32(at + 4, pos);
        c.wr8(at + 0xc, kind);
        c.wr8(at + 0xd, 1);
        c.wr8(at + 0xe, 1);
    };
    const uint32_t head = heap, m0 = heap + 0x100, m1 = heap + 0x200;
    heap += 0x300;
    for (uint32_t a = head; a < heap; a += 4) c.wr32(a, 0);
    marker(m0, 0, 1);
    marker(m1, (uint32_t)frames, 2);
    c.wr32(head + 0xbc, m0);   // next at +0xbc, previous at +0xc0
    c.wr32(m0 + 0xc0, head);
    c.wr32(m0 + 0xbc, m1);
    c.wr32(m1 + 0xc0, m0);
    c.wr32(kMarkHead, head);
    c.wr32(kMarkCount, 2);
}

// The markers given fit the wave: at least a start and an end, the first 0 and the last its frames
bool marksFit(const std::vector<uint32_t>& markers, long frames) {
    return markers.size() >= 2 && markers.front() == 0 && markers.back() == (uint32_t)frames;
}

// The sample being encoded, in the encode module's sample block: its type, channels and frames, from 0 to the end
void putSample(Sh4& c, int type, int channels, long frames) {
    c.wr8(kSmp, (uint8_t)type);
    c.wr8(kSmp + 2, (uint8_t)channels);
    c.wr32(kSmp + 8, (uint32_t)frames);
    c.wr32(kSmp + 0x20, 0);
    c.wr32(kSmp + 0x24, (uint32_t)frames);
    c.wr32(kSmp + 0x7c, kSamp);
}

// The OS's sample-memory chunks handed out of the machine's heap, never given back
void hookChunks(Sh4& c, Machine& M) {
    c.hook(kOsAlloc, [&M](Sh4& cpu) {
        const uint32_t sz = 0x400000, p = M.heap;
        if (p + sz > kRam + kRamSize) {
            cpu.ret(0);
            return;
        }
        if (uint8_t* m = cpu.ptr(p, sz)) std::memset(m, 0, sz);
        M.heap += sz;
        cpu.ret(p);
    });
    c.hook(kOsFree, [](Sh4& cpu) { cpu.ret(0); });
}

// The vpdt is complete when the driver moves on to the note chunk (0x89515b40, after 0x89515a70 succeeded): taken there
// and the run stopped; `events`, if given, its event count
void takeVpdt(Sh4& c, std::vector<uint8_t>& vpdt, bool& done, int* events) {
    c.hook(kAfterVpdt, [&vpdt, &done, events](Sh4& cpu) {
        const uint32_t p = cpu.rd32(kChunks + 8 * 5), n = cpu.rd32(kChunks + 8 * 5 + 4);
        if ((int32_t)cpu.rd32(0x89ff0dd0) == 0 && p && n && n <= 0x400000) {
            vpdt.resize(n);
            cpu.read(p, vpdt.data(), n);
            if (events) *events = (int)(cpu.rd32(p + 8) >> 16);
            done = true;
        }
        cpu.ret(0);
    });
}

bool failed(const char* what, Sh4& c) {
    std::fprintf(stderr, "%s: %s\n", what, c.error().c_str());
    return false;
}

} // namespace

bool xtFwEncodeMulti(int type, const int16_t* pcm, long frames, int channels, std::vector<uint8_t>& vpdt,
                     int depth, int* events, const std::vector<uint32_t>* markers) {
    vpdt.clear();
    auto M = waveMachine(xtFwRomPath(), pcm, frames, channels, 0x800000);
    if (!M || (type != 3 && type != 5)) return false;
    Sh4& c = M->cpu;
    if (markers && !marksFit(*markers, frames)) return false;
    if ((depth >= 0 || markers) && !c.call(kEncInit, {}, kStack)) return failed("sh4 init", c);
    putSample(c, depth >= 0 && !markers ? 0 : type, channels, frames);
    hookChunks(c, *M);
    if (markers) putMarkers(c, M->heap, *markers);   // the events given, in place of the detection
    else if (depth >= 0) {   // the XT's attack detection at this Encode Depth builds the markers
        uint32_t rc = 0;
        if (!c.call(kDetect, { (uint32_t)std::min(depth, 127) }, kStack, &rc)) return failed("sh4 detect", c);
        // the Encode screen's own adds and deletes, where a scope gives them, each counted again (v481)
        if (!xtFwEditEvents(c, frames, depth)) return failed("sh4 events", c);
        c.wr8(kSmp, (uint8_t)type);
    } else {
        putEnds(c, M->heap, frames);
    }
    bool done = false;
    takeVpdt(c, vpdt, done, events);
    uint32_t rc = 0;
    if (!c.call(kEncMulti, { 0 }, kStack, &rc)) return failed("sh4", c);
    return done;
}

bool xtFwEncodeSolo(const int16_t* pcm, long frames, int channels, int depth, std::vector<uint8_t>& vpdt,
                    std::vector<uint32_t>* events, bool onlyWithAttacks, const std::vector<uint32_t>* markers) {
    vpdt.clear();
    if (events) events->clear();
    auto M = waveMachine(xtFwRomPath(), pcm, frames, channels, 0x800000);
    if (!M) return false;
    Sh4& c = M->cpu;
    if (!c.call(kEncInit, {}, kStack)) return failed("sh4 init", c);
    putSample(c, 0, channels, frames);
    hookChunks(c, *M);
    uint32_t rc = 0;
    if (markers) {                      // the events given, in place of the detection
        if (!marksFit(*markers, frames)) return false;
        putMarkers(c, M->heap, *markers);
    } else if (!c.call(kDetect, { (uint32_t)std::clamp(depth, 0, 127) }, kStack, &rc)) {
        return failed("sh4 detect", c);
    } else if (!xtFwEditEvents(c, frames, depth)) {   // the screen's edits on SOLO's detection, as a band wave's (v481)
        return failed("sh4 events", c);
    }
    c.wr8(kSmp, 1);
    // The event list the encoder gets: the start, the attacks, the end.  With none between, the port's encoder gives the
    // same records ({n·16}: the driver's {0, n·16} changes none), so the caller may stop here
    bool attacks = true;
    c.hook(kSoloNew, [&](Sh4& cpu) {
        if (events) for (uint32_t k = 0; k < cpu.r[6] && k < 4096; ++k) events->push_back(cpu.rd32(cpu.r[7] + 4 * k));
        if (cpu.r[6] <= 2) attacks = false;
        // stop the whole run there (an unmapped address: the call ends as a fault)
        if (!attacks && onlyWithAttacks) cpu.pc = 0x10;
    });
    bool done = false;
    takeVpdt(c, vpdt, done, nullptr);
    if (!c.call(kEncSolo, {}, kStack, &rc)) {
        if (!attacks && onlyWithAttacks) return false;
        return failed("sh4", c);
    }
    return done;
}
