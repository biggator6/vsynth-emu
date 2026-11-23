// The draws of what an OSC voice's own parts are given: see Vc2OscDraws.h (v319; xt_vc2osc's, moved: the vibrato's
// block and its fade's hold from Vc2OscVibratoCheck.cpp, the ramps' words from Vc2RampCheck.cpp, the mic ring from
// Vc2OscKindsCheck.cpp).  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include "Vc2OscVoiceKinds.h"
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
    shared.calls.clear();
    constexpr uint32_t kFramesPointerAt = 0x8993eadc;   // [FW] where the part reads the effect chip's frames from
    for (int32_t& w : d.frames) w = (int32_t)shared.renders();
    fw.cpu.write(kOscFramesAt, d.frames.data(), sizeof d.frames);
    fw.cpu.wr32(kFramesPointerAt, kOscFramesAt);
}

void oscDrawHalfwords(std::span<int16_t> wave, std::mt19937& rng) {
    constexpr int16_t kEdges[2] = { -32768, 32767 };
    for (int16_t& h : wave) h = rng() % 16 == 0 ? kEdges[rng() % 2] : (int16_t)rng();
}

namespace {
// [FW] LITE's heads' words their renders read, by their offsets in rom.4's object (Vc2OscHeads.cpp: XtLitePlayer.h's
// map four bytes up from +0x94): the pitch, the formant, the walk, the gain's target and the gain; the loop events'
// record; the beat's factor; the loops' modes, the samples' address, the heads, the filter's rate and its two states,
// the heads' two fractions, the walk's position, the windows' phases and clocks (A's two, then B's), the last sample,
// the forward loop (start, end, span) and its rate, the first sample, the backward loop (end, start, span) and its
// rate, the period and its inverse
// one-off: the offsets of LITE's heads' words, which only look like the analog's (Vc2OscAnalog.h): other words
namespace OscHeads {
constexpr uint32_t kPitchAt = 0x70, kFormantAt = 0x74, kWalkAt = 0x7c, kTargetAt = 0x80, kGainAt = 0x84;
constexpr uint32_t kEventsAt = 0x6c, kFactorAt = 0x8c, kFwdModeAt = 0xec, kBwdModeAt = 0xf0, kSamplesAt = 0xf4;
constexpr uint32_t kHeadAAt = 0xf8, kHeadBAt = 0xfc, kRateAt = 0x100, kFilterAt = 0x104, kFracAt = 0x10c;
constexpr uint32_t kWalkPosAt = 0x114, kWindowsAt = 0x118, kLastAt = 0x128, kFwdLoopAt = 0x12c;
constexpr uint32_t kFwdRateAt = 0x138, kFirstAt = 0x13c, kBwdLoopAt = 0x140, kBwdRateAt = 0x14c;
constexpr uint32_t kHeadsPeriodAt = 0x150, kInvPeriodAt = 0x154, kBeatPosAt = 0x64;
constexpr uint32_t kTailBitsAt = 0x03;   // [FW] byte 3, whose bits the beat block's tail reads
} // namespace OscHeads
constexpr uint8_t kBeatBits = 0x06;   // [FW] byte 0's Tempo Sync and Time Trip

// A loop's three words as floats, as the note-on writes them: its two ends in the record's order and their span
void putHeadsLoop(Vc2OscVoice& o, uint32_t at, int32_t first, int32_t second) {
    o.setF32(Vc2OscVoice::kObject + at, (float)first);
    o.setF32(Vc2OscVoice::kObject + at + 4, (float)second);
    o.setF32(Vc2OscVoice::kObject + at + 8, (float)(second > first ? second - first : first - second));
}

// The range and the loops, from frame 2500; the walk's position about the range, which it returns
float drawHeadsRange(Vc2OscVoice& o, std::mt19937& rng) {
    using namespace OscHeads;
    constexpr int32_t kLoopsFrom = 3000, kLoopsOver = 500, kSpanLeast = 64, kSpanOver = 800;
    const int32_t fwdStart = kLoopsFrom + (int32_t)(rng() % kLoopsOver);
    const int32_t fwdEnd = fwdStart + kSpanLeast + (int32_t)(rng() % kSpanOver);
    const int32_t bwdStart = kLoopsFrom + (int32_t)(rng() % kLoopsOver);
    const int32_t bwdEnd = bwdStart + kSpanLeast + (int32_t)(rng() % kSpanOver);
    putHeadsLoop(o, kFwdLoopAt, fwdStart, fwdEnd);
    putHeadsLoop(o, kBwdLoopAt, bwdEnd, bwdStart);
    const float first = (float)(kLoopsFrom - kLoopsOver + (int32_t)(rng() % (2 * kLoopsOver)));
    const float last = (float)(fwdEnd - 100 + (int32_t)(rng() % kLoopsOver));
    o.setF32(Vc2OscVoice::kObject + kFirstAt, first);
    o.setF32(Vc2OscVoice::kObject + kLastAt, last);
    return oscWithin(rng, first - 100.0f, last + 100.0f);
}
} // namespace

void oscDrawBeatWords(Vc2OscVoice& o, std::mt19937& rng) {
    using namespace OscHeads;
    const auto put = [&](uint32_t at, uint32_t word) { o.setU32(Vc2OscVoice::kObject + at, word); };
    const auto putReal = [&](uint32_t at, float v) { o.setF32(Vc2OscVoice::kObject + at, v); };
    putReal(kWalkAt, (rng() % 2 ? 1.0f : -1.0f) * oscWithin(rng, 0.1f, 3.0f));
    for (uint32_t at : { kFactorAt, kFactorAt + 4 }) putReal(at, oscWithin(rng, 0.0f, 2.0f));   // and its integral
    constexpr float kBeatUnits = 512.0f;   // [FW] the beat's position in 1/512 sample, about the loop events'
    put(kBeatPosAt, (uint32_t)(int32_t)(oscWithin(rng, 2400.0f, 5500.0f) * kBeatUnits));
    if (rng() % 4 != 0) o.setU8(Vc2OscVoice::kObject, o.u8(Vc2OscVoice::kObject) & (uint8_t)~kBeatBits);
    if (rng() % 2 != 0) o.setU8(Vc2OscVoice::kObject + kTailBitsAt, (uint8_t)rng());   // a hold, a turn, a sample
}

void oscDrawHeadsWords(Vc2OscVoice& o, std::mt19937& rng, uint32_t waveAt) {
    using namespace OscHeads;
    const auto put = [&](uint32_t at, uint32_t word) { o.setU32(Vc2OscVoice::kObject + at, word); };
    const auto putReal = [&](uint32_t at, float v) { o.setF32(Vc2OscVoice::kObject + at, v); };
    const float walkPos = drawHeadsRange(o, rng);
    const float period = oscWithin(rng, 50.0f, 400.0f);
    putReal(kWalkPosAt, walkPos);
    putReal(kHeadsPeriodAt, period);
    putReal(kInvPeriodAt, 1.0f / period);
    for (uint32_t at : { kHeadAAt, kHeadBAt }) {
        put(at, (uint32_t)(int32_t)oscWithin(rng, walkPos - period, walkPos + period));
    }
    for (uint32_t at : { kFracAt, kFracAt + 4 }) putReal(at, vc2Unit(rng));
    for (uint32_t k = 0; k < 4; ++k) putReal(kWindowsAt + 4 * k, period * vc2Unit(rng));
    for (uint32_t at : { kFwdModeAt, kBwdModeAt }) put(at, rng() % 3);
    for (uint32_t at : { kFwdRateAt, kBwdRateAt, kPitchAt, kFormantAt }) putReal(at, oscWithin(rng, 0.5f, 2.0f));
    oscDrawBeatWords(o, rng);
    for (uint32_t at : { kTargetAt, kGainAt }) putReal(at, oscWithin(rng, 0.0f, 0.5f));
    putReal(kRateAt, oscWithin(rng, 0.0f, 0.05f));
    for (uint32_t at : { kFilterAt, kFilterAt + 4 }) putReal(at, oscWithin(rng, -2.0e8f, 2.0e8f));
    put(kSamplesAt, waveAt);
    put(kEventsAt, oscPairAt((int)(rng() % kOscRecords)));
}

void oscDrawVariOffWords(Vc2OscVoice& o, std::mt19937& rng, uint32_t waveAt) {
    using namespace OscVariOff;
    constexpr int32_t kFirstFrame = 1024;
    const auto put = [&](uint32_t at, uint32_t word) { o.setU32(Vc2OscVoice::kObject + at, word); };
    const auto putReal = [&](uint32_t at, float v) { o.setF32(Vc2OscVoice::kObject + at, v); };
    for (uint32_t at = kFirst; at < kFirst + kBytes; at += 4) put(at, (uint32_t)rng());
    const int32_t span = 1 + (int32_t)(rng() % 600);
    const int32_t start = kFirstFrame + (int32_t)(rng() % 1024);
    const int32_t end = start + span;
    const int32_t past = end + (int32_t)(rng() % 64);
    const int32_t last = rng() % 4 != 0 ? past : start + (int32_t)(rng() % (uint32_t)span);
    const auto nearLoop = [&] { return start - 32 + (int32_t)(rng() % (uint32_t)(2 * span + 64)); };
    constexpr uint32_t kModes[8] = { 0, 1, 1, 1, 2, 2, 2, 2 };
    put(kPosAt, (uint32_t)nearLoop());
    put(kTurnAt, (uint32_t)nearLoop());
    put(kLastAt, (uint32_t)last);
    put(kStartAt, (uint32_t)start);
    put(kEndAt, (uint32_t)end);
    put(kSpanAt, (uint32_t)span);
    put(kSamplesAt, waveAt);
    put(kModeAt, kModes[rng() % 8]);
    put(kFoldAt, rng() % 3 == 0 ? (uint32_t)rng() | 1u : 0u);
    putReal(kPitchAt, 0.05f + 6.0f * vc2Unit(rng) * vc2Unit(rng));
    putReal(kRateAt, 0.5f + 1.5f * vc2Unit(rng));
    for (const uint32_t at : { kTargetAt, kGainAt, kFracAt, kFadeAt }) putReal(at, vc2Unit(rng));
}

bool oscRendersBy(const Vc2OscVoice& o, std::span<const uint32_t> renders) {
    return vc2OscKindRow(o.kind()).renders == Vc2OscRender::byRender
        && std::ranges::find(renders, o.tail.render) != renders.end();
}

void oscSteerRenders(Vc2OscVoice& o, OscShared& shared) {
    constexpr uint32_t kLevelAt = 0x1e0, kTargetAt = 0x1dc;   // [FW] the object's +0x84 and +0x80
    std::mt19937& rng = shared.renders;
    if (oscRendersBy(o, kOscVariOffRenders)) oscDrawVariOffWords(o, rng, kOscWaveAt);   // v323
    if (oscRendersBy(o, kOscHeadsRenders)) oscDrawHeadsWords(o, rng, kOscWaveAt);       // v326
    if (oscRendersBy(o, kOscModeOneRenders)) oscDrawModeOneWords(o, rng, shared.records, kOscWaveAt);   // v327
    oscSteerBands(o, shared);                                                                           // v332
    if (!oscRendersBy(o, kOscExtInRenders)) return;
    for (uint32_t at : { kLevelAt, kTargetAt }) {
        if (!(std::fabs(o.f32(at)) <= 1.0f)) o.setF32(at, vc2Unit(rng));
    }
}

bool oscSteerLiteSettings(Vc2OscVoice& o, std::span<uint32_t> settings, uint32_t voiceAt, std::mt19937& rng) {
    constexpr uint32_t kLite = 1, kHeadAt = 0x1a0, kHeadOf = 0x108, kTypeWord = 0xb0 / 4, kTypes = 3;   // [FW]
    // [FW] the settings' switches LITE's settings test, from its +0xb0: the three bits' and the seek's ask
    constexpr std::array<uint32_t, 4> kSwitches { 0x34, 0x38, 0x3c, 0x40 };
    if (o.kind() != kLite) return false;
    o.setU32(kHeadAt, voiceAt + kHeadOf);
    if (rng() % 2) settings[kTypeWord] = (uint32_t)(rng() % kTypes);
    for (uint32_t at : kSwitches) {
        if (rng() % 2) settings[kTypeWord + at / 4] = rng() % 2;
    }
    oscSteerLiteAsk(o, settings.subspan(kTypeWord));   // v332: lite-set-ask-at had been one case's chance
    return settings[kTypeWord] < kTypes;
}

void oscDrawWave(Vc2Fw& fw, OscShared& shared) {
    const std::span<int16_t> wave(shared.wave.data(), kOscWaveHalfwords);   // v332: the band waves above it stay
    oscDrawHalfwords(wave, shared.renders);
    fw.cpu.write(kOscWaveAt, wave.data(), wave.size_bytes());
    oscDrawEventTable(fw, shared);   // v327
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
