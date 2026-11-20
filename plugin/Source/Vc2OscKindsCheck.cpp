// xt_vc2osc's kinds 5 and 6 (v266): the carrier and the pitch shifter on the voice, rom.4's own routines against the
// port's words.  What they read from outside the voice is drawn on both sides: the OS's clock, the mic ring with its
// periods, Unvoice Level and the unvoiced decision.  Their inputs stay as a note gives them: rom.4 runs these on what the
// converters write, and noise in a gain or a coefficient would take both sides to NaNs, whose payloads are the host's.
// The ring's set-up and blocks and the words as a note gives them are Vc2OscDraws's from v319, shared with xt_vc2tone.
// XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
constexpr uint32_t kCarrierInit = 0x893ebed0, kKind6Start = 0x893ec700;   // [FW]
constexpr uint32_t kCarrier = 0xa90, kKind6 = 0xafc;   // [FW] where the two lie in a voice

Vc2CarrierParams drawCarrierP(std::mt19937& rng) {
    Vc2CarrierParams p {};
    p.gain = vc2Unit(rng);
    p.pitch = oscWithin(rng,0.25f, 4.0f);
    for (float* level : { &p.sawCutoff, &p.sawLevel, &p.noiseLowpass, &p.noiseHighpass, &p.noiseLevel }) {
        *level = vc2DrawLevel(rng);
    }
    return p;
}

Vc2Kind6Params drawKind6P(std::mt19937& rng) {
    return { oscWithin(rng,0.25f, 4.0f), oscWithin(rng,0.5f, 2.0f), vc2Unit(rng), vc2Unit(rng) };
}

// The mic ring's sides of xt_vc2osc's two
OscRingSides micRingOf(OscSides& s) { return { s.fw, s.shared.ring, s.shared.unvoiced, s.shared.draws.clock }; }

std::string partsDifference(OscSides& s, int v) {
    const Vc2OscVoice& o = s.voices[(size_t)v];
    return vc2RegionsDifference(s.fw, { { oscVoiceAt(v) + kCarrier, &o.carrier.w, sizeof o.carrier.w, "the carrier" },
                                        { oscVoiceAt(v) + kKind6, &o.kind6.w, sizeof o.kind6.w, "kind 6's object" } });
}

// A voice's carrier and pitch shifter as a note leaves them
std::string startParts(OscSides& s, std::mt19937& rng, int v) {
    Vc2OscVoice& o = s.voices[(size_t)v];
    const uint32_t at = oscVoiceAt(v);
    const std::array<uint8_t, sizeof o.carrier.w + sizeof o.kind6.w> zeros {};
    s.fw.cpu.write(at + kCarrier, zeros.data(), zeros.size());
    o.carrier.w = {};
    o.kind6.w = {};
    const Vc2CarrierParams carrierP = drawCarrierP(rng);
    s.fw.cpu.write(kOscKindPAt, &carrierP, sizeof carrierP);
    std::string what = vc2BothOn(s.fw, kCarrierInit, { at + kCarrier, kOscKindPAt }, "the carrier's init",
                                 [&] { o.carrier.init(carrierP, s.shared.draws.clock); },
                                 [&] { return partsDifference(s, v); });
    if (!what.empty()) return what;
    const Vc2Kind6Params kind6P = drawKind6P(rng);
    s.fw.cpu.write(kOscKindPAt, &kind6P, sizeof kind6P);
    return vc2BothOn(s.fw, kKind6Start, { at + kKind6, kOscKindPAt }, "kind 6's start",
                     [&] { o.kind6.setAndStart(s.shared.ring, kind6P); }, [&] { return partsDifference(s, v); });
}
} // namespace

std::string oscKindsSetUp(OscSides& s) { return oscRingSetUp(micRingOf(s)); }

std::string oscKindsCase(OscSides& s, std::mt19937& rng) {
    std::string what = oscRingCase(micRingOf(s), rng);
    for (int v = 0; v < kOscVoices && what.empty(); ++v) what = startParts(s, rng, v);
    return what;
}

std::string oscMicBlock(OscSides& s, std::mt19937& rng) { return oscRingBlock(micRingOf(s), rng); }

void oscCountBlock(OscSides& s, uint32_t kind) {
    if (!oscIsVoiced(kind)) return;
    const std::array<float, Vc2OscVoice::kOut>& out = s.shared.out;
    s.n.voicedSounding += std::ranges::any_of(out, [](float x) { return x != 0.0f; });
    s.n.voicedNotFinite += std::ranges::any_of(out, [](float x) { return !std::isfinite(x); });
}

std::string oscPartsDifference(OscSides& s) {
    for (int v = 0; v < kOscVoices; ++v) {
        std::string what = partsDifference(s, v);
        if (!what.empty()) return what;
    }
    constexpr uint32_t kStarted = offsetof(Vc2MicRingWords, started);
    return vc2FirstDifference(s.fw, kOscRingAt + kStarted, &s.shared.ring.w.started, sizeof(int32_t),
                              "the ring's start");
}
#endif
