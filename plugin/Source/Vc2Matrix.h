#pragma once
// rom.4's Matrix object M, as far as the COSM converters reach it (v235): its words to +0x166 word for word; the getter
// 0x893a2350, a destination's sum of the slots' terms held to its limit; and the group readers, COSM 1's 0x893a2960 and
// COSM 2's 0x893a2aa0, as one routine over a group's row: when a slot names any of the group's destinations, the
// refresh (0x893a2230, the sources read again: behind Vc2MatrixRefresh, the Matrix core's), then each destination it
// marked summed again, and the group's sums returned.  Bit for bit: xt_vc2cosm's second part against rom.4's own,
// the refresh hooked.  The Matrix core (v243): the refresh itself, Vc2MatrixCore, its sources behind Vc2MatrixSources,
// and every group's row; xt_vc2cosm's third part.  HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame", "The
// VC-2's Matrix core".  The tone generator's thread.
#include "Vc2CosmInput.h"
#include "Vc2MatrixTables.h"
#include <cstdint>
#include <span>

// A slot: two a source, eight sources
struct Vc2MatrixSlot {
    uint8_t dest;        // +0: its destination, 1…78, 0 none
    uint8_t unread;      // +1
    int16_t depth;       // +2: its depth, which the refresh scales its source by
};

struct Vc2MatrixWords {
    static constexpr int kSlots = 16, kGroups = 11;
    uint8_t head[4];               // +0x000…+0x003
    uint8_t sources[8];            // +0x004…+0x00b: each source's byte, which the refresh gives its routine
    uint16_t uses[kGroups];        // +0x00c…+0x021: by group, the destinations a slot names (bit k: its k-th)
    Vc2MatrixSlot slots[kSlots];   // +0x022…+0x061
    uint8_t unread062[2];          // +0x062, +0x063
    uint32_t routines[8];          // +0x064…+0x083: the sources' routines, which the refresh calls
    uint16_t dirty[kGroups];       // +0x084…+0x099: by group, the destinations to sum again (the refresh's)
    int16_t last[8];               // +0x09a…+0x0a9: the sources' last values (the refresh's)
    int16_t terms[kSlots];         // +0x0aa…+0x0c9: each slot's term (the refresh's)
    int16_t sums[Vc2MatrixTables::kDestinations - 1];   // +0x0ca…+0x165: by destination from 1, as last summed
    uint8_t unread166[2];          // +0x166, +0x167; M goes on past them
};
static_assert(sizeof(Vc2MatrixWords) == 0x168, "rom.4's layout, the Matrix object to +0x168");

// A group as its reader takes it: its word in `uses` and `dirty`, its first destination and its destinations' bits
struct Vc2MatrixGroup {
    int index;
    int first;
    uint16_t bits;
};
inline constexpr Vc2MatrixGroup kVc2MatrixCosm1 { 8, 46, 0x7ff }, kVc2MatrixCosm2 { 9, 57, 0x7ff };   // [FW]

// Every group's row, by its reader (v243): OSC 1's 0x893a23b0 and OSC 2's 0x893a2440 (Pitch, Time, Formant, Level),
// 0x893a24d0, 0x893a2550, OSC 1's LFO's 0x893a26b0 and OSC 2's 0x893a28b0 (Rate, Pitch, Time, Formant, Level),
// 0x893a2750, COSM's two (v235) and 0x893a2bf0, destinations 68…78 (its 76…78 the getter's sum inline).  No reader
// takes group 7 (uses +0x1a): destinations 1…78 run on through the others
std::span<const Vc2MatrixGroup> vc2MatrixGroups();

// The refresh the readers call first (0x893a2230): the Matrix core's, which reads the sources again into `terms` and,
// when one moved, sets every group's dirty bits to its uses
class Vc2MatrixRefresh {
public:
    virtual ~Vc2MatrixRefresh() = default;
    virtual void refresh(Vc2MatrixWords& m) = 0;
};

// The sources' routines the refresh calls (v243): each source's routine (M's +0x64…) with M and its byte (+0x4…),
// whose answer's low 16 bits are its value; rom.4's at 0x893a1de0…0x893a21f0, reading M's words past +0x166 (the next
// part's)
class Vc2MatrixSources {
public:
    virtual ~Vc2MatrixSources() = default;
    virtual int32_t value(uint32_t routine, uint8_t byte) = 0;
};

// 0x893a2230, the refresh (v243): each source's value again; one that moved has its two slots' terms made again, its
// depth × the value >> 13 held to ± its destination's limit (0 at a value of 0); when any moved, every group's dirty
// bits are its uses
class Vc2MatrixCore final : public Vc2MatrixRefresh {
public:
    static constexpr int kSources = 8;   // [FW] two slots each
    explicit Vc2MatrixCore(Vc2MatrixSources& sources) : sources_(sources) {}
    void refresh(Vc2MatrixWords& m) override;

private:
    Vc2MatrixSources& sources_;
};

int32_t vc2MatrixSum(const Vc2MatrixWords& m, int dest);   // 0x893a2350
const int16_t* vc2MatrixRead(Vc2MatrixWords& m, const Vc2MatrixGroup& g, Vc2MatrixRefresh& refresh);

// The COSM converters' Matrix sums on M: the group by the input's COSM; the input's word for M names this M
class Vc2CosmMatrixReader final : public Vc2CosmMatrix {
public:
    Vc2CosmMatrixReader(Vc2MatrixWords& m, Vc2MatrixRefresh& refresh) : m_(m), refresh_(refresh) {}
    const int16_t* sums(bool second, uint32_t) override;

private:
    Vc2MatrixWords& m_;
    Vc2MatrixRefresh& refresh_;
};
