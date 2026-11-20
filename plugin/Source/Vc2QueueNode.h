#pragma once
// A node of the part's update queue that is not a voice (v295's, its own header from v312): its links and words as
// rom.4 lays them, the first 0x14 bytes a voice's node shares; and the static M's, the one rom.4 has (v295's +0x170).
#include <array>
#include <cstdint>

// one-off: rom.4's own layout; a declaration, not a job to template
struct Vc2QueueNode {
    uint32_t next = 0, prev = 0;   // +0x00, +0x04: its links, itself out of a queue
    uint32_t queued = 0;           // +0x08
    uint8_t queue = 0;             // +0x0c
    uint8_t unread0d[3] {};
    uint32_t vtable = 0;           // +0x10
};

inline constexpr uint32_t kVc2StaticNodeAt = 0x89997278;   // [FW] the static M's node, 0x89997108 + 0x170
