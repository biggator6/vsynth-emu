// The draws of what an OSC voice's own parts are given: see Vc2OscDraws.h (v319; xt_vc2osc's, moved: the vibrato's
// block and its fade's hold from Vc2OscVibratoCheck.cpp, the ramps' words from Vc2RampCheck.cpp, the mic ring from
// Vc2OscKindsCheck.cpp).  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace {
constexpr uint32_t kBlockWords = sizeof(Vc2VibratoBlock) / 4;
constexpr uint32_t kBlockFirst = 0x80 / 4;   // [FW] the block's word the vibrato's start at, in a block or the settings
constexpr uint32_t kVoiceBlock = 0xd4;       // [FW] the same in a voice
// [FW] the static initializers: the carriers' rate (0x899d9964) and kind 6's globals; the ring's reset and write
constexpr uint32_t kCarrierRate = 0x893ec6a0, kKind6Globals = 0x893ecef0, kRingReset = 0x893ec7d0;
constexpr uint32_t kRingWrite = 0x893ec820;
// [FW] Unvoice Level, the tracker's period and the unvoiced decision, in rom.4's memory
constexpr uint32_t kUnvoiceLevelAt = 0x899d9968, kPeriodAt = 0x899d9bd0, kUnvoicedAt = 0x899d99dc;

// A delay, rise, hold or fall of the block's: none, its edges, or up to two seconds' worth
float drawVibratoSpan(std::mt19937& rng) {
    constexpr std::array<float, 4> kEdges { 0.0f, -0.0f, 1.0f, 2000.0f };
    return rng() % 3 == 0 ? kEdges[rng() % kEdges.size()] : oscWithin(rng, 1.0f, 2000.0f);
}

// Vibrato Rate: the converter's 2…8 most often, and past both ends of the step's bounds
float drawVibratoRate(std::mt19937& rng) {
    constexpr std::array<float, 4> kEdges { 0.0f, -1.0f, 300.0f, 0.001f };
    return rng() % 4 == 0 ? kEdges[rng() % kEdges.size()] : oscWithin(rng, 2.0f, 8.0f);
}

// The block's +0x24: the converter's 0…63, and 100 with either side of it
int32_t drawVibratoScaleBy(std::mt19937& rng) {
    constexpr std::array<int32_t, 4> kEdges { 99, 100, 101, 200 };
    return rng() % 3 == 0 ? kEdges[rng() % kEdges.size()] : (int32_t)(rng() % 64);
}

// A glide's time: its law's edges (1, 50 and 100, either side of them, 0, negative, huge) or 1…3000 ms
float drawGlideTime(std::mt19937& rng) {
    constexpr std::array<float, 8> kEdges { 1.0f, 50.0f, 100.0f, 0.0f, 0.5f, 2.0f, -3.0f, 1e6f };
    const float edge = kEdges[rng() % kEdges.size()];
    if (rng() % 4 == 0) return edge;
    if (rng() % 6 == 0) return std::nextafter(edge, rng() % 2 ? -1e9f : 1e9f);
    return 1.0f + 3000.0f * vc2Unit(rng);
}

// A block of the mic into the ring on both sides, with the tracker's period and the unvoiced decision for it
std::string writeMic(const OscRingSides& r, std::mt19937& rng) {
    std::array<float, Vc2Kind6::kBlock> mic {};
    const float level = vc2Unit(rng);
    for (float& x : mic) x = oscWithin(rng,-level, level);
    const float period = vc2DrawPeriod(rng);
    r.fw.cpu.write(kOscMicAt, mic.data(), sizeof mic);
    r.fw.cpu.write(kPeriodAt, &period, sizeof period);
    for (float& u : r.unvoiced) u = rng() % 2 ? 0.0f : vc2Unit(rng);
    r.fw.cpu.write(kUnvoicedAt, r.unvoiced.data(), r.unvoiced.size_bytes());
    return vc2BothOn(r.fw, kRingWrite, { kOscMicAt, (uint32_t)Vc2Kind6::kBlock }, "the ring's write",
                     [&] { r.ring.write(mic.data(), period); }, [] { return std::string(); });
}
} // namespace

Vc2VibratoBlock oscVibratoBlockDrawn(std::mt19937& rng) {
    Vc2VibratoBlock b {};
    b.wave = (int32_t)(rng() % kVc2VibratoWaves);
    b.rate = drawVibratoRate(rng);
    b.pitchBias = oscWithin(rng, -1.0f, 1.0f);
    b.levelBias = oscWithin(rng, -1.0f, 1.0f);
    b.delay = rng() % 4 == 0 ? -drawVibratoSpan(rng) : drawVibratoSpan(rng);
    b.hold = drawVibratoSpan(rng);
    b.rise = rng() % 4 == 0 ? -oscWithin(rng, 0.0f, b.hold) : drawVibratoSpan(rng);   // never a sum below 0, as the rows'
    b.fall = rng() % 4 == 0 ? -drawVibratoSpan(rng) : drawVibratoSpan(rng);
    b.stepRatio = rng() % 2 ? 1.0f : oscWithin(rng, 0.0f, 1.5f);
    b.scaleBy = drawVibratoScaleBy(rng);
    return b;
}

void oscDrawVibratoBlock(std::span<uint32_t> words, std::mt19937& rng) {
    const Vc2VibratoBlock b = oscVibratoBlockDrawn(rng);
    std::memcpy(words.data() + kBlockFirst, &b, sizeof b);
}

void oscDrawVibratoVoice(Vc2OscVoice& o, std::mt19937& rng) {
    const auto words = std::bit_cast<std::array<uint32_t, kBlockWords>>(oscVibratoBlockDrawn(rng));
    for (uint32_t k = 0; k < kBlockWords; ++k) o.setU32(kVoiceBlock + 4 * k, words[k]);
}

void oscHoldVibrato(Vc2VibratoWords& w, int32_t n) {
    const float most = 1.0f / (float)std::max(n, 1);
    w.fadeRate = std::min(w.fadeRate, most);
    w.fallRate = std::min(w.fallRate, 0.1f * most);
}

uint32_t oscDrawOnSwitch(std::mt19937& rng) { return rng() % 2 ? 1u : (rng() % 2 ? 0u : (uint32_t)rng()); }

Vc2RampSettings oscDrawRampSettings(std::mt19937& rng) {
    Vc2RampSettings s;
    s.on = oscDrawOnSwitch(rng);
    s.formantOn = oscDrawOnSwitch(rng);
    s.walkOn = oscDrawOnSwitch(rng);
    s.time = drawGlideTime(rng);
    s.timed = oscDrawOnSwitch(rng);
    s.unread = (uint32_t)rng();
    return s;
}

void oscDrawAnswers(Vc2Fw& fw, OscShared& shared, std::mt19937& rng) {
    constexpr uint32_t kPeakAt = 0x899d9a5c;   // [FW] the input stage's peak
    OscDraws& d = shared.draws;
    d.clock = (int32_t)rng();
    d.peak = rng() % 4 == 0 ? 0.0f : vc2DrawLevel(rng);
    fw.cpu.wr32(kPeakAt, std::bit_cast<uint32_t>(d.peak));
    d.rendered = (uint32_t)rng();
    d.seekFlags = (uint8_t)rng();
    for (int32_t& p : d.positions) p = (int32_t)rng();   // v238: the event lookup's, each side from the first
    d.aside = (uint32_t)rng();
    d.romTaken = 0;
    shared.positionsTaken = 0;
    shared.calls.clear();
}

std::string oscRingWordsDifference(const OscRingSides& r) {
    return vc2RegionsDifference(r.fw, { { kOscRingAt, &r.ring.w, sizeof r.ring.w, "the mic ring" },
                                        { kUnvoiceLevelAt, &r.ring.g, sizeof r.ring.g, "kind 6's globals" } });
}

std::string oscRingSetUp(const OscRingSides& r) {
    if (!r.fw.call(kCarrierRate, {})) return "the carriers' rate setter faults: " + r.fw.cpu.error();
    return vc2BothOn(r.fw, kKind6Globals, {}, "kind 6's globals' setter", [&] { r.ring.setGlobals(); },
                     [&] { return oscRingWordsDifference(r); });
}

std::string oscRingCase(const OscRingSides& r, std::mt19937& rng) {
    r.ring.g.unvoiceLevel = vc2DrawLevel(rng);
    r.fw.cpu.write(kUnvoiceLevelAt, &r.ring.g.unvoiceLevel, sizeof r.ring.g.unvoiceLevel);
    r.clock = (int32_t)rng();
    std::string what = vc2BothOn(r.fw, kRingReset, {}, "the ring's reset", [&] { r.ring.reset(); },
                                 [&] { return oscRingWordsDifference(r); });
    for (int block = 0; block < Vc2MicRingWords::kBlocks && what.empty(); ++block) what = writeMic(r, rng);
    return what.empty() ? oscRingWordsDifference(r) : what;
}

std::string oscRingBlock(const OscRingSides& r, std::mt19937& rng) {
    const std::string what = writeMic(r, rng);
    return what.empty() ? oscRingWordsDifference(r) : what;
}
#endif
