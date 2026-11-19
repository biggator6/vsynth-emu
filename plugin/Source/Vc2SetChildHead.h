#pragma once
// What C's envelopes (v282) and LFOs (v283) share: each object begins with the note's kind, its index, the voice's key
// (+0x41) and +0x43, and holds the part's five words for it from +6, by kind and index; their conversions, updates and
// copies are one code (the envelopes' 0x8939a790, 0x8939a7b0, 0x8939a800; the LFOs' 0x8939b750, 0x8939b770,
// 0x8939b7c0).  On the object's bytes, as rom.4 writes them.  The notes' task.
#include "Vc2NoteMatrix.h"
#include "Vc2PoolOf.h"

// The part's five words for a child by part, kind and index (Vc2SetReads::envelopeOf, lfoOf)
using Vc2ChildWords = std::array<uint16_t, 5> (Vc2SetReads::*)(uint32_t, uint32_t, uint32_t);

// +0 the kind and +1 the index the constructor is given, then the voice's two bytes
void vc2ChildMade(uint8_t* object, const Vc2SetChildCall& c, Vc2SetReads& reads);
// +2 the voice's key and +3 its +0x43
void vc2ChildVoice(uint8_t* object, const Vc2SetCall& call, Vc2SetReads& reads);
// +6…+0xf the part's five words, by the call's part and the object's kind and index
void vc2ChildPart(uint8_t* object, Vc2ChildWords of, const Vc2SetCall& call, Vc2SetReads& reads);

// A child's words as rom.4's bytes
template <class Words>
uint8_t* vc2ChildBytes(Words& w) {
    return reinterpret_cast<uint8_t*>(&w);
}

// A pool of a kind of C's children `PerVoice` a voice, placed by the voice's number and the index; its conversion,
// update and copy, with `Derived::blockOf` its block writer and `Of` its part's words
template <class Derived, class Words, uint32_t PerVoice, uint32_t At, Vc2ChildWords Of>
class Vc2SetChildPool : public Vc2PoolOf<Words, 24 * PerVoice, At> {
public:
    static constexpr uint32_t kPerVoice = PerVoice;
    uint32_t placeOf(uint32_t number, uint32_t index) const { return this->addressOf(kPerVoice * number + index); }
    // The voice's two bytes again, then the block
    void convert(uint32_t object, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads) {
        Words& w = this->at(object);
        vc2ChildVoice(vc2ChildBytes(w), call, reads);
        Derived::blockOf(w, block, reads);
    }
    // The part's words again
    void update(uint32_t object, const Vc2SetCall& call, Vc2SetReads& reads) {
        vc2ChildPart(vc2ChildBytes(this->at(object)), Of, call, reads);
    }
    // The block
    void copy(uint32_t object, uint32_t* block, Vc2SetReads& reads) {
        Derived::blockOf(this->at(object), block, reads);
    }
};
