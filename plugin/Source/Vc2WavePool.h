#pragma once
// The VC-2's wave pool (v501): rom.4's pool at 0x8997c51c, which the wave store (Vc2WaveStore, 0x8997c52c) takes its
// records and chunks from: 32 MB from the OS's allocator at the store's set-up (0x89353ac0), a ring of blocks.  A
// block is a 24-byte header (the previous block, the next, its state: 0 free, 1 used, 2 the ring's head; its payload's
// size in 24-byte units), then its payload.  An object in the pool starts at its block's header: +0x10 a word its
// wrapper gives, +0x14 its vtable, whose entry at +0x18 (the object's place by the word there, its routine at +0x20)
// the pool calls when it moves the block, with how far.  Ported: the set-up 0x89350c10, alloc 0x89350cd0, calloc
// 0x89350de0, free 0x89350e40, realloc 0x89350e90, the largest free block 0x89351090, the compaction 0x89351120, their
// helpers (the split 0x89351430, the release 0x893514f0, a block's move 0x89351590, rom.4's memmove 0x89351220), and
// the objects' wrappers 0x89351690, 0x893516c0, 0x893516e0, 0x893517c0.  Not ported: the OS's lock around each (a
// semaphore, 0x893018b0 and 0x893018c0), and the `bra .` rom.4 stops in on a ring it finds broken (a block smaller
// than asked to split, a used block where a free one must be).  One call reaches it on a whole ring: a realloc to the
// same size whose next block is used moves into a free block of exactly that size, then finds a used block after it
// and stops there; the port goes on, and the check draws no such call.  Bit for bit:
// xt_vc2waves' second part.  HARDWARE_FIRMWARE.md, "The VC-2's wave pool".  The notes' task.
#include <cstdint>
#include <vector>

// [FW] the pool's words at 0x8997c51c (.bss): its memory's address, the block a search starts from, the memory's size,
// the bytes counted free
struct Vc2WavePoolWords {
    uint32_t base = 0, rover = 0, size = 0, free = 0;
};

// What the pool asks of the rest: the OS's allocator (0x89300770, 0x89300840), and a moved block's object told
class Vc2PoolHost {
public:
    virtual ~Vc2PoolHost() = default;
    virtual uint32_t osAlloc(uint32_t bytes) = 0;
    virtual void osFree(uint32_t at) = 0;
    // The block now at `block` has moved by `by` bytes: its object's method, through the vtable the block holds
    virtual void moved(uint32_t block, int32_t by) = 0;
};

class Vc2WavePool {
public:
    static constexpr uint32_t kAt = 0x8997c51c;
    Vc2WavePoolWords words {};
    // The pool's memory, from words.base: the OS's, zeroed when the port takes it, and 24 bytes past it, which the size
    // law can give the last block (rom.4 writes on into whatever follows the OS's memory there)
    std::vector<uint8_t> memory;
    // A byte of the memory by rom.4's address; none outside it
    uint8_t* bytesAt(uint32_t at);
    // A word of the memory by rom.4's address (0 outside it, a write there lost)
    uint32_t word(uint32_t at) const;
    void setWord(uint32_t at, uint32_t v);

    // The size law: the units a request of `bytes` takes
    static uint32_t unitsFor(uint32_t bytes);
    uint32_t setUp(uint32_t bytes, Vc2PoolHost& host);   // 0x89350c10: 1, or 0 when the OS gives no memory
    uint32_t alloc(uint32_t bytes);                                                      // 0x89350cd0: a payload, or 0
    uint32_t calloc(uint32_t count, uint32_t size);                                      // 0x89350de0
    void free(uint32_t payload);                                                         // 0x89350e40
    uint32_t realloc(uint32_t payload, uint32_t bytes, Vc2PoolHost& host);               // 0x89350e90
    uint32_t largest(uint32_t& bytes);                                                   // 0x89351090
    void compact(Vc2PoolHost& host);                                                     // 0x89351120
    // The objects' wrappers: an object is its block, its +0x10 the extra bytes asked for (or the largest's size)
    uint32_t allocObject(uint32_t bytes, uint32_t extra);                                // 0x89351690
    uint32_t largestObject(uint32_t& bytes);                                             // 0x893516c0
    void freeObject(uint32_t object);                                                    // 0x893516e0
    uint32_t reallocObject(uint32_t object, uint32_t bytes, uint32_t extra, Vc2PoolHost& host);   // 0x893517c0

private:
    void split(uint32_t block, uint32_t units);                 // 0x89351430
    void release(uint32_t block);                               // 0x893514f0
    void move(uint32_t to, uint32_t from, Vc2PoolHost& host);   // 0x89351590
    void takeNext(uint32_t block);                              // a free next block joined to `block`
    void copy(uint32_t to, uint32_t from, uint32_t bytes);      // 0x89351220
    // A block's header written: its previous and next blocks, its payload's units and its state
    void header(uint32_t block, uint32_t prev, uint32_t next, uint32_t units, uint32_t state);
    // The object a payload is in, its +0x10 given; none for none
    uint32_t tagged(uint32_t payload, uint32_t tag);
};
