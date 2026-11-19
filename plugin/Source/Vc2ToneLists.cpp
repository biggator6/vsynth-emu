// The VC-2 part's two lists of tones: see Vc2ToneLists.h (v273).
#include "Vc2ToneLists.h"
#include "Vc2Tone.h"
#include <algorithm>

namespace {
// The heads by their address, each with the word the port keeps of it: its first tone
struct ToneListHead {
    uint32_t at;
    uint32_t Vc2ToneLists::* first;
};
constexpr ToneListHead kHeads[] = { { Vc2ToneLists::kSoundingAt, &Vc2ToneLists::sounding },
                                    { Vc2ToneLists::kNewAt, &Vc2ToneLists::fresh } };
} // namespace

// A head's first word, or a tone's +0
uint32_t& Vc2ToneLists::nextOf(uint32_t at, Vc2TonePool& pool) {
    const auto* head = std::ranges::find(kHeads, at, &ToneListHead::at);
    return head != std::end(kHeads) ? this->*(head->first) : pool.at(at).head.next;
}

// [FW] each list is walked to its end: nothing keeps its tail
uint32_t Vc2ToneLists::tailOf(uint32_t head, Vc2TonePool& pool) {
    uint32_t tail = head;
    while (nextOf(tail, pool) != 0) tail = nextOf(tail, pool);
    return tail;
}

// [FW] 0x893ad9a0, 0x893ada30: found from the head on; the mark, when it is the tone, steps back to the one before it
uint32_t Vc2ToneLists::unlist(uint32_t head, uint32_t& mark, uint32_t tone, Vc2TonePool& pool) {
    for (uint32_t at = head; at != tone;) {
        at = nextOf(at, pool);
        if (at == 0) return 0;
    }
    Vc2ToneHead& h = pool.at(tone).head;
    if (mark == tone) mark = h.prev == head ? 0 : h.prev;
    nextOf(h.prev, pool) = h.next;
    if (h.next != 0) pool.at(h.next).head.prev = h.prev;
    h.next = h.prev = 0;
    return tone;
}

void Vc2ToneLists::enlist(uint32_t tone, Vc2TonePool& pool) {
    const uint32_t tail = tailOf(kNewAt, pool);
    nextOf(tail, pool) = tone;
    // one-off: a list node's two links as rom.4's append writes them, not an object built
    pool.at(tone).head.prev = tail;
    pool.at(tone).head.next = 0;
    newest = tone;
}

uint32_t Vc2ToneLists::unlistNew(uint32_t tone, Vc2TonePool& pool) { return unlist(kNewAt, newest, tone, pool); }

uint32_t Vc2ToneLists::unlistSounding(uint32_t tone, Vc2TonePool& pool) {
    return unlist(kSoundingAt, joined, tone, pool);
}

void Vc2ToneLists::join(Vc2TonePool& pool) {
    const uint32_t tail = tailOf(kSoundingAt, pool);
    nextOf(tail, pool) = fresh;
    if (fresh == 0) return;
    pool.at(fresh).head.prev = tail;
    fresh = 0;
    joined = newest;
    newest = 0;
}

void Vc2ToneLists::start(Vc2TonePool& pool) {
    join(pool);
    for (uint32_t at = sounding; at != 0; at = pool.at(at).head.next) {
        Vc2Tone& tone = pool.at(at);
        if (tone.head.state == Vc2Tone::kFromOne) tone.start();   // [FW] 0x893b1cfe: its vtable's +0x6c
    }
}
