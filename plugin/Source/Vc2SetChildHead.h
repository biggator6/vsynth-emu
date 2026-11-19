#pragma once
// What C's envelopes (v282), LFOs (v283), mix (v284) and TVA (v285) share: each object begins with the note's kind,
// takes the voice's key (+0x41) and +0x43 into two bytes (the envelopes' and LFOs' +2, after their index; the mix's and
// TVA's +1), and holds the part's words for it from +6, by the kind (and the index); their updates are one code, and
// the envelopes', LFOs' and mix's conversions and copies (the envelopes' 0x8939a790, 0x8939a7b0, 0x8939a800; the LFOs'
// 0x8939b750, 0x8939b770, 0x8939b7c0; the mix's 0x8939be40, 0x8939be60, 0x8939bec0; the TVA's update 0x893a6940).  On
// the object's bytes, as rom.4 writes them.  The notes' task.
#include "Vc2NoteMatrix.h"
#include "Vc2PoolOf.h"
#include <bit>
#include <cstddef>
#include <span>

// The part's words for a child: five by part, kind and index (Vc2SetReads::envelopeOf, lfoOf), or N by part and kind
// (mixOf's 11, v284; tvaOf's 12, v285)
using Vc2ChildWords = std::array<uint16_t, 5> (Vc2SetReads::*)(uint32_t, uint32_t, uint32_t);
template <size_t N>
using Vc2KindWords = std::array<uint16_t, N> (Vc2SetReads::*)(uint32_t, uint32_t);

// +0 the kind and +1 the index the constructor is given, then the voice's two bytes at +2
void vc2ChildMade(uint8_t* object, const Vc2SetChildCall& c, Vc2SetReads& reads);
// A kind-only child's constructor's head (the mix's, the TVA's): M at `matrixAt`, the kind at +0, the voice's two bytes
// at +1
void vc2KindMade(uint8_t* object, uint32_t matrixAt, const Vc2SetChildCall& c, Vc2SetReads& reads);
// The voice's key and +0x43 into `at` and the byte after
void vc2ChildVoice(uint8_t* at, const Vc2SetCall& call, Vc2SetReads& reads);
// The call's part: the first byte of what the voice's part word points at
uint32_t vc2CallsPart(const Vc2SetCall& call, Vc2SetReads& reads);
// +6… the part's words, as rom.4 writes the halfwords
void vc2PartWords(uint8_t* object, std::span<const uint16_t> words);
// +6… the part's words, by the call's part and the object's kind (+0) and index (+1), or by the kind alone
void vc2ChildPart(uint8_t* object, Vc2ChildWords of, const Vc2SetCall& call, Vc2SetReads& reads);

template <size_t N>
void vc2ChildPart(uint8_t* object, Vc2KindWords<N> of, const Vc2SetCall& call, Vc2SetReads& reads) {
    vc2PartWords(object, (reads.*of)(vc2CallsPart(call, reads), object[0]));
}

// rom.4's signed division by a reciprocal `magic`: the product's high word, plus n where the magic is past 2^31 as a
// word, shifted by `shift`, rounded toward 0 (the common's by 127², the envelopes' by 6400, the TVA's by 6000 and 63)
int32_t vc2ByMagic(int32_t n, int32_t magic, int shift);

// A float as the block's word (the envelopes', LFOs' and mix's writers)
inline uint32_t floatWord(float f) {
    return std::bit_cast<uint32_t>(f);
}

// A child's words as rom.4's bytes
template <class Words>
uint8_t* vc2ChildBytes(Words& w) {
    return reinterpret_cast<uint8_t*>(&w);
}

// A pool of a kind of C's children `PerVoice` a voice, placed by the voice's number and the index; its conversion,
// update and copy, with `Derived::blockOf` its block writer, `Of` its part's words and `KeyAt` where the voice's bytes go
template <class Derived, class Words, uint32_t PerVoice, uint32_t At, auto Of, uint32_t KeyAt = 2>
class Vc2SetChildPool : public Vc2PoolOf<Words, 24 * PerVoice, At> {
public:
    static constexpr uint32_t kPerVoice = PerVoice;
    uint32_t placeOf(uint32_t number, uint32_t index) const { return this->addressOf(kPerVoice * number + index); }
    // The voice's two bytes again, then the block
    void convert(uint32_t object, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads) {
        Words& w = this->at(object);
        vc2ChildVoice(vc2ChildBytes(w) + KeyAt, call, reads);
        derived().blockOf(w, block, reads);
    }
    // The part's words again
    void update(uint32_t object, const Vc2SetCall& call, Vc2SetReads& reads) {
        vc2ChildPart(vc2ChildBytes(this->at(object)), Of, call, reads);
    }
    // The block (the TVA's writer keeps a word of its own, so through the pool)
    void copy(uint32_t object, uint32_t* block, Vc2SetReads& reads) {
        derived().blockOf(this->at(object), block, reads);
    }
    // A kind-only child's constructor's head (the mix's, the TVA's): M, the kind, the voice's bytes, the part's words
    Words& madeByKind(const Vc2SetChildCall& c, Vc2SetReads& reads) {
        Words& w = this->at(c.object);
        vc2KindMade(vc2ChildBytes(w), offsetof(Words, matrix), c, reads);
        update(c.object, c.call, reads);
        return w;
    }

private:
    Derived& derived() { return static_cast<Derived&>(*this); }
};
