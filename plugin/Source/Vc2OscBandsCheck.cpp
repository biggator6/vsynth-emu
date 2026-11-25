// xt_vc2osc's band part (v332): the band players' renders, BACKING's 0x893cadc0 and ENSEMBLE's 0x893ca1f0, through the
// voice's own render against rom.4's own on voice 0, before any hook.  Each case a band wave (Vc2OscBandDraws.cpp) in
// kOscBandAt's room, then notes: the object's words as VariPhrase's note-on leaves them, the player's set-up on both
// sides (v333: BACKING's 0x893cd1d0 or ENSEMBLE's 0x893cc8f0 against vc2OscBandSetUp, the voices compared), then runs
// of blocks, before each run the words a note's updates, settings and clock move drawn again on both sides (the pitch,
// the walk, the gain's target, byte 0's loops, Vari off and wrap, byte 2, the beat's clocks and steps) and one time in
// three (v333) a seek on both sides (0x893cd440, its word about the wave's events by mode 1's ways); after each block
// the output and the voices' words.  v334: SOLO's part the same on SOLO's waves in kOscSoloAt's room (types 1 and 6,
// set-ups 0x893cc2e0 and 0x893cc310 against vc2OscSoloSetUp, its render 0x893c9450, its seek 0x893cc630), its runs
// moving its formant, Energy and Robot Voice too.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscPlayers.h"
#include "Vc2OscSides.h"
#include <cmath>
#include <memory>

namespace {
constexpr int kNotes = 3, kRuns = 8, kBlocks = 8;
// [FW] the object's words (the voice's offsets): byte 0 (its loops, wrap, range in samples and Vari off bits), byte 2,
// byte 3, the end, the current event, the clocks and steps, the pitch, the walk, the gain's target; v334: SOLO's
// formant, Energy and Robot Voice's depth
constexpr uint32_t kFlagsAt = 0x15c, kEventBitsAt = 0x15e, kWalkBitsAt = 0x15f, kEndedAt = 0x160, kEventAt = 0x168;
constexpr uint32_t kClocksAt = 0x19c, kStepsAt = 0x19d, kPitchAt = 0x1cc, kWalkAt = 0x1d8, kTargetAt = 0x1dc;
constexpr uint32_t kFormantAt = 0x1d0, kEnergyAt = 0x1d4, kDepthAt = 0x1f0;
constexpr uint8_t kLoopsOn = 0x01, kBeat = 0x06, kWraps = 0x08, kRobot = 0x10, kVariOff = 0x20, kInSamples = 0x80;
constexpr uint32_t kObject = Vc2OscVoice::kObject;

constexpr uint32_t kEnsemble = 5, kSix = 6, kJump = 8;   // [FW] ENSEMBLE's, SOLO's type 6, the period-jump player's

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

// The words a note's updates, settings and clock move, drawn again before a run, by the note's type; v334: SOLO's
// formant, Energy (0 now and then), Robot Voice and its depth besides.  ENSEMBLE's loops stay off; v335: the period-jump
// player's range's way (in samples or by events) the note's, its pitch held to 4 (oscBandNote)
void bandMoves(Vc2OscVoice& v, uint32_t type, std::mt19937& rng) {
    if (rng() % 2 == 0) v.setF32(kPitchAt, std::exp2(oscWithin(rng, -2.0f, 2.0f)));
    if (rng() % 2 == 0) v.setF32(kWalkAt, movedWalk(rng));
    if (rng() % 4 == 0) v.setF32(kTargetAt, oscWithin(rng, 0.0f, 2.0f) / 65536.0f);
    if (type != kEnsemble) flipped(v, rng, kLoopsOn, 4);
    if ((v.u8(kFlagsAt) & kBeat) == 0) flipped(v, rng, kVariOff, 8);   // never under the beat (bandFlags)
    flipped(v, rng, kWraps, 8);
    if (type != kJump) flipped(v, rng, kInSamples, 8);
    if (rng() % 8 == 0) v.setU8(kEventBitsAt, (uint8_t)(rng() & 7));
    v.setU8(kClocksAt, (uint8_t)(v.u8(kClocksAt) + rng() % 3));
    if (rng() % 4 == 0) v.setU8(kStepsAt, (uint8_t)rng());
    if (!(type == 1 || type == kSix)) return;   // [FW] SOLO's types
    // v334: one run in eight a pitch of 4…8, where the grain parameters' cutoff and the filter's coefficient meet
    // their caps
    if (rng() % 8 == 0) v.setF32(kPitchAt, std::exp2(oscWithin(rng, 2.0f, 3.0f)));
    if (rng() % 2 == 0) v.setF32(kFormantAt, std::exp2(oscWithin(rng, -1.0f, 1.0f)));
    if (rng() % 2 == 0) v.setF32(kEnergyAt, rng() % 4 == 0 ? 0.0f : oscWithin(rng, 0.0f, 1.5f));
    flipped(v, rng, kRobot, 4);
    if (rng() % 4 == 0) v.setF32(kDepthAt, oscWithin(rng, 0.0f, 1.0f));
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

// Noise in the head's words a set-up writes: bytes 4…7 (the end, the event's two flag bytes, which its jump writes,
// the events' first), the table, the cached events, the stride's and the frame's bytes; then `floats` finite
void noiseInHead(Vc2OscVoice& v, std::span<const uint32_t> floats, std::mt19937& rng) {
    constexpr std::array<uint32_t, 3> kWords { 0x4, 0x10, 0x6c };   // [FW]
    for (uint32_t at : kWords) v.setU32(kObject + at, (uint32_t)rng());
    v.setU8(kObject + 8, (uint8_t)rng());   // [FW] the stride's byte; byte 9 the frame's
    v.setU8(kObject + 9, (uint8_t)rng());
    for (uint32_t at : floats) v.setF32(kObject + at, oscWithin(rng, -2.0f, 2.0f));
}

// v333: noise in the words the set-up writes (the end, the table, the layout's bytes, the cached events, the gain's
// rate, the loops' rates, every band's words, the tree's state), finite where floats, so that a write left out shows;
// band 0's smoother, which it leaves, is read as a float too.  But band 0's shift and stride, which neither set-up
// writes (nor rom.2's note-ons, the same code), as the note leaves them: noise there, a render reads far from the wave
void noiseUnderSetUp(Vc2OscVoice& v, std::mt19937& rng) {
    constexpr uint32_t kBandsFrom = 0xec, kBandsTo = 0x41c, kBandStride = 0x88;   // [FW]
    constexpr std::array<uint32_t, 3> kBandFloats { 0x40, 0x44, 0x48 };            // [FW] the gain, the smoother's two
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
    noiseInHead(v, kFloats, rng);
    for (const auto& [from, to] : kTrees) {
        for (uint32_t at = from; at < to; at += 4) v.setF32(kObject + at, oscWithin(rng, -1.0f, 1.0f));
    }
}
// v334: SOLO's: noise in its record block and grains (+0xec…+0x190) and the head's words, the gain's rate, the rate
// and the loops' rates finite; every one of them its set-up writes before it reads it
void noiseUnderSoloSetUp(Vc2OscVoice& v, std::mt19937& rng) {
    constexpr uint32_t kFrom = 0xec, kTo = 0x190;                                        // [FW]
    constexpr std::array<uint32_t, 5> kFloats { 0x88, 0x9c, 0xa0, 0xa4, 0xa8 };           // [FW]
    for (uint32_t at = kFrom; at < kTo; at += 4) v.setU32(kObject + at, (uint32_t)rng());
    noiseInHead(v, kFloats, rng);
}

// v334: SOLO's range in samples ending where the forward loop ends (its cached end's position), where its set-up takes
// the last sample from the last event's
void soloLoopAtEnd(OscSides& s, Vc2OscVoice& v, const OscBandWave& w) {
    constexpr uint32_t kFwdEndPos = 0xb4 + 0x4, kEndAt = 0x178;   // [FW] the cached events'; the voice's range end
    if (!w.looped || (v.u8(kFlagsAt) & kInSamples) == 0) return;
    int32_t end = 0;
    std::memcpy(&end, reinterpret_cast<const uint8_t*>(s.shared.wave.data()) + (w.cached + kFwdEndPos - kOscWaveAt),
                sizeof end);
    v.setU32(kEndAt, (uint32_t)end);
}

// v333: the player's set-up on both sides, on the words a note-on leaves; v334: by the wave's type
std::string setUpBoth(OscSides& s, BandCounts& n, uint32_t type) {
    Vc2OscVoice& v = s.voices[0];
    n.setUpsInSamples += (v.u8(kFlagsAt) & kInSamples) != 0;
    const OscPlayerRow& row = *oscPlayerOf(type);
    return oscBothSides(s, row.setUp, { oscVoiceAt(0) + Vc2OscVoice::kObject }, OscAnswer::none, [&] {
        if (row.family == OscFamily::jump) vc2OscJumpSetUp(v, s.parts[0], s.romShared);
        else if (row.family == OscFamily::solo) vc2OscSoloSetUp(v, s.parts[0], s.romShared, type == kSix);
        else vc2OscBandSetUp(v, s.parts[0], s.romShared, type == kEnsemble);
        return 0u;
    });
}

// v333: a seek on both sides (0x893cd440 against the voice's own, its beat's reset with it), its word about the wave's
// events; v334: SOLO's 0x893cc630 on its waves; v335: the type's (the period-jump player's 0x893cd090)
std::string seekBoth(OscSides& s, BandCounts& n, const OscBandWave& w, uint32_t type, std::mt19937& rng) {
    Vc2OscVoice& v = s.voices[0];
    const bool inSamples = (v.u8(kFlagsAt) & kInSamples) != 0;
    int32_t at = oscSeekWord(inSamples, w.positions, w.events, rng);
    if (type == kJump) {
        // v335: within the note's range (the object's +0x14…+0x1c): a position outside it is held at its end with the
        // records left where they were, and the period-jump player's glide takes its read out of the wave
        const int32_t from = inSamples ? (int32_t)v.u32(kObject + 0x18) : v.u16(kObject + 0x14);
        const int32_t to = inSamples ? (int32_t)v.u32(kObject + 0x1c) : v.u16(kObject + 0x16);
        at = std::clamp(at, std::min(from, to), std::max(from, to));
    }
    const uint32_t routine = oscPlayerOf(type)->seek;
    ++n.seeks;
    n.seeksInSamples += inSamples;
    return oscBothSides(s, routine, { oscVoiceAt(0) + Vc2OscVoice::kObject, (uint32_t)at }, OscAnswer::none, [&] {
        v.seekBy(routine, at, s.parts[0]);
        return 0u;
    });
}

// A note of one of the part's types: BACKING's or ENSEMBLE's, SOLO's two, or (v335, on a wave of periods) the
// period-jump player's
std::string bandPartNote(OscSides& s, BandCounts& n, const OscBandWave& w, std::mt19937& rng) {
    constexpr std::array<uint32_t, 2> kBandTypes { 3, 5 }, kSoloTypes { 1, 6 };   // [FW]
    const bool second = rng() % 2 == 0;
    const uint32_t type = w.periods ? kJump : (w.solo ? kSoloTypes : kBandTypes)[second ? 1 : 0];
    Vc2OscVoice& v = s.voices[0];
    oscBandNote(v, w, oscVoiceAt(0), type, rng);
    if (w.solo && rng() % 8 == 0) soloLoopAtEnd(s, v, w);
    if (w.solo) noiseUnderSoloSetUp(v, rng);
    else noiseUnderSetUp(v, rng);
    oscWriteVoice(s, 0);
    std::string what = setUpBoth(s, n, type);
    ++n.notes;
    n.ensembles += type == kEnsemble || type == kSix;
    n.stereo += w.stereo;
    const uint32_t routine = oscPlayerOf(type)->render;
    for (int run = 0; run < kRuns && what.empty(); ++run) {
        bandMoves(v, type, rng);
        oscWriteVoice(s, 0);
        if (rng() % 3 == 0) what = seekBoth(s, n, w, type, rng);
        for (int k = 0; k < kBlocks && what.empty(); ++k) what = bandBlock(s, n, routine, rng);
    }
    return what;
}

// A case: a wave of the family's in its room (v335: by the family, the period-jump player's waves of periods), notes
std::string bandCase(OscSides& s, BandCounts& n, OscFamily family, std::mt19937& rng) {
    const OscBandWave w = oscDrawBandWave(s.fw, s.shared, oscFamilyRoom(family), kOscSoloBytes, rng,
                                          oscFamilyRecords(family));
    std::string what;
    for (int k = 0; k < kNotes && what.empty(); ++k) what = bandPartNote(s, n, w, rng);
    return what;
}

// The players' part: the band players', SOLO's or (v335) the period-jump player's
void playersPart(OscSides& s, const Vc2Run& run, Vc2Tally& tally, OscFamily family) {
    BandCounts n;
    for (int v = 0; v < kOscVoices; ++v) oscWriteVoice(s, v);
    vc2RunCases(run, tally, "", n.blocks, [&](std::mt19937& rng, bool) { return bandCase(s, n, family, rng); });
    constexpr std::array<const char*, 3> kNames { "bands", "solo", "jump" }, kSeconds { " ENSEMBLE's, ", " type 6's, ",
                                                                                      "" };
    const size_t f = (size_t)family;
    const std::string second = family == OscFamily::jump ? "" : std::to_string(n.ensembles) + kSeconds[f];
    std::printf("%s: %d cases, %ld notes (%s%ld stereo, %ld set up in samples), %ld seeks (%ld in samples), %ld "
                "blocks (%ld crossing an event or a loop, %ld past an end, %ld with the wrap on, %ld under Vari off)\n",
                kNames[f], run.cases, n.notes, second.c_str(), n.stereo, n.setUpsInSamples, n.seeks, n.seeksInSamples,
                n.blocks, n.crossed, n.ended, n.wraps, n.variOff);
}
} // namespace

void oscPlayersPart(OscSides& s, const Vc2Run& run, Vc2Tally& tally, OscFamily family) {
    playersPart(s, run, tally, family);
}
#endif
