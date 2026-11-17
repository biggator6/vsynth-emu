#pragma once
// rom.4's COSM buffer pools (v250), bit for bit: the region 0x89999290…0x8999965b word for word (a float, two pools of
// 24 slots, two floats), its static initializer 0x893ab550 with a pool's set-up 0x893a8480, a slot's taking 0x893a8560
// and freeing 0x893a85e0, and the COSM object's destructor 0x893aa060, which frees its slot.  A slot holds four buffers
// of 3,267 floats (the delay lines of the types that take one, codes 7…10 and 15…18): rom.4's addresses in the slot's
// words, the port's own floats beside them.  The note-on's allocator (Vc2CosmNoteOn) takes and clears them.
// xt_vc2cosm's sixth part.  HARDWARE_FIRMWARE.md, "The VC-2's COSM note-on frame".  The tone generator's.
#include "Vc2CosmObject.h"
#include <array>
#include <cstdint>
#include <vector>

// A slot: in use, and its four buffers' addresses
struct Vc2CosmSlot {
    uint32_t inUse;
    std::array<uint32_t, 4> buffers;
};

// The region, word for word: two pools, pool 0 COSM 1's and pool 1 COSM 2's (an object's +0x140)
struct Vc2CosmPoolWords {
    static constexpr int kSlots = 24, kPools = 2;
    float before;                                         // 0x89999290
    std::array<std::array<Vc2CosmSlot, kSlots>, kPools> pools;   // 0x89999294, 0x89999474
    float after[2];                                       // 0x89999654, 0x89999658
};
static_assert(sizeof(Vc2CosmPoolWords) == 0x3cc, "rom.4's layout, the COSM pools");

class Vc2CosmPools {
public:
    static constexpr int kBufferFloats = 3267;   // [FW] a buffer's floats, what the allocator clears
    // 0x893ab550: the three floats, then each pool's slots free with their buffers' addresses
    void init();
    // 0x893a8560: the first free slot of `pool` taken and its buffers' addresses given, or −1 with none
    int32_t take(uint32_t pool, std::array<uint32_t, 4>& buffers);
    // 0x893a85e0: a slot freed
    void release(uint32_t pool, int32_t slot);
    // The port's floats of a slot's buffer `k`, made when it is first asked for
    std::vector<float>& buffer(uint32_t pool, int32_t slot, int k);

    Vc2CosmPoolWords w {};

private:
    std::array<std::array<std::array<std::vector<float>, 4>, Vc2CosmPoolWords::kSlots>, Vc2CosmPoolWords::kPools> own_;
};

// 0x893aa060: the vtable, the object's slot freed; whether it is deleted too (flag bit 0: operator delete's)
bool vc2CosmDestroy(Vc2CosmObjectWords& o, Vc2CosmPools& pools, uint32_t flags);
