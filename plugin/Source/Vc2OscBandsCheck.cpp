// xt_vc2osc's band part (v332): the band players' renders, BACKING's 0x893cadc0 and ENSEMBLE's 0x893ca1f0, through the
// voice's own render against rom.4's own on voice 0, before any hook.  Each case a band wave (Vc2OscBandDraws.cpp) in
// kOscBandAt's room, then notes: the object's words as VariPhrase's note-on leaves them, the player's set-up on both
// sides (v333: BACKING's 0x893cd1d0 or ENSEMBLE's 0x893cc8f0 against vc2OscBandSetUp, the voices compared), then runs
// of blocks, before each run the words a note's updates, settings and clock move drawn again on both sides (the pitch,
// the walk, the gain's target, byte 0's loops, Vari off and wrap, byte 2, the beat's clocks and steps) and one time in
// three (v333) a seek on both sides (0x893cd440, its word about the wave's events by mode 1's ways); after each block
// the output and the voices' words.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscPlayers.h"
#include "Vc2OscSides.h"
#include <cmath>
#include <memory>

namespace {
constexpr int kNotes = 3, kRuns = 8, kBlocks = 8;
// [FW] the object's words (the voice's offsets): byte 0 (its loops, wrap, range in samples and Vari off bits), byte 2,
// byte 3, the end, the current event, the clocks and steps, the pitch, the walk, the gain's target
constexpr uint32_t kFlagsAt = 0x15c, kEventBitsAt = 0x15e, kWalkBitsAt = 0x15f, kEndedAt = 0x160, kEventAt = 0x168;
constexpr uint32_t kClocksAt = 0x19c, kStepsAt = 0x19d, kPitchAt = 0x1cc, kWalkAt = 0x1d8, kTargetAt = 0x1dc;
constexpr uint8_t kLoopsOn = 0x01, kBeat = 0x06, kWraps = 0x08, kVariOff = 0x20, kInSamples = 0x80;

struct BandCounts {
    long notes = 0, ensembles = 0, stereo = 0, blocks = 0, crossed = 0, ended = 0, wraps = 0, variOff = 0;
    long setUpsInSamples = 0, seeks = 0, seeksInSamples = 0;   // v333
};

void flipped(Vc2OscVoice& v, std::mt19937& rng, uint8_t bit, uint32_t in) {
    if (rng() % in == 0) v.setU8(kFlagsAt, v.u8(kFlagsAt) ^ bit);
}

float movedWalk(std::mt19937& rng) {
    if (rng() % 8 == 0) return 0.0f;
    return (rng() % 2 ? 1.0f : -1.0f) * oscWithin(rng, 0.05f, 1.5f);
}

// The words a note's updates, settings and clock move, drawn again before a run
void bandMoves(Vc2OscVoice& v, bool ensemble, std::mt19937& rng) {
    if (rng() % 2 == 0) v.setF32(kPitchAt, std::exp2(oscWithin(rng, -2.0f, 2.0f)));
    if (rng() % 2 == 0) v.setF32(kWalkAt, movedWalk(rng));
    if (rng() % 4 == 0) v.setF32(kTargetAt, oscWithin(rng, 0.0f, 2.0f) / 65536.0f);
    if (!ensemble) flipped(v, rng, kLoopsOn, 4);   // ENSEMBLE's off (oscBandNote)
    if ((v.u8(kFlagsAt) & kBeat) == 0) flipped(v, rng, kVariOff, 8);   // never under the beat (bandFlags)
    flipped(v, rng, kWraps, 8);
    flipped(v, rng, kInSamples, 8);
    if (rng() % 8 == 0) v.setU8(kEventBitsAt, (uint8_t)(rng() & 7));
    v.setU8(kClocksAt, (uint8_t)(v.u8(kClocksAt) + rng() % 3));
    if (rng() % 4 == 0) v.setU8(kStepsAt, (uint8_t)rng());
}

std::string bandBlock(OscSides& s, BandCounts& n, uint32_t routine, std::mt19937& rng) {
    for (float& x : s.shared.out) x = oscDrawFloat(rng);
    s.fw.cpu.write(kOscOutAt, s.shared.out.data(), sizeof s.shared.out);
    Vc2OscVoice& v = s.voices[0];
    const uint16_t event = v.u16(kEventAt);
    const uint8_t walkBits = v.u8(kWalkBitsAt);
    const std::string what = oscBothSides(s, routine, { kOscOutAt, oscVoiceAt(0) + Vc2OscVoice::kObject },
                                          OscAnswer::none, [&] {
                                              v.renderOwn(routine, s.shared.out.data(), s.parts[0]);
                                              return 0u;
                                          });
    if (!what.empty()) return what;
    ++n.blocks;
    n.crossed += v.u16(kEventAt) != event || v.u8(kWalkBitsAt) != walkBits;
    n.ended += v.u8(kEndedAt) != 0;
    n.wraps += (v.u8(kFlagsAt) & (kWraps | kInSamples)) == kWraps;
    n.variOff += (v.u8(kFlagsAt) & kVariOff) != 0;
    return vc2FirstDifference(s.fw, kOscOutAt, s.shared.out.data(), sizeof s.shared.out, "the output");
}

// v333: noise in the words the set-up writes (the end, the table, the layout's bytes, the cached events, the gain's
// rate, the loops' rates, every band's words, the tree's state), finite where floats, so that a write left out shows;
// band 0's smoother, which it leaves, is read as a float too.  But band 0's shift and stride, which neither set-up
// writes (nor rom.2's note-ons, the same code), as the note leaves them: noise there, a render reads far from the wave
void noiseUnderSetUp(Vc2OscVoice& v, std::mt19937& rng) {
    constexpr uint32_t kObject = Vc2OscVoice::kObject;
    constexpr uint32_t kBandsFrom = 0xec, kBandsTo = 0x41c, kBandStride = 0x88;   // [FW]
    constexpr std::array<uint32_t, 3> kBandFloats { 0x40, 0x44, 0x48 };            // [FW] the gain, the smoother's two
    // [FW] bytes 4…7 (the end, the event's two flag bytes, which its jump writes, the events' first), the table, cached
    constexpr std::array<uint32_t, 3> kWords { 0x4, 0x10, 0x6c };
    constexpr std::array<uint32_t, 4> kFloats { 0x88, 0xa0, 0xa4, 0xa8 };           // [FW] the gain's rate, the rates
    constexpr std::array<std::pair<uint32_t, uint32_t>, 2> kTrees { { { 0x51c, 0x5e4 }, { 0x6e4, 0x7ac } } };
    constexpr std::array<uint32_t, 2> kLeft { 0x4, 0x8 };   // [FW] band 0's shift and stride
    for (uint32_t at = kBandsFrom; at < kBandsTo; at += 4) {
        const bool left = at - kBandsFrom == kLeft[0] || at - kBandsFrom == kLeft[1];
        if (!left) v.setU32(kObject + at, (uint32_t)rng());
    }
    for (uint32_t k = 0; k < kBandsTo - kBandsFrom; k += kBandStride) {
        for (uint32_t f : kBandFloats) v.setF32(kObject + kBandsFrom + k + f, oscWithin(rng, -2.0f, 2.0f));
    }
    for (uint32_t at : kWords) v.setU32(kObject + at, (uint32_t)rng());
    v.setU8(kObject + 8, (uint8_t)rng());   // [FW] the stride's byte; byte 9 the frame's
    v.setU8(kObject + 9, (uint8_t)rng());
    for (uint32_t at : kFloats) v.setF32(kObject + at, oscWithin(rng, -2.0f, 2.0f));
    for (const auto& [from, to] : kTrees) {
        for (uint32_t at = from; at < to; at += 4) v.setF32(kObject + at, oscWithin(rng, -1.0f, 1.0f));
    }
}

// v333: the player's set-up on both sides, on the words a note-on leaves
std::string setUpBoth(OscSides& s, BandCounts& n, bool ensemble) {
    Vc2OscVoice& v = s.voices[0];
    n.setUpsInSamples += (v.u8(kFlagsAt) & kInSamples) != 0;
    return oscBothSides(s, kOscBandSetUps[ensemble ? 1 : 0], { oscVoiceAt(0) + Vc2OscVoice::kObject },
                        OscAnswer::none, [&] {
                            vc2OscBandSetUp(v, s.parts[0], s.romShared, ensemble);
                            return 0u;
                        });
}

// v333: a seek on both sides (0x893cd440 against the voice's own, its beat's reset with it), its word about the wave's
// events
std::string seekBoth(OscSides& s, BandCounts& n, const OscBandWave& w, std::mt19937& rng) {
    Vc2OscVoice& v = s.voices[0];
    const bool inSamples = (v.u8(kFlagsAt) & kInSamples) != 0;
    const int32_t at = oscSeekWord(inSamples, w.positions, w.events, rng);
    ++n.seeks;
    n.seeksInSamples += inSamples;
    return oscBothSides(s, kOscBandSeek, { oscVoiceAt(0) + Vc2OscVoice::kObject, (uint32_t)at }, OscAnswer::none,
                        [&] {
                            v.seekBy(kOscBandSeek, at, s.parts[0]);
                            return 0u;
                        });
}

std::string bandPartNote(OscSides& s, BandCounts& n, const OscBandWave& w, std::mt19937& rng) {
    const bool ensemble = rng() % 2 == 0;
    Vc2OscVoice& v = s.voices[0];
    oscBandNote(v, w, oscVoiceAt(0), ensemble, rng);
    noiseUnderSetUp(v, rng);
    oscWriteVoice(s, 0);
    std::string what = setUpBoth(s, n, ensemble);
    ++n.notes;
    n.ensembles += ensemble;
    n.stereo += w.stereo;
    for (int run = 0; run < kRuns && what.empty(); ++run) {
        bandMoves(v, ensemble, rng);
        oscWriteVoice(s, 0);
        if (rng() % 3 == 0) what = seekBoth(s, n, w, rng);
        for (int k = 0; k < kBlocks && what.empty(); ++k) what = bandBlock(s, n, kOscBandRenders[ensemble], rng);
    }
    return what;
}

std::string bandCase(OscSides& s, BandCounts& n, std::mt19937& rng) {
    const OscBandWave w = oscDrawBandWave(s.fw, s.shared, kOscBandAt, kOscBandBytes, rng);
    std::string what;
    for (int k = 0; k < kNotes && what.empty(); ++k) what = bandPartNote(s, n, w, rng);
    return what;
}
} // namespace

void oscBandPart(OscSides& s, const Vc2Run& run, Vc2Tally& tally) {
    BandCounts n;
    for (int v = 0; v < kOscVoices; ++v) oscWriteVoice(s, v);
    vc2RunCases(run, tally, "", n.blocks, [&](std::mt19937& rng, bool) { return bandCase(s, n, rng); });
    std::printf("bands: %d cases, %ld notes (%ld ENSEMBLE's, %ld stereo, %ld set up in samples), %ld seeks (%ld in "
                "samples), %ld blocks (%ld crossing an event or a loop, %ld past an end, %ld with the wrap on, %ld "
                "under Vari off)\n", run.cases, n.notes, n.ensembles, n.stereo, n.setUpsInSamples, n.seeks,
                n.seeksInSamples, n.blocks, n.crossed, n.ended, n.wraps, n.variOff);
}
#endif
