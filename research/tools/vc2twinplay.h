#pragma once
// The VariPhrase players side by side (vc2run's `band` and `solo`, vc2twinband.cpp and vc2twinsolo.cpp): what both runs
// share.  A player is set up in rom.2's machine as XtFirmware's xtFwBandPlay and xtFwSoloPlay set it up, its object copied
// into rom.4's layout, and each image's routines run on it: the note-on, then block by block, the 32 frames, the object
// and its cached loop events compared after each.  The wave's data (samples, bands, vpdt, the wave struct) is at one
// address in both machines, written to both.
#include "vc2twin.h"
#include "XtLoop.h"
#include <bit>
#include <span>
#include <utility>
#include <vector>

namespace vc2 {

// Where a player run keeps things, in both images' harness RAM (as xtFwBandPlay and xtFwSoloPlay place them)
inline constexpr uint32_t kPlayer = kRam, kPlayerBytes = 0x300;
inline constexpr uint32_t kWaveStruct = kRam + 0x2000, kCache = kRam + 0x3000, kCacheBytes = 4 * 0xb4;
inline constexpr uint32_t kVpdt = kRam + 0x10000, kSamples = kScratch;
inline constexpr uint32_t kBlockOut = 0x8CF00000, kBlockFrames = 32, kBlockOutBytes = kBlockFrames * 8;
// The fields a note's moves write, below the insert in both images
inline constexpr uint32_t kWalk = 0x7c, kGain = 0x80;
static_assert(kGain + 4 <= kOscInsert.at);
// The OSC object's layout in rom.4 (vc2twin.h's kOscInsert), the band players' as it stands
inline constexpr Twin::Segment kOscMap[] = {
    {0, 0, kOscInsert.at}, {kOscInsert.at, kOscInsert.at + kOscInsert.bytes, kPlayerBytes - kOscInsert.at}};

// A player as a run calls it: its block routine pair, its object's layout from rom.2's into rom.4's, its name
struct PlayerRun {
    Twin::Pair block;
    std::span<const Twin::Segment> map;
    const char* name;
};

// A test wave: a tone of a few harmonics, struck one to three times, mono or stereo int16
struct TestWave {
    std::vector<int16_t> pcm;
    long frames = 0;
    int channels = 1;
};
TestWave makeTestWave(Rng& r);

// A note's start: the pitch ratio, the walk, and its loop (on one note in three, points and modes at random)
struct TestNote {
    float pitch = 1, walk = 1;
    XtLoop loop;
};
TestNote makeTestNote(Rng& r, const TestWave& w);

// What a note changes as it plays: the walk and the output gain target (1 = unity) from a given block
struct NoteMoves {
    std::vector<std::pair<int, float>> walkAt, gainAt;
};
NoteMoves makeMoves(Rng& r, int blocks);

// A field a run sets in rom.2's player before it is copied into rom.4's: its offset, its width in bytes and its bits
struct SetField {
    uint32_t offset, bytes, bits;
};
constexpr SetField u8At(uint32_t offset, uint32_t v) {
    return {offset, 1, v};
}
constexpr SetField u16At(uint32_t offset, uint32_t v) {
    return {offset, 2, v};
}
constexpr SetField u32At(uint32_t offset, uint32_t v) {
    return {offset, 4, v};
}
constexpr SetField f32At(uint32_t offset, float v) {
    return {offset, 4, std::bit_cast<uint32_t>(v)};
}
// The fields into rom.2's player (little-endian, as the SH-4 here)
void setFields(Twin& t, std::span<const SetField> fields);

// The images after a call on the player: its output (when `withOut`), the object and its cache compared (not r0, where
// the players leave a code address or an offset), counted in `tally`, the first difference said under `what`
void tallyCall(Twin& t, const PlayerRun& p, bool withOut, const char* what, Twin::Returns& tally);
// The note's blocks on both images through the block routine pair, each tallied; false on a fault
bool playBlocks(Twin& t, const PlayerRun& p, int blocks, const NoteMoves& moves, Twin::Returns& tally);

} // namespace vc2
