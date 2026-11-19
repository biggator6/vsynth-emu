// The VC-2 tone's remaining methods (v273): see Vc2Tone.h.  Each routine's calls in rom.4's order; every address is
// rom.4's.  The OS's lock and unlock around the setters and key-off do nothing here.
#include "Vc2Tone.h"
#include "XtFloat.h"

namespace {
constexpr uint32_t kTwo = 2;   // [FW] 0x893b312a, 0x893b325a, 0x893b3fd2: an OSC's or COSM's number past 1 reaches none
constexpr int kOscs[] = { kVc2ToneOsc1, kVc2ToneOsc2 };
} // namespace

void Vc2Tone::setOscBlock(uint32_t osc, const uint32_t* block, Vc2ToneParts& parts) {
    if (osc < kTwo) parts.settingsOsc((int)osc, block);
}

void Vc2Tone::setLfoBlock(uint32_t lfo, const uint32_t* block, Vc2ToneParts& parts) { parts.settingsLfo(lfo, block); }

void Vc2Tone::setEnvelopeBlock(uint32_t envelope, const uint32_t* block, Vc2ToneParts& parts) {
    parts.settingsEnvelope(envelope, block);
}

void Vc2Tone::setCosmBlock(uint32_t cosm, const uint32_t* block, Vc2ToneParts& parts) {
    if (cosm < kTwo) parts.newBlockCosm((int)cosm, block);
}

void Vc2Tone::setMixBlock(const uint32_t* block, Vc2ToneParts& parts) { parts.settingsMix(block); }
void Vc2Tone::setTvaBlock(const uint32_t* block, Vc2ToneParts& parts) { parts.newBlockTva(block); }

void Vc2Tone::setLegato(const uint32_t* ramp, Vc2ToneParts& parts) {
    for (int osc : kOscs) parts.legatoOsc(osc, ramp);
}

void Vc2Tone::keyOff(Vc2ToneParts& parts) {
    if (head.state != kSounding) return;
    for (int lfo = 0; lfo < kLfos; ++lfo) parts.releaseLfo(lfo);   // all eight, LFO 0 too
    for (int envelope = 0; envelope < kEnvelopes; ++envelope) parts.releaseEnvelope(envelope);
    for (int osc : kOscs) parts.keyOffOsc(osc);
}

// [FW] 0x893b3be0: the fade read before the first wait and after each
void Vc2Tone::truncateAndWait(Vc2ToneParts& parts) {
    parts.truncateTvaTold();
    while (parts.tvaFade() > 0.0f) parts.waitFade();
    parts.truncated();
}

void Vc2Tone::truncateAtOnce(Vc2ToneParts& parts) { parts.truncateTva(); }

float Vc2Tone::cost(Vc2ToneParts& parts) const {
    const float oscs = xtf::add(parts.oscCost(kVc2ToneOsc1), parts.oscCost(kVc2ToneOsc2));
    return xtf::add(oscs, parts.cosmCost());
}

uint32_t Vc2Tone::oscWord168(uint32_t osc, Vc2ToneParts& parts) const {
    if (osc >= kTwo) return 0;
    return static_cast<uint16_t>(parts.oscWord168((int)osc));   // [FW] 0x893b3fac, 0x893b3fca: extu.w
}

uint32_t Vc2Tone::oscPosition(uint32_t osc, Vc2ToneParts& parts) const {
    return osc < kTwo ? parts.oscPosition((int)osc) : 0;
}
