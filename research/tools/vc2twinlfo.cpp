// vc2run lfo <rom.2>: the LFO on rom.2's routines and on rom.4's, paired by codepairs.py (constructor 0x894f0b40 /
// 0x893acbf0, note-on 0x894f0cf0 / 0x893acda0, tick 0x894f1050 / 0x893ad100, note-off 0x894f0810 / 0x893ac8c0, clock
// 0x894f0fd0 / 0x893ad080), and the port (XtLfo) beside rom.4's as xt_lfo holds it beside rom.2's.  After every call the
// LFO's whole object is compared between the two images, and rom.4's output, phase, amplitude and clock state with the
// port's.  The constructor draws from a chaos generator's global, seeded alike in each (rom.2 0x89f91c48, rom.4
// 0x89999668, by codepairs.py --pools).  Fails on any difference.
#include "XtLfo.h"
#include "vc2runs.h"
#include "vc2twin.h"
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

namespace vc2 {

namespace {

struct Routines {
    uint32_t ctor, noteOn, tick, noteOff, clock, chaos;
};
constexpr Routines kVSynth{0x894f0b40, 0x894f0cf0, 0x894f1050, 0x894f0810, 0x894f0fd0, 0x89f91c48};   // [FW] rom.2
constexpr Routines kVc2{0x893acbf0, 0x893acda0, 0x893ad100, 0x893ac8c0, 0x893ad080, 0x89999668};      // [FW] rom.4
// [FW] what the LFO object holds that moved: its +0x24, the same pair codepairs.py --pools finds in the routines' literals
constexpr Twin::Moved kMoved[] = {{0x89833ef8, 0x898cd490}};
constexpr uint32_t kLfo = kScratch, kParams = kScratch + 0x200, kObject = 0x100;
constexpr float kChaosSeed = 0.3141f;

struct LfoCase {
    int wave, rate, offset, delay, fadeMode, fade, off, ticks, clockTicks;
    float tempo;
};

// Every wave, slow to fast rates, the offsets, the delay and fade modes, a note-off, and note values (128 on) whose clocks
// come every clockTicks ticks at a moving interval
constexpr LfoCase kCases[] = {
    {0, 64, 2, 0, 0, 0, -1, 300, 0, 120},
    {1, 64, 2, 0, 0, 0, -1, 300, 0, 120},
    {2, 80, 2, 0, 0, 0, -1, 300, 0, 120},
    {3, 70, 2, 0, 0, 0, -1, 300, 0, 120},
    {4, 90, 2, 0, 0, 0, -1, 500, 0, 120},
    {5, 100, 2, 0, 0, 0, -1, 500, 0, 120},
    {6, 110, 2, 0, 0, 0, -1, 500, 0, 120},
    {7, 64, 2, 0, 0, 0, -1, 300, 0, 120},
    {1, 127, 0, 0, 0, 0, -1, 200, 0, 120},
    {1, 0, 4, 0, 0, 0, -1, 200, 0, 120},
    {2, 64, 3, 40, 0, 50, -1, 800, 0, 120},
    {1, 64, 1, 30, 1, 60, -1, 800, 0, 120},
    {0, 64, 2, 20, 2, 40, 300, 800, 0, 120},
    {3, 64, 2, 0, 3, 70, 200, 800, 0, 120},
    {1, 50, 2, 127, 0, 127, -1, 1200, 0, 120},
    {0, 135, 2, 0, 0, 0, -1, 1000, 2, 120},
    {2, 144, 2, 0, 0, 0, -1, 2000, 2, 90},
    {3, 140, 3, 20, 1, 40, 500, 1200, 5, 45},
    {6, 142, 2, 0, 0, 0, -1, 1500, 1, 140},
    {7, 136, 2, 0, 0, 0, -1, 400, 2, 233.4f},
};

// The clock's interval for tick t: about a clock's, with some past each limit
float clockDt(int t) {
    if (t % 7 == 0) return 0.5f;
    if (t % 11 == 0) return 0.001f;
    return 0.000726f * (float)(25 + t % 9);
}

// What rom.4's object says against the port, or nullptr when they agree
const char* lfoAgainstPort(Machine& m, const XtLfo& lfo) {
    if (m.f32(kLfo + 0x38) != lfo.out() || m.f32(kLfo + 0x40) != lfo.phase() || m.f32(kLfo + 0x20) != lfo.amplitude())
        return "output, phase or amplitude";
    const bool clockSame = m.f32(kLfo + 0x84) == lfo.clockStep() && m.f32(kLfo + 0x88) == lfo.clockTarget()
                           && m.f32(kLfo + 0x8c) == lfo.clockTime() && (int)m.cpu.rd32(kLfo + 0x90) == lfo.clockWrap()
                           && (int)m.cpu.rd32(kLfo + 0x94) == lfo.clockCount();
    return clockSame ? nullptr : "clock state";
}

// One case: false when the images or the port part; `calls` counts the states compared
bool lfoCase(Twin& t, const LfoCase& k, long& calls) {
    const XtLfoParams p = xtLfoConvert(k.wave, k.rate, k.offset, k.delay, k.fadeMode, k.fade, true);
    const std::vector<uint8_t> zero(kObject, 0);
    t.write(kLfo, zero.data(), zero.size());
    t.writeF32(kVSynth.chaos, kVc2.chaos, kChaosSeed);
    if (!t.call(kVSynth.ctor, kVc2.ctor, {kLfo})) return false;
    uint32_t w[8] = {(uint32_t)p.wave, 0, 0, 0, (uint32_t)p.keySync, (uint32_t)p.sync, (uint32_t)p.offset,
                     (uint32_t)p.fadeMode};
    std::memcpy(&w[1], &p.rate, 4);
    std::memcpy(&w[2], &p.delayMs, 4);
    std::memcpy(&w[3], &p.fadeMs, 4);
    t.write(kParams, w, sizeof w);
    XtLfo port;
    port.noteOn(p, t.vc2.f32(kLfo + 0x6c), k.tempo);
    if (!t.call(kVSynth.noteOn, kVc2.noteOn, {kLfo, kParams}, k.tempo)) return false;
    for (int tick = 0; tick <= k.ticks; ++tick) {
        if (tick > 0 && tick == k.off) {
            if (!t.call(kVSynth.noteOff, kVc2.noteOff, {kLfo})) return false;
            port.noteOff();
        }
        if (tick > 0 && k.clockTicks && tick % k.clockTicks == 0) {
            if (!t.call(kVSynth.clock, kVc2.clock, {kLfo}, clockDt(tick))) return false;
            port.clock(clockDt(tick));
        }
        if (tick > 0) {
            if (!t.call(kVSynth.tick, kVc2.tick, {kLfo, 1})) return false;
            port.tick(1);
        }
        ++calls;
        const long at = t.differs(kLfo, kObject, kMoved);
        const char* what = lfoAgainstPort(t.vc2, port);
        if (at < 0 && !what) continue;
        std::printf("wave %d rate %d tick %d: ", k.wave, k.rate, tick);
        if (at >= 0) t.sayDiffers(kLfo, at);
        std::printf("%s\n", what ? what : "the port agrees with rom.4");
        return false;
    }
    return true;
}

int lfoCases(Twin& t) {
    long calls = 0;
    int bad = 0;
    for (const LfoCase& k : kCases) bad += lfoCase(t, k, calls) ? 0 : 1;
    std::printf("lfo: %zu cases, %ld states compared, %d cases different\n", std::size(kCases), calls, bad);
    return bad;
}

} // namespace

int lfo(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, xtLfoLoadTables, lfoCases);
}

} // namespace vc2
