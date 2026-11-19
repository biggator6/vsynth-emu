// What C's envelopes and LFOs share: see Vc2SetChildHead.h (v283).
#include "Vc2SetChildHead.h"
#include <cstring>

namespace {
constexpr uint32_t kKind = 0, kIndex = 1, kKey = 2, kVelocity = 3, kPartWords = 6;   // [FW] the objects' bytes
} // namespace

void vc2ChildMade(uint8_t* object, const Vc2SetChildCall& c, Vc2SetReads& reads) {
    object[kKind] = static_cast<uint8_t>(c.kind);
    object[kIndex] = static_cast<uint8_t>(c.index);
    vc2ChildVoice(object, c.call, reads);
}

void vc2ChildVoice(uint8_t* object, const Vc2SetCall& call, Vc2SetReads& reads) {
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    object[kKey] = v.key;
    object[kVelocity] = v.byte43;
}

void vc2ChildPart(uint8_t* object, Vc2ChildWords of, const Vc2SetCall& call, Vc2SetReads& reads) {
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    const std::array<uint16_t, 5> words = (reads.*of)(reads.firstByte(v.part), object[kKind], object[kIndex]);
    std::memcpy(object + kPartWords, words.data(), sizeof words);
}
