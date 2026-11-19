#pragma once
// The VC-2 part's nodes (v291): rom.4's 64 nodes of 16 bytes from 0x899888d0 word for word, one a source (1…4) and
// part, 0x100 bytes a source; the mode row (0x89396e20) reads the channel each receives on.  From v293 their routines
// (Vc2PartNodes.cpp): the channel's move 0x89393100 on the lists by source and channel at 0x899890d0, kept in address
// order; the class installs 0x893932c0 (held) and 0x89393310 (plain) on the part objects at 0x8998246c (v278's part
// nodes, which rom.4's table 0x89535458 points at by the node's index), with the class bytes at 0x899888bd and the
// part's glides' reset (Vc2Keys, 0x893904d0); the voices' stop 0x89393360, through Vc2NodeParts.  The init
// (0x89393370) is the front's, item 10.  Not in the sound yet.  Bit for bit: xt_vc2keys' second part.
// HARDWARE_FIRMWARE.md, "The VC-2 part's nodes".
#include "Vc2Keys.h"
#include <array>
#include <cstdint>

// [FW] a node, as rom.4 lays it
struct Vc2PartNode {
    uint32_t next = 0;      // +0x0: the next node on its channel's list (rom.4's address, 0 the end)
    uint32_t object = 0;    // +0x4: its part object (rom.4's address)
    uint32_t word8 = 0;     // +0x8: given to 0x89390f70 and 0x89390fd0
    int8_t source = 0;      // +0xc: its source less 1
    int8_t channel = -1;    // +0xd: the channel it receives on: −1 none, −2 every one
    uint8_t byteE = 0;      // +0xe: its index into the objects' table 0x89535458
    uint8_t byteF = 0;      // +0xf
};
static_assert(sizeof(Vc2PartNode) == 16);

// [FW] a part object (v278's part node), 16 bytes: its index, its class's vtable
struct Vc2PartObject {
    uint8_t index = 0;
    uint8_t unread1[3] {};
    uint32_t vtable = 0;
    uint32_t unread8[2] {};
};
static_assert(sizeof(Vc2PartObject) == 16);

// A node's class: the held one (0x893932c0, vtable 0x898cd3b8) or the plain (0x89393310, 0x898cd184)
enum class Vc2NodeClass : uint8_t { kPlain, kHeld };

// What a node hands on (v293): the sounding voices of its word stopped (0x89390f70)
class Vc2NodeParts {
public:
    virtual ~Vc2NodeParts() = default;
    virtual void stopVoices(uint32_t word) = 0;
};

struct Vc2PartNodes {
    static constexpr int kSources = 4, kParts = 16, kChannels = 16, kObjects = 16;                    // [FW]
    static constexpr uint32_t kNodesAt = 0x899888d0, kListsAt = 0x899890d0, kClassesAt = 0x899888bd;  // [FW]
    static constexpr uint32_t kObjectsAt = 0x8998246c;                                                // [FW]
    std::array<Vc2PartNode, kSources * kParts> nodes {};
    std::array<uint32_t, kSources * kChannels> lists {};   // 0x899890d0: by source and channel, the first node
    std::array<uint8_t, kObjects> classes {};              // 0x899888bd: by object index, 1 the held class
    std::array<Vc2PartObject, kObjects> objects {};        // 0x8998246c

    // The node of `source` (1…4) and `part`
    const Vc2PartNode& of(int source, int part) const { return nodes[kParts * (source - 1) + part]; }
    // 0x89393100: the node at `node` onto `channel`'s list of `source` (less 1), or none
    void move(uint32_t node, int32_t source, int32_t channel);
    // 0x893932c0 and 0x89393310: the node's part object given its class, the part's glides reset
    void install(uint32_t node, Vc2NodeClass cls, Vc2Keys& keys);
    // 0x89393360: the voices of the node's word stopped
    void stop(uint32_t node, Vc2NodeParts& parts);

private:
    Vc2PartNode& at(uint32_t address);
    uint32_t& listOf(int32_t source, int32_t channel);
    void unlink(uint32_t& head, uint32_t node);
    void linkInOrder(uint32_t& head, uint32_t node);
};
