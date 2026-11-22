// What C's children share: see Vc2SetChildHead.h (v283, v284, v285).
#include "Vc2SetChildHead.h"
#include <cstring>

namespace {
constexpr uint32_t kKind = 0, kIndex = 1, kKey = 2, kPartWords = 6;   // [FW] the objects' bytes
constexpr uint32_t kKindKey = 1;                                      // [FW] the kind-only children's voice bytes
} // namespace

int32_t vc2ByMagic(int32_t n, int32_t magic, int shift) {
    int32_t high = static_cast<int32_t>((static_cast<int64_t>(n) * magic) >> 32);
    if (magic < 0) high = static_cast<int32_t>(static_cast<uint32_t>(high) + static_cast<uint32_t>(n));
    const int32_t q = high >> shift;
    return q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
}

uint32_t vc2CallsPart(const Vc2SetCall& call, Vc2SetReads& reads) {
    return reads.firstByte(reads.voiceAt(call.channelAt).part);
}

void vc2PartWords(uint8_t* object, std::span<const uint16_t> words) {
    std::memcpy(object + kPartWords, words.data(), words.size_bytes());
}

void vc2ChildMade(uint8_t* object, const Vc2SetChildCall& c, Vc2SetReads& reads) {
    object[kKind] = static_cast<uint8_t>(c.kind);
    object[kIndex] = static_cast<uint8_t>(c.index);
    vc2ChildVoice(object + kKey, c.call, reads);
}

void vc2KindMade(uint8_t* object, uint32_t matrixAt, const Vc2SetChildCall& c, Vc2SetReads& reads) {
    std::memcpy(object + matrixAt, &c.matrix, sizeof c.matrix);
    object[kKind] = static_cast<uint8_t>(c.kind);
    vc2ChildVoice(object + kKindKey, c.call, reads);
}

void vc2ChildVoice(uint8_t* at, const Vc2SetCall& call, Vc2SetReads& reads) {
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    at[0] = v.key;
    at[1] = v.byte43;
}

void vc2ChildPart(uint8_t* object, Vc2ChildWords of, const Vc2SetCall& call, Vc2SetReads& reads) {
    vc2PartWords(object, (reads.*of)(vc2CallsPart(call, reads), object[kKind], object[kIndex]));
}
