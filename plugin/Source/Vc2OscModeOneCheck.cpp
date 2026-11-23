// xt_vc2osc's mode 1 part (v327): vc2OscModeOne against rom.4's own 0x893c9f60 on voice 0's object, after the hooks'
// install (whose page takes the cached jump's stray write) and before the voice part.  Each case a wave and an events'
// table, then 32 runs of 8 blocks, the object's words drawn before each run as a note gives them, and the records' loop
// events with them, so that the walk goes on through its events and loops, where the voice part draws the words again
// before every block; after each block the output and the voice's own words.  From v328 16 runs more, each from mode
// 1's set-up on both sides (0x893cc760), the voice compared after it; from v329 16 more after those, each from mode 1's
// seek on both sides (0x893cc830, the voice's by its seeks' table), the same.  The draws are the voice part's
// (Vc2OscWalkDraws.cpp), on a generator of the part's own, the renders' put back after.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscPlayers.h"
#include "Vc2OscSides.h"
#include <cstring>

namespace {
constexpr uint32_t kModeOne = 0x893c9f60, kLite = 1;   // [FW] the render; LITE's kind, whose install writes it
// 32 runs a case: at 8, a draw's move left the rarer of the walk's ways to one or two cases at 40, and plants came and
// went with each one; then (v328) 16 from a set-up, which with the beat's start sets the walk's state the steers draw
// (as half of the 32, they left three of v327's plants missed)
constexpr int kRuns = 32, kSetUpRuns = 16, kSeekRuns = 16, kBlocks = 8, kOutWords = 64;
// [FW] the object's flags (0x40 stereo, 0x80 the range in samples), the walk's bits (0x30 in a loop, 0x0c bouncing),
// the last event's number, the range's start and end, the last event's position, the position
constexpr uint32_t kFlags = Vc2OscVoice::kObject, kWalkBits = kFlags + 3, kLast = kFlags + 0xe, kStart = kFlags + 0x18;
constexpr uint32_t kEnd = kFlags + 0x1c, kFinal = kFlags + 0x20, kPos = kFlags + 0x38;
constexpr uint8_t kStereo = 0x40, kInSamples = 0x80, kInLoop = 0x30, kBouncing = 0x0c;

struct ModeOneCounts {
    long blocks = 0, stereo = 0, looping = 0, bouncing = 0, held = 0, setUps = 0, seeks = 0, inSamples = 0;
};

void tallyBlock(const Vc2OscVoice& o, ModeOneCounts& n) {
    ++n.blocks;
    n.stereo += (o.u8(kFlags) & kStereo) != 0;
    n.looping += (o.u8(kWalkBits) & kInLoop) != 0;
    n.bouncing += (o.u8(kWalkBits) & kBouncing) != 0;
    n.held += o.u32(kPos) == o.u32(kStart) || o.u32(kPos) == o.u32(kEnd);
}

// One block on both sides, the output noise first
std::string modeOneBlock(OscSides& s, std::mt19937& rng, ModeOneCounts& n) {
    uint32_t noise[kOutWords];
    for (uint32_t& w : noise) w = (uint32_t)rng();
    float out[kOutWords];
    std::memcpy(out, noise, sizeof out);
    s.fw.cpu.write(kOscOutAt, noise, sizeof noise);
    Vc2OscVoice& o = s.voices[0];
    const uint32_t own = Vc2OscVoice::kOwn, ownBytes = Vc2OscVoice::kOwnEnd - Vc2OscVoice::kOwn;
    std::string what = vc2BothOn(
        s.fw, kModeOne, { kOscOutAt, oscVoiceAt(0) + Vc2OscVoice::kObject }, "mode 1's render",
        [&] { vc2OscModeOne(o, out, s.parts[0]); },
        [&] {
            return vc2RegionsDifference(s.fw, { { kOscOutAt, out, sizeof out, "the output" },
                                                { oscVoiceAt(0) + own, o.at(own), ownBytes, "the voice" } });
        });
    tallyBlock(o, n);
    return what;
}

// Voice 0's own words against rom.4's
std::string firstVoiceDifference(OscSides& s) {
    const uint32_t own = Vc2OscVoice::kOwn, ownBytes = Vc2OscVoice::kOwnEnd - Vc2OscVoice::kOwn;
    return vc2RegionsDifference(s.fw, { { oscVoiceAt(0) + own, s.voices[0].at(own), ownBytes, "the voice" } });
}

// v328: the set-up on both sides (0x893cc760 against vc2OscModeOneSetUp), on words as an install finds them, then the
// voice compared
std::string modeOneSetUp(OscSides& s, ModeOneCounts& n) {
    constexpr uint32_t kSetUp = 0x893cc760;   // [FW]
    ++n.setUps;
    return vc2BothOn(
        s.fw, kSetUp, { oscVoiceAt(0) + Vc2OscVoice::kObject }, "mode 1's set-up",
        [&] { vc2OscModeOneSetUp(s.voices[0], s.parts[0], s.romShared); }, [&] { return firstVoiceDifference(s); });
}

// v329: a seek's word by a way drawn from eight.  With the range in samples: about an event's position half the time,
// on one a quarter, below 0 or past the last event's an eighth each (the hold's two ends).  By events: a number up to
// two past the table's last, one time in eight 0x8000 up (the hold is unsigned), one in four with high bits besides,
// which the seek drops
struct SeekDraw {
    std::span<const int32_t> positions;   // the events' (v333: a table's or a band wave's)
    int32_t event;                        // an event's position
    uint32_t number;                      // an event's number
    std::mt19937& rng;
};
using SeekWay = int32_t (*)(SeekDraw&);
int32_t belowZero(SeekDraw& d) { return -(int32_t)(d.rng() % 4096); }
int32_t pastTheLast(SeekDraw& d) { return d.positions.back() + (int32_t)(d.rng() % 4096); }
int32_t onAnEvent(SeekDraw& d) { return d.event; }
int32_t nearEvent(SeekDraw& d) { return d.event + (int32_t)(d.rng() % 1024) - 512; }
int32_t topBit(SeekDraw& d) { return (int32_t)(0x8000u | d.number); }
int32_t highBits(SeekDraw& d) { return (int32_t)((d.rng() & 0xffff0000u) | d.number); }
int32_t plainNumber(SeekDraw& d) { return (int32_t)d.number; }
constexpr std::array<SeekWay, 8> kSampleWays { belowZero, pastTheLast, onAnEvent, onAnEvent,
                                               nearEvent, nearEvent,   nearEvent, nearEvent };
constexpr std::array<SeekWay, 8> kEventWays { topBit,      highBits,    highBits,    plainNumber,
                                              plainNumber, plainNumber, plainNumber, plainNumber };

} // namespace

int32_t oscSeekWord(bool inSamples, std::span<const int32_t> positions, int count, std::mt19937& rng) {
    SeekDraw d { positions, positions[rng() % positions.size()], rng() % (uint32_t)(count + 2), rng };
    const auto& ways = inSamples ? kSampleWays : kEventWays;
    return ways[rng() % ways.size()](d);
}

namespace {

// v329: the seek on both sides (0x893cc830 against the voice's own, its beat's reset with it), then the voice compared.
// One time in four the last event's position (+0x20) past the event +0xe names: on the words the start leaves the
// seek's hold stops the find's walk forward one before the last event, where its own stop there is.  Only past an
// event 0: from it the stop leaves the event −1, whose entry the jump then reads far past the table
std::string seekOnBothSides(OscSides& s, std::mt19937& rng, ModeOneCounts& n) {
    constexpr uint32_t kSeek = 0x893cc830;   // [FW]
    Vc2OscVoice& o = s.voices[0];
    if (o.u16(kLast) > 0 && rng() % 4 == 0) {
        o.setU32(kFinal, o.u32(kFinal) + 1 + rng() % 4096);
        oscWriteVoice(s, 0);
    }
    const OscEventTable& t = s.shared.records.events;
    const int32_t at = oscSeekWord((o.u8(kFlags) & kInSamples) != 0, t.positions, t.count, rng);
    ++n.seeks;
    n.inSamples += (o.u8(kFlags) & kInSamples) != 0;
    return vc2BothOn(
        s.fw, kSeek, { oscVoiceAt(0) + Vc2OscVoice::kObject, (uint32_t)at }, "mode 1's seek",
        [&] { o.seekBy(kSeek, at, s.parts[0]); }, [&] { return firstVoiceDifference(s); });
}

std::string modeOneCase(OscSides& s, std::mt19937& rng, ModeOneCounts& n) {
    s.shared.renders = std::mt19937((uint32_t)rng());
    oscDrawWave(s.fw, s.shared);   // and the events' table
    Vc2OscVoice& o = s.voices[0];
    o.setU32(Vc2OscVoice::kKind, kLite);
    std::string what;
    for (int run = 0; run < kRuns + kSetUpRuns + kSeekRuns && what.empty(); ++run) {
        oscDrawLoopEvents(s.shared.records, s.shared.renders);
        oscWriteRecords(s.fw, s.shared.records);
        oscDrawModeOneWords(o, rng, s.shared.records, kOscWaveAt);
        const bool setUp = run >= kRuns && run < kRuns + kSetUpRuns, seek = run >= kRuns + kSetUpRuns;
        if (setUp) oscSteerModeOneSetUp(o, s.shared, oscVoiceAt(0));
        oscWriteVoice(s, 0);
        if (setUp) what = modeOneSetUp(s, n);
        if (seek) what = seekOnBothSides(s, rng, n);
        for (int b = 0; b < kBlocks && what.empty(); ++b) what = modeOneBlock(s, rng, n);
    }
    return what;
}
} // namespace

void oscModeOnePart(OscSides& s, const Vc2Run& run, Vc2Tally& tally) {
    const std::mt19937 renders = s.shared.renders;
    ModeOneCounts n;
    vc2RunCases(run, tally, "", n.blocks, [&](std::mt19937& rng, bool) { return modeOneCase(s, rng, n); });
    s.shared.renders = renders;
    std::printf("mode 1: %d cases, %ld blocks (%ld stereo, %ld in a loop, %ld bouncing, %ld held at the range's end), "
                "%ld set-ups, %ld seeks (%ld in samples)\n",
                run.cases, n.blocks, n.stereo, n.looping, n.bouncing, n.held, n.setUps, n.seeks, n.inSamples);
}
#endif
