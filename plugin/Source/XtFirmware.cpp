// XT firmware routines run in the SH-4 interpreter — see XtFirmware.h.
// The setup mirrors research/ghidra_scripts/EmuEnc.java and EmuLoad.java.
#include "XtFirmware.h"
#include "Sh4Interp.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace {

// rom.2 entry points and OS calls (see research/HARDWARE_FIRMWARE.md)
constexpr uint32_t kMalloc = 0x89527910, kCalloc = 0x89527930, kFree = 0x89527950, kOsInit = 0x895278f0, kUi = 0x89511360;
constexpr uint32_t kSoloNew = 0x8951dc40, kSoloEncode = 0x8951e120;
constexpr uint32_t kLoadInit = 0x89529e90, kLoad = 0x894a9940;
constexpr uint32_t kEncMulti = 0x89516000;                       // BACKING/ENSEMBLE encode driver
constexpr uint32_t kOsAlloc = 0x895113a0, kOsFree = 0x895113e0;  // sample-memory chunks (OS pointers 0x8987c8f0/8f8)
constexpr uint32_t kAfterVpdt = 0x89515b40;                       // the note chunk, after the vpdt is written
constexpr uint32_t kSmp = 0x8987c940;    // sample block: +0 type, +2 channels, +8 frames, +0x20 start, +0x24 end, +0x7c samples
constexpr uint32_t kChunks = 0x8987c97c; // (ptr, size) per chunk kind 0..8, set by 0x89517320
constexpr uint32_t kMarkCount = 0x89ff0cfc, kMarkHead = 0x89ff0d04;
// harness memory (as the emulator scripts use it)
constexpr uint32_t kRam = 0x8C000000, kRamSize = 0x02000000;
constexpr uint32_t kStack = 0x8C0E0000, kEv = 0x8C0F0000, kL = 0x8C0F0000, kInfo = 0x8C0F0100;
constexpr uint32_t kSamp = 0x8C100000, kHeap = 0x8D000000;

std::string g_rom;
bool g_nearest = false;

struct Machine {
    Sh4 cpu;
    uint32_t heap = kHeap;
    bool ok = false;
    explicit Machine(const std::string& rom) {
        if (!cpu.loadRom(rom.c_str())) return;
        cpu.mapRam(kRam, kRamSize);
        cpu.forceNearest = g_nearest;
        auto alloc = [this](uint32_t sz) -> uint32_t {
            sz = (sz + 15) & ~15u;
            const uint32_t p = heap;
            if (heap + sz > kRam + kRamSize) return 0;
            if (uint8_t* m = cpu.ptr(p, sz)) std::memset(m, 0, sz);
            heap += sz;
            return p;
        };
        cpu.hook(kMalloc, [alloc](Sh4& c) { c.ret(alloc(c.r[4])); });
        cpu.hook(kCalloc, [alloc](Sh4& c) { c.ret(alloc(c.r[4] * std::max(1u, c.r[5]))); });
        cpu.hook(kFree, [](Sh4& c) { c.ret(0); });
        cpu.hook(kOsInit, [](Sh4& c) { c.ret(0); });
        cpu.hook(kUi, [](Sh4& c) { c.ret(0); });
        ok = true;
    }
};

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
void xtFwForceNearest(bool on) { g_nearest = on; }

bool xtFwSoloEncode(const int16_t* x, int n, std::vector<XtRecord>& out) {
    out.clear();
    if (g_rom.empty() || !x || n <= 0) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    c.write(kSamp, x, (size_t)n * 2);
    c.wr32(kEv, (uint32_t)n * 16);          // event list {n·16}: one sustained note
    if (!c.call(kSoloNew, { (uint32_t)n, kSamp, 1, kEv }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    if (!c.call(kSoloEncode, { 2 }, kStack)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    // Node pool: 16-byte nodes {u16 low, u16 v, i32 pos, u32 lower, u32 higher};
    // the record chain is the longest walk up from a node with no lower link.
    struct Node { uint32_t low, v; int32_t pos; uint32_t lo, hi; };
    std::map<uint32_t, Node> nodes;
    for (uint32_t p = kHeap + 16; p + 16 <= M->heap; p += 16) {
        Node d{ c.rd16(p), c.rd16(p + 2), (int32_t)c.rd32(p + 4), c.rd32(p + 8), c.rd32(p + 12) };
        if (d.low || d.v || d.pos || d.lo || d.hi) nodes[p] = d;
    }
    std::vector<Node> best;
    for (const auto& e : nodes) {
        if (e.second.lo != 0 || !nodes.count(e.second.hi)) continue;
        std::vector<Node> seq;
        std::set<uint32_t> seen;
        for (uint32_t p = e.first; nodes.count(p) && seen.insert(p).second; p = nodes[p].hi) seq.push_back(nodes[p]);
        if (seq.size() > best.size()) best = seq;
    }
    for (size_t k = 0; k + 1 < best.size(); ++k)
        out.push_back({ best[k].pos, best[k + 1].pos - best[k].pos, (int)(((best[k + 1].v >> 8) & 0xf8) | best[k + 1].low) });
    return true;
}

bool xtFwEncodeMulti(int type, const int16_t* pcm, long frames, int channels, std::vector<uint8_t>& vpdt) {
    vpdt.clear();
    if (g_rom.empty() || !pcm || frames <= 0 || (channels != 1 && channels != 2) || (type != 3 && type != 5)) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    const size_t bytes = (size_t)frames * channels * 2;
    if (kSamp + bytes + 0x800000 > kHeap) return false;
    c.write(kSamp, pcm, bytes);
    // the sample being encoded
    c.wr8(kSmp, (uint8_t)type);
    c.wr8(kSmp + 2, (uint8_t)channels);
    c.wr32(kSmp + 8, (uint32_t)frames);
    c.wr32(kSmp + 0x20, 0);
    c.wr32(kSmp + 0x24, (uint32_t)frames);
    c.wr32(kSmp + 0x7c, kSamp);
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
    // OS sample-memory chunks: hand out heap blocks
    c.hook(kOsAlloc, [&M](Sh4& cpu) {
        const uint32_t sz = 0x400000, p = M->heap;
        if (p + sz > kRam + kRamSize) { cpu.ret(0); return; }
        if (uint8_t* m = cpu.ptr(p, sz)) std::memset(m, 0, sz);
        M->heap += sz;
        cpu.ret(p);
    });
    c.hook(kOsFree, [](Sh4& cpu) { cpu.ret(0); });
    // The vpdt is complete when the driver moves on to the note chunk
    // (0x89515b40, after 0x89515a70 succeeded): take it there and stop.
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
    uint32_t rc = 0;
    if (!c.call(kEncMulti, { 0 }, kStack, &rc)) { std::fprintf(stderr, "sh4: %s\n", c.error().c_str()); return false; }
    if (std::getenv("XT_FW_TRACE")) std::fprintf(stderr, "encode driver returned %u; [89ff0dd0]=%d; vpdt %zu bytes\n", rc, (int)c.rd32(0x89ff0dd0), vpdt.size());
    return done;
}

bool xtFwLoadBands(int type, const int16_t* pcm, long frames, int channels, XtMbBands& out) {
    if (g_rom.empty() || !pcm || frames <= 0 || (channels != 1 && channels != 2) || (type != 3 && type != 5)) return false;
    auto M = std::make_unique<Machine>(g_rom);
    if (!M->ok) return false;
    Sh4& c = M->cpu;
    const size_t bytes = (size_t)frames * channels * 2;
    if (kSamp + bytes + 0x400000 > kHeap) return false;   // sample memory must stay below the heap
    c.write(kSamp, pcm, bytes);
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
// xt_fw <rom.2.bin> encode <3|5> <samp.bin> <channels> <out.vpdt> : the multi-band encoder
// xt_fw <rom.2.bin> play <3|5> <samp.bin> <channels> <vpdt.bin> <pitch> <walk> <out.f32>
//   : bands from the firmware, then XtMbPlayer; float32 L/R pairs scaled to ±1
//   (XT_ROUND=nearest: round to nearest, as Ghidra's emulator)
#include <cmath>
#include <cstdlib>
#include <chrono>
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
        if (!xtFwEncodeMulti(type, reinterpret_cast<const int16_t*>(raw.data()), (long)(raw.size() / (2 * ch)), ch, vpdt)) { std::fprintf(stderr, "encode failed\n"); return 1; }
        FILE* fo = std::fopen(argv[6], "wb");
        if (!fo) return 1;
        std::fwrite(vpdt.data(), 1, vpdt.size(), fo);
        std::fclose(fo);
        std::fprintf(stderr, "vpdt %zu bytes\n", vpdt.size());
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
