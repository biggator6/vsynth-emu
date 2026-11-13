// vc2run <rom.4> autonote: where the VC-2's kind 6 (and every OSC's) pitch comes from, run on rom.4's own routines
// (research/VC2_VOCAL_DESIGNER.md, "Kind 6's pitch source, run").  The vocoder module's init entries (0x898c609c…
// 0x898c6108), then kind 6's mode as the mic ring's reset (0x893ec7d0) leaves it; then, from the tracker's and the ring's
// resets (0x893ee040, 0x893ec7d0, as the glue 0x893b1680 calls them) and the Auto Note state as .data starts it, known
// tones in 32-sample blocks into the input stage (0x893ebb00(samples, n)) under an Auto Note On Switch (its setter
// 0x893ebc20), with the tone generator's note-on, note-off and keys-held calls (0x89391dc0, 0x89391e20, 0x89391db0)
// answered here and logged.  Says each case's notes and, at the end of each segment, the Auto Note state (0x8993fe3c the
// note, 0x8993fe40 its period, 0x8993fe4c the ratio), the tracked pitch and what the pitch-source getter (0x89394a40)
// gives a converter for that note.  Last, the getter for a note Auto Note didn't start (its +0 −1: 0x893ebc70 with the
// object's +8) across a step in the sung pitch.  Fails on a fault.
#include "vc2track.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kTrackerReset = 0x893ee040, kRingReset = 0x893ec7d0;   // [FW] what the glue calls
constexpr uint32_t kInputStage = 0x893ebb00;   // [FW] (samples, n): the detector if 0x8993fe48, the tracker, Auto Note
constexpr uint32_t kSetAutoNote = 0x893ebc20;  // [FW] Auto Note On Switch's setter, 0…2
constexpr uint32_t kPitchSource = 0x89394a40;  // [FW] (obj): +0 the note, +8 a depth → the ratio a converter takes
constexpr uint32_t kNoteOn = 0x89391dc0, kNoteOff = 0x89391e20, kKeysHeld = 0x89391db0;   // [FW] answered here
constexpr uint32_t kNote = 0x8993fe3c, kNotePeriod = 0x8993fe40, kRelease = 0x8993fe44, kRatio = 0x8993fe4c;
constexpr uint32_t kAutoNote = 0x8993fe50, kDetectorOn = 0x8993fe48;   // [FW]
constexpr uint32_t kKind6Mode = 0x8993fe54, kPitchFlag = 0x8a0f73d8, kC4Period = 0x899d9954;   // [FW]
constexpr uint32_t kIn = kScratch, kObj = kScratch + 0x1000;
constexpr uint32_t kBlock = 32;
constexpr int kHalf = kTrackSecond / 2, kMoment = 10;
constexpr int kC4 = 60;
constexpr double kLoud = 0.5;
constexpr int kAt[] = {0, 5, 20, 100, kHalf - 1};   // blocks after a step the no-note getter is said at

struct Event {
    int block;
    bool on;
    int note;
    float fr4;
};
struct Run {
    std::vector<Event> events;
    uint32_t keysHeld = 0;
    int block = 0;
};

TrackInput tone(double hz) {
    return {TrackKind::tone, hz, kLoud};
}

float fr4Of(const Sh4& c) {
    float f = 0;
    std::memcpy(&f, &c.fb[0][4], 4);
    return f;
}

void hookTones(Machine& m, Run& run) {
    m.cpu.hook(kNoteOn, [&run](Sh4& c) {
        run.events.push_back({run.block, true, (int)c.r[4], fr4Of(c)});
        c.ret(1);
    });
    m.cpu.hook(kNoteOff, [&run](Sh4& c) {
        run.events.push_back({run.block, false, (int)c.r[4], 0});
        c.ret(0);
    });
    m.cpu.hook(kKeysHeld, [&run](Sh4& c) { c.ret(run.keysHeld); });
}

// The resets, and the Auto Note state as .data starts it; false on a fault
bool reset(Machine& m, uint32_t autoNote) {
    m.cpu.wr32(kNote, 0xffffffffu);
    m.setF32(kNotePeriod, 0);
    m.cpu.wr32(kRelease, 0);
    m.setF32(kRatio, 1);
    m.cpu.wr32(kAutoNote, 0);
    if (!m.run(kTrackerReset, {}) || !m.run(kRingReset, {})) return false;
    return m.run(kSetAutoNote, {autoNote});
}

// One segment's blocks into the input stage, `each(block)` after every one; false on a fault or when `each` says so
template <class Each>
bool feed(Machine& m, Run& run, const TrackSegment& s, long& n, Each each) {
    Rng r;
    for (int b = 0; b < s.blocks; ++b) {
        for (uint32_t k = 0; k < kBlock; ++k) {
            m.setF32(kIn + 4 * k, (float)trackSample(s.in, n, r));
            ++n;
        }
        if (!m.run(kInputStage, {kIn, kBlock}) || !each(b)) return false;
        ++run.block;
    }
    return true;
}

bool always(int) {
    return true;
}

// What the getter gives a converter for an object with this note and depth; false on a fault
bool pitchRatioOf(Machine& m, int note, float depth, double& out) {
    m.cpu.wr32(kObj, (uint32_t)note);
    m.cpu.wr32(kObj + 4, 0);
    m.setF32(kObj + 8, depth);
    if (!m.run(kPitchSource, {kObj})) return false;
    float f = 0;
    std::memcpy(&f, &m.cpu.fb[0][0], 4);
    out = f;
    return true;
}

bool sayState(Machine& m, const char* after) {
    const int note = (int32_t)m.cpu.rd32(kNote);
    const double ratio = m.f32(kRatio), c4 = kTrackRate / m.f32(kC4Period);
    double source = 0;
    if (!pitchRatioOf(m, note < 0 ? kC4 : note, 0, source)) return false;
    std::printf("    after %s: note %d, its period %.3f, ratio %.5f (C4 × it %.2f Hz), tracked %.2f Hz, getter %.5f\n",
                after, note, m.f32(kNotePeriod), ratio, c4 * ratio, kTrackRate / m.f32(kTrackPeriod), source);
    return true;
}

struct Case {
    const char* name;
    uint32_t autoNote, keysHeld;
    std::vector<std::pair<const char*, TrackSegment>> segments;
    bool offAtEnd;   // the switch set to 0 after the segments, then a moment more
};

bool sayAutoNoteCase(Machine& m, Run& run, const Case& c) {
    run.events.clear();
    run.block = 0;
    run.keysHeld = c.keysHeld;
    std::printf("  %s:\n", c.name);
    if (!reset(m, c.autoNote)) return false;
    long n = 0;
    for (const auto& [name, s] : c.segments)
        if (!feed(m, run, s, n, always) || !sayState(m, name)) return false;
    if (c.offAtEnd) {
        const TrackSegment moment{{TrackKind::silence, 0, 0}, kMoment};
        if (!m.run(kSetAutoNote, {0}) || !feed(m, run, moment, n, always)) return false;
        if (!sayState(m, "the switch set to 0")) return false;
    }
    if (run.events.empty()) std::printf("    no notes\n");
    for (const Event& e : run.events)
        std::printf("    block %d: note-%s %d (fr4 %g)\n", e.block, e.on ? "on" : "off", e.note, e.fr4);
    return true;
}

std::vector<Case> cases() {
    const TrackSegment silence{{TrackKind::silence, 0, 0}, kHalf};
    const std::vector<std::pair<const char*, TrackSegment>> sung = {
        {"220 Hz", {tone(220), kHalf}}, {"330 Hz", {tone(330), kHalf}}, {"silence", silence}};
    return {
        {"Auto Note 0", 0, 0, sung, false},
        {"Auto Note 1, no key held", 1, 0, sung, false},
        {"Auto Note 1, a key held", 1, 1, sung, false},
        {"Auto Note 2, a key held", 2, 1, sung, false},
        {"Auto Note 2, then switched off", 2, 0, {{"220 Hz", {tone(220), kHalf}}}, true},
    };
}

// The getter for a note Auto Note didn't start, across a step from 220 Hz to 247 Hz; false on a fault
bool noNoteGetter(Machine& m, Run& run, float depth) {
    if (!reset(m, 0)) return false;
    long n = 0;
    double value = 0;
    const auto take = [&](int) { return pitchRatioOf(m, -1, depth, value); };
    if (!feed(m, run, {tone(220), kHalf}, n, take)) return false;
    std::printf("  depth %.2f: at 220 Hz %.5f; after the step to 247 Hz,", depth, value);
    const auto sayAt = [&](int b) {
        if (!pitchRatioOf(m, -1, depth, value)) return false;
        for (const int at : kAt)
            if (b == at) std::printf(" block %d %.5f", b, value);
        return true;
    };
    if (!feed(m, run, {tone(247), kHalf}, n, sayAt)) return false;
    std::printf("\n");
    return true;
}

bool init(Machine& m) {
    if (!initVocoderModule(m)) return false;
    std::printf("after the init: kind 6's mode word %d, pitch flag %d; C4's period %.4f (%.3f Hz); detector word %d\n",
                (int32_t)m.cpu.rd32(kKind6Mode), (int32_t)m.cpu.rd32(kPitchFlag), m.f32(kC4Period),
                kTrackRate / m.f32(kC4Period), (int32_t)m.cpu.rd32(kDetectorOn));
    if (!m.run(kRingReset, {})) return false;
    std::printf("after the ring's reset: mode word %d, pitch flag %d\n", (int32_t)m.cpu.rd32(kKind6Mode),
                (int32_t)m.cpu.rd32(kPitchFlag));
    return true;
}

} // namespace

int autonote(Machine& m, int, char**) {
    Run run;
    bool ok = init(m);
    hookTones(m, run);
    std::printf("\nAuto Note, half a second each of 220 Hz, 330 Hz and silence\n");
    for (const Case& c : cases()) ok = ok && sayAutoNoteCase(m, run, c);
    std::printf("\nthe getter for a note Auto Note didn't start (+0 -1), by its depth (+8)\n");
    for (const float depth : {0.0f, 0.5f, 1.0f}) ok = ok && noNoteGetter(m, run, depth);
    if (ok) return 0;
    std::printf("autonote: faulted: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
