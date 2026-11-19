// The VC-2 part's conversion sets' common: see Vc2SetCommon.h (v281).  Each routine's reads and writes in rom.4's order.
#include "Vc2SetCommon.h"
#include <bit>

namespace {
constexpr int32_t kMagic = static_cast<int32_t>(0x82061029u);   // [FW] rom.4's reciprocal of 127² = 16129
constexpr int kBySquare = 13, kBySquareSixteen = 17;            // [FW] its shifts: n / 16129, n / (16129 × 16)
constexpr uint32_t kLevelScale = 10000;                         // [FW] the squared level's full scale
constexpr uint32_t kTop = 127;                                  // [FW] the velocity a negative depth counts down from

// rom.4's signed division by the magic: the product's high word plus n, shifted, rounded toward 0
int32_t byMagic(int32_t n, int shift) {
    const int32_t high = static_cast<int32_t>((static_cast<int64_t>(n) * kMagic) >> 32);
    const int32_t q = static_cast<int32_t>(static_cast<uint32_t>(high) + static_cast<uint32_t>(n)) >> shift;
    return q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
}

uint32_t asWord(int32_t v) { return static_cast<uint32_t>(v); }
} // namespace

void Vc2SetCommons::blockOfCommon(const Vc2CommonWords& w, uint32_t* block) {
    block[0] = w.firstSwitch != 0;
    block[1] = 0;
    block[2] = 0;
    block[4] = w.secondSwitch != 0;
    uint32_t velocity = w.velocity;
    int32_t sens = w.sens;
    if (sens < 0) {
        velocity = kTop - velocity;
        sens = -sens;
    }
    const uint32_t x = w.level;
    const int32_t square = x == 1 ? 1 : byMagic(static_cast<int32_t>(x * x * kLevelScale), kBySquare);
    const int16_t level = static_cast<int16_t>(square);
    const uint32_t v = velocity & 0xff;
    const uint32_t product = asWord(static_cast<int16_t>(sens)) * v * v * asWord(level);
    const auto out = static_cast<int16_t>(level - byMagic(static_cast<int32_t>(product), kBySquareSixteen));
    block[3] = std::bit_cast<uint32_t>(static_cast<float>(out < 0 ? 0 : out));
}

void Vc2SetCommons::fromVoice(Vc2CommonWords& w, const Vc2SetCall& call, Vc2SetReads& reads) {
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    w.key = v.key;
    w.velocity = v.byte43;
}

void Vc2SetCommons::fromPart(Vc2CommonWords& w, const Vc2SetCall& call, Vc2SetReads& reads) {
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    const std::array<uint16_t, 3> words = reads.commonOf(reads.firstByte(v.part));
    w.firstSwitch = static_cast<uint8_t>(words[0]);
    w.secondSwitch = static_cast<uint8_t>(words[0] >> 8);
    w.level = static_cast<uint8_t>(words[1]);
    w.unread07 = static_cast<uint8_t>(words[1] >> 8);
    w.sens = static_cast<int16_t>(words[2]);
}

void Vc2SetCommons::construct(uint32_t common, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads) {
    Vc2CommonWords& w = at(common);
    fromVoice(w, call, reads);
    fromPart(w, call, reads);
    blockOfCommon(w, block);
}

void Vc2SetCommons::convert(uint32_t common, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads) {
    Vc2CommonWords& w = at(common);
    fromVoice(w, call, reads);
    blockOfCommon(w, block);
}

void Vc2SetCommons::update(uint32_t common, const Vc2SetCall& call, Vc2SetReads& reads) {
    fromPart(at(common), call, reads);
}
