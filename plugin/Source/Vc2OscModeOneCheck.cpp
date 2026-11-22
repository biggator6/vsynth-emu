// xt_vc2osc's mode 1 part (v327): vc2OscModeOne against rom.4's own 0x893c9f60 on voice 0's object, after the hooks'
// install (whose page takes the cached jump's stray write) and before the voice part.  Each case a wave and an events'
// table, then 32 runs of 8 blocks, the object's words drawn before each run as a note gives them, and the records' loop
// events with them, so that the walk goes on through its events and loops, where the voice part draws the words again
// before every block; after each block the output and the voice's own words.  From v328 16 runs more, each from mode
// 1's set-up on both sides (0x893cc760), the voice compared after it.  The draws are the voice part's
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
constexpr int kRuns = 32, kSetUpRuns = 16, kBlocks = 8, kOutWords = 64;
// [FW] the object's flags (0x40 stereo), the walk's bits (0x30 in a loop, 0x0c bouncing), the range's start and end,
// the position
constexpr uint32_t kFlags = Vc2OscVoice::kObject, kWalkBits = kFlags + 3, kStart = kFlags + 0x18;
constexpr uint32_t kEnd = kFlags + 0x1c, kPos = kFlags + 0x38;
constexpr uint8_t kStereo = 0x40, kInLoop = 0x30, kBouncing = 0x0c;

struct ModeOneCounts {
    long blocks = 0, stereo = 0, looping = 0, bouncing = 0, held = 0, setUps = 0;
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

// v328: the set-up on both sides (0x893cc760 against vc2OscModeOneSetUp), on words as an install finds them, then the
// voice compared
std::string modeOneSetUp(OscSides& s, ModeOneCounts& n) {
    constexpr uint32_t kSetUp = 0x893cc760;   // [FW]
    Vc2OscVoice& o = s.voices[0];
    const uint32_t own = Vc2OscVoice::kOwn, ownBytes = Vc2OscVoice::kOwnEnd - Vc2OscVoice::kOwn;
    ++n.setUps;
    return vc2BothOn(
        s.fw, kSetUp, { oscVoiceAt(0) + Vc2OscVoice::kObject }, "mode 1's set-up",
        [&] { vc2OscModeOneSetUp(o, s.parts[0], s.romShared); },
        [&] { return vc2RegionsDifference(s.fw, { { oscVoiceAt(0) + own, o.at(own), ownBytes, "the voice" } }); });
}

std::string modeOneCase(OscSides& s, std::mt19937& rng, ModeOneCounts& n) {
    s.shared.renders = std::mt19937((uint32_t)rng());
    oscDrawWave(s.fw, s.shared);   // and the events' table
    Vc2OscVoice& o = s.voices[0];
    o.setU32(Vc2OscVoice::kKind, kLite);
    std::string what;
    for (int run = 0; run < kRuns + kSetUpRuns && what.empty(); ++run) {
        oscDrawLoopEvents(s.shared.records, s.shared.renders);
        oscWriteRecords(s.fw, s.shared.records);
        oscDrawModeOneWords(o, rng, s.shared.records, kOscWaveAt);
        const bool setUp = run >= kRuns;
        if (setUp) oscSteerModeOneSetUp(o, s.shared, oscVoiceAt(0));
        oscWriteVoice(s, 0);
        if (setUp) what = modeOneSetUp(s, n);
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
                "%ld set-ups\n", run.cases, n.blocks, n.stereo, n.looping, n.bouncing, n.held, n.setUps);
}
#endif
