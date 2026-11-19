// What C's envelopes, LFOs and mix share: see Vc2SetChildHead.h (v283, v284).
#include "Vc2SetChildHead.h"
#include <cstring>

namespace {
constexpr uint32_t kKind = 0, kIndex = 1, kKey = 2, kPartWords = 6;   // [FW] the objects' bytes

// The call's part: the first byte of what the voice's part word points at
uint32_t callsPart(const Vc2SetCall& call, Vc2SetReads& reads) {
    return reads.firstByte(reads.voiceAt(call.channelAt).part);
}

// +6… the part's words, as rom.4 writes the halfwords
template <size_t N>
void partWords(uint8_t* object, const std::array<uint16_t, N>& words) {
    std::memcpy(object + kPartWords, words.data(), sizeof words);
}
} // namespace

void vc2ChildMade(uint8_t* object, const Vc2SetChildCall& c, Vc2SetReads& reads) {
    object[kKind] = static_cast<uint8_t>(c.kind);
    object[kIndex] = static_cast<uint8_t>(c.index);
    vc2ChildVoice(object + kKey, c.call, reads);
}

void vc2ChildVoice(uint8_t* at, const Vc2SetCall& call, Vc2SetReads& reads) {
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    at[0] = v.key;
    at[1] = v.byte43;
}

void vc2ChildPart(uint8_t* object, Vc2ChildWords of, const Vc2SetCall& call, Vc2SetReads& reads) {
    partWords(object, (reads.*of)(callsPart(call, reads), object[kKind], object[kIndex]));
}

void vc2ChildPart(uint8_t* object, Vc2MixWordsOf of, const Vc2SetCall& call, Vc2SetReads& reads) {
    partWords(object, (reads.*of)(callsPart(call, reads), object[kKind]));
}
