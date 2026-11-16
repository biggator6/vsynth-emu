// xt_vc2tracker <rom.4> [cases] [seed] (v223): the VC-2's pitch tracker's frame, Vc2Tracker, against rom.4's own on its
// state in rom.4's .bss (0x899d9b70…0x899d9d1c): its two static initializers (0x893edf90, 0x893ef240) once, then each
// case the reset (0x893ee040) on both, now and then the words the analysis writes (the period, the fallback, the lock,
// the recent peak, the noise count, the noisy-hold flag and its switch, the band-pass's targets) drawn on both, and
// blocks of Vc2Signal's input (tones, noise and silence, from full scale to 1e-5) through the block entry (0x893ee280),
// a block mostly 32 samples and now and then fewer; between blocks now and then the mode setter (0x893ee100, −1…4) or
// the noisy-hold setter (0x893ee030).  rom.4's crossing routines (0x893ee570, 0x893ee7d0, v224's) are hooked to record
// their calls and return, and the port's crossings record theirs.  After each routine the state word for word, after a
// block the period returned, the noisy-hold getter (0x893ee020) and the crossings' calls.  XT_VC2TRACKER_FIRST=1 says
// where the first case to differ does, XT_VC2TRACKER_STOP=1 stops there.  XT_VC2TRACKER_MAIN only.
#ifdef XT_VC2TRACKER_MAIN
#include "Vc2CheckRig.h"
#include "Vc2Tracker.h"
#include <bit>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kStatic = 0x893edf90, kSetter = 0x893ef240, kReset = 0x893ee040, kMode = 0x893ee100;   // [FW]
constexpr uint32_t kBlockEntry = 0x893ee280, kGetNoisy = 0x893ee020, kSetNoisy = 0x893ee030;              // [FW]
constexpr uint32_t kUp = 0x893ee570, kDown = 0x893ee7d0, kStateAt = 0x899d9b70;                            // [FW]
constexpr uint32_t kInputAt = 0x8C035000;   // in Vc2Fw's RAM
constexpr int kBlocks = 600, kEventOneIn = 40, kScrambleOneIn = 150;

// A crossing as either side saw it: its direction and the two samples it lies between
struct Crossing {
    bool up;
    uint32_t previous, now;
    bool operator==(const Crossing&) const = default;
};

// The port's crossings, recorded as rom.4's hooks record its own
class Recorder final : public Vc2TrackerCrossings {
public:
    void up(Vc2TrackerWords&, float previous, float now) override { record(true, previous, now); }
    void down(Vc2TrackerWords&, float previous, float now) override { record(false, previous, now); }
    std::vector<Crossing> calls;

private:
    void record(bool up, float previous, float now) {
        calls.push_back({ up, std::bit_cast<uint32_t>(previous), std::bit_cast<uint32_t>(now) });
    }
};

struct Counts {
    long blocks = 0, ups = 0, downs = 0, modes = 0, holds = 0, noisy = 0;
};

// Both sides: rom.4 in its interpreter with its crossings' log, the port with its recorder, and what has been counted
struct Sides {
    Vc2Fw& fw;
    Vc2Tracker& tracker;
    Recorder& port;
    std::vector<Crossing>& rom;
    Counts n;
};

std::string trackerDifference(Vc2Fw& fw, const Vc2Tracker& t) {
    return vc2FirstDifference(fw, kStateAt, &t.s, sizeof t.s, "the tracker's words");
}

// The words the analysis writes, drawn on the port and copied to rom.4's: so the frame's branches on them are reached,
// and what the reset and the mode setter clear is seen cleared
void scrambleTracker(Sides& b, std::mt19937& rng) {
    Vc2TrackerWords& s = b.tracker.s;
    for (float& w : s.crossing) w = vc2Unit(rng);
    for (float& w : s.halves) w = 100.0f * vc2Unit(rng);
    for (int32_t& c : s.counts) c = (int32_t)(rng() % 100);
    s.estimate = s.analysisBdc = s.analysisCbc = s.analysisCf8 = vc2Unit(rng);
    s.smoothing = 0.5f;
    s.lastUp = s.handOff = (int32_t)(rng() % 2);
    s.follow = (int32_t)(rng() % 3);
    s.failures = s.lastSince = (int32_t)(rng() % 300);
    s.sinceCrossing = (int32_t)(rng() % 400);   // as a valid crossing leaves it: P stays apart from the fallback a while
    s.period = rng() % 4 == 0 ? s.fallback : 30.0f + 600.0f * vc2Unit(rng);
    s.fallback = rng() % 2 == 0 ? s.period : 30.0f + 600.0f * vc2Unit(rng);
    s.lock = rng() % 2 == 0 ? 0 : (int32_t)(rng() % 257);
    s.recentPeak = 0.05f * vc2Unit(rng);
    s.noise = (int32_t)(rng() % 40);
    s.noisyHold = (int32_t)(rng() % 2);
    s.noisyHoldOn = rng() % 4 == 0 ? 0 : 1;
    s.lowpassTarget = 0.12f * vc2Unit(rng);
    s.highpassTarget = 0.06f * vc2Unit(rng);
    b.fw.cpu.write(kStateAt, &s, sizeof s);
}

// One block of n samples on both, then the state, the return, the getter and the crossings
std::string trackerBlockOn(Sides& b, const float* x, int n) {
    b.fw.cpu.write(kInputAt, x, sizeof(float) * (size_t)n);
    b.rom.clear();
    b.port.calls.clear();
    float returned = 0.0f;
    std::string what = vc2BothOn(b.fw, kBlockEntry, { kInputAt, (uint32_t)n }, "the block entry",
                                 [&] { returned = b.tracker.block(x, n); },
                                 [&] { return trackerDifference(b.fw, b.tracker); });
    ++b.n.blocks;
    for (const Crossing& c : b.port.calls) ++(c.up ? b.n.ups : b.n.downs);
    if (what.empty() && !vc2SameAsFr0(b.fw, returned)) what = "the period returned";
    if (what.empty() && b.rom != b.port.calls) what = "the crossings' calls";
    uint32_t noisy = 0;
    if (what.empty() && !b.fw.call(kGetNoisy, {}, &noisy)) what = "the getter faults: " + b.fw.cpu.error();
    if (what.empty() && (int32_t)noisy != b.tracker.noisyHold()) what = "the noisy-hold getter";
    b.n.noisy += b.tracker.noisyHold() != 0 ? 1 : 0;
    return what;
}

// An event between blocks: the mode setter (−1…4) or the noisy-hold setter, on both
std::string trackerEventOn(Sides& b, std::mt19937& rng) {
    Vc2Tracker& t = b.tracker;
    const auto differs = [&] { return trackerDifference(b.fw, t); };
    if (rng() % 2 == 0) {
        ++b.n.modes;
        const int mode = (int)(rng() % 6) - 1;
        return vc2BothOn(b.fw, kMode, { (uint32_t)mode }, "the mode setter", [&] { t.setMode(mode); }, differs);
    }
    ++b.n.holds;
    const int32_t on = (int32_t)(rng() % 2);
    return vc2BothOn(b.fw, kSetNoisy, { (uint32_t)on }, "the noisy-hold setter", [&] { t.setNoisyHold(on); },
                     differs);
}

// The case's blocks after its reset, each with what may come before it
std::string trackerCase(Sides& b, std::mt19937& rng, bool fades) {
    Vc2Signal input { std::mt19937(rng()), fades };
    std::string what = vc2ResetOn(b.fw, kReset, {}, b.tracker, trackerDifference);
    if (what.empty() && rng() % 2 == 0) scrambleTracker(b, rng);
    float x[kVc2Block];
    for (int k = 0; k < kBlocks && what.empty(); ++k) {
        if (rng() % kScrambleOneIn == 0) scrambleTracker(b, rng);
        if (rng() % kEventOneIn == 0) what = trackerEventOn(b, rng);
        input.block(x);
        const int samples = rng() % 8 == 0 ? 1 + (int)(rng() % kVc2Block) : kVc2Block;
        if (what.empty()) what = trackerBlockOn(b, x, samples);
    }
    return what;
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2tracker", vc2LoadTracker);
    if (!run) return 1;
    std::vector<Crossing> rom;
    for (const auto& [at, up] : { std::pair { kUp, true }, std::pair { kDown, false } }) {
        fw.cpu.hook(at, [&rom, up = up](Sh4& cpu) {
            rom.push_back({ up, cpu.fb[0][4], cpu.fb[0][5] });
            cpu.ret(0);
        });
    }
    Recorder port;
    Vc2Tracker tracker(port);
    Sides b { fw, tracker, port, rom, {} };
    std::string what;
    if (!fw.call(kStatic, {}) || !fw.call(kSetter, {})) what = "the static initializers fault: " + fw.cpu.error();
    if (what.empty()) what = trackerDifference(fw, tracker);
    if (!what.empty()) {
        std::printf("  after the static initializers: %s\nxt_vc2tracker: 1 different\n", what.c_str());
        return 0;
    }
    std::mt19937 rng((uint32_t)run->seed);
    Vc2Tally tally { "XT_VC2TRACKER_FIRST", "XT_VC2TRACKER_STOP" };
    for (int c = 0; c < run->cases; ++c) {
        if (vc2CaseOutcome(tally, trackerCase(b, rng, c % 8 == 7), c, b.n.blocks)) break;
    }
    std::printf("tracker: %d cases, %ld blocks, %ld up and %ld down crossings, %ld modes, %ld holds, %ld noisy, "
                "%ld different\n", run->cases, b.n.blocks, b.n.ups, b.n.downs, b.n.modes, b.n.holds, b.n.noisy,
                tally.different);
    std::printf("xt_vc2tracker: %ld different\n", tally.different);
    return 0;
}
#endif
