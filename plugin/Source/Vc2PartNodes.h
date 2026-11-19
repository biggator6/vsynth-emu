#pragma once
// The VC-2 part's nodes (v291): rom.4's 64 nodes of 16 bytes from 0x899888d0 word for word, one a source (1…4) and
// part, 0x100 bytes a source; what the mode row (0x89396e20) reads of them, the channel each receives on.  Their
// routines (the init 0x89393370, the channel's move 0x89393100, the object's swaps 0x893932c0 and 0x89393310) are
// item 9's later part.  Not in the sound yet.  HARDWARE_FIRMWARE.md, "The VC-2 part's settings by ID: the mode row".
#include <array>
#include <cstdint>

// [FW] a node, as rom.4 lays it
struct Vc2PartNode {
    uint32_t next = 0;      // +0x0: the next node on its channel's list (rom.4's address, 0 the end)
    uint32_t object = 0;    // +0x4: its object (rom.4's address)
    uint32_t word8 = 0;     // +0x8: given to 0x89390f70 and 0x89390fd0
    int8_t source = 0;      // +0xc: its source less 1
    int8_t channel = -1;    // +0xd: the channel it receives on, below 0 none
    uint8_t byteE = 0;      // +0xe: its index into the objects' table 0x89535458
    uint8_t byteF = 0;      // +0xf
};
static_assert(sizeof(Vc2PartNode) == 16);

struct Vc2PartNodes {
    static constexpr int kSources = 4, kParts = 16;   // [FW]
    std::array<Vc2PartNode, kSources * kParts> nodes {};
    // The node of `source` (1…4) and `part`
    const Vc2PartNode& of(int source, int part) const { return nodes[kParts * (source - 1) + part]; }
};
