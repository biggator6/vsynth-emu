#pragma once
// The VC-2 part's own Matrix object, the static M (v295): rom.4's 0x290 bytes from 0x89997108 word for word.  M's
// words and links (v243's, v247's), its update queue node at +0x170 (a list head's first 0x14 bytes, v272's layout),
// the object's two vtables (+0x180, the node's, whose class the queue runs; +0x184, whose +0x14 is M's init
// 0x893a2ee0), the part's levels at +0x188 (v247's 0x89997290), a float, its own channel state at +0x198 (0xec bytes,
// as the channel states' table's are), the word G's channel word takes with Receive Switch off at +0x284 (v292's
// 0x8999738c) and two floats.  Its static initializer 0x893a3020 and the part's init 0x893a2fe0; the channel state's
// set-up (0x89317a10) and the queue's enqueue (0x89394fa0, v279's) through Vc2StaticMatrixParts.  Not in the sound
// yet.  Bit for bit: xt_vc2settings' ninth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's static M".
#include "Vc2MatrixSetup.h"
#include <array>
#include <cstdint>

// What the static M's routines hand on
class Vc2StaticMatrixParts {
public:
    virtual ~Vc2StaticMatrixParts() = default;
    virtual void channelStateInit(uint32_t state, int32_t arg) = 0;   // 0x89317a10
    virtual void enqueue(uint32_t queue, uint32_t node) = 0;          // 0x89394fa0
};

// [FW] an update queue's node, as a list's head and a voice begin (v272's Vc2VoiceNode's first 0x14 bytes)
struct Vc2QueueNode {
    uint32_t next = 0, prev = 0;   // +0x00, +0x04: its links, itself out of a queue
    uint32_t queued = 0;           // +0x08
    uint8_t queue = 0;             // +0x0c
    uint8_t unread0d[3] {};
    uint32_t vtable = 0;           // +0x10
};

struct Vc2StaticMatrix {
    static constexpr uint32_t kAt = 0x89997108;                                    // [FW]
    static constexpr uint32_t kNodeAt = kAt + 0x170, kStateAt = kAt + 0x198;       // [FW]
    Vc2MatrixWords m {};                  // +0x000
    Vc2MatrixLinks links {};              // +0x168: the channel state's word, the part
    Vc2QueueNode node {};                 // +0x170
    uint32_t vtable = 0;                  // +0x184
    Vc2MatrixLevels levels {};            // +0x188
    float scale = 0.0f;                   // +0x194
    std::array<uint8_t, 0xec> state {};   // +0x198: its own channel state
    uint32_t unreceived = 0;              // +0x284
    std::array<float, 2> floats {};       // +0x288, +0x28c

    void staticInit(Vc2StaticMatrixParts& parts);   // 0x893a3020
    void partInit(Vc2StaticMatrixParts& parts);     // 0x893a2fe0
};
static_assert(sizeof(Vc2StaticMatrix) == 0x290, "rom.4's layout, the static M");
