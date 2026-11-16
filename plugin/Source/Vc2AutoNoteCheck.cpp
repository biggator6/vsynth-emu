// xt_vc2autonote <rom.4> [cases] [seed] (v226): the VC-2's Auto Note and the pitch source, Vc2AutoNote with the pitch
// tracker inside, against rom.4's own routines: the tracker's static initializers and Auto Note's init (0x893ebe50) once,
// then each case the tracker's reset (0x893ee040) on both and Auto Note's words drawn (the detector's switch left 0, as
// rom.4 leaves it), and blocks of the mic (Vc2TrackerSignal) through the input stage (0x893ebb00), mostly 32 samples;
// before each the tone generator's object (0x89989748: Auto Note's key, the other keys, the depth) drawn, now and then
// an event (the On Switch's setter 0x893ebc20, −1…3; the Legato setter 0x893ebc60; the release request 0x893ebcf0), and
// after each the pitch-source getter (0x89394a40) on the object.  rom.4's note-on and note-off (0x89391dc0, 0x89391e20)
// are hooked to log their calls and answer as drawn; the port's host logs and answers the same.  After each routine
// Auto Note's words, the bend's globals and the tracker's state word for word, the calls, and the getter's return.
// XT_VC2AUTONOTE_FIRST=1 says where the first case to differ does, XT_VC2AUTONOTE_STOP=1 stops there.
// XT_VC2AUTONOTE_MAIN only.
#ifdef XT_VC2AUTONOTE_MAIN
#include "Vc2AutoNote.h"
#include "Vc2AutoNoteTables.h"
#include "Vc2CheckRig.h"
#include "Vc2TrackerAnalysis.h"
#include "Vc2TrackerSignal.h"
#include "Vc2TrackerTables.h"
#include <bit>
#include <cstdio>

namespace {
// [FW] rom.4's routines
constexpr uint32_t kInit = 0x893ebe50;           // Auto Note's init, the init table's
constexpr uint32_t kStage = 0x893ebb00;          // the input stage
constexpr uint32_t kSetSwitch = 0x893ebc20;      // the On Switch's setter
constexpr uint32_t kSetLegato = 0x893ebc60;      // the Legato Switch's setter: the detector's switch off
constexpr uint32_t kAskRelease = 0x893ebcf0;     // the release request, the tone generator's
constexpr uint32_t kGetter = 0x89394a40;         // the pitch-source getter, the converters'
constexpr uint32_t kNoteOn = 0x89391dc0;         // the tone generator's mailbox: a note-on
constexpr uint32_t kNoteOff = 0x89391e20;        // and a note-off
constexpr uint32_t kTrackerStatic = 0x893edf90;  // the tracker's two static initializers and its reset
constexpr uint32_t kTrackerSetter = 0x893ef240;
constexpr uint32_t kTrackerReset = 0x893ee040;
// [FW] their state
constexpr uint32_t kWordsAt = 0x8993fe3c;        // Auto Note's, .data
constexpr uint32_t kGlobalsAt = 0x899d994c;      // the bend's, .bss
constexpr uint32_t kTrackerAt = 0x899d9b70;      // the tracker's
constexpr uint32_t kSourceAt = 0x89989748;       // the tone generator's pitch source
constexpr uint32_t kInputAt = 0x8C035000;   // in Vc2Fw's RAM
constexpr int kBlocks = 600, kEventOneIn = 30, kScrambleOneIn = 200;

// A call to the tone generator as either side made it: on or off, the note, the level's bits
struct Call {
    bool on;
    int32_t note;
    uint32_t level;
    bool operator==(const Call&) const = default;
};

// The port's tone generator: its calls logged, a note-on answered as drawn
class Host final : public Vc2AutoNoteHost {
public:
    bool noteOn(int32_t note, float level) override {
        calls.push_back({ true, note, std::bit_cast<uint32_t>(level) });
        return posts;
    }
    void noteOff(int32_t note) override { calls.push_back({ false, note, 0 }); }
    std::vector<Call> calls;
    bool posts = true;
};

struct Counts {
    long blocks = 0, posted = 0, refused = 0, offs = 0, bent = 0, ratios = 0, switches = 0, releases = 0;
};

// Both sides: rom.4 in its interpreter with its calls' log, the port's tracker and Auto Note with its host
struct Sides {
    Vc2Fw& fw;
    Vc2Tracker& tracker;
    Vc2AutoNote& autoNote;
    Host& host;
    std::vector<Call>& rom;
    Vc2PitchSource source {};
    Counts n;
};

std::string autoNoteDifference(Sides& b) {
    const Vc2AutoNote& a = b.autoNote;
    std::string what = vc2RegionsDifference(b.fw, { { kWordsAt, &a.w, sizeof a.w, "Auto Note's words" },
                                                    { kGlobalsAt, &a.g, sizeof a.g, "the globals" },
                                                    { kTrackerAt, &b.tracker.s, sizeof b.tracker.s, "the tracker" } });
    if (what.empty() && b.rom != b.host.calls) what = "the tone generator's calls";
    return what;
}

// Auto Note's words as a part leaves them: a note sounding or none, a release asked for or not, any switch
void scrambleAutoNote(Sides& b, std::mt19937& rng) {
    Vc2AutoNoteWords& w = b.autoNote.w;
    const bool sounding = rng() % 2 == 0;
    w.note = sounding ? Vc2AutoNote::kC4 : -1;
    w.period = sounding ? (rng() % 4 == 0 ? 50.0f + 450.0f * vc2Unit(rng) : b.autoNote.g.c4Period) : 0.0f;
    w.release = (int32_t)(rng() % 4 == 0);
    w.ratio = rng() % 2 == 0 ? 1.0f : 0.5f + 1.5f * vc2Unit(rng);
    w.onSwitch = (int32_t)(rng() % 3);
    b.fw.cpu.write(kWordsAt, &w, sizeof w);
}

// The tone generator's object: Auto Note's key or none, other keys now and then, a depth or none
void drawSource(Sides& b, std::mt19937& rng) {
    Vc2PitchSource& s = b.source;
    s.note = rng() % 2 == 0 ? -1 : (int32_t)(rng() % 128);
    s.others = rng() % 3 == 0 ? (int32_t)(1 + rng() % 3) : 0;
    s.depth = rng() % 3 == 0 ? 0.0f : vc2Unit(rng);
    b.fw.cpu.write(kSourceAt, &s, sizeof s);
}

// An event between blocks: the On Switch's setter, the Legato setter, or the release request
std::string autoNoteEventOn(Sides& b, std::mt19937& rng) {
    const auto differs = [&] { return autoNoteDifference(b); };
    const uint32_t pick = rng() % 4;
    if (pick < 2) {
        ++b.n.switches;
        const int32_t on = (int32_t)(rng() % 5) - 1;
        return vc2BothOn(
            b.fw, kSetSwitch, { (uint32_t)on }, "the On Switch's setter",
            [&] { b.autoNote.setSwitch(on); }, differs);
    }
    if (pick == 2) {
        ++b.n.releases;
        return vc2BothOn(
            b.fw, kAskRelease, {}, "the release request",
            [&] { b.autoNote.askRelease(); }, differs);
    }
    const uint32_t given = rng() % 2;
    return vc2BothOn(
        b.fw, kSetLegato, { given }, "the Legato setter",
        [&] { b.autoNote.setLegato(); }, differs);
}

// What a block's calls were: note-ons posted and refused, note-offs
void countCalls(Sides& b) {
    for (const Call& c : b.host.calls) {
        b.n.offs += c.on ? 0 : 1;
        b.n.posted += c.on && b.host.posts ? 1 : 0;
        b.n.refused += c.on && !b.host.posts ? 1 : 0;
    }
    b.n.ratios += b.source.note >= 0 ? 1 : 0;
    b.n.bent += b.source.note < 0 && b.source.depth != 0.0f ? 1 : 0;
}

// A block on both, then the getter
std::string autoNoteBlockOn(Sides& b, const float* x, int samples, std::mt19937& rng) {
    ++b.n.blocks;
    b.host.posts = rng() % 4 != 0;
    b.rom.clear();
    b.host.calls.clear();
    b.fw.cpu.write(kInputAt, x, sizeof(float) * (size_t)samples);
    std::string what = vc2BothOn(
        b.fw, kStage, { kInputAt, (uint32_t)samples }, "the input stage",
        [&] { b.autoNote.block(b.tracker, x, samples, b.source, b.host); },
        [&] { return autoNoteDifference(b); });
    countCalls(b);
    float ratio = 0.0f;
    if (what.empty()) {
        what = vc2BothOn(
            b.fw, kGetter, { kSourceAt }, "the getter",
            [&] { ratio = b.autoNote.ratioFor(b.tracker, b.source); },
            [&] { return autoNoteDifference(b); });
    }
    if (what.empty() && !vc2SameAsFr0(b.fw, ratio)) what = "the getter's ratio";
    return what;
}

// The case: the tracker reset, Auto Note's words drawn, then its blocks with what may come before each
std::string autoNoteCase(Sides& b, std::mt19937& rng, bool fades) {
    std::string what = vc2ResetOn(b.fw, kTrackerReset, {}, b.tracker,
                                  [&](Vc2Fw&, Vc2Tracker&) { return autoNoteDifference(b); });
    scrambleAutoNote(b, rng);
    Vc2TrackerSignal input(rng(), fades);
    float x[kVc2Block];
    for (int k = 0; k < kBlocks && what.empty(); ++k) {
        if (rng() % kScrambleOneIn == 0) scrambleAutoNote(b, rng);
        drawSource(b, rng);
        if (rng() % kEventOneIn == 0) what = autoNoteEventOn(b, rng);
        input.block(x);
        const int samples = rng() % 8 == 0 ? 1 + (int)(rng() % kVc2Block) : kVc2Block;
        if (what.empty()) what = autoNoteBlockOn(b, x, samples, rng);
    }
    return what;
}

bool bothLoaded(const char* path) { return vc2LoadTracker(path) && vc2LoadAutoNote(path); }
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2autonote", bothLoaded);
    if (!run) return 1;
    Vc2TrackerAnalysis analysis;
    Vc2Tracker tracker(analysis);
    Vc2AutoNote autoNote;
    Host host;
    std::vector<Call> rom;
    Sides b { fw, tracker, autoNote, host, rom, {}, {} };
    fw.cpu.hook(kNoteOn, [&](Sh4& cpu) {
        rom.push_back({ true, (int32_t)cpu.r[4], cpu.fb[0][4] });
        cpu.ret(host.posts ? 1 : 0);
    });
    fw.cpu.hook(kNoteOff, [&](Sh4& cpu) {
        rom.push_back({ false, (int32_t)cpu.r[4], 0 });
        cpu.ret(0);
    });
    std::string what;
    if (!fw.call(kTrackerStatic, {}) || !fw.call(kTrackerSetter, {}) || !fw.call(kInit, {})) {
        what = "the initializers fault: " + fw.cpu.error();
    }
    autoNote.init();
    if (what.empty()) what = autoNoteDifference(b);
    Vc2Tally tally { "XT_VC2AUTONOTE_FIRST", "XT_VC2AUTONOTE_STOP" };
    vc2RunCases(*run, tally, what, b.n.blocks,
                [&](std::mt19937& rng, bool fades) { return autoNoteCase(b, rng, fades); });
    const Counts& n = b.n;
    std::printf("auto note: %d cases, %ld blocks, %ld note-ons posted, %ld refused, %ld note-offs, %ld getter ratios, "
                "%ld bends, %ld switches, %ld releases, %ld different\n", run->cases, n.blocks, n.posted, n.refused,
                n.offs, n.ratios, n.bent, n.switches, n.releases, tally.different);
    std::printf("xt_vc2autonote: %ld different\n", tally.different);
    return 0;
}
#endif
