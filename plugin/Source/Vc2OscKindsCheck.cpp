// xt_vc2osc's kinds 5 and 6 (v266): the carrier and the pitch shifter on the voice, rom.4's own routines against the
// port's words.  What they read from outside the voice is drawn on both sides: the OS's clock, the mic ring with its
// periods, Unvoice Level and the unvoiced decision.  Their inputs stay as a note gives them: rom.4 runs these on what the
// converters write, and noise in a gain or a coefficient would take both sides to NaNs, whose payloads are the host's.
// XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>

namespace {
// [FW] the static initializers: the carriers' rate (0x899d9964) and kind 6's globals; the ring's reset and write
constexpr uint32_t kCarrierRate = 0x893ec6a0, kKind6Globals = 0x893ecef0, kRingReset = 0x893ec7d0;
constexpr uint32_t kRingWrite = 0x893ec820, kCarrierInit = 0x893ebed0, kKind6Start = 0x893ec700;
// [FW] the ring, Unvoice Level, the tracker's period and the unvoiced decision, in rom.4's memory
constexpr uint32_t kRingAt = 0x8a0f31cc, kUnvoiceLevelAt = 0x899d9968, kPeriodAt = 0x899d9bd0, kUnvoicedAt = 0x899d99dc;
constexpr uint32_t kCarrier = 0xa90, kKind6 = 0xafc;   // [FW] where the two lie in a voice
constexpr uint32_t kCarrierKind = 5, kKind6Kind = 6;
// [FW] the voice's words the targets give P from: the ratio's scale, the gain's two factors, the vibrato's depth on them,
// kind 6's level, the formant's base and its envelope's and LFO's depths
constexpr uint32_t kRatioScale = 0x60, kLevel = 0x88, kGain = 0xb0, kLevelDepth = 0x100, kKind6Level = 0x6c;
constexpr uint32_t kFormantBase = 0x68, kFormantEnvelope = 0x80, kFormantLfo = 0xa0;
// [FW] P's levels the carrier's init and settings take: Saw Wave LPF and Level, Noise LPF, HPF and Level
constexpr std::array<uint32_t, 5> kCarrierLevels { 0x110, 0x114, 0x118, 0x11c, 0x120 };
constexpr uint32_t kSettingsWordsBelow = 0x54;   // [FW] a voice's word lies this far above its place in the settings
constexpr float kFormantDepth = 0.001f;          // with an output of up to ±1000, an octave of the formant

float within(std::mt19937& rng, float lo, float hi) { return lo + (hi - lo) * vc2Unit(rng); }

// The words as a note gives them, each put at its voice offset
template <class Put>
void drawNoteWords(Put put, std::mt19937& rng) {
    put(kRatioScale, within(rng, 0.5f, 2.0f));
    put(kLevel, vc2Unit(rng));
    put(kGain, vc2Unit(rng));
    put(kLevelDepth, within(rng, -1.0f, 1.0f));
    put(kKind6Level, vc2Unit(rng));
    put(kFormantBase, within(rng, -1.0f, 1.0f));
    put(kFormantEnvelope, within(rng, -kFormantDepth, kFormantDepth));
    put(kFormantLfo, within(rng, -kFormantDepth, kFormantDepth));
    for (uint32_t at : kCarrierLevels) put(at, vc2DrawLevel(rng));
}

Vc2CarrierParams drawCarrierP(std::mt19937& rng) {
    Vc2CarrierParams p {};
    p.gain = vc2Unit(rng);
    p.pitch = within(rng, 0.25f, 4.0f);
    for (float* level : { &p.sawCutoff, &p.sawLevel, &p.noiseLowpass, &p.noiseHighpass, &p.noiseLevel }) {
        *level = vc2DrawLevel(rng);
    }
    return p;
}

Vc2Kind6Params drawKind6P(std::mt19937& rng) {
    return { within(rng, 0.25f, 4.0f), within(rng, 0.5f, 2.0f), vc2Unit(rng), vc2Unit(rng) };
}

// A block of the mic into the ring on both sides, with the tracker's period and the unvoiced decision for it
std::string writeMic(OscSides& s, std::mt19937& rng) {
    std::array<float, Vc2Kind6::kBlock> mic {};
    const float level = vc2Unit(rng);
    for (float& x : mic) x = within(rng, -level, level);
    const float period = vc2DrawPeriod(rng);
    s.fw.cpu.write(kOscMicAt, mic.data(), sizeof mic);
    s.fw.cpu.write(kPeriodAt, &period, sizeof period);
    for (float& u : s.shared.unvoiced) u = rng() % 2 ? 0.0f : vc2Unit(rng);
    s.fw.cpu.write(kUnvoicedAt, s.shared.unvoiced.data(), sizeof s.shared.unvoiced);
    return vc2BothOn(s.fw, kRingWrite, { kOscMicAt, (uint32_t)Vc2Kind6::kBlock }, "the ring's write",
                     [&] { s.shared.ring.write(mic.data(), period); }, [] { return std::string(); });
}

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

bool oscIsVoiced(uint32_t kind) { return kind == kCarrierKind || kind == kKind6Kind; }

std::string oscRingDifference(OscSides& s) {
    const Vc2MicRing& ring = s.shared.ring;
    return vc2RegionsDifference(s.fw, { { kRingAt, &ring.w, sizeof ring.w, "the mic ring" },
                                        { kUnvoiceLevelAt, &ring.g, sizeof ring.g, "kind 6's globals" } });
}

std::string oscKindsSetUp(OscSides& s) {
    if (!s.fw.call(kCarrierRate, {})) return "the carriers' rate setter faults: " + s.fw.cpu.error();
    return vc2BothOn(s.fw, kKind6Globals, {}, "kind 6's globals' setter", [&] { s.shared.ring.setGlobals(); },
                     [&] { return oscRingDifference(s); });
}

std::string oscKindsCase(OscSides& s, std::mt19937& rng) {
    Vc2MicRing& ring = s.shared.ring;
    ring.g.unvoiceLevel = vc2DrawLevel(rng);
    s.fw.cpu.write(kUnvoiceLevelAt, &ring.g.unvoiceLevel, sizeof ring.g.unvoiceLevel);
    s.shared.draws.clock = (int32_t)rng();
    std::string what = vc2BothOn(s.fw, kRingReset, {}, "the ring's reset", [&] { ring.reset(); },
                                 [&] { return oscRingDifference(s); });
    // the whole ring written, so that a voice's reads 512 samples behind the write point find the mic
    for (int block = 0; block < Vc2MicRingWords::kBlocks && what.empty(); ++block) what = writeMic(s, rng);
    if (what.empty()) what = oscRingDifference(s);
    for (int v = 0; v < kOscVoices && what.empty(); ++v) what = startParts(s, rng, v);
    return what;
}

void oscDrawVoicedVoice(Vc2OscVoice& o, std::mt19937& rng) {
    if (!oscIsVoiced(o.kind())) return;
    drawNoteWords([&](uint32_t at, float x) { o.setF32(at, x); }, rng);
    o.tail.rateRatio = within(rng, 0.5f, 2.0f);
    o.tail.formantFactor = within(rng, 0.5f, 2.0f);
}

void oscDrawVoicedWords(std::span<uint32_t> words, std::mt19937& rng, uint32_t below) {
    drawNoteWords([&](uint32_t at, float x) { words[(at - below) / 4] = std::bit_cast<uint32_t>(x); }, rng);
}

void oscDrawVoicedSettings(std::span<uint32_t> settings, std::mt19937& rng) {
    oscDrawVoicedWords(settings, rng, kSettingsWordsBelow);
}

std::string oscMicBlock(OscSides& s, std::mt19937& rng) {
    const std::string what = writeMic(s, rng);
    return what.empty() ? oscRingDifference(s) : what;
}

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
    return vc2FirstDifference(s.fw, kRingAt + kStarted, &s.shared.ring.w.started, sizeof(int32_t), "the ring's start");
}
#endif
