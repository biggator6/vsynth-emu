#pragma once
// xt_vc2keys' shared parts (v276): the two sides (rom.4's interpreter, the OS clock hooked to answer the ms an event
// draws, and the port's keys), the counts and the events.  XT_VC2KEYS_MAIN only.
#include "Vc2CheckMain.h"
#include "Vc2Keys.h"
#include <functional>
#include <span>

inline constexpr uint32_t kKeysClockAt = 0x893018d0;   // [FW] the OS clock, in ms
inline constexpr int kKeysEvents = 20;
inline constexpr int32_t kKeysClockMost = 1 << 30;     // the clocks drawn, below it

struct KeysCounts {
    long events = 0;
    std::array<long, kKeysEvents> byEvent {};
    long saturated = 0, released = 0, latched = 0, found = 0, glidesFound = 0;
};

// The two sides: rom.4's interpreter with its clock hooked, the port's keys, the clock's next answer, the table and key
// whose counts were drawn last (an event takes them half the time) and the counts
struct KeysSides {
    explicit KeysSides(Vc2Fw& rig);
    Vc2Fw& fw;
    Vc2Keys port {};
    int32_t ms = 0;
    uint32_t drawnTable = Vc2Keys::kTablesAt, drawnKey = 0;
    KeysCounts n {};
};

// An event on both sides: what differs, or nothing; the routines' events, and the three initializers' (rarer, as each
// sets every table or glide up)
using KeysEvent = std::function<std::string(KeysSides&, std::mt19937&)>;
std::span<const KeysEvent> keysEvents();
std::span<const KeysEvent> keysInits();
// rom.4's routine on `args` after the port's words are written into its memory, then the port's `mine()` giving its
// answer; then the span's words, and the answer when the routine answers
std::string keysOnBoth(KeysSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                       const std::function<uint32_t()>& mine);
// A key table of the 66 and a glide of the 129, by address
uint32_t keysAnyTable(std::mt19937& rng);
// The second part (v293, Vc2PartNodesCheck.cpp): the part's nodes, on the seed after the run's; how many cases differ
long keysNodesPart(KeysSides& keys, const Vc2Run& run);
uint32_t keysAnyGlide(std::mt19937& rng);
