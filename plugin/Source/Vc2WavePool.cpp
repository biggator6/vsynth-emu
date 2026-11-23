// The VC-2's wave pool: see Vc2WavePool.h (v501).  The words are rom.4's little-endian ones; a size's units and the
// compares rom.4 makes signed are taken as its registers hold them.  A header's four words are written together where
// rom.4 writes them one after another: they are four places, so the order leaves nothing different.
#include "Vc2WavePool.h"
#include <cstring>

namespace {
// [FW] a block's header: 24 bytes, the previous block, the next, the state and the payload's units; an object's word
constexpr uint32_t kHeader = 24, kPrev = 0, kNext = 4, kState = 8, kUnits = 0xc, kTag = 0x10;
constexpr uint32_t kFree = 0, kUsed = 1, kHead = 2;   // [FW] the states
// [FW] the size law: a request of n bytes takes ((n - 1) / 40 · 40 + 63) / 24 units, the set-up's whole block from the
// memory less 89 the same way; the largest free block is taken from 10 units
constexpr int32_t kSetUpLess = 89, kUnitRound = 40, kRoundUp = 63;
constexpr int32_t kLargestLeast = 10;
// The size law can give the set-up's block up to 22 bytes past the OS's memory, which rom.4 then writes when it moves
// or fills the last block: the port's memory runs on that far
constexpr uint32_t kOverhang = 24;

uint32_t unitsOf(int32_t less) {
    const int32_t q = less / kUnitRound;
    return static_cast<uint32_t>(kUnitRound * q + kRoundUp) / kHeader;
}
int32_t sw(uint32_t v) { return static_cast<int32_t>(v); }
} // namespace

uint32_t Vc2WavePool::unitsFor(uint32_t bytes) { return unitsOf(sw(bytes - 1)); }

uint8_t Vc2WavePool::byte(uint32_t at) const {
    const uint32_t o = at - words.base;
    return o < memory.size() ? memory[o] : 0;
}

uint8_t* Vc2WavePool::bytesAt(uint32_t at) {
    const uint32_t o = at - words.base;
    return o < memory.size() ? &memory[o] : nullptr;
}

uint32_t Vc2WavePool::word(uint32_t at) const {
    const uint32_t o = at - words.base;
    if (o >= memory.size() || memory.size() - o < 4) return 0;
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(memory[o + static_cast<uint32_t>(i)]) << (8 * i);
    return v;
}

void Vc2WavePool::setWord(uint32_t at, uint32_t v) {
    const uint32_t o = at - words.base;
    if (o >= memory.size() || memory.size() - o < 4) return;
    for (int i = 0; i < 4; ++i) memory[o + static_cast<uint32_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}

void Vc2WavePool::header(uint32_t block, uint32_t prev, uint32_t next, uint32_t units, uint32_t state) {
    setWord(block + kPrev, prev);
    setWord(block + kNext, next);
    setWord(block + kUnits, units);
    setWord(block + kState, state);
}

uint32_t Vc2WavePool::tagged(uint32_t payload, uint32_t tag) {
    if (payload == 0) return 0;
    setWord(payload - kHeader + kTag, tag);
    return payload - kHeader;
}

void Vc2WavePool::copy(uint32_t to, uint32_t from, uint32_t bytes) {
    uint8_t* d = bytesAt(to);
    const uint8_t* s = bytesAt(from);
    if (d && s && bytesAt(to + bytes - 1) && bytesAt(from + bytes - 1)) std::memmove(d, s, bytes);
}

uint32_t Vc2WavePool::setUp(uint32_t bytes, Vc2PoolHost& host) {
    if (words.base != 0) {
        host.osFree(words.base);
        words.base = 0;
    }
    memory.clear();
    if (sw(bytes) <= 0 || (words.base = host.osAlloc(bytes)) == 0) {
        words.rover = words.size = words.free = 0;
        return 0;
    }
    memory.assign(bytes + kOverhang, 0);
    words.size = bytes;
    const uint32_t head = words.base, first = head + kHeader;
    words.rover = head;
    header(head, first, first, 0, kHead);
    header(first, head, head, unitsOf(sw(bytes) - kSetUpLess), kFree);
    words.free = word(first + kUnits) * kHeader;
    return 1;
}

void Vc2WavePool::takeNext(uint32_t block) {
    const uint32_t next = word(block + kNext);
    if (words.rover == next) words.rover = block;
    setWord(block + kNext, word(next + kNext));
    setWord(word(next + kNext) + kPrev, block);
    setWord(block + kUnits, word(next + kUnits) + word(block + kUnits) + 1);
    words.free += kHeader;
}

void Vc2WavePool::split(uint32_t block, uint32_t units) {
    const uint32_t have = word(block + kUnits);
    if (have == units) {
        setWord(block + kState, kUsed);
        return;
    }
    const uint32_t rest = block + (units + 1) * kHeader;
    header(rest, block, word(block + kNext), have - units - 1, kFree);
    words.rover = rest;
    setWord(word(block + kNext) + kPrev, rest);
    header(block, word(block + kPrev), rest, units, kUsed);
    words.free -= kHeader;
    if (word(word(rest + kNext) + kState) == kFree) takeNext(rest);
}

void Vc2WavePool::release(uint32_t block) {
    setWord(block + kState, kFree);
    const uint32_t prev = word(block + kPrev), next = word(block + kNext);
    if (word(prev + kState) != kFree) {
        if (word(next + kState) == kFree) takeNext(block);
        words.rover = block;
        return;
    }
    setWord(prev + kUnits, word(block + kUnits) + word(prev + kUnits) + 1);
    if (word(next + kState) == kFree) {
        setWord(prev + kNext, word(next + kNext));
        setWord(prev + kUnits, word(next + kUnits) + word(prev + kUnits) + 1);
        setWord(word(next + kNext) + kPrev, prev);
        words.free += 2 * kHeader;
    } else {
        setWord(prev + kNext, next);
        setWord(next + kPrev, prev);
        words.free += kHeader;
    }
    words.rover = prev;
}

void Vc2WavePool::move(uint32_t to, uint32_t from, Vc2PoolHost& host) {
    const uint32_t rest = word(to + kUnits) - word(from + kUnits);
    if (rest != 0) {
        const uint32_t after = to + (word(from + kUnits) + 1) * kHeader;
        header(after, to, word(to + kNext), rest - 1, kFree);
        setWord(word(to + kNext) + kPrev, after);
        setWord(to + kNext, after);
        setWord(to + kUnits, word(from + kUnits));
        words.free -= kHeader;
    }
    const uint32_t prev = word(to + kPrev), next = word(to + kNext);
    copy(to, from, (word(from + kUnits) + 1) * kHeader);
    setWord(to + kPrev, prev);
    setWord(to + kNext, next);
    host.moved(to, sw(to - from));
    release(from);
}

uint32_t Vc2WavePool::alloc(uint32_t bytes) {
    if (bytes == 0 || bytes > words.free || words.rover == 0) return 0;
    const uint32_t units = unitsOf(sw(bytes - 1)), start = words.rover;
    uint32_t block = start;
    do {
        if (word(block + kState) == kFree && sw(word(block + kUnits)) >= sw(units)) {
            split(block, units);
            words.free -= units * kHeader;
            return block + kHeader;
        }
        block = word(block + kNext);
    } while (block != start);
    return 0;
}

uint32_t Vc2WavePool::calloc(uint32_t count, uint32_t size) {
    const uint32_t bytes = count * size, payload = alloc(bytes);
    if (payload != 0 && bytesAt(payload) && bytesAt(payload + bytes - 1)) std::memset(bytesAt(payload), 0, bytes);
    return payload;
}

void Vc2WavePool::free(uint32_t payload) {
    if (payload == 0) return;
    const uint32_t block = payload - kHeader;
    words.free += word(block + kUnits) * kHeader;
    release(block);
}

uint32_t Vc2WavePool::realloc(uint32_t payload, uint32_t bytes, Vc2PoolHost& host) {
    if (payload == 0) return 0;
    const uint32_t units = unitsOf(sw(bytes - 1)), block = payload - kHeader;
    const int32_t grow = sw(units - word(block + kUnits));
    if (grow < 0) {
        split(block, units);
        words.free += static_cast<uint32_t>(-grow) * kHeader;
        return payload;
    }
    const uint32_t need = static_cast<uint32_t>(grow) * kHeader;
    const uint32_t next = word(block + kNext);
    const bool fits = sw(words.free) >= sw(need);
    if (fits && word(next + kState) == kFree && sw(word(next + kUnits) + 1) >= grow) {
        takeNext(block);
        split(block, units);
        words.free -= need;
        return payload;
    }
    const uint32_t start = words.rover;
    for (uint32_t to = start; fits;) {
        if (word(to + kState) == kFree && sw(word(to + kUnits)) >= sw(units)) {
            move(to, block, host);
            takeNext(to);
            split(to, units);
            words.free -= need;
            return to + kHeader;
        }
        to = word(to + kNext);
        if (to == start) break;
    }
    return 0;
}

uint32_t Vc2WavePool::largest(uint32_t& bytes) {
    uint32_t most = 0, at = 0;
    const uint32_t start = words.rover;
    uint32_t block = start;
    do {
        if (word(block + kState) == kFree && sw(word(block + kUnits)) > sw(most)) {
            most = word(block + kUnits);
            at = block;
        }
        block = word(block + kNext);
    } while (block != start);
    if (sw(most) < kLargestLeast) return 0;
    setWord(at + kState, kUsed);
    bytes = most * kHeader;
    const int32_t left = sw(words.free - bytes);
    words.free = left < 0 ? 0 : static_cast<uint32_t>(left);
    return at + kHeader;
}

void Vc2WavePool::compact(Vc2PoolHost& host) {
    uint32_t block = words.base;
    if (word(block + kState) != kHead) return;
    do {
        const uint32_t next = word(block + kNext);
        if (word(block + kState) == kFree && word(next + kState) == kUsed) {
            const uint32_t prev = word(block + kPrev), units = word(block + kUnits), after = word(next + kNext);
            const uint32_t bytes = (word(next + kUnits) + 1) * kHeader, rest = block + bytes;
            copy(block, next, bytes);
            setWord(block + kPrev, prev);
            setWord(after + kPrev, rest);
            setWord(block + kNext, rest);
            header(rest, block, after, units, kFree);
            host.moved(block, sw(block - next));
            if (word(after + kState) == kFree) takeNext(rest);
        }
        block = word(block + kNext);
    } while (block != words.base);
    const uint32_t last = word(block + kPrev);
    if (word(last + kState) == kFree) words.free = word(last + kUnits) * kHeader;
    words.rover = words.base;
}

uint32_t Vc2WavePool::allocObject(uint32_t bytes, uint32_t extra) { return tagged(alloc(bytes + extra), extra); }

uint32_t Vc2WavePool::largestObject(uint32_t& bytes) {
    const uint32_t payload = largest(bytes);
    return tagged(payload, bytes);
}

void Vc2WavePool::freeObject(uint32_t object) {
    if (object != 0) free(object + kHeader);
}

uint32_t Vc2WavePool::reallocObject(uint32_t object, uint32_t bytes, uint32_t extra, Vc2PoolHost& host) {
    return tagged(realloc(object + kHeader, bytes + extra, host), extra);
}
