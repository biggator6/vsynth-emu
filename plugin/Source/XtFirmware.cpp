// XT firmware routines run in the SH-4 interpreter — see XtFirmware.h.
// The setup mirrors research/ghidra_scripts/EmuEnc.java and EmuLoad.java.
#include "XtFirmware.h"
#include "Sh4Interp.h"
#include "XtFloat.h"
#include "XtFwMachine.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>

using namespace xtfw;   // the machine the routines run on, and its memory (XtFwMachine.h)

namespace {

// rom.2 entry points (see research/HARDWARE_FIRMWARE.md)
constexpr uint32_t kSoloNew = 0x8951dc40, kSoloEncode = 0x8951e120;
constexpr uint32_t kLoadInit = 0x89529e90, kLoad = 0x894a9940;
constexpr uint32_t kEncMulti = 0x89516000;                       // BACKING/ENSEMBLE encode driver
constexpr uint32_t kEncSolo = 0x89515e28;                        // SOLO encode driver
constexpr uint32_t kOsAlloc = 0x895113a0, kOsFree = 0x895113e0;  // sample-memory chunks (OS pointers 0x8987c8f0/8f8)
constexpr uint32_t kAfterVpdt = 0x89515b40;                       // the note chunk, after the vpdt is written
constexpr uint32_t kEncInit = 0x89516400, kDetect = 0x895164c0;   // encode module init; attack detection (depth)
constexpr uint32_t kSmp = 0x8987c940;    // sample block: +0 type, +2 channels, +8 frames, +0x20 start, +0x24 end, +0x7c samples
constexpr uint32_t kChunks = 0x8987c97c; // (ptr, size) per chunk kind 0..8, set by 0x89517320
constexpr uint32_t kMarkCount = 0x89ff0cfc, kMarkHead = 0x89ff0d04;
constexpr uint32_t kSrcSetup = 0x8952dec0, kSrcFeed16 = 0x8952e4d0;  // WAV import converter (16-bit input)

std::string g_rom;

// A machine on the image, each call it makes allowed its steps for `samples` of a wave (XtFwMachine.h)
std::unique_ptr<Machine> machine(size_t samples) { return std::make_unique<Machine>(g_rom, samples); }

} // namespace

bool xtFwOpen(const char* romPath) {
    if (!romPath) return false;
    FILE* f = std::fopen(romPath, "rb");
    if (!f) return false;
    std::fclose(f);
    g_rom = romPath;
    return true;
}
bool xtFwOpened() { return !g_rom.empty(); }
const std::string& xtFwRomPath() { return g_rom; }

// ── V-Synth's files ──────────────────────────────────────────────────────────

bool xtFwRdacDecode(const uint8_t* src, size_t bytes, long frames, int channels, std::vector<int16_t>& out) {
    out.clear();
    if (g_rom.empty() || frames < 0 || (channels != 1 && channels != 2)) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    const size_t n = (size_t)frames * (size_t)channels;     // a count of samples, and of the bytes that count: past it they read as 0
    out.assign(n, 0);
    constexpr uint32_t kBlock = kRam, kOut = kRam + 0x100;
    uint32_t prev[2] = { 0, 0 };
    size_t at = 0;
    for (size_t f = 0; f < (size_t)frames; f += 16)
        for (int k = 0; k < channels; ++k, at += 16) {
            for (uint32_t i = 0; i < 16; ++i) c.wr8(kBlock + i, at + i < n && at + i < bytes ? src[at + i] : 0);
            uint32_t r0 = 0;
            if (!c.call(0x89529570, { kBlock, kOut, prev[k], 1 }, kStack, &r0)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
            prev[k] = (uint32_t)(int32_t)(int16_t)r0;
            for (uint32_t i = 0; i < 16 && f + i < (size_t)frames; ++i) out[(f + i) * (size_t)channels + (size_t)k] = (int16_t)c.rd16(kOut + 2 * i);
        }
    return true;
}

bool xtFwRdacWhole(const uint8_t* src, size_t bytes, long frames, int channels, std::vector<int16_t>& out) {
    out.clear();
    if (g_rom.empty() || frames < 0 || (channels != 1 && channels != 2)) return false;
    auto M = machine((size_t)frames * (size_t)channels);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    const size_t n = (size_t)frames * (size_t)channels;
    const uint32_t dst = kSamp + (uint32_t)((bytes + 0x1000) & ~0xfffu);
    if (dst + n * 2 + 0x1000 > kRam + kRamSize) return false;
    c.write(kSamp, src, bytes);
    if (channels == 1) {
        if (!c.call(0x89529c20, { kSamp, (uint32_t)n, dst, 0 }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    } else {   // on the stack as 0x8952eeb0 leaves it: where to put the last samples, then the first predictions
        const uint32_t sp = kStack - 8, prevs = kRam + 0x200;
        c.wr32(sp, prevs); c.wr32(sp + 4, 0);
        if (!c.call(0x89529ce0, { kSamp, (uint32_t)n, dst, 0 }, sp)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    }
    out.resize(n);
    c.read(dst, out.data(), n * 2);
    return true;
}

bool xtFwPatchStore(const uint8_t* data, size_t bytes, std::vector<uint8_t>& store) {
    store.clear();
    if (g_rom.empty() || bytes > 0x14fa) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    constexpr uint32_t kFile = kSamp, kStore = kSamp + 0x2000;
    if (!c.call(0x8947e230, { 1 }, kStack) || !c.call(0x8947dad0, { kFile }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    c.write(kFile, data, bytes);
    if (!c.call(0x8947dae0, { kStore, kFile }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    store.resize(0x14fa);
    c.read(kStore, store.data(), store.size());
    return true;
}

struct XtFwPatchReader::Impl { Machine m { g_rom }; };
XtFwPatchReader::XtFwPatchReader() : impl(std::make_unique<Impl>()) {}
XtFwPatchReader::~XtFwPatchReader() = default;
void XtFwPatchReader::load(const std::vector<uint8_t>& store) { impl->m.cpu.write(kSamp, store.data(), store.size()); }
bool XtFwPatchReader::read(int at, int id, int& raw, int& value) {
    Sh4& c = impl->m.cpu;
    uint32_t r0 = 0;
    if (!c.call(0x894817b0, { kSamp + (uint32_t)at, 0x897a167cu + 12u * (uint32_t)id }, kStack, &r0)) return false;
    raw = (int)r0;
    if (!c.call(0x89484310, { kSamp + (uint32_t)at, (uint32_t)id }, kStack, &r0)) return false;
    value = (int)r0;
    return true;
}
bool XtFwPatchReader::readTyped(int at, int base, int id, int& value) {
    uint32_t r0 = 0;
    if (!impl->m.cpu.call(0x89481d70, { kSamp + (uint32_t)at, 0x897a167cu + 12u * (uint32_t)base, 0x897a56d8u + 8u * (uint32_t)(id - 0x55d) }, kStack, &r0)) return false;
    value = (int)r0;
    return true;
}
// The reads are 0x8950e980's and 0x8950eb60's (mono, stereo; each twice): the bytes at r7,
// from the band at r5 (for the kernel's, a few bytes before it), r7 loaded again before each.
void xtFwHookBandReads(Sh4& c, XtFwBandReads& reads) {
    static constexpr struct { uint32_t pc; int32_t bytes; } kReads[] = {
        { 0x8950e99a, 4 }, { 0x8950e9cc, 4 }, { 0x8950ea00, 8 }, { 0x8950ea50, 8 },
        { 0x8950eb96, 12 }, { 0x8950ec08, 12 }, { 0x8950ec7e, 24 }, { 0x8950ed4c, 24 } };
    for (const auto& s : kReads)
        c.hook(s.pc, [&reads, bytes = s.bytes](Sh4& m) {
            auto b = reads.bandEnd.upper_bound(m.r[5] + 16);
            if (b == reads.bandEnd.begin()) return;
            --b;
            const int32_t off = (int32_t)(m.r[7] - b->first), pad = (int32_t)kXtFwWavePad;
            if (off >= -pad && off + bytes <= (int32_t)(b->second - b->first) + pad) return;
            m.r[7] = b->first - (uint32_t)pad;
            ++reads.far;
        });
}

void xtFwForceNearest(bool on) { roundNearest(on); }

bool xtFwSoloEncode(const int16_t* x, int n, std::vector<XtRecord>& out, int channels, std::vector<uint64_t>* trace, const std::vector<uint32_t>* events) {
    out.clear();
    auto M = waveMachine(g_rom, x, n, channels, 0);
    if (!M) return false;
    Sh4& c = M->cpu;
    // the event list: {n·16}, one sustained note, or the attack events' (0x895126d0's, as the SOLO driver gives them)
    const std::vector<uint32_t> one { (uint32_t)n * 16 };
    const std::vector<uint32_t>& ev = events && !events->empty() ? *events : one;
    if (ev.size() > 60) return false;
    for (size_t k = 0; k < ev.size(); ++k) c.wr32(kEv + 4 * (uint32_t)k, ev[k]);
    if (!c.call(kSoloNew, { (uint32_t)n, kSamp, (uint32_t)ev.size(), kEv }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    if (trace) {   // at the common exit of 0x8951f790, 0x8951f880, 0x8951fab0, 0x89520d30: as xtSoloEncode's
        trace->clear();
        auto add = [trace](Sh4& m, int kind, uint32_t at, uint32_t bytes) { trace->push_back(xtEncHash(kind, m.ptr(at, bytes), bytes)); };
        c.hook(0x8951f860, [add](Sh4& m) { add(m, 0, 0x8a0090f4, 0xc000); });
        c.hook(0x8951f8de, [add](Sh4& m) { add(m, 0, 0x8a0090f4, 0xc000); });
        c.hook(0x8951fb48, [add](Sh4& m) { add(m, 1, 0x8a0090f4, 0xc000); });
        c.hook(0x89520f16, [add](Sh4& m) { add(m, 2, 0x89ff10c8, 0x18000); });
    }
    if (!c.call(kSoloEncode, { channels == 2 ? 3u : 2u }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    // The record list: 16-byte nodes {u8 low, u8, u16 v, i32 pos, u32 lower, u32 higher}, from the head at 0x89ff10c4.
    struct Node { uint32_t low, v; int32_t pos; };
    std::vector<Node> list;
    uint32_t count = c.rd32(0x89ff10c0);                     // the firmware's own node count
    for (uint32_t p = c.rd32(0x89ff10c4); p && count; p = c.rd32(p + 12), --count)
        list.push_back({ c.rd8(p), c.rd16(p + 2), (int32_t)c.rd32(p + 4) });
    for (size_t k = 0; k + 1 < list.size(); ++k)
        out.push_back({ list[k].pos, list[k + 1].pos - list[k].pos, (int)(((list[k + 1].v >> 8) & 0xf8) | list[k + 1].low) });
    return true;
}

namespace {
// The markers as the attack detection leaves them (0x895164c0, read back after it): a list head and one marker an event, each
// {position, a later position, a level} in samples with +0xd set, doubly linked (next at +0xbc, previous at +0xc0).  The driver's
// 0x895126d0 takes the events' positions from the first word alone; the later position here is the event's own, as at the end.
void putMarkers(Sh4& c, uint32_t& heap, const std::vector<uint32_t>& at) {
    const uint32_t head = heap; heap += 0x100;
    for (uint32_t a = head; a < heap; a += 4) c.wr32(a, 0);
    uint32_t prev = head;
    for (uint32_t pos : at) {
        const uint32_t m = heap; heap += 0x100;
        for (uint32_t a = m; a < heap; a += 4) c.wr32(a, 0);
        c.wr32(m, pos); c.wr32(m + 4, pos); c.wr8(m + 0xd, 1);
        c.wr32(prev + 0xbc, m); c.wr32(m + 0xc0, prev);
        prev = m;
    }
    c.wr32(kMarkHead, head);
    c.wr32(kMarkCount, (uint32_t)at.size());
}
}

bool xtFwEncodeMulti(int type, const int16_t* pcm, long frames, int channels, std::vector<uint8_t>& vpdt,
                     int depth, int* events, const std::vector<uint32_t>* markers) {
    vpdt.clear();
    auto M = waveMachine(g_rom, pcm, frames, channels, 0x800000);
    if (!M || (type != 3 && type != 5)) return false;
    Sh4& c = M->cpu;
    if (markers && (markers->size() < 2 || markers->front() != 0 || markers->back() != (uint32_t)frames)) return false;
    if ((depth >= 0 || markers) && !c.call(kEncInit, {}, kStack)) { std::fprintf(stderr, "sh4 init: %s\n", c.error().c_str()); return false; }
    // the sample being encoded
    c.wr8(kSmp, (uint8_t)(depth >= 0 && !markers ? 0 : type));
    c.wr8(kSmp + 2, (uint8_t)channels);
    c.wr32(kSmp + 8, (uint32_t)frames);
    c.wr32(kSmp + 0x20, 0);
    c.wr32(kSmp + 0x24, (uint32_t)frames);
    c.wr32(kSmp + 0x7c, kSamp);
    // OS sample-memory chunks: hand out heap blocks
    c.hook(kOsAlloc, [&M](Sh4& cpu) {
        const uint32_t sz = 0x400000, p = M->heap;
        if (p + sz > kRam + kRamSize) { cpu.ret(0); return; }
        if (uint8_t* m = cpu.ptr(p, sz)) std::memset(m, 0, sz);
        M->heap += sz;
        cpu.ret(p);
    });
    c.hook(kOsFree, [](Sh4& cpu) { cpu.ret(0); });
    if (markers) putMarkers(c, M->heap, *markers);   // the events given, in place of the detection
    else if (depth >= 0) {   // the XT's attack detection at this Encode Depth builds the markers
        uint32_t rc = 0;
        if (!c.call(kDetect, { (uint32_t)std::min(depth, 127) }, kStack, &rc)) { std::fprintf(stderr, "sh4 detect: %s\n", c.error().c_str()); return false; }
        c.wr8(kSmp, (uint8_t)type);
    } else {
    // markers: a list head and {start (type 1), end (type 2)}, doubly linked
    auto marker = [&](uint32_t at, uint32_t pos, uint8_t kind) {
        c.wr32(at, pos); c.wr32(at + 4, pos);
        c.wr8(at + 0xc, kind); c.wr8(at + 0xd, 1); c.wr8(at + 0xe, 1);
    };
    const uint32_t head = M->heap; M->heap += 0x100;
    const uint32_t m0 = M->heap; M->heap += 0x100;
    const uint32_t m1 = M->heap; M->heap += 0x100;
    for (uint32_t a = head; a < M->heap; a += 4) c.wr32(a, 0);
    marker(m0, 0, 1);
    marker(m1, (uint32_t)frames, 2);
    c.wr32(head + 0xbc, m0); c.wr32(m0 + 0xc0, head);    // next at +0xbc, previous at +0xc0
    c.wr32(m0 + 0xbc, m1);   c.wr32(m1 + 0xc0, m0);
    c.wr32(kMarkHead, head);
    c.wr32(kMarkCount, 2);
    }
    // The vpdt is complete when the driver moves on to the note chunk
    // (0x89515b40, after 0x89515a70 succeeded): take it there and stop.
    bool done = false;
    c.hook(kAfterVpdt, [&](Sh4& cpu) {
        const uint32_t p = cpu.rd32(kChunks + 8 * 5), n = cpu.rd32(kChunks + 8 * 5 + 4);
        if ((int32_t)cpu.rd32(0x89ff0dd0) == 0 && p && n && n <= 0x400000) {
            vpdt.resize(n);
            cpu.read(p, vpdt.data(), n);
            if (events) *events = (int)(cpu.rd32(p + 8) >> 16);
            done = true;
        }
        cpu.ret(0);
    });
    uint32_t rc = 0;
    if (!c.call(kEncMulti, { 0 }, kStack, &rc)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    return done;
}

bool xtFwEncodeSolo(const int16_t* pcm, long frames, int channels, int depth, std::vector<uint8_t>& vpdt, std::vector<uint32_t>* events, bool onlyWithAttacks,
                    const std::vector<uint32_t>* markers) {
    vpdt.clear();
    if (events) events->clear();
    auto M = waveMachine(g_rom, pcm, frames, channels, 0x800000);
    if (!M) return false;
    Sh4& c = M->cpu;
    if (!c.call(kEncInit, {}, kStack)) { std::fprintf(stderr, "sh4 init: %s\n", c.error().c_str()); return false; }
    c.wr8(kSmp, 0);
    c.wr8(kSmp + 2, (uint8_t)channels);
    c.wr32(kSmp + 8, (uint32_t)frames);
    c.wr32(kSmp + 0x20, 0);
    c.wr32(kSmp + 0x24, (uint32_t)frames);
    c.wr32(kSmp + 0x7c, kSamp);
    c.hook(kOsAlloc, [&M](Sh4& cpu) {
        const uint32_t sz = 0x400000, p = M->heap;
        if (p + sz > kRam + kRamSize) { cpu.ret(0); return; }
        if (uint8_t* m = cpu.ptr(p, sz)) std::memset(m, 0, sz);
        M->heap += sz;
        cpu.ret(p);
    });
    c.hook(kOsFree, [](Sh4& cpu) { cpu.ret(0); });
    uint32_t rc = 0;
    if (markers) {                      // the events given, in place of the detection
        if (markers->size() < 2 || markers->front() != 0 || markers->back() != (uint32_t)frames) return false;
        putMarkers(c, M->heap, *markers);
    } else if (!c.call(kDetect, { (uint32_t)std::clamp(depth, 0, 127) }, kStack, &rc)) { std::fprintf(stderr, "sh4 detect: %s\n", c.error().c_str()); return false; }
    c.wr8(kSmp, 1);
    // The event list the encoder gets: the start, the attacks, the end.  With none between, the port's encoder gives the same
    // records ({n·16}: the driver's {0, n·16} changes none), so the caller may stop here
    bool attacks = true;
    c.hook(kSoloNew, [&](Sh4& cpu) {
        if (events) for (uint32_t k = 0; k < cpu.r[6] && k < 4096; ++k) events->push_back(cpu.rd32(cpu.r[7] + 4 * k));
        if (cpu.r[6] <= 2) attacks = false;
        if (!attacks && onlyWithAttacks) cpu.pc = 0x10;   // stop the whole run there (an unmapped address: the call ends as a fault)
    });
    bool done = false;
    c.hook(kAfterVpdt, [&](Sh4& cpu) {
        const uint32_t p = cpu.rd32(kChunks + 8 * 5), n = cpu.rd32(kChunks + 8 * 5 + 4);
        if ((int32_t)cpu.rd32(0x89ff0dd0) == 0 && p && n && n <= 0x400000) {
            vpdt.resize(n);
            cpu.read(p, vpdt.data(), n);
            done = true;
        }
        cpu.ret(0);
    });
    if (!c.call(kEncSolo, {}, kStack, &rc)) {
        if (!attacks && onlyWithAttacks) return false;
        std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false;
    }
    return done;
}

bool xtFwImport(const int16_t* pcm, long frames, int channels, int rate, std::vector<int16_t>& out) {
    out.clear();
    if (g_rom.empty() || !pcm || frames <= 0 || (channels != 1 && channels != 2) || rate <= 0) return false;
    const size_t bytes = (size_t)frames * channels * 2;
    // input below the heap; the output, at most rate/44100 shorter or longer, above it
    const double outMax = ((double)frames * 44100.0 / rate + 64.0) * channels * 2;
    if (kSamp + bytes > kHeap || (double)kHeap + outMax > (double)kRam + kRamSize) return false;
    auto M = machine(bytes / 2 + ((size_t)frames * 44100 / (size_t)rate + 64) * (size_t)channels);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    c.write(kSamp, pcm, bytes);
    // parameter block {output, channels, bits, rate}; the converter's context follows it
    const uint32_t prm = kInfo, ctx = kInfo + 0x40;
    const uint32_t p[4] = { kHeap, (uint32_t)channels, 16, (uint32_t)rate };
    for (int i = 0; i < 4; ++i) c.wr32(prm + 4 * i, p[i]);
    if (!c.call(kSrcSetup, { ctx, prm }, kStack)) { std::fprintf(stderr, "sh4 src setup: %s\n", c.error().c_str()); return false; }
    if (!c.call(kSrcFeed16, { ctx, (uint32_t)bytes, kSamp }, kStack)) { std::fprintf(stderr, "sh4 src: %s\n", c.error().c_str()); return false; }
    if (!c.call(kSrcFeed16, { ctx, 0, 0 }, kStack)) { std::fprintf(stderr, "sh4 src flush: %s\n", c.error().c_str()); return false; }
    const uint32_t end = c.rd32(ctx + 8);
    if (end < kHeap || end > kRam + kRamSize) return false;
    out.resize((end - kHeap) / 2);
    c.read(kHeap, out.data(), out.size() * 2);
    return true;
}

bool xtFwEnvKeyScales(std::vector<uint32_t>& out) {
    out.clear();
    if (g_rom.empty()) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    out.resize(41 * 128);
    for (int i = 0; i < 41; ++i)
        for (int key = 0; key < 128; ++key) {
            uint32_t r0 = 0;
            if (!M->cpu.call(0x894df760, { (uint32_t)key, (uint32_t)(-200 + 10 * i) }, kStack, &r0)) { out.clear(); return false; }
            out[(size_t)(i * 128 + key)] = r0;
        }
    return true;
}

void xtFwSoloImage(const std::vector<XtRecord>& recs, long frames, int channels, const XtLoop& loop,
                   std::vector<uint8_t>& vpdt, uint8_t events[4][kXtFwSoloEventBytes]) {
    const uint32_t endPos = (uint32_t)frames * 16;
    // The vpdt: events at (stride·e + 8)·2, each {u32 flags, u32 position,
    // u32 segment-list offset, u32 record offset, u16 first mark}; segment
    // entries {u32 record offset, u32 flags, u32 start}; the record count,
    // then {u16 length, u16 info} with 0xFFFF continuing a length.
    const int st = channels == 2 ? 16 : 10;
    const uint32_t ev0 = 16, ev1 = (uint32_t)(st + 8) * 2, seg0 = (uint32_t)(2 * st + 8) * 2, seg1 = seg0 + 12;
    const uint32_t cnt = seg1 + 12, rec0 = cnt + 4;
    std::vector<uint16_t> stream;
    std::vector<uint32_t> recOff;                 // each record's first stream entry
    for (const XtRecord& r : recs) {
        recOff.push_back(rec0 + (uint32_t)stream.size() * 2);
        int len = r.len16;
        while (len >= 0xffff) { stream.push_back(0xffff); stream.push_back((uint16_t)r.info); len -= 0xffff; }
        stream.push_back((uint16_t)len); stream.push_back((uint16_t)r.info);
    }
    const uint32_t term = rec0 + (uint32_t)stream.size() * 2;
    stream.push_back(256); stream.push_back(1);
    vpdt.assign(term + 4, 0);
    std::vector<uint8_t>& v = vpdt;
    auto w32 = [&](uint32_t o, uint32_t x) { std::memcpy(&v[o], &x, 4); };
    auto w16 = [&](uint32_t o, uint16_t x) { std::memcpy(&v[o], &x, 2); };
    w32(8, 2u << 16);
    w32(ev0, 0x0d); w32(ev0 + 4, 0); w32(ev0 + 8, seg0); w32(ev0 + 12, rec0); w16(ev0 + 16, stream[0]);
    w32(ev1, 0x0e); w32(ev1 + 4, endPos); w32(ev1 + 8, seg1); w32(ev1 + 12, term); w16(ev1 + 16, 256);
    w32(seg0, rec0); w32(seg0 + 4, 1); w32(seg0 + 8, 0);
    w32(seg1, term); w32(seg1 + 4, 1); w32(seg1 + 8, endPos);
    w32(cnt, (uint32_t)(stream.size() / 2));
    std::memcpy(&v[rec0], stream.data(), stream.size() * 2);
    // the cached loop events at W2 + 0xb4·k (forward start, forward end,
    // backward end, backward start): {u16 event, u16 mode, u32 position,
    // u32 segment entry, u32 record, u16 mark}; the event jump puts the
    // record's start at position + mark − its first length
    const int pts[4] = { loop.fwdStart, loop.fwdEnd, loop.bwdEnd, loop.bwdStart };
    const int modes[4] = { 0, loop.fwdMode, 0, loop.bwdMode };
    for (int k = 0; k < 4; ++k) {
        const int pos = pts[k] * 16;
        uint32_t off = term;
        int start = (int)endPos, first = 256;
        for (size_t i = 0, s0 = 0; i < recs.size(); s0 += (size_t)recs[i].len16, ++i)
            if ((int)s0 + recs[i].len16 > pos) { off = recOff[i]; start = (int)s0; first = std::min(recs[i].len16, 0xffff); break; }
        uint8_t* R = events[k];
        const uint16_t ev = 0, mode = (uint16_t)modes[k], mark = (uint16_t)(start + first - pos);
        const uint32_t p32 = (uint32_t)pos;
        std::memcpy(R, &ev, 2); std::memcpy(R + 2, &mode, 2); std::memcpy(R + 4, &p32, 4);
        std::memcpy(R + 8, &seg0, 4); std::memcpy(R + 12, &off, 4); std::memcpy(R + 16, &mark, 2);
    }
}

bool xtFwSoloPlay(const std::vector<XtRecord>& recs, const int16_t* pcm, long frames, int channels,
                  const XtFwSoloNote& note, long outFrames, std::vector<float>& out) {
    out.clear();
    if (g_rom.empty() || recs.empty() || !pcm || frames <= 0 || (channels != 1 && channels != 2)) return false;
    auto M = machine((size_t)frames * (size_t)channels);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    constexpr uint32_t R8 = kRam, R9 = R8 + 0xe8, R10 = R9 + 0x3c, W2 = kRam + 0x1000, VP = kRam + 0x10000, OUT = 0x8D800000;
    const uint32_t endPos = (uint32_t)frames * 16;
    const int st = channels == 2 ? 16 : 10;
    std::vector<uint8_t> v;
    uint8_t ev[4][kXtFwSoloEventBytes];
    xtFwSoloImage(recs, frames, channels, note.loop, v, ev);
    if (VP + v.size() + 0x100 > kStack - 0x1000 || kSamp + (uint32_t)(frames * channels * 2) + 0x10000 > OUT) return false;
    c.write(VP, v.data(), v.size());
    c.write(kSamp, pcm, (size_t)(frames * channels * 2));
    auto wf = [&](uint32_t a, float x) { uint32_t u; std::memcpy(&u, &x, 4); c.wr32(a, u); };
    auto rf = [&](uint32_t a) { const uint32_t u = c.rd32(a); float x; std::memcpy(&x, &u, 4); return x; };
    // the player (0x8950fba0 / 0x8950fa60 as EmuSolo.java sets them)
    if (!note.loop.on) {
        c.wr32(W2 + 0x16c, 0x7fffffff); c.wr32(W2 + 0xb8, 0x7fffffff);   // no loop points or cache bounds
    } else {
        for (uint32_t k = 0; k < 4; ++k) c.write(W2 + 0xb4 * k, ev[k], kXtFwSoloEventBytes);
    }
    c.wr8(R8 + 0, (uint8_t)(0x80 | (channels == 2 ? 0x40 : 0) | (note.robot ? 0x10 : 0) | (note.loop.on ? 1 : 0)));
    c.wr8(R8 + 3, 0x80); c.wr8(R8 + 7, 8); c.wr8(R8 + 8, (uint8_t)st); c.wr8(R8 + 9, channels == 2 ? 12 : 6);
    c.wr32(R8 + 0x10, VP);
    c.wr32(R8 + 0x18, 0); c.wr32(R8 + 0x1c, endPos); c.wr32(R8 + 0x20, endPos);
    c.wr32(R8 + 0x48, 44100); c.wr32(R8 + 0x4c, 44100);
    c.wr32(R8 + 0x50, (uint32_t)(int)(5.644799e11 / 44100)); c.wr32(R8 + 0x54, 2u * (uint32_t)(int)(5.644799e11 / 44100));
    c.wr32(R8 + 0x6c, W2);
    wf(R8 + 0x70, note.pitch); wf(R8 + 0x98, note.pitch); wf(R8 + 0x74, note.formant); wf(R8 + 0x78, note.energy);
    wf(R8 + 0x7c, note.walk); wf(R8 + 0x8c, 1.0f); wf(R8 + 0x90, 1.0f); wf(R8 + 0x9c, 1.0f);
    wf(R8 + 0x80, 1.0f / 65536.0f); wf(R8 + 0x84, 1.0f / 65536.0f); wf(R8 + 0x88, 1.0f / 64.0f);
    wf(R8 + 0xa0, 1.0f); wf(R8 + 0xa4, 1.0f);                         // the loop regions' rate factors
    c.wr32(R9 + 0, kSamp);
    if (!c.call(0x8950fa60, { R8, 1 }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    // (the event jump's 0x8950c7e0 set R10+0: 256, or 0 for Robot Voice)
    if (!c.call(0x8950d580, { R8, R9 }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    // 0x8950fba0's tail: timer and ages (anchor − record start)·256, both
    // grains at the record start, B's window at 0.5, q 1.2, no cutoff
    const uint32_t age = (c.rd32(R9 + 0x28) - c.rd32(R9 + 0x20)) * 256u;
    c.wr32(R10 + 0xc, age); c.wr32(R10 + 0x10, age); c.wr32(R10 + 0x14, age);
    c.wr32(R10 + 0x18, c.rd32(R9 + 0x20)); c.wr32(R10 + 0x1c, c.rd32(R9 + 0x20));
    c.wr32(R10 + 0x20, (uint32_t)frames - 1);
    wf(R10 + 0x44, 0.0f); wf(R10 + 0x48, 0.5f);
    c.wr32(R10 + 0x2c, c.rd32(R10 + 0x28)); c.wr32(R10 + 0x30, c.rd32(R10 + 0x28));
    c.wr32(R10 + 0x38, c.rd32(R10 + 0x34)); c.wr32(R10 + 0x3c, c.rd32(R10 + 0x34));
    c.wr32(R9 + 0x34, c.rd32(R9 + 0x2c)); c.wr32(R9 + 0x38, c.rd32(R9 + 0x2c));
    wf(R10 + 0x24, 0.0f); wf(R10 + 0x4c, 0.0f); wf(R10 + 0x50, 1.2f);
    out.assign((size_t)outFrames * 2, 0.0f);
    size_t next = 0, nextG = 0;
    for (long done = 0; done < outFrames; done += 32) {
        while (next < note.walkAt.size() && note.walkAt[next].first <= done) wf(R8 + 0x7c, note.walkAt[next++].second);
        while (nextG < note.gainAt.size() && note.gainAt[nextG].first <= done) wf(R8 + 0x80, note.gainAt[nextG++].second / 65536.0f);
        if (!c.call(0x8950cd90, { OUT, R8 }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
        for (long i = 0; i < 32 && done + i < outFrames; ++i) {
            out[(size_t)(2 * (done + i))] = rf(OUT + (uint32_t)(8 * i)) / 32768.0f;
            out[(size_t)(2 * (done + i) + 1)] = rf(OUT + (uint32_t)(8 * i + 4)) / 32768.0f;
        }
    }
    return true;
}

bool xtFwBandPlay(int type, const int16_t* pcm, long frames, int channels, const std::vector<uint8_t>& vpdt,
                  const XtFwSoloNote& note, long outFrames, std::vector<float>& out) {
    out.clear();
    if (g_rom.empty() || !pcm || frames <= 0 || (channels != 1 && channels != 2) || (type != 3 && type != 5) || vpdt.size() < 64) return false;
    auto M = machine((size_t)frames * (size_t)channels);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    constexpr uint32_t P = kRam, W = kRam + 0x2000, TUNE = kRam + 0x2100, W2 = kRam + 0x3000, VP = kRam + 0x10000;
    const size_t bytes = (size_t)frames * (size_t)channels * 2;
    if (kSamp + bytes + 0x400000 > kHeap || VP + vpdt.size() + 0x1000 > kStack - 0x10000) return false;
    // the load-time band rebuild, in place (as xtFwLoadBands)
    c.write(kSamp, pcm, bytes);
    c.wr32(kL + 4, (uint32_t)frames);
    const uint32_t info[7] = { (uint32_t)type, kSamp, (uint32_t)frames, (uint32_t)channels, 0, 0, 0 };
    for (uint32_t i = 0; i < 7; ++i) c.wr32(kInfo + 4 * i, info[i]);
    if (!c.call(kLoadInit, { kL + 0x34, kInfo }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    if (!c.call(kLoad, { kL }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    const uint32_t OUT = (M->heap + 0xfff) & ~0xfffu;
    if (OUT + 0x1000 > kRam + kRamSize) return false;
    c.write(VP, vpdt.data(), vpdt.size());
    // the wave struct: vpdt, band 0 (the audio) and bands 1..5, tuning, loop/cache struct
    c.wr32(W, VP); c.wr32(W + 4, kSamp);
    const uint32_t use[5] = { 2, 3, 4, 5, 6 };
    for (uint32_t k = 0; k < 5; ++k) c.wr32(W + 8 + 4 * k, c.rd32(kL + 0x10 + 4 * use[k]));
    c.wr32(W + 0x28, TUNE); c.wr32(W + 0x2c, W2);
    bool loopOn = false;
    if (note.loop.on) {   // the cached loop events at W2 + 0xb4·k, as the port builds them
        XtMbWave lw;
        lw.vpdt = vpdt.data(); lw.vpdtBytes = vpdt.size(); lw.channels = channels;
        uint8_t ev[4][kXtMbLoopEventBytes];
        if (xtMbLoopEvents(lw, note.loop, ev)) {
            for (uint32_t k = 0; k < 4; ++k) c.write(W2 + (uint32_t)kXtMbLoopEventBytes * k, ev[k], kXtMbLoopEventBytes);
            loopOn = true;
        }
    }
    if (!loopOn) { c.wr32(W2 + 0x16c, 0x7fffffff); c.wr32(W2 + 0xb8, 0x7fffffff); }   // no loop points or cache bounds
    auto wf = [&](uint32_t a, float x) { uint32_t u; std::memcpy(&u, &x, 4); c.wr32(a, u); };
    auto rf = [&](uint32_t a) { const uint32_t u = c.rd32(a); float x; std::memcpy(&x, &u, 4); return x; };
    // bit 7: start by position (the wave's start, or its end for a walk starting
    // backward), as an OSC's note-on starts it
    c.wr8(P, (uint8_t)(0x80 | (channels == 2 ? 0x40 : 0) | (loopOn ? 1 : 0)));
    wf(P + 0xa0, 1.0f); wf(P + 0xa4, 1.0f);                           // the loop regions' rate factors
    c.wr16(P + 0x14, 0); c.wr16(P + 0x16, 0xffff);                     // start / end event
    {
        uint32_t ev;
        std::memcpy(&ev, vpdt.data() + 8, 4);
        const size_t last = (((size_t)(ev >> 16) - 1) * (channels == 2 ? 0x58 : 0x2e) + 0xe) * 2;
        uint32_t end = 0;
        if (last + 8 <= vpdt.size()) std::memcpy(&end, vpdt.data() + last + 4, 4);
        c.wr32(P + 0x18, 0); c.wr32(P + 0x1c, end); c.wr32(P + 0x20, end / 16 * 16);
    }
    c.wr32(P + 0x28, 0x7fffffff); c.wr32(P + 0x2c, 0x7fffffff);
    c.wr32(P + 0x44, W);
    c.wr32(P + 0x48, 44100); c.wr32(P + 0x4c, 44100);
    wf(P + 0x70, note.pitch); wf(P + 0x98, note.pitch); wf(P + 0x74, 1.0f); wf(P + 0x78, 0.0f);
    wf(P + 0x7c, note.walk); wf(P + 0x9c, 1.0f);
    wf(P + 0x80, 1.0f / 65536.0f); wf(P + 0x84, 1.0f / 65536.0f); wf(P + 0x88, 1.0f / 64.0f);
    if (!c.call(type == 5 ? 0x89510130 : 0x89510980, { P, 0 }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    out.assign((size_t)outFrames * 2, 0.0f);
    size_t next = 0, nextG = 0;
    std::vector<uint8_t> zero(32 * 8, 0);
    for (long done = 0; done < outFrames; done += 32) {
        while (next < note.walkAt.size() && note.walkAt[next].first <= done) wf(P + 0x7c, note.walkAt[next++].second);
        while (nextG < note.gainAt.size() && note.gainAt[nextG].first <= done) wf(P + 0x80, note.gainAt[nextG++].second / 65536.0f);
        c.write(OUT, zero.data(), zero.size());
        if (!c.call(type == 5 ? 0x8950db10 : 0x8950e6e0, { OUT, P }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
        for (long i = 0; i < 32 && done + i < outFrames; ++i) {
            out[(size_t)(2 * (done + i))] = rf(OUT + (uint32_t)(8 * i)) / 32768.0f;
            out[(size_t)(2 * (done + i) + 1)] = rf(OUT + (uint32_t)(8 * i + 4)) / 32768.0f;
        }
    }
    return true;
}

bool xtFwOscTimeKeyScales(std::vector<float>& out) {
    out.clear();
    if (g_rom.empty()) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    out.resize(41 * 128);
    for (int i = 0; i < 41; ++i)
        for (int key = 0; key < 128; ++key) {
            // fr4 = 2, fr5 = (float)(kf·(key − 60)) / 1200
            const float two = 2.0f, e = xtf::div(xtf::flt((-200 + 10 * i) * (key - 60)), 1200.0f);
            std::memcpy(&c.fb[0][4], &two, 4);
            std::memcpy(&c.fb[0][5], &e, 4);
            if (!c.call(0x8952ff9c, {}, kStack)) { out.clear(); return false; }
            std::memcpy(&out[(size_t)(i * 128 + key)], &c.fb[0][0], 4);
        }
    return true;
}

// 2^(x/6), x from −top to top, by the firmware's powf
static bool xtFwPowSixths(int top, std::vector<float>& out) {
    out.clear();
    if (g_rom.empty()) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    out.resize((size_t)(2 * top + 1));
    for (int i = 0; i <= 2 * top; ++i) {
        const float two = 2.0f, e = xtf::div(xtf::flt(i - top), 6.0f);
        std::memcpy(&c.fb[0][4], &two, 4);
        std::memcpy(&c.fb[0][5], &e, 4);
        if (!c.call(0x8952ff9c, {}, kStack)) { out.clear(); return false; }
        std::memcpy(&out[(size_t)i], &c.fb[0][0], 4);
    }
    return true;
}

bool xtFwOscWaveGains(std::vector<float>& out) { return xtFwPowSixths(12, out); }
bool xtFwOscOutGains(std::vector<float>& out) { return xtFwPowSixths(48, out); }

bool xtFwMatrixTables(std::vector<uint8_t>& slots, std::vector<int16_t>& sens) {
    slots.clear(); sens.clear();
    if (g_rom.empty()) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    slots.resize(79);
    sens.resize(79 * 127);
    for (uint32_t d = 0; d < 79; ++d) {
        uint32_t r0 = 0;
        if (!c.call(0x894e7d10, { 1, d, 1, kL }, kStack, &r0)) return false;      // its masks in scratch
        slots[d] = (uint8_t)r0;
        for (int s = -63; s <= 63; ++s) {
            if (!c.call(0x894e82e0, { 1, d, (uint32_t)s }, kStack, &r0)) return false;
            sens[d * 127 + (uint32_t)(s + 63)] = (int16_t)r0;
        }
    }
    return true;
}

bool xtFwOscPitchRatios(std::vector<float>& out) {
    out.clear();
    if (g_rom.empty()) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    out.resize(127);
    for (int i = 0; i < 127; ++i) {
        const float knob = xtf::flt(i - 63);
        std::memcpy(&c.fb[0][4], &knob, 4);                 // fr4 = the knob; cents in fr0
        if (!c.call(0x894e0ce0, {}, kStack)) { out.clear(); return false; }
        float cents;
        std::memcpy(&cents, &c.fb[0][0], 4);
        // fr5 = (2·b183 − b182)/127 + cents/1200, the part's term 0
        const float two = 2.0f, e = xtf::add(xtf::div(0.0f, 127.0f), xtf::div(cents, 1200.0f));
        std::memcpy(&c.fb[0][4], &two, 4);
        std::memcpy(&c.fb[0][5], &e, 4);
        if (!c.call(0x8952ff9c, {}, kStack)) { out.clear(); return false; }
        std::memcpy(&out[(size_t)i], &c.fb[0][0], 4);
    }
    return true;
}

bool xtFwCosmKeySquares(std::vector<float>& out) {
    out.clear();
    if (g_rom.empty()) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    out.resize(41 * 128);
    for (int i = 0; i < 41; ++i)
        for (int key = 0; key < 128; ++key) {
            // fr4 = (float)(key − 60)·(float)(KF·10) / 100 / 12, fr5 = 2
            const float x = xtf::div(xtf::div(xtf::mul(xtf::flt(key - 60), xtf::flt((i - 20) * 10)), 100.0f), 12.0f), two = 2.0f;
            std::memcpy(&c.fb[0][4], &x, 4);
            std::memcpy(&c.fb[0][5], &two, 4);
            if (!c.call(0x8952ff9c, {}, kStack)) { out.clear(); return false; }
            std::memcpy(&out[(size_t)(i * 128 + key)], &c.fb[0][0], 4);
        }
    return true;
}

bool xtFwLoadBands(int type, const int16_t* pcm, long frames, int channels, XtMbBands& out) {
    auto M = waveMachine(g_rom, pcm, frames, channels, 0x400000);   // sample memory must stay below the heap
    if (!M || (type != 3 && type != 5)) return false;
    Sh4& c = M->cpu;
    c.wr32(kL + 4, (uint32_t)frames);
    const uint32_t info[7] = { (uint32_t)type, kSamp, (uint32_t)frames, (uint32_t)channels, 0, 0, 0 };
    for (int i = 0; i < 7; ++i) c.wr32(kInfo + 4 * i, info[i]);
    if (!c.call(kLoadInit, { kL + 0x34, kInfo }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    if (!c.call(kLoad, { kL }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    uint32_t slot[7];
    for (int k = 0; k < 7; ++k) slot[k] = c.rd32(kL + 0x10 + 4 * k);
    const int use[6] = { 0, 2, 3, 4, 5, 6 };
    std::vector<uint32_t> starts;
    for (int k = 0; k < 7; ++k) if (slot[k]) starts.push_back(slot[k]);
    std::sort(starts.begin(), starts.end());
    const uint32_t memEnd = kHeap;
    out.channels = channels;
    for (int i = 0; i < 6; ++i) {
        const uint32_t a = slot[use[i]];
        uint32_t e = 0;
        for (uint32_t s : starts) if (s > a) { e = s; break; }
        if (!e) e = std::min(memEnd, a + 0x10000);             // last band: its padding, not the rest of RAM
        if (!a || e <= a) { out.buf[i].clear(); out.len[i] = 0; continue; }
        const uint32_t fb = 2u * (uint32_t)channels, nf = (e - a) / fb;
        out.buf[i].resize((size_t)nf * channels);
        c.read(a, out.buf[i].data(), (size_t)nf * fb);
        out.len[i] = nf;
    }
    return true;
}

#ifdef XT_FW_MAIN
// xt_fw <rom.2.bin> solo <samp.bin>                         : SOLO records "start len info"
// xt_fw <rom.2.bin> load <3|5> <samp.bin> <channels> <out>  : the six bands, concatenated
// xt_fw <rom.2.bin> encode <3|5> <samp.bin> <channels> <out.vpdt> [depth] : the multi-band encoder
// xt_fw <rom.2.bin> import <samp.bin> <channels> <rate> <out.bin> : the WAV import converter to 44.1 kHz (int16)
// xt_fw <rom.2.bin> play <3|5> <samp.bin> <channels> <vpdt.bin> <pitch> <walk> <out.f32>
//   : bands from the firmware, then XtMbPlayer; float32 L/R pairs scaled to ±1
//   (XT_ROUND=nearest: round to nearest, as Ghidra's emulator)
// xt_fw <rom.2.bin> soloplay <samp.bin> <channels> <R> <F> <walk> <energy> <frames> <out.f32> [frame:walk ...]
//   : the firmware's SOLO encoder and player (xtFwSoloPlay); L/R pairs.
//   frame:walk changes the walk, frame:gX the output gain target (1 = unity);
//   "robot" turns Robot Voice on (R is then the pitch against A440)
// xt_fw <rom.2.bin> solocmp <samp.bin> <channels> <R> <F> <walk> <energy> <frames> [frame:walk ...] [loop:on,fwdMode,bwdMode,fs,fe,bs,be]
//   : the same against XtSoloVoice on the same records, word for word: how many differ, and if any
//   the largest difference and the difference's level against the signal
// xt_fw <rom.2.bin> bandcmp <3|5> <samp.bin> <channels> <pitch> <walk> <frames> [frame:walk ...] [loop:on,fwdMode,bwdMode,fs,fe,bs,be]
//   : the firmware's multi-band encoder, band rebuild and player
//   (xtFwBandPlay) against XtMbPlayer on the same bands
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include "XtSoloVoice.h"
static std::vector<uint8_t> slurp(const char* p) {
    std::vector<uint8_t> v; FILE* f = std::fopen(p, "rb"); if (!f) return v;
    std::fseek(f, 0, SEEK_END); v.resize((size_t)std::ftell(f)); std::fseek(f, 0, SEEK_SET);
    if (std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear(); std::fclose(f); return v;
}
int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: %s rom.2.bin solo samp.bin | load <3|5> samp.bin channels out\n", argv[0]); return 1; }
    if (!xtFwOpen(argv[1])) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    if (const char* r = std::getenv("XT_ROUND"); r && std::string(r) == "nearest") xtFwForceNearest(true);
    const std::string mode = argv[2];
    const auto t0 = std::chrono::steady_clock::now();
    if (mode == "solo") {
        std::vector<uint8_t> raw = slurp(argv[3]);
        std::vector<XtRecord> recs;
        if (!xtFwSoloEncode(reinterpret_cast<const int16_t*>(raw.data()), (int)(raw.size() / 2), recs)) return 1;
        for (const XtRecord& r : recs) std::printf("%d\t%d\t%d\n", r.start16, r.len16, r.info);
    } else if (mode == "import" && argc >= 7) {
        std::vector<uint8_t> raw = slurp(argv[3]);
        const int ch = std::atoi(argv[4]), rate = std::atoi(argv[5]);
        std::vector<int16_t> out;
        if (!xtFwImport(reinterpret_cast<const int16_t*>(raw.data()), (long)(raw.size() / (2 * ch)), ch, rate, out)) return 1;
        FILE* fo = std::fopen(argv[6], "wb");
        if (!fo) return 1;
        std::fwrite(out.data(), 2, out.size(), fo);
        std::fclose(fo);
        std::fprintf(stderr, "%ld frames in, %zu out\n", (long)(raw.size() / (2 * ch)), out.size() / (size_t)ch);
    } else if (mode == "load" && argc >= 7) {
        std::vector<uint8_t> raw = slurp(argv[4]);
        const int type = std::atoi(argv[3]), ch = std::atoi(argv[5]);
        XtMbBands b;
        if (!xtFwLoadBands(type, reinterpret_cast<const int16_t*>(raw.data()), (long)(raw.size() / (2 * ch)), ch, b)) return 1;
        FILE* fo = std::fopen(argv[6], "wb");
        if (!fo) return 1;
        for (int k = 0; k < 6; ++k) { std::fprintf(stderr, "band %d: %ld frames\n", k, b.len[k]); std::fwrite(b.buf[k].data(), 2, b.buf[k].size(), fo); }
        std::fclose(fo);
    } else if (mode == "encode" && argc >= 7) {
        std::vector<uint8_t> raw = slurp(argv[4]), vpdt;
        const int type = std::atoi(argv[3]), ch = std::atoi(argv[5]);
        const int depth = argc >= 8 ? std::atoi(argv[7]) : -1;
        int ev = 0;
        if (!xtFwEncodeMulti(type, reinterpret_cast<const int16_t*>(raw.data()), (long)(raw.size() / (2 * ch)), ch, vpdt, depth, &ev)) { std::fprintf(stderr, "encode failed\n"); return 1; }
        std::fprintf(stderr, "events %d\n", ev);
        FILE* fo = std::fopen(argv[6], "wb");
        if (!fo) return 1;
        std::fwrite(vpdt.data(), 1, vpdt.size(), fo);
        std::fclose(fo);
        std::fprintf(stderr, "vpdt %zu bytes\n", vpdt.size());
    } else if ((mode == "soloplay" && argc >= 11) || (mode == "solocmp" && argc >= 10)) {
        alarm(120);                        // a walk that never ends fails the check instead of hanging it
#if defined(__aarch64__)
        {   // denormals flushed, as the SH-4 does (FPSCR.DN = 1) and the plugin's audio thread does
            uint64_t fpcr;
            __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
            __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
        }
#endif
        std::vector<uint8_t> raw = slurp(argv[3]);
        const int ch = std::atoi(argv[4]);
        const long frames = (long)(raw.size() / (2 * (size_t)ch));
        const int16_t* pcm = reinterpret_cast<const int16_t*>(raw.data());
        XtFwSoloNote note;
        note.pitch = (float)std::atof(argv[5]); note.formant = (float)std::atof(argv[6]);
        note.walk = (float)std::atof(argv[7]); note.energy = (float)std::atof(argv[8]);
        const long outFrames = std::atol(argv[9]);
        for (int a = mode == "soloplay" ? 11 : 10; a < argc; ++a) {
            long at = 0; float w = 0;
            int lo[7];
            if (std::string(argv[a]) == "robot") note.robot = true;
            else if (std::sscanf(argv[a], "loop:%d,%d,%d,%d,%d,%d,%d", &lo[0], &lo[1], &lo[2], &lo[3], &lo[4], &lo[5], &lo[6]) == 7) {
                note.loop.on = lo[0] != 0; note.loop.fwdMode = lo[1]; note.loop.bwdMode = lo[2];
                note.loop.fwdStart = lo[3]; note.loop.fwdEnd = lo[4]; note.loop.bwdStart = lo[5]; note.loop.bwdEnd = lo[6];
            }
            else if (std::sscanf(argv[a], "%ld:g%f", &at, &w) == 2) note.gainAt.push_back({ at, w });
            else if (std::sscanf(argv[a], "%ld:%f", &at, &w) == 2) note.walkAt.push_back({ at, w });
        }
        std::vector<XtRecord> recs;
        if (!xtFwSoloEncode(pcm, (int)frames, recs, ch) || recs.size() < 2) { std::fprintf(stderr, "encode failed\n"); return 1; }
        for (size_t i = 0; i < recs.size(); ++i)   // the vpdt holds lengths only: records must run from 0, back to back
            if (recs[i].start16 != (i ? recs[i - 1].start16 + recs[i - 1].len16 : 0)) { std::fprintf(stderr, "record %zu not contiguous\n", i); return 1; }
        std::vector<float> fw;
        if (!xtFwSoloPlay(recs, pcm, frames, ch, note, outFrames, fw)) { std::fprintf(stderr, "play failed\n"); return 1; }
        if (mode == "soloplay") {
            FILE* fo = std::fopen(argv[10], "wb");
            if (!fo) return 1;
            std::fwrite(fw.data(), sizeof(float), fw.size(), fo);
            std::fclose(fo);
        } else {
            // the plan as xtSynthEncode builds it
            XtSoloPlan P;
            P.in.resize((size_t)frames);
            if (ch == 2) P.inR.resize((size_t)frames);
            for (long i = 0; i < frames; ++i) {
                P.in[(size_t)i] = (float)pcm[i * ch] / 32768.0f;
                if (ch == 2) P.inR[(size_t)i] = (float)pcm[2 * i + 1] / 32768.0f;
            }
            const size_t nc = recs.size();
            P.segStart.resize(nc); P.segLen.resize(nc); P.voiced.resize(nc); P.info.resize(nc);
            for (size_t i = 0; i < nc; ++i) {
                P.segStart[i] = recs[i].start16 / 16.0;
                const double end = (i + 1 < nc) ? recs[i + 1].start16 / 16.0 : (double)frames;
                P.segLen[i] = std::max(1.0 / 16.0, end - P.segStart[i]);
                P.voiced[i] = (recs[i].info & 4) != 0;
                P.info[i] = recs[i].info;
            }
            XtSoloVoice v;
            v.setRobot(note.robot);
            v.setLoop(note.loop);
            v.noteOn(P, note.pitch, note.formant, note.walk, note.energy);
            v.resetGain(1.0f / 65536.0f);                      // as the firmware's here: unity in its int16 scale
            float lr[64];
            double maxd = 0, sig = 0, dif = 0;
            long first = -1, nDiff = 0;
            size_t next = 0, nextG = 0;
            for (long done = 0; done < outFrames; done += 32) {
                while (next < note.walkAt.size() && note.walkAt[next].first <= done) {
                    v.setRates(note.pitch, note.formant, note.walkAt[next++].second);
                }
                while (nextG < note.gainAt.size() && note.gainAt[nextG].first <= done) v.setGain(note.gainAt[nextG++].second / 65536.0f);
                v.renderXt(lr, 32);
                for (long i = 0; i < 32 && done + i < outFrames; ++i)
                    for (int k = 0; k < 2; ++k) {
                        const float a = fw[(size_t)(2 * (done + i) + k)], b = lr[2 * i + k] / 32768.0f;
                        const double d = std::fabs((double)a - (double)b);
                        if (d > 0) { if (first < 0) first = done + i; ++nDiff; }
                        maxd = std::max(maxd, d); sig += (double)a * a; dif += d * d;
                    }
            }
            std::printf("%zu records, %ld frames: %ld different of %ld (signal rms %.4f)", nc, outFrames, nDiff, 2 * outFrames, std::sqrt(sig / std::max(1L, 2 * outFrames)));
            if (nDiff) std::printf(", max |firmware - port| %.3g, difference %.1f dB against the signal, first at %ld", maxd, 10.0 * std::log10(dif / std::max(sig, 1e-30)), first);
            std::printf("\n");
        }
    } else if (mode == "bandcmp" && argc >= 9) {
        std::vector<uint8_t> raw = slurp(argv[4]), vpdt;
        const int type = std::atoi(argv[3]), ch = std::atoi(argv[5]);
        const long frames = (long)(raw.size() / (2 * (size_t)ch));
        const int16_t* pcm = reinterpret_cast<const int16_t*>(raw.data());
        XtFwSoloNote note;
        note.pitch = (float)std::atof(argv[6]); note.walk = (float)std::atof(argv[7]);
        const long outFrames = std::atol(argv[8]);
        for (int a = 9; a < argc; ++a) {
            long at = 0; float w = 0;
            int lo[7];
            if (std::sscanf(argv[a], "loop:%d,%d,%d,%d,%d,%d,%d", &lo[0], &lo[1], &lo[2], &lo[3], &lo[4], &lo[5], &lo[6]) == 7) {
                note.loop.on = lo[0] != 0; note.loop.fwdMode = lo[1]; note.loop.bwdMode = lo[2];
                note.loop.fwdStart = lo[3]; note.loop.fwdEnd = lo[4]; note.loop.bwdStart = lo[5]; note.loop.bwdEnd = lo[6];
            }
            else if (std::sscanf(argv[a], "%ld:g%f", &at, &w) == 2) note.gainAt.push_back({ at, w });
            else if (std::sscanf(argv[a], "%ld:%f", &at, &w) == 2) note.walkAt.push_back({ at, w });
        }
        if (!xtFwEncodeMulti(type, pcm, frames, ch, vpdt, 64)) { std::fprintf(stderr, "encode failed\n"); return 1; }
        std::vector<float> fw;
        if (!xtFwBandPlay(type, pcm, frames, ch, vpdt, note, outFrames, fw)) { std::fprintf(stderr, "firmware play failed\n"); return 1; }
        XtMbBands b;
        if (!xtFwLoadBands(type, pcm, frames, ch, b) || !xtMbLoadTables(argv[1])) return 1;
        XtMbWave w;
        w.vpdt = vpdt.data(); w.vpdtBytes = vpdt.size();
        xtMbAttach(b, w);
        XtMbPlayer pl;
        pl.setLoop(note.loop);
        if (!pl.noteOn(w, type, note.pitch, note.walk)) { std::fprintf(stderr, "port note-on failed\n"); return 1; }
        float blk[64];
        double maxd = 0, sig = 0, dif = 0;
        long first = -1;
        size_t next = 0, nextG = 0;
        for (long done = 0; done < outFrames; done += 32) {
            while (next < note.walkAt.size() && note.walkAt[next].first <= done) pl.setRates(note.pitch, note.walkAt[next++].second);
            while (nextG < note.gainAt.size() && note.gainAt[nextG].first <= done) pl.setGain(note.gainAt[nextG++].second);
            pl.block(blk);
            for (long i = 0; i < 32 && done + i < outFrames; ++i)
                for (int k = 0; k < 2; ++k) {
                    const double a = fw[(size_t)(2 * (done + i) + k)], bb = blk[2 * i + k] / 32768.0;
                    const double d = std::fabs(a - bb);
                    if (d > 0 && first < 0) first = done + i;
                    maxd = std::max(maxd, d); sig += a * a; dif += d * d;
                }
        }
        std::printf("%s, %ld frames: max |firmware - port| %.3g, difference %.1f dB against the signal (rms %.4f), first at %ld\n",
                    type == 5 ? "ENSEMBLE" : "BACKING", outFrames, maxd, 10.0 * std::log10(std::max(dif, 1e-30) / std::max(sig, 1e-30)),
                    std::sqrt(sig / std::max(1L, 2 * outFrames)), first);
    } else if (mode == "play" && argc >= 10) {
        std::vector<uint8_t> raw = slurp(argv[4]), vpdt = slurp(argv[6]);
        const int type = std::atoi(argv[3]), ch = std::atoi(argv[5]);
        XtMbBands b;
        if (!xtFwLoadBands(type, reinterpret_cast<const int16_t*>(raw.data()), (long)(raw.size() / (2 * ch)), ch, b)) return 1;
        if (!xtMbLoadTables(argv[1])) return 1;
        XtMbWave w;
        w.vpdt = vpdt.data(); w.vpdtBytes = vpdt.size();
        xtMbAttach(b, w);
        const float pitch = (float)std::atof(argv[7]), walk = (float)std::atof(argv[8]);
        XtMbPlayer pl;
        if (!pl.noteOn(w, type, pitch, walk)) return 1;
        const long frames = std::lround((double)(raw.size() / (2 * ch)) / walk);
        FILE* fo = std::fopen(argv[9], "wb");
        if (!fo) return 1;
        float blk[64];
        for (long done = 0; done < frames; done += 32) {
            pl.block(blk);
            const long nn = std::min(32L, frames - done);
            for (long i = 0; i < 2 * nn; ++i) blk[i] /= 32768.0f;
            std::fwrite(blk, sizeof(float), (size_t)(2 * nn), fo);
        }
        std::fclose(fo);
    } else return 1;
    std::fprintf(stderr, "%.3f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    return 0;
}
#endif
