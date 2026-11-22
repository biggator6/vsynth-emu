// xt_vc2tone's TVA (v317): see Vc2ToneSides.h.  rom.4's own TVA in its tone against the host's, Vc2ToneHost: its words
// drawn on both sides each event by xt_vc2tva's laws (Vc2TvaDraws), its envelope and LFOs the tone's real ones, its
// fade's step rom.4's word at 0x8993eb08; its words and a render's output compared.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include "Vc2TvaDraws.h"
#include <bit>

namespace {
using In = Vc2ToneInput;
constexpr float kOutScale = 0.5f;   // what the output holds before a render adds to it
} // namespace

std::string toneTvaDifference(ToneSides& s, int i) {
    const Vc2TvaWords& w = s.parts[(size_t)i].tva.w;
    return vc2FirstDifference(s.fw, toneTvaAt(s.pool.addressOf(i)), &w, sizeof w, "its TVA");
}

// The words its routines decide by as xt_vc2tva steers them, bound every time to an envelope and two LFOs of the tone's
// own, as its note-on binds them; the word its fade's end sets one time in four, as the tone's update ends a tone by it
void toneDrawTva(ToneSides& s, int i, std::mt19937& rng) {
    Vc2TvaWords& w = s.parts[(size_t)i].tva.w;
    const uint32_t tone = s.pool.addressOf(i);
    tvaSteer(w, rng, false, [tone](Vc2TvaWords& t, std::mt19937& r) {
        t.envelope = toneEnvelopeAt(tone, (int)(r() % Vc2Tone::kEnvelopes));
        t.lfo = toneLfoAt(tone, (int)(r() % Vc2Tone::kLfos));
        t.panLfo = toneLfoAt(tone, (int)(r() % Vc2Tone::kLfos));
    });
    if (rng() % 4 == 0) w.stopped = tvaDrawSwitch(rng);
    s.fw.cpu.write(toneTvaAt(tone), &w, sizeof w);
    s.shared.fadeStep = std::bit_cast<float>(tvaDrawFadeStep(rng, w));
    s.fw.cpu.write(kToneFadeStepAt, &s.shared.fadeStep, sizeof s.shared.fadeStep);
}

// The words of its block the TVA reads as floats; its first two and its sixth, the envelope's and LFOs' numbers the
// note-on binds by, as they stand
void toneDrawTvaBlock(Vc2ToneInput& in, std::mt19937& rng) { tvaDrawBlockFloats(&in.at(In::kTva), rng); }

void toneDrawOut(ToneSides& s, std::mt19937& rng) {
    for (float& x : s.shared.out) x = kOutScale * (2.0f * vc2Unit(rng) - 1.0f);
    s.fw.cpu.write(kToneOutAt, s.shared.out.data(), sizeof s.shared.out);
}

std::string toneOutDifference(ToneSides& s) {
    return vc2FirstDifference(s.fw, kToneOutAt, s.shared.out.data(), sizeof s.shared.out, "the output");
}
#endif
