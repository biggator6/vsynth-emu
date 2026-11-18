// xt_vc2carrier's second part (v222): the VOCODER OSC's converter's own part and kind 5's update against rom.4's, on the
// first part's interpreter (the rate setter run, the clock hooked).  Each case a converter input drawn (the header's OSC,
// key and velocity; the record's VOCODER bytes, now and then past their IDs' ranges; the rest 0) and the Matrix's two
// groups for the OSC, which hooks on rom.4's four getters (0x893a23b0, 0x893a2440, 0x893a26b0, 0x893a28b0) give; P noise
// on both; rom.4's whole converter (0x8939db00) on the input and the OSC block at the voice + 0x54, the port's own part
// on the same; P word for word (the converter's first half, the OSC block's, is the voice's and not compared).  Then the
// carrier's init on both and updates of kind 5 (0x893afff0) on a voice of drawn words (those the converter's first half
// writes among them), its five modulators' outputs and the pitch ramp's state, the input stage's peak drawn; after each,
// P and the carrier word for word.  The shared routines (the velocity curve, exp2) are rom.4's own on both sides, run in
// the interpreter.  XT_VC2CARRIER_FIRST and XT_VC2CARRIER_STOP as the first part's.  XT_VC2CARRIER_MAIN only.
#ifdef XT_VC2CARRIER_MAIN
#include "Vc2CarrierCheck.h"
#include "Vc2RomShared.h"
#include "Vc2VocoderOsc.h"
#include "XtSwitches.h"
#include <bit>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kConvert = 0x8939db00, kKind5 = 0x893afff0, kCarrierInit = 0x893ebed0;   // [FW]
constexpr uint32_t kPeakAt = 0x899d9a5c;   // [FW]
constexpr uint32_t kVoiceAt = 0x8C060000, kInputAt = 0x8C058000, kOscSumsAt = 0x8C05a000;   // in Vc2Fw's RAM
constexpr uint32_t kLfoSumsAt = 0x8C05a020, kModsAt = 0x8C05b000, kMatrixAt = 0x8C05c000;
constexpr uint32_t kBlock = 0x54, kP = 0x104, kCarrier = 0xa90, kRamp = 0x90c;   // [FW] in the voice
constexpr int kInputBytes = 0x108, kUpdates = 30;

struct Counts {
    long cases = 0, updates = 0, ramps[3] = {}, different = 0;
};

// [FW] the record's VOCODER bytes by the input's offset, and their IDs' ranges (VC2_VOCAL_DESIGNER.md, "The VOCODER
// OSC's IDs, run"): the levels, Growl Level, the depths, the balance, the curves
constexpr Vc2ByteDraw kBytes[] = {
    { 0x6c, 0, 127 }, { 0x6d, 0, 127 }, { 0x6e, 0, 127 }, { 0x6f, 0, 127 }, { 0x70, 0, 127 }, { 0x7c, 0, 127 },
    { 0x7d, 0, 127 }, { 0x72, 0, 127 }, { 0x76, -63, 63 }, { 0x77, -63, 63 }, { 0x78, 0, 7 }, { 0x7e, 0, 127 },
    { 0x82, -63, 63 }, { 0x83, -63, 63 }, { 0x84, 0, 7 },
};
constexpr Vc2ByteDraw kHalfwords[] = { { 0x7a, -200, 200 }, { 0x86, -200, 200 } };   // the velocity sens
constexpr int kFollowAt = 0x74;   // [FW] Growl Keyfollow ×10, −2000…+2000

// Growl Keyfollow: 0 half the time, as most patches have it, else mostly a little (±20%), now and then any (±200%), so
// the level's own term survives the clamp as often as the keyfollow's
int drawKeyfollow(std::mt19937& rng) {
    if (rng() % 2 == 0) return 0;
    const int range = rng() % 4 == 0 ? 2000 : 200;
    return (int)(rng() % (uint32_t)(2 * range + 1)) - range;
}

// A converter input: the header, the VOCODER bytes and halfwords, the Matrix object's address; the rest 0
void drawInput(std::mt19937& rng, uint8_t* in) {
    std::memset(in, 0, kInputBytes);
    in[2] = (uint8_t)(rng() % 2);
    in[3] = (uint8_t)(rng() % 128);
    in[4] = (uint8_t)(1 + rng() % 127);
    for (const Vc2ByteDraw& b : kBytes) in[b.at] = (uint8_t)vc2DrawByte(rng, b.lo, b.hi);
    for (const Vc2ByteDraw& h : kHalfwords) vc2Put16(in + h.at, h.lo + (int)(rng() % (uint32_t)(h.hi - h.lo + 1)));
    vc2Put16(in + kFollowAt, drawKeyfollow(rng));
    std::memcpy(in + 0x100, &kMatrixAt, sizeof kMatrixAt);
}

// [FW] the LFO depths' bytes and the LFO group's sums they are added to (LFO Time, LFO Formant)
constexpr Vc2LfoEdges kLfoEdges { { 0x76, 0x82 }, { 2, 3 } };

// The groups: 0 half the time, else across their ranges; one case in four the LFO sums set so that sum + depth lands
// on a bound's edge, ±63 or ±64, where the clamp's off-by-one would show
void drawSums(std::mt19937& rng, const uint8_t* in, Vc2MatrixGroups& s) {
    for (int16_t& v : s.osc) v = rng() % 2 == 0 ? 0 : (int16_t)((int)(rng() % 24001) - 12000);
    vc2DrawLfoSums(rng, in, s.lfo, 64, kLfoEdges);
}

// [FW] the voice's words kind 5's update reads, by offset, and what is drawn into each (the second ratio now and then
// below 0, which no voice gives, so the ratio's lower bound is reached)
struct WordDraw {
    uint32_t at;
    float Vc2Kind5Voice::*field;
    float lo, hi;
};
constexpr WordDraw kWords[] = {
    { 0x5c, &Vc2Kind5Voice::cents, -7000.0f, 7000.0f }, { 0x70, &Vc2Kind5Voice::pitchEnvDepth, -1200.0f, 1200.0f },
    { 0x2c, &Vc2Kind5Voice::pitchMod, -1.0f, 1.0f }, { 0xfc, &Vc2Kind5Voice::pitchModDepth, -600.0f, 600.0f },
    { 0xa70, &Vc2Kind5Voice::centsOffset, -200.0f, 200.0f }, { 0x60, &Vc2Kind5Voice::ratio, 0.0f, 3.0f },
    { 0xa6c, &Vc2Kind5Voice::ratio2, -1.0f, 3.0f }, { 0x30, &Vc2Kind5Voice::gainMod, -1.0f, 1.0f },
    { 0x100, &Vc2Kind5Voice::gainModDepth, -1.0f, 1.0f },
};
constexpr uint32_t kGainAt[3] = { 0x88, 0xb0, 0x960 };   // [FW] the gain's other factors
// [FW] the five modulators: the voice's word pointing at each, and where each keeps its output
struct ModDraw {
    uint32_t pointerAt, outputAt;
    float Vc2Kind5Voice::*field;
    float lo;
};
constexpr ModDraw kMods[] = {
    { 0xa4c, 0x4c, &Vc2Kind5Voice::pitchEnv, 0.0f }, { 0xa50, 0x4c, &Vc2Kind5Voice::growlEnv, 0.0f },
    { 0xa60, 0x38, &Vc2Kind5Voice::growlLfo, -1.0f }, { 0xa54, 0x4c, &Vc2Kind5Voice::balanceEnv, 0.0f },
    { 0xa64, 0x38, &Vc2Kind5Voice::balanceLfo, -1.0f },
};

void putFloat(Vc2Fw& fw, uint32_t at, float v) { fw.cpu.wr32(at, std::bit_cast<uint32_t>(v)); }

// A voice's words, its modulators and its ramp drawn into rom.4's memory and the port's input
Vc2Kind5Voice drawVoice(Vc2Fw& fw, std::mt19937& rng, Counts& n) {
    Vc2Kind5Voice v {};
    for (const WordDraw& w : kWords) putFloat(fw, kVoiceAt + w.at, v.*w.field = vc2Within(rng, w.lo, w.hi));
    for (int i = 0; i < 3; ++i) putFloat(fw, kVoiceAt + kGainAt[i], v.gain[i] = vc2Within(rng, 0.0f, 1.0f));
    for (uint32_t k = 0; k < std::size(kMods); ++k) {
        const uint32_t object = kModsAt + 0x80 * k;
        fw.cpu.wr32(kVoiceAt + kMods[k].pointerAt, object);
        putFloat(fw, object + kMods[k].outputAt, v.*kMods[k].field = vc2Within(rng, kMods[k].lo, 1.0f));
    }
    const int ramp = (int)(rng() % 3);   // [FW] 0x893b1160: +4 set gives +0x3c; else +0 set gives 1; else 0
    ++n.ramps[ramp];
    const float value = vc2Within(rng, -300.0f, 300.0f);
    fw.cpu.wr32(kVoiceAt + kRamp + 4, ramp == 0 ? 1u : 0u);
    fw.cpu.wr32(kVoiceAt + kRamp, ramp == 1 ? 1u : 0u);
    putFloat(fw, kVoiceAt + kRamp + 0x3c, value);
    constexpr float kRampValues[2] = { 1.0f, 0.0f };
    v.ramp = ramp == 0 ? value : kRampValues[ramp - 1];
    return v;
}

std::string pDifference(Vc2Fw& fw, const Vc2VocoderOscP& p, const Vc2Carrier& c) {
    const std::string what = vc2FirstDifference(fw, kVoiceAt + kP, &p, sizeof p, "P");
    return what.empty() ? vc2FirstDifference(fw, kVoiceAt + kCarrier, &c.w, sizeof c.w, "the carrier's words") : what;
}

// One case: the converter on both, the carrier's init, then the updates
std::string oscCase(Vc2Fw& fw, std::mt19937& rng, int32_t& clock, Counts& n) {
    uint8_t in[kInputBytes];
    drawInput(rng, in);
    Vc2MatrixGroups sums {};
    drawSums(rng, in, sums);
    fw.cpu.write(kInputAt, in, sizeof in);
    fw.cpu.write(kOscSumsAt, sums.osc, sizeof sums.osc);
    fw.cpu.write(kLfoSumsAt, sums.lfo, sizeof sums.lfo);
    Vc2VocoderOscP p {};
    for (uint32_t w = 0; w < sizeof p / 4; ++w) {   // P noise on both, so a word the converter leaves stays as it was
        const float x = vc2Within(rng, -1.0f, 1.0f);
        std::memcpy(reinterpret_cast<uint8_t*>(&p) + 4 * w, &x, sizeof x);
    }
    fw.cpu.write(kVoiceAt + kP, &p, sizeof p);
    const RomShared shared(fw);
    Vc2Carrier carrier;
    std::string what = vc2BothOn(fw, kConvert, { kInputAt, kVoiceAt + kBlock }, "the converter",
                                 [&] { vc2VocoderOscConvert(in, sums, shared, p); },
                                 [&] { return vc2FirstDifference(fw, kVoiceAt + kP, &p, sizeof p, "P"); });
    clock = (int32_t)rng();
    if (what.empty()) {
        what = vc2BothOn(fw, kCarrierInit, { kVoiceAt + kCarrier, kVoiceAt + kP }, "the carrier's init",
                         [&] { carrier.init(p.carrier, clock); }, [&] { return pDifference(fw, p, carrier); });
    }
    for (int u = 0; u < kUpdates && what.empty(); ++u, ++n.updates) {
        const Vc2Kind5Voice v = drawVoice(fw, rng, n);
        const float peak = vc2Within(rng, 0.0f, 1.0f);
        fw.cpu.wr32(kPeakAt, std::bit_cast<uint32_t>(peak));
        what = vc2BothOn(fw, kKind5, { kVoiceAt }, "kind 5's update",
                         [&] { vc2Kind5Update(v, shared, p, carrier, peak); },
                         [&] { return pDifference(fw, p, carrier); });
    }
    return shared.faulted && what.empty() ? "a shared routine faults: " + fw.cpu.error() : what;
}
} // namespace

long vc2VocoderOscCases(Vc2Fw& fw, const Vc2Run& run, int32_t& clock) {
    vc2HookGroups(fw, kOscSumsAt, kLfoSumsAt);
    std::mt19937 rng((uint32_t)run.seed + 1);
    const bool first = xtSwitch("XT_VC2CARRIER_FIRST") != nullptr;
    Counts n;
    for (int c = 0; c < run.cases; ++c, ++n.cases) {
        const std::string what = oscCase(fw, rng, clock, n);
        if (what.empty()) continue;
        if (!n.different++ && first) std::printf("  osc case %d: %s\n", c, what.c_str());
        if (xtSwitch("XT_VC2CARRIER_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("vocoder osc: %ld cases, %ld updates (ramps running %ld, armed %ld, off %ld), %ld different\n",
                n.cases, n.updates, n.ramps[0], n.ramps[1], n.ramps[2], n.different);
    return n.different;
}
#endif
