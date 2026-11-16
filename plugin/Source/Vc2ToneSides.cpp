// xt_vc2tone's sides: their compare and the parts' draws, see Vc2ToneCheck.h (v228).  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneCheck.h"
#include <algorithm>

namespace {
// [FW] the pool's words
constexpr uint32_t kHeadAt = 0x8993fb48;
constexpr uint32_t kSerialAt = 0x8993fb4c;
// [FW] a tone's own words by offset: the tail, the envelopes' and LFOs' numbers, the pool's link
constexpr uint32_t kTailAt = 0x1af8;
constexpr uint32_t kEnvelopesAt = 0x1b38;
constexpr uint32_t kEnvelopeStride = 0x70;
constexpr uint32_t kEnvelopeNumber = 4;
constexpr uint32_t kLfosAt = 0x2158;
constexpr uint32_t kLfoStride = 0x98;
constexpr uint32_t kLfoNumber = 0x34;
constexpr uint32_t kLinkAt = 0x2618;

// A part's numbers (the envelopes' or the LFOs') against rom.4's words at `first` + `stride` × each
template <size_t N>
std::string numbersDifference(Vc2Fw& fw, uint32_t first, uint32_t stride, const std::array<int32_t, N>& mine,
                              const char* what) {
    for (size_t k = 0; k < N; ++k) {
        std::string differs = vc2FirstDifference(fw, first + stride * (uint32_t)k, &mine[k], sizeof(int32_t), what);
        if (!differs.empty()) return differs;
    }
    return "";
}

// Tone i's own words against rom.4's
std::string toneWordsDifference(ToneSides& s, int i) {
    const Vc2Tone& t = s.pool.tones[(size_t)i];
    const uint32_t at = s.pool.addressOf(i);
    std::string what = vc2RegionsDifference(s.fw, { { at, &t.head, sizeof t.head, "its head" },
                                                    { at + kTailAt, &t.tail, sizeof t.tail, "its tail" },
                                                    { at + kLinkAt, &t.link, sizeof t.link, "its link" } });
    const uint32_t envelopes = at + kEnvelopesAt + kEnvelopeNumber, lfos = at + kLfosAt + kLfoNumber;
    if (what.empty()) what = numbersDifference(s.fw, envelopes, kEnvelopeStride, t.envelopeIndex, "an envelope's");
    if (what.empty()) what = numbersDifference(s.fw, lfos, kLfoStride, t.lfoIndex, "an LFO's number");
    return what.empty() ? what : "tone " + std::to_string(i) + ", " + what;
}

bool drawFlag(std::mt19937& rng) { return rng() % 2 == 0; }
} // namespace

std::string toneDifference(ToneSides& s, int only) {
    const Vc2TonePool& p = s.pool;
    std::string what = vc2RegionsDifference(s.fw, { { kHeadAt, &p.head, sizeof p.head, "the pool's head" },
                                                    { kSerialAt, &p.serial, sizeof p.serial, "the serial" } });
    for (int i = 0; i < Vc2TonePool::kTones && what.empty(); ++i) {
        if (only < 0 || i == only) what = toneWordsDifference(s, i);
    }
    return what.empty() ? toneCallsDifference(s.rom, s.parts.calls) : what;
}

void tonePrepare(ToneSides& s, int i, std::mt19937& rng) {
    ToneDraws& d = s.draws;
    std::ranges::generate(d.envelopeOn, [&] { return drawFlag(rng); });
    std::ranges::generate(d.lfoOn, [&] { return drawFlag(rng); });
    std::ranges::generate(d.lfoFollows, [&] { return drawFlag(rng); });
    d.oscValue = 4.0f * vc2Unit(rng) - 2.0f;
    d.tvaLevel = rng() % 4 == 0 ? std::bit_cast<float>((uint32_t)rng()) : vc2Unit(rng) - 0.5f;   // now and then any word
    d.oscKind = (int32_t)(rng() % 8);
    d.oscFlag = drawFlag(rng) ? rng() % 2 : rng();
    d.tvaOver = rng() % 4 == 0;
    d.tvaStopped = rng() % 4 == 0;
    d.tvaHeld = drawFlag(rng);
    std::ranges::generate(d.installOsc, [&] { return (uint32_t)rng(); });
    std::ranges::generate(d.installCosm, [&] { return drawFlag(rng) ? rng() % 2 : (uint32_t)rng(); });
    toneWriteRom(s.fw, s.pool.addressOf(i), d);
    s.parts.tone = i;
    s.rom.clear();
    s.parts.calls.clear();
    ++s.n.events;
}
#endif
