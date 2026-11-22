#pragma once
// A VC-2 tone's 14 envelopes and 8 LFOs (v268), and what the tone's other parts read from them by rom.4's addresses: an
// OSC voice's bound words, the TVA's envelope and LFO outputs.  HARDWARE_FIRMWARE.md, "The VC-2's envelopes and LFOs".
#include "Vc2Tone.h"
#include "Vc2ToneEnvelope.h"
#include "Vc2ToneLfo.h"
#include "Vc2Tva.h"
#include <array>
#include <cstdint>

class Vc2ToneModulators {
public:
    static constexpr uint32_t kEnvelopesAt = 0x1b38, kEnvelopeBytes = 0x70;   // [FW] in the tone
    static constexpr uint32_t kLfosAt = 0x2158, kLfoBytes = 0x98;             // [FW]

    void buildEnvelopes();
    void buildLfos(float& generator);
    void destroyLfos();
    // A bound part's word, the part by its address in the tone at `self`; 0 for an address that is neither
    float boundFloat(uint32_t self, uint32_t part, uint32_t offset) const;
    Vc2TvaEnvelope tvaEnvelope(uint32_t self, uint32_t part) const;   // the envelope the TVA binds
    float lfoOut(uint32_t self, uint32_t part) const;                 // an LFO's +0x38
    // The float at `at`, anywhere in an envelope or an LFO of the tone at `self`, 0 elsewhere: what a COSM's note-on and
    // update hold, its envelopes' levels and rates and its LFOs' levels by address (v318)
    float wordAt(uint32_t self, uint32_t at) const;

    std::array<Vc2ToneEnvelope, Vc2Tone::kEnvelopes> envelopes {};
    std::array<Vc2ToneLfo, Vc2Tone::kLfos> lfos {};
};
