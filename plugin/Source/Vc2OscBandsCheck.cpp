// xt_vc2osc's band part (v332): the band players' renders, BACKING's 0x893cadc0 and ENSEMBLE's 0x893ca1f0, through the
// voice's own render against rom.4's own on voice 0, before any hook.  Each case a band wave (Vc2OscBandDraws.cpp) in
// kOscBandAt's room, then notes: the object's words as VariPhrase's note-on leaves them, rom.4's own set-up of the
// player (not ported yet: its words go to the port's voice as they are), then runs of blocks, before each run the
// words a note's updates, settings and clock move drawn again on both sides (the pitch, the walk, the gain's target,
// byte 0's loops, Vari off and wrap, byte 2, the beat's clocks and steps); after each block the output and the voices'
// words.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
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

std::string bandPartNote(OscSides& s, BandCounts& n, const OscBandWave& w, std::mt19937& rng) {
    const bool ensemble = rng() % 2 == 0;
    Vc2OscVoice& v = s.voices[0];
    oscBandNote(v, w, oscVoiceAt(0), ensemble, rng);
    oscWriteVoice(s, 0);
    std::string what = oscBandSetUp(s.fw, v, oscVoiceAt(0), ensemble);
    ++n.notes;
    n.ensembles += ensemble;
    n.stereo += w.stereo;
    for (int run = 0; run < kRuns && what.empty(); ++run) {
        bandMoves(v, ensemble, rng);
        oscWriteVoice(s, 0);
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
    std::printf("bands: %d cases, %ld notes (%ld ENSEMBLE's, %ld stereo), %ld blocks (%ld crossing an event or a "
                "loop, %ld past an end, %ld with the wrap on, %ld under Vari off)\n", run.cases, n.notes, n.ensembles,
                n.stereo, n.blocks, n.crossed, n.ended, n.wraps, n.variOff);
}
#endif
