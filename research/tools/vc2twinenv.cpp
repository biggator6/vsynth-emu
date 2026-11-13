// vc2run env <rom.2>: the envelope on rom.2's routines and on rom.4's, paired by codepairs.py (constructor 0x894efdb0 /
// 0x893abe40, note-on 0x894f0130 / 0x893ac1c0, tick 0x894efdf0 / 0x893abe80, note-off 0x894f03e0 / 0x893ac470), and the
// port (XtEnvelope) beside rom.4's as xt_env holds it beside rom.2's: the object compared between the images after every
// call, rom.4's stage, phase, output, coefficient, onset and stage time against the port's.  Then its converters, rom.2's
// against rom.4's return for return, over xt_env conv's ranges: the Time KF scale (0x894df760 / 0x8939a920), the time
// converter (0x894df7b0 / 0x8939a970, the one of rom.4's two shaped alike that reads the same table), and the pitch depth
// (0x894e0ce0 / 0x8939c1a0).  Fails on any difference.
#include "XtEnvelope.h"
#include "vc2runs.h"
#include "vc2twin.h"
#include <cstdio>
#include <cstring>
#include <iterator>
#include <vector>

namespace vc2 {

namespace {

struct Pair {
    uint32_t vsynth, vc2;   // a routine in rom.2 and its counterpart in rom.4
};
constexpr Pair kCtor{0x894efdb0, 0x893abe40}, kNoteOn{0x894f0130, 0x893ac1c0};          // [FW]
constexpr Pair kTick{0x894efdf0, 0x893abe80}, kNoteOff{0x894f03e0, 0x893ac470};         // [FW]
constexpr Pair kKeyScale{0x894df760, 0x8939a920}, kTime{0x894df7b0, 0x8939a970};        // [FW]
constexpr Pair kPitchDepth{0x894e0ce0, 0x8939c1a0};                                     // [FW]
// [FW] the output routine note-on installs at +0x64 by the curve type, curve 2's and curve 0's: each pair codepairs.py
// finds alike, at the envelope block's distance (0x143f70)
constexpr Twin::Moved kMoved[] = {{0x894efca0, 0x893abd30}, {0x894ef7d0, 0x893ab860}};
constexpr uint32_t kEnv = kScratch, kSource = kScratch + 0x100, kObject = 0x100;
constexpr uint32_t kSyncFields = 0x00124f80;   // [FW] as the converter writes them (xt_env)

struct EnvCase {
    int attack, decay, sustain, release, curve, off, ticks;
};

// Fast to slow segments, sustain at both ends, both output curves, and a release from each stage
constexpr EnvCase kCases[] = {
    {0, 64, 100, 64, 2, 200, 700},
    {64, 64, 64, 64, 2, 300, 1000},
    {127, 127, 127, 127, 2, 600, 2500},
    {10, 90, 0, 30, 0, 150, 600},
    {100, 20, 127, 100, 2, 400, 1500},
    {30, 50, 80, 10, 0, 250, 700},
    {90, 90, 40, 90, 2, 20, 900},
    {0, 0, 127, 0, 2, 5, 60},
    {50, 110, 20, 120, 0, 700, 2400},
    {127, 0, 64, 64, 2, 60, 900},
};

// What rom.4's envelope says against the port, or nullptr when they agree
const char* envAgainstPort(Machine& m, const XtEnvelope& e) {
    const bool same = (int)m.cpu.rd32(kEnv) == e.stage() && (int)m.cpu.rd32(kEnv + 0x44) == e.phase16()
                      && m.f32(kEnv + 0x4c) == e.level() && m.f32(kEnv + 0x54) == e.smooth()
                      && m.f32(kEnv + 0x58) == e.onsetLevel() && m.f32(kEnv + 0x6c) == e.stageTime();
    return same ? nullptr : "stage, phase, output, coefficient, onset or stage time";
}

bool call(Twin& t, Pair p, std::initializer_list<uint32_t> args, float fr4 = 0) {
    return t.call(p.vsynth, p.vc2, args, fr4);
}

bool envCase(Twin& t, const EnvCase& k, long& calls) {
    const XtEnvParams p = xtEnvAdsr(k.attack, k.decay, k.sustain, k.release, k.curve);
    uint32_t words[14] = {};
    words[0] = (uint32_t)p.curve;
    std::memcpy(&words[1], &p.scale, 4);
    words[3] = words[4] = kSyncFields;
    std::memcpy(&words[5], p.time, 16);
    std::memcpy(&words[9], p.level, 20);
    const std::vector<uint8_t> zero(kObject, 0);
    t.write(kEnv, zero.data(), zero.size());
    t.write(kSource, words, sizeof words);
    if (!call(t, kCtor, {kEnv}) || !call(t, kNoteOn, {kEnv, kSource})) return false;
    XtEnvelope port;
    port.noteOn(p);
    for (int tick = 0; tick <= k.ticks; ++tick) {
        if (tick == k.off) {
            if (!call(t, kNoteOff, {kEnv})) return false;
            port.noteOff();
        }
        if (tick > 0) {
            if (!call(t, kTick, {kEnv, 1})) return false;
            port.tick(1);
        }
        ++calls;
        const long at = t.differs(kEnv, kObject, kMoved);
        const char* what = envAgainstPort(t.vc2, port);
        if (at < 0 && !what) continue;
        std::printf("ADSR %d %d %d %d curve %d tick %d: ", k.attack, k.decay, k.sustain, k.release, k.curve, tick);
        if (at >= 0) t.sayDiffers(kEnv, at);
        std::printf("%s\n", what ? what : "the port agrees with rom.4");
        return false;
    }
    return true;
}

struct Returns {
    long cases = 0, differ = 0;
};

// One converter call on both images, its return compared (r0, or fr0 for a float routine): false on a fault
bool converter(Twin& t, Pair p, std::initializer_list<uint32_t> args, float fr4, Returns& r) {
    if (!call(t, p, args, fr4)) return false;
    const bool isFloat = p.vsynth == kPitchDepth.vsynth;
    const uint32_t x = isFloat ? t.vsynth.fb[0][0] : t.vsynth.r[0];
    const uint32_t y = isFloat ? t.vc2.cpu.fb[0][0] : t.vc2.cpu.r[0];
    ++r.cases;
    r.differ += x != y ? 1 : 0;
    return true;
}

// The converters over xt_env conv's ranges: false on a fault
bool converters(Twin& t, Returns& r) {
    for (int key = 0; key < 128; ++key)
        for (int kf = -200; kf <= 200; kf += 10)
            if (!converter(t, kKeyScale, {(uint32_t)key, (uint32_t)kf}, 0, r)) return false;
    constexpr uint32_t kScales[] = {0x10000, 0x8000, 0x4a3d, 0xffff, 0x10001, 0x1a000, 0x28000, 0x40000, 0x1, 0x0};
    for (int knob = 0; knob < 128; ++knob)
        for (int vel = 0; vel < 128; vel += 3)
            for (int sens = -63; sens <= 63; sens += 7)
                for (const uint32_t scale : kScales) {
                    const std::initializer_list<uint32_t> args{(uint32_t)knob, (uint32_t)vel, (uint32_t)sens, scale};
                    if (!converter(t, kTime, args, 0, r)) return false;
                }
    for (int d = -63; d <= 63; ++d)
        if (!converter(t, kPitchDepth, {}, (float)d, r)) return false;
    return true;
}

int envCases(Twin& t) {
    long calls = 0;
    int bad = 0;
    for (const EnvCase& k : kCases) bad += envCase(t, k, calls) ? 0 : 1;
    Returns r;
    if (!converters(t, r)) return -1;
    std::printf("env: %zu cases, %ld states compared, %d cases different; converters: %ld cases, %ld different\n",
                std::size(kCases), calls, bad, r.cases, r.differ);
    return bad + (int)(r.differ != 0);
}

} // namespace

int env(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, xtEnvLoadTables, envCases);
}

} // namespace vc2
