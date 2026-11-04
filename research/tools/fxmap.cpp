// fxmap: the firmware's effect driver in the SH-4 interpreter, with the effect chip's bus hooked (HARDWARE_FIRMWARE.md, "The effects").
//   fxmap <rom> table             per type: program table, active words, what the update writes, memory operations
//   fxmap <rom> mfx <type> [i v]  one MFX type at its defaults (param i = v): each word as fields, and what wrote it
//   fxmap <rom> boot              boot, Chorus and Reverb through their real entry points: programs loaded, words set going
// Prints to stdout, for build/ only: a program as words is a listing of the firmware and is never committed.
// Build: clang++ -std=c++20 -O2 -Iplugin/Source research/tools/fxmap.cpp plugin/Source/Sh4Interp.cpp -o build/tools/fx/fxmap
#include "Sh4Interp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
constexpr uint32_t kSp = 0x8C0E0000, kMfxObj = 0x89cef260;
constexpr uint32_t kMfxIds = 0x897a8218, kRevIds = 0x897a8310, kChoIds = 0x897a82e4;   // first, last ID per type
constexpr uint32_t kDesc = 0x897a56d8;                                                  // second descriptor bank, from ID 0x55d
const char* kRevName[] = { "Off", "Room 1", "Room 2", "Room 3", "Hall 1", "Hall 2", "Hall 3", "Garage", "Plate", "Non-Linear", "Delay" };

uint32_t W[0x800];                       // the chip's words as the driver has written them
std::map<int, std::string> note;         // what touched a word
int nCoef, nField, nDly; std::set<int> coefWords; int writers; uint32_t lastTable; bool logProg;

// The driver's bus primitives: everything else it does to the chip goes through these.
void hookBus(Sh4& c) {
    auto add = [](int i, const char* fmt, auto... a) { char b[80]; std::snprintf(b, sizeof b, fmt, a...); note[i] = b; };
    c.hook(0x894af3a0, [](Sh4& m) { m.ret(0); });                                       // short wait
    c.hook(0x894af3c0, [](Sh4& m) { m.ret(0); });                                       // wait while busy
    c.hook(0x894b26a0, [](Sh4& m) { m.ret(0); });                                       // wait, n ms
    c.hook(0x894af400, [](Sh4& m) { int i = (int16_t)m.r[4]; if (i >= 0 && i < 0x800) W[i] = m.r[5]; m.ret(0); });       // command 0x34
    c.hook(0x894af510, [](Sh4& m) { int i = (int16_t)m.r[4]; m.ret(i >= 0 && i < 0x800 ? W[i] & 0x0fffffff : 0); });     // command 0x37
    c.hook(0x894af610, [add](Sh4& m) {                                                  // command 0x35: a coefficient
        int i = (int16_t)m.r[4]; uint32_t v = (uint16_t)m.r[5], s = m.r[6] & 3;
        W[i] = (W[i] & ~0x3ffu) | (s << 8) | (v >> 8); W[i + 1] = (W[i + 1] & ~0x3ffu) | (s << 8) | ((v >> 1) & 0x7f);
        nCoef++; coefWords.insert(i); add(i, "coef %04x sh%d", v, s); m.ret(0); });
    c.hook(0x894af6a0, [add](Sh4& m) {                                                  // command 0x36: a memory offset
        int i = (int16_t)m.r[4]; uint32_t v = m.r[5];
        auto put = [&](int k, uint32_t f) { W[i + k] = (W[i + k] & 0x7fffff) | (f << 23); };
        put(1, v & 31); put(2, (v >> 5) & 31); put(3, (v >> 10) & 31); put(4, (v >> 15) & 7);
        nDly++; add(i, "offset %05x", v); m.ret(0); });
    c.hook(0x894afa10, [](Sh4& m) { lastTable = m.r[6]; if (logProg) std::printf("  program at %03x, %03x words, from %08x\n", (int16_t)m.r[4], (int16_t)m.r[5], m.r[6]); });
    c.hook(0x894af970, [](Sh4& m) { if (logProg) std::printf("  clear %03x, %03x words\n", (int16_t)m.r[4], (int16_t)m.r[5]); });
    c.hook(0x894af7a0, [add](Sh4& m) { writers++; add((int16_t)m.r[4], "write on"); if (logProg) std::printf("  write on %03x\n", (int16_t)m.r[4]); });
    c.hook(0x894af7d0, [add](Sh4& m) { writers--; add((int16_t)m.r[4], "write off"); });
    c.hook(0x894af7f0, [add](Sh4& m) { nField++; add((int16_t)m.r[4], "field %02x sh%d", m.r[5] & 0xff, m.r[6] & 3); });
}
void resetLog() { nCoef = nField = nDly = writers = 0; coefWords.clear(); note.clear(); }

struct Mfx {
    Sh4& c; uint32_t obj = 0, vt = 0; int type;
    Mfx(Sh4& cpu, int t) : c(cpu), type(t) { c.call(0x894b43a0, { kMfxObj, (uint32_t)t }, kSp, &obj); vt = c.rd32(obj + 0x54); c.wr32(obj + 0x58, t); c.wr32(obj + 0xdc, 12000); }
    uint32_t vcall(uint32_t off) { uint32_t d = c.rd32(vt + off), fn = c.rd32(vt + off + 8), r = 0; if (!c.call(fn, { obj + d }, kSp, &r)) std::printf("fault in %08x: %s\n", fn, c.error().c_str()); return r; }
    int params() { return type ? (int16_t)c.rd16(kMfxIds + 4 * type + 2) - (int16_t)c.rd16(kMfxIds + 4 * type) + 1 : 0; }
    void defaults() { int first = (int16_t)c.rd16(kMfxIds + 4 * type); for (int i = 0; i < params() && i < 32; i++) set(i, (int16_t)c.rd16(kDesc + 8 * (first - 0x55d + i) + 2)); }
    void set(int i, int v) { c.wr16(obj + 0x5c + 4 * i, (uint16_t)v); }
    void load() { vcall(0x6c); }                                                        // +0x74: the program
    void update() { for (int k = 0; k < 100 && !vcall(0x78); k++) {} }                  // +0x80: compares the params with the last, writes; 0 = needs a mute first
    void unload() { vcall(0x84); }                                                      // +0x8c
};

// A memory operation takes five words: [27:26] of the first says what (1 read, 2 write, 3 read at the address register), the others' [27:23] the offset.
void memOps(int lo, int hi, int& wr, int& rd, int& ind, uint32_t& top) {
    wr = rd = ind = 0; top = 0;
    for (int i = lo; i < hi; i++) {
        int k = W[i] >> 26 & 3; if (!k) continue;
        auto f = [&](int j) { return (W[j] >> 23) & 31u; };
        uint32_t a = f(i + 1) | f(i + 2) << 5 | f(i + 3) << 10 | (f(i + 4) & 7) << 15;
        if (k == 1) rd++; else if (k == 2) wr++; else ind++;
        if (k != 3 && a > top) top = a;
        i += 4;
    }
}
void row(int t, const char* name, int np, int lo, int hi, uint32_t fnLoad, uint32_t fnUpd) {
    int act = 0, wr, rd, ind; uint32_t top; for (int i = lo; i < hi; i++) if (W[i]) act++;
    memOps(lo, hi, wr, rd, ind, top);
    std::printf("%2d|%-20s|%2d|%08x|%08x|%08x|%3d/%3d|%3zu|%3d|%2d|%2d|%2d|%2d|%05x\n", t, name, np, fnLoad, fnUpd, lastTable, act, hi - lo, coefWords.size(), nField, wr, rd, ind, nDly, top);
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "fxmap <rom> table | mfx <type> [i v]... | boot\n"); return 2; }
    Sh4 c; if (!c.loadRom(argv[1])) { std::fprintf(stderr, "cannot load %s\n", argv[1]); return 1; }
    c.mapRam(0x8C000000, 0x02000000);
    hookBus(c);
    const std::string what = argv[2];
    if (what == "mfx" && argc > 3) {
        const int t = std::atoi(argv[3]); Mfx m(c, t); m.load(); m.defaults();
        for (int a = 4; a + 1 < argc; a += 2) m.set(std::atoi(argv[a]), std::atoi(argv[a + 1]));
        m.update();
        for (int i = 0x5c2; i < 0x6f5; i++) { const uint32_t x = W[i]; if (!x && !note.count(i)) continue;
            std::printf("%03x %07x  mem %02x  x%d op %x  r%02x  sh%d  c%02x  %s\n", i, x, x >> 23 & 31, x >> 22 & 1, x >> 18 & 15, x >> 10 & 255, x >> 8 & 3, x & 255, note.count(i) ? note[i].c_str() : ""); }
        return 0;
    }
    if (what == "boot") {
        logProg = true;
        std::puts("boot"); c.call(0x894afc50, {}, kSp);
        std::puts("reverb"); c.call(0x894d09b0, {}, kSp);
        for (int t = 0; t <= 10; t++) { std::printf("type %d %s\n", t, kRevName[t]); c.call(0x894d0a70, { (uint32_t)t }, kSp); c.call(0x894d0b00, { 0 }, kSp); }
        std::puts("chorus"); c.call(0x894d3350, {}, kSp);
        for (int t = 0; t <= 8; t++) { resetLog(); c.call(0x894d3420, { (uint32_t)t }, kSp); c.call(0x894d3470, { 0 }, kSp); std::printf("type %d: %d coefficients, %d fields\n", t, nCoef, nField); }
        return 0;
    }
    if (what == "table") {
        std::puts("type|name|params|load|update|program|active|coef|field|writes|reads|indirect|offsets|top");
        for (int t = 0; t < 42; t++) {
            std::memset(W, 0, sizeof W); resetLog();
            char nm[24]; c.read(0x89788c98 + 24 * t, nm, 20); nm[20] = 0; for (int e = 19; e > 0 && nm[e] == ' '; e--) nm[e] = 0;
            Mfx m(c, t); m.load(); m.defaults(); m.update();
            row(t, nm, m.params(), 0x5c2, 0x6f5, c.rd32(m.vt + 0x74), c.rd32(m.vt + 0x80));
            m.unload();
        }
        std::puts("--- reverb"); c.call(0x894d09b0, {}, kSp);
        for (int t = 0; t <= 10; t++) {
            c.call(0x894d0a70, { (uint32_t)t }, kSp);
            const int first = (int16_t)c.rd16(kRevIds + 4 * t), last = (int16_t)c.rd16(kRevIds + 4 * t + 2);
            for (int i = 0; i < 24; i++) { const int16_t d = i <= last - first ? (int16_t)c.rd16(kDesc + 8 * (first + i - 0x55d) + 2) : 0; c.call(0x894d0a80, { (uint32_t)i, (uint32_t)(int32_t)d }, kSp); }
            resetLog(); c.call(0x894d0b00, { 0 }, kSp);
            const uint32_t s = c.rd32(0x8987ae3c + 4 * t);
            row(t, kRevName[t], last - first + 1, 0x464, 0x5c2, c.rd32(s + 4), c.rd32(s + 0xc));
        }
        std::puts("--- chorus (no program of its own)"); c.call(0x894d3350, {}, kSp);
        for (int t = 0; t <= 8; t++) {
            resetLog(); c.call(0x894d3420, { (uint32_t)t }, kSp);
            const int first = (int16_t)c.rd16(kChoIds + 4 * t);
            for (int i = 0; i < 10; i++) c.call(0x894d3430, { (uint32_t)i, (uint32_t)(int32_t)(int16_t)c.rd16(kDesc + 8 * (first + i - 0x55d) + 2) }, kSp);
            c.call(0x894d3470, { 0 }, kSp);
            std::printf("%d: %zu coefficients, %d fields\n", t, coefWords.size(), nField);
        }
        return 0;
    }
    return 2;
}
