#pragma once
// xt_vc2voices' third part's shared parts (v278): the assigner's sides with the port's keys (v276) beside them, the
// messages and nodes the part node's routines are given, the counts and the events.  XT_VC2VOICES_MAIN only.
#include "Vc2AssignCheck.h"
#include "Vc2CheckMain.h"
#include "Vc2Keys.h"

inline constexpr uint32_t kReleaseMessagesAt = 0x8C030000, kReleaseNodesAt = 0x8C030100;   // in Vc2Fw's RAM
inline constexpr uint32_t kReleaseObjectBytes = 16;
inline constexpr int kReleaseObjects = 4, kReleaseParts = 4, kReleaseTables = 3;

struct ReleaseCounts {
    long events = 0;
    std::array<long, 11> byEvent {};
    long keyOffs = 0, builds = 0, retunes = 0;
};

// The third part's sides: the assigner's, the port's keys, the counts
struct ReleaseSides {
    AssignSides a;
    Vc2Keys keys {};
    ReleaseCounts n {};
};

// rom.4's routine on `args` after the keys' words are written into its memory too, then the port's `mine()`; what
// differs in the voices' span, the calls or the keys' span
std::string releaseOnBoth(ReleaseSides& r, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                          const std::function<void()>& mine);
// A key table of the three the voices are drawn on, a message, a node
uint32_t releaseTable(std::mt19937& rng);
uint32_t releaseMessage(std::mt19937& rng);
uint32_t releaseNode(std::mt19937& rng);

using ReleaseEvent = std::string (*)(ReleaseSides&, std::mt19937&);
std::span<const ReleaseEvent> releaseEvents();
