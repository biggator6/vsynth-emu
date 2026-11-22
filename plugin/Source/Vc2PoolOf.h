#pragma once
// A pool of rom.4's objects by number, word for word where rom.4 keeps it: the n-th at At + sizeof(Words) × n, as its
// placement routine gives it (v280: the conversion sets C, the notes' Matrix objects M).
#include <array>
#include <cstdint>

template <class Words, int Count, uint32_t At>
struct Vc2PoolOf {
    static constexpr int kCount = Count;
    static constexpr uint32_t kAt = At, kBytes = sizeof(Words);

    uint32_t addressOf(uint32_t number) const { return kAt + kBytes * number; }
    Words& at(uint32_t object) { return items[(object - kAt) / kBytes]; }

    std::array<Words, Count> items {};
};
