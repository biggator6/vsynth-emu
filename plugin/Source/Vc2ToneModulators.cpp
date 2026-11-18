// A VC-2 tone's envelopes and LFOs: see Vc2ToneModulators.h (v268).
#include "Vc2ToneModulators.h"
#include <cstring>

namespace {
constexpr uint32_t kLfoOut = 0x38;   // [FW] an LFO's output

// The float at `offset` of the part `at` bytes into an array of parts `stride` bytes apart; false when it isn't there
template <class Parts>
bool boundWordIn(const Parts& parts, uint32_t at, uint32_t stride, uint32_t offset, float& word) {
    const uint32_t element = at / stride;
    if (element >= parts.size()) return false;
    std::memcpy(&word, reinterpret_cast<const char*>(&parts[element].w) + offset, sizeof word);
    return true;
}
} // namespace

void Vc2ToneModulators::buildEnvelopes() {
    for (Vc2ToneEnvelope& e : envelopes) e.construct();
}

void Vc2ToneModulators::buildLfos(float& generator) {
    for (Vc2ToneLfo& l : lfos) l.construct(generator);
}

void Vc2ToneModulators::destroyLfos() {
    for (Vc2ToneLfo& l : lfos) l.destroy();
}

float Vc2ToneModulators::boundFloat(uint32_t self, uint32_t part, uint32_t offset) const {
    float word = 0.0f;
    const uint32_t at = part - self;
    if (boundWordIn(envelopes, at - kEnvelopesAt, kEnvelopeBytes, offset, word)) return word;
    boundWordIn(lfos, at - kLfosAt, kLfoBytes, offset, word);
    return word;
}

Vc2TvaEnvelope Vc2ToneModulators::tvaEnvelope(uint32_t self, uint32_t part) const {
    const Vc2EnvelopeWords& w = envelopes[(part - self - kEnvelopesAt) / kEnvelopeBytes].w;
    return { w.stage, w.time[0], w.time[1], w.out, w.onset, w.stageTime };
}

float Vc2ToneModulators::lfoOut(uint32_t self, uint32_t part) const { return boundFloat(self, part, kLfoOut); }
