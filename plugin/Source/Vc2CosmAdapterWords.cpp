// The COSM adapters' access to rom.4's words: see Vc2CosmAdapterWords.h (v255).
#include "Vc2CosmAdapterWords.h"
#include <bit>
#include <cstring>

namespace {
constexpr uint32_t kStartLevel = 0x58, kLfoLevel = 0x38;   // [FW] the bound objects' words the start values read
} // namespace

float Vc2CosmAdapter::blockFloat(const Vc2CosmBlockWords& b, int k) { return std::bit_cast<float>(b.words[k]); }

Vc2CosmAdapter::Vc2CosmStarts Vc2CosmAdapter::startsOf(Vc2CosmNoteOn& n, const Vc2CosmObjectWords& o) {
    Vc2CosmBound& bound = n.bound();
    return { bound.word(o.bindings[0] + kStartLevel), bound.word(o.bindings[2] + kLfoLevel),
             bound.word(o.bindings[1] + kStartLevel), bound.word(o.bindings[3] + kLfoLevel) };
}

Vc2CosmAdapter::Vc2CosmStarts Vc2CosmAdapter::heldOf(const Vc2CosmObjectWords& o) {
    return { o.held[0], o.held[2], o.held[1], o.held[3] };
}

void Vc2CosmAdapter::copyBlock(Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, size_t from, size_t bytes) {
    std::memcpy(reinterpret_cast<uint8_t*>(&o) + offsetof(Vc2CosmObjectWords, code) + from,
                reinterpret_cast<const uint8_t*>(&b) + from, bytes);
}

Vc2CosmBlockWords Vc2CosmAdapter::blockIn(const Vc2CosmObjectWords& o) {
    Vc2CosmBlockWords b {};
    b.code = static_cast<int8_t>(o.dspCode);
    std::memcpy(b.words, reinterpret_cast<const uint8_t*>(&o) + offsetof(Vc2CosmObjectWords, variant), sizeof b.words);
    return b;
}
