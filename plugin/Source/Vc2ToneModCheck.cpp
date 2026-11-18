// xt_vc2tone's envelopes and LFOs (v268): see Vc2ToneSides.h.  Their objects against rom.4's word for word, the blocks a
// note-on is given as the converters write them, and the words the tone's events find drawn on both sides.
// XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include <algorithm>
#include <cstddef>

namespace {
using In = Vc2ToneInput;
constexpr uint32_t kCurves[] = { 0, 2 };          // [FW] 0x8939a412…0x8939a420: what the converter writes
constexpr uint32_t kConverterClock = 0x124f80;    // [FW] 0x8939a426: its +0x0c and +0x10
constexpr int kTimes = 4, kLevels = 5;
constexpr int kWaves = 8, kOffsets = 5, kFadeModes = 4;
constexpr uint32_t kFewestClocks = 3, kClockChoices = 190, kClocksDrawnPast = 300;   // a note value's clocks, 3…192
constexpr float kLongestMs = 12000.0f, kTickMs = 10.0f, kFullFadeMs = 10000.0f, kMostHz = 100.0f, kLeastHz = 0.01f;
constexpr float kSlowestBpm = 20.0f, kBpmSpan = 280.0f, kPhaseSpan = 65535.0f;
constexpr float kLeastClockTime = 0.01f, kClockTimeSpan = 0.115f, kLeastScale = 0.25f;

uint32_t bits(float x) { return std::bit_cast<uint32_t>(x); }

// An envelope stage's time in ms: none, under a tick, a tick, or any
float drawStageMs(std::mt19937& rng) {
    const uint32_t pick = rng() % 4;
    if (pick == 0) return 0.0f;
    if (pick == 1) return kTickMs * vc2Unit(rng);
    return pick == 2 ? kTickMs : (float)(int)(kLongestMs * vc2Unit(rng));
}

// [FW] an envelope's block, the words 0x893ac1c0 copies: curve, scale (the converter's 1, and below it so that the scale
// is seen to reach the rates), sync, two clock words, the times, the levels: the ADSR's or a depth's
void drawEnvelopeBlock(uint32_t* b, std::mt19937& rng) {
    b[0] = kCurves[rng() % std::size(kCurves)];
    b[1] = bits(rng() % 2 == 0 ? 1.0f : kLeastScale + (1.0f - kLeastScale) * vc2Unit(rng));
    b[2] = 0;
    b[3] = b[4] = kConverterClock;
    std::generate_n(b + 5, kTimes, [&] { return bits(drawStageMs(rng)); });
    const bool adsr = rng() % 2 == 0;
    const float sustain = vc2DrawLevel(rng);
    const float shape[kLevels] = { 0.0f, 1.0f, 1.0f, sustain, 0.0f };
    for (int l = 0; l < kLevels; ++l) b[5 + kTimes + l] = bits(adsr ? shape[l] : 2.0f * vc2Unit(rng) - 1.0f);
}

// A fade or delay as the converter's (k/127)² × 10 s: none half the time
float drawFadeMs(std::mt19937& rng) {
    const float k = rng() % 2 == 0 ? 0.0f : vc2DrawLevel(rng);
    return k * k * kFullFadeMs;
}

// [FW] an LFO's block, the words 0x893acda0 copies but the key sync at +0x10, which the note-on decides by
void drawLfoBlock(uint32_t* b, std::mt19937& rng) {
    const bool sync = rng() % 3 == 0;
    const float unit = vc2Unit(rng);
    // A note value's clocks, now and then past either end of what the note-on keeps
    const uint32_t clocks = rng() % 4 == 0 ? 1 + rng() % kClocksDrawnPast : kFewestClocks + rng() % kClockChoices;
    b[0] = rng() % kWaves;
    b[1] = bits(sync ? (float)clocks : kLeastHz + unit * unit * kMostHz);
    b[2] = bits(drawFadeMs(rng));
    b[3] = bits(drawFadeMs(rng));
    b[5] = sync ? 1 : 0;
    b[6] = rng() % kOffsets;
    b[7] = rng() % kFadeModes;
}

// A word of a part set on both sides
template <class Word>
void bothAt(ToneSides& s, uint32_t at, Word& mine, Word value) {
    mine = value;
    s.fw.cpu.write(at, &value, sizeof value);
}

// An LFO's clock words as a note value leaves them, or no note value
void drawClock(ToneSides& s, uint32_t at, Vc2LfoWords& w, std::mt19937& rng) {
    using W = Vc2LfoWords;
    const int32_t sync = (int32_t)(rng() % 2);
    bothAt(s, at + offsetof(W, sync), w.sync, sync);
    if (sync == 0) return;
    const float clocks = (float)(kFewestClocks + rng() % kClockChoices);
    bothAt(s, at + offsetof(W, rate), w.rate, clocks);
    bothAt(s, at + offsetof(W, clockStep), w.clockStep, kPhaseSpan / clocks);
    bothAt(s, at + offsetof(W, clockTarget), w.clockTarget, kPhaseSpan * vc2Unit(rng));
    bothAt(s, at + offsetof(W, clockTime), w.clockTime, kLeastClockTime + kClockTimeSpan * vc2Unit(rng));
    bothAt(s, at + offsetof(W, clockWrap), w.clockWrap, (int32_t)(rng() % 2));
    bothAt(s, at + offsetof(W, clockCount), w.clockCount, (int32_t)(rng() % (uint32_t)clocks));
}

// A tone's parts of one kind against rom.4's words, each with the number the tone keeps for it
template <class Part, size_t N>
std::string partsDifference(Vc2Fw& fw, const std::array<Part, N>& parts, const std::array<int32_t, N>& numbers,
                            uint32_t tone, uint32_t (*partAt)(uint32_t, int), const char* name) {
    std::string what;
    for (size_t k = 0; k < N && what.empty(); ++k) {
        auto words = parts[k].w;
        words.index = numbers[k];
        what = vc2FirstDifference(fw, partAt(tone, (int)k), &words, sizeof words, name);
    }
    return what;
}
} // namespace

std::string toneModulatorsDifference(ToneSides& s, int i) {
    const Vc2Tone& t = s.pool.tones[(size_t)i];
    const Vc2ToneModulators& m = s.parts[(size_t)i].mods;
    const uint32_t at = s.pool.addressOf(i);
    const std::string what = partsDifference(s.fw, m.envelopes, t.envelopeIndex, at, toneEnvelopeAt, "an envelope");
    return what.empty() ? partsDifference(s.fw, m.lfos, t.lfoIndex, at, toneLfoAt, "an LFO") : what;
}

void toneDrawModulatorBlocks(Vc2ToneInput& in, std::mt19937& rng) {
    for (uint32_t e = 0; e < Vc2Tone::kEnvelopes; ++e) {
        drawEnvelopeBlock(&in.at(In::kEnvelopes + In::kEnvelopeStride * e), rng);
    }
    for (uint32_t l = 0; l < Vc2Tone::kLfos; ++l) drawLfoBlock(&in.at(In::kLfos + In::kLfoStride * l), rng);
    in.at(In::kLfoValue) = bits(kSlowestBpm + kBpmSpan * vc2Unit(rng));
}

// What the tone reads of them: an envelope ticks only with an output routine, which only its note-on gives it, but for
// envelope 0, which the tone never ticks; and a word only the LFO's constructor clears, as a last use might leave it
void toneDrawModulatorWords(ToneSides& s, int i, std::mt19937& rng) {
    Vc2ToneModulators& m = s.parts[(size_t)i].mods;
    const uint32_t tone = s.pool.addressOf(i);
    for (int e = 0; e < Vc2Tone::kEnvelopes; ++e) {
        Vc2ToneEnvelope& envelope = m.envelopes[(size_t)e];
        const uint32_t on = e == 0 || envelope.started() ? rng() % 2 : 0u;
        bothAt(s, toneEnvelopeAt(tone, e) + offsetof(Vc2EnvelopeWords, on), envelope.w.on, on);
    }
    for (int l = 0; l < Vc2Tone::kLfos; ++l) {
        Vc2LfoWords& w = m.lfos[(size_t)l].w;
        const uint32_t at = toneLfoAt(tone, l);
        bothAt(s, at + offsetof(Vc2LfoWords, on), w.on, rng() % 2);
        bothAt(s, at + offsetof(Vc2LfoWords, follow), w.follow, rng() % 2 == 0 ? 0u : (uint32_t)rng());
        bothAt(s, at + offsetof(Vc2LfoWords, unread3c), w.unread3c, vc2Unit(rng));
        if (rng() % 4 == 0) drawClock(s, at, w, rng);
    }
}
#endif
