#pragma once
// The pitch tracker run's shared parts (vc2track.cpp): a known signal fed to rom.4's tracker in 32-sample blocks from a
// reset into a mode, and what it read after each block.  vc2trackstate.cpp reads the tracker's state after the same feed.
#include "vc2runs.h"
#include <vector>

namespace vc2 {

inline constexpr uint32_t kTrackPeriod = 0x899d9bd0;   // [FW] the tracked period, samples (what 0x893ee010 returns)
inline constexpr uint32_t kTrackFirmwareMode = 0;      // [FW] the reset's r4 (0x893ee04a), the mode setter's only call
inline constexpr int kTrackSecond = 1400;              // blocks of 32: about a second
inline constexpr double kTrackRate = 44100;

enum class TrackKind { tone, noise, silence };
struct TrackInput {
    TrackKind kind;
    double hz, level;
};
struct TrackSegment {
    TrackInput in;
    int blocks;
};
struct TrackReadings {
    std::vector<double> hz, ret;   // after each block: 44100 / the period, and the block entry's return
    bool ok = false;
};

// The tracker reset into `mode`, then each segment's blocks in turn; ok is false on a fault
TrackReadings trackReadings(Machine& m, uint32_t mode, const std::vector<TrackSegment>& signal);
// The tracker's settings, then its state after a few feeds (vc2trackstate.cpp); false on a fault
bool trackState(Machine& m);

} // namespace vc2
