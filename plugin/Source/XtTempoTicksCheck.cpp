// xt_arp's tempo ticks (XT_ARP_MAIN): XtTempoTicks against the clock object's own routines in the interpreter, ms by ms.
// Each case builds the object (0x89471040), gives it a quarter's ms, a speed and INTERNAL or EXTERNAL, then runs its ms
// routine (0x894710e0) and the task's feed (0x894710b0, whose ticks go to the tempo timer, hooked here), with F8s
// (0x894712f0) and tempo changes between.  The object's counts are compared after each ms, and each feed's ticks.
#ifdef XT_ARP_MAIN
#include "XtArpeggioCheck.h"
#include "XtTempoTicks.h"
#include <cstdio>
#include <random>

namespace {
constexpr uint32_t kClock = 0x89979bfc;                        // [FW 0x894712c4] the clock object
constexpr uint32_t kBuild = 0x89471040, kMs = 0x894710e0, kFeed = 0x894710b0, kF8 = 0x894712f0;
constexpr uint32_t kTempoTimer = 0x8930eb00, kPost = 0x893018b0, kMidiOut = 0x8946f150;
constexpr uint32_t kQuarterMs = 0x14, kSpeed = 0x38, kExternal = 0x40, kSince = 0x46, kSix = 0x48;
constexpr uint32_t kObjBytes = 0x50;
constexpr int kCaseMs = 3000, kSaid = 20;
constexpr uint8_t kClockByte = 0xf8;

struct Field {
    const char* name;
    uint32_t off;
    int bytes;
};
constexpr Field kFields[] = { { "units", 0x1c, 4 }, { "unit fraction", 0x20, 2 }, { "units taken", 0x24, 4 },
                              { "ticks", 0x28, 4 }, { "remainder", 0x2c, 2 }, { "ramp", 0x4a, 2 }, { "fed", 0x34, 4 } };

struct Side {
    xtfw::Machine m;
    int32_t given = 0;                                         // what the feed gave the tempo timer
    explicit Side(const std::string& rom) : m(rom) {
        m.cpu.hook(kTempoTimer, [this](Sh4& c) {
            given = (int32_t)c.r[4];
            c.ret(0);
        });
        m.cpu.hook(kPost, [](Sh4& c) { c.ret(0); });
        m.cpu.hook(kMidiOut, [](Sh4& c) { c.ret(1); });
    }
    uint32_t rd(uint32_t off, int bytes) {
        if (bytes == 2) return m.cpu.rd16(kClock + off);
        return bytes == 1 ? m.cpu.rd8(kClock + off) : m.cpu.rd32(kClock + off);
    }
    bool call(uint32_t addr, std::initializer_list<uint32_t> args) { return m.cpu.call(addr, args, xtfw::kStack); }
};

struct Case {
    Side& fw;
    XtTempoTicks port;
    XtTicksTally& t;
    bool first;
    long ms = 0;
    void differs(const char* what, long fwValue, long portValue) {
        if (t.different++ < kSaid && first)
            std::printf("  ticks: ms %ld %s: firmware %ld, port %ld\n", ms, what, fwValue, portValue);
    }
    void compare() {
        const long port_[] = { port.units, port.unitFrac, port.unitsTaken, (long)port.ticks, port.remainder, port.ramp,
                               (long)port.fed };
        int i = 0;
        for (const Field& f : kFields) {
            const long want = f.bytes == 4 ? (long)(int32_t)fw.rd(f.off, 4) : (long)fw.rd(f.off, f.bytes);
            const long have = f.bytes == 4 ? (long)(int32_t)port_[i] : port_[i];
            ++t.values;
            if (want != have) differs(f.name, want, have);
            ++i;
        }
    }
    bool msStep(bool ext) {
        if (ext) port.msExternal((int)fw.rd(kSix, 2), (int)fw.rd(kSince, 2));
        else port.msInternal((int)fw.rd(kQuarterMs, 4));
        fw.given = 0;
        if (!fw.call(kMs, { kClock }) || !fw.call(kFeed, { kClock })) return false;
        ++t.values;
        const int32_t d = port.take();
        if (fw.given != d) differs("ticks fed", fw.given, d);
        compare();
        ++t.ms;
        return true;
    }
};

int quarterMsOf(int tempo10) { return 3000 * 20 / (tempo10 / 10); }   // [FW 0x894714b0] as XtSynth::clockPeriod
} // namespace

XtTicksTally xtTempoTicksCheck(const std::string& rom, long cases, unsigned seed, bool first) {
    XtTicksTally t;
    Side fw(rom);
    if (!fw.m.ok) {
        t.failed = true;
        return t;
    }
    std::mt19937 r(seed);
    const auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(r); };
    for (long n = 0; n < cases && !t.failed; ++n) {
        for (uint32_t o = 0; o < kObjBytes; o += 4) fw.m.cpu.wr32(kClock + o, 0);
        if (!fw.call(kBuild, { kClock })) t.failed = true;
        Case c{ fw, {}, t, first };
        const bool ext = n % 2 == 1;
        fw.m.cpu.wr32(kClock + kQuarterMs, (uint32_t)quarterMsOf(pick(200, 2500)));
        c.port.speed = pick(0, 3) ? 0x4000 : (uint16_t)pick(0x1000, 0xc000);
        fw.m.cpu.wr16(kClock + kSpeed, c.port.speed);
        fw.m.cpu.wr8(kClock + kExternal, ext ? 1 : 0);
        const double clockMs = 60000.0 / (pick(30, 300) * 24.0);
        double nextF8 = clockMs;
        for (int ms = 0; ms < kCaseMs && !t.failed; ++ms) {
            if (!ext && pick(0, 999) == 0) fw.m.cpu.wr32(kClock + kQuarterMs, (uint32_t)quarterMsOf(pick(200, 2500)));
            const bool held = ext && ms > kCaseMs / 2 && ms < kCaseMs / 2 + 400;   // the clocks stop a while
            if (!t.failed && !c.msStep(ext)) t.failed = true;
            for (; ext && nextF8 <= ms + 1; nextF8 += clockMs + (pick(0, 9) ? 0.0 : pick(-1, 1))) {
                if (held) continue;
                c.port.f8();
                if (!fw.call(kF8, { kClock, kClockByte })) t.failed = true;
                c.compare();
            }
        }
        ++t.cases;
    }
    return t;
}
#endif
