#pragma once
// rom.4's Matrix object M, as far as the COSM converters reach it (v235): its words to +0x166 word for word; the getter
// 0x893a2350, a destination's sum of the slots' terms held to its limit; and the group readers, COSM 1's 0x893a2960 and
// COSM 2's 0x893a2aa0, as one routine over a group's row: when a slot names any of the group's destinations, the
// refresh (0x893a2230, the sources read again: behind Vc2MatrixRefresh, the Matrix core's), then each destination it
// marked summed again, and the group's sums returned.  Bit for bit: xt_vc2cosm's second part against rom.4's own,
// the refresh hooked.  HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame".  The tone generator's thread.
#include "Vc2CosmInput.h"
#include "Vc2MatrixTables.h"
#include <cstdint>

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

// The refresh the readers call first (0x893a2230): the Matrix core's, which reads the sources again into `terms` and,
// when one moved, sets every group's dirty bits to its uses
class Vc2MatrixRefresh {
public:
    virtual ~Vc2MatrixRefresh() = default;
    virtual void refresh(Vc2MatrixWords& m) = 0;
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
