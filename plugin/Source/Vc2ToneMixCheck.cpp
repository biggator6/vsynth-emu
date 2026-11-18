// xt_vc2tone's part for the real OSC mix (v269): see Vc2ToneSides.h.  rom.4's own constructor, note-on, install, update and
// eight renders run beside Vc2ToneMix: the mix's 0x40 words, the tail's words it shares, the output buffer and the delay
// lines compared.  The buffers a render reads are drawn on both sides, within the OSCs' full scale, and the block's
// targets within 0…1: a delay past that would read outside rom.4's lines.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include "Vc2ToneMixTables.h"
#include <algorithm>
#include <cstddef>

namespace {
using In = Vc2ToneInput;
// [FW] the mix in a tone, its routines, the renders' buffers by number
constexpr uint32_t kMixAt = 0x1ab8, kNoteOnAt = 0x893adbf0, kInstallAt = 0x893ade90, kBlockAt = 0x893ade50;
constexpr uint32_t kBufferStride = 0x100, kSyncAt = 0x1b00;
constexpr uint32_t kMemberPointerAt = 0x1b04;   // [FW] the tail's two words the install sets before the routine's
constexpr size_t kDelayMono = 2, kDelayStereo = 3;   // the delays' numbers in kVc2MixRoutines
constexpr int kTargets = 4, kMostBlocks = 12;
constexpr uint32_t kBufferA = kToneBufferAt, kBufferB = kToneBufferAt + kBufferStride;
constexpr uint32_t kBufferC = kToneBufferAt + 2 * kBufferStride;

float drawMixTarget(std::mt19937& rng) {
    const uint32_t pick = rng() % 8;
    return pick == 0 ? 0.0f : (pick == 1 ? 1.0f : vc2Unit(rng));
}

// A sample of a buffer as an OSC voice might leave it, at a level drawn for the buffer
using SampleDraw = float (*)(float level, std::mt19937& rng);
constexpr SampleDraw kSignals[] = {
    [](float, std::mt19937&) { return 0.0f; },                                // silence
    [](float level, std::mt19937&) { return level; },                         // a level held
    [](float, std::mt19937& rng) {                                            // full scale, either way
        const float full = vc2ToneMixTables().fullScale;
        return rng() % 2 == 0 ? full : -full;
    },
    [](float level, std::mt19937& rng) { return level * (2.0f * vc2Unit(rng) - 1.0f); },   // noise
};

void drawMixSignal(float* to, std::mt19937& rng) {
    const float level = vc2ToneMixTables().fullScale * vc2Unit(rng);
    const SampleDraw sample = kSignals[std::min<size_t>(rng() % 6, std::size(kSignals) - 1)];   // noise half the time
    std::generate_n(to, Vc2ToneBuffers::kFloats, [&] { return sample(level, rng); });
}

// The render the tone's install chose, its number in the table
size_t installedMixRender(const Vc2Tone& tone) {
    const auto* found = std::ranges::find(kVc2MixRoutines, tone.tail.unread[kVc2MixRoutineWord]);
    return (size_t)(found - kVc2MixRoutines.begin());
}

// A mix's words set on both sides
template <class Word>
void mixBothWords(ToneSides& s, int i, size_t offset, const Word& word) {
    s.fw.cpu.write(s.pool.addressOf(i) + kMixAt + (uint32_t)offset, &word, sizeof word);
}

// What a block reads, on both sides: the OSCs' buffers, the sync mask, half the time new targets (as the settings update
// 0x893add60 writes them during a note: the gains then ramp), and a delay's gain within 0…1, as the held render, run
// between a delay's note-on and its install, leaves samples there and rom.4 would read outside its lines
void drawMixBlockInputs(ToneSides& s, int i, std::mt19937& rng) {
    Vc2ToneBuffers& b = s.shared.buffers;
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    Vc2MixWords& w = s.parts[(size_t)i].mix.w;
    drawMixSignal(b.a, rng);
    drawMixSignal(b.b, rng);
    s.fw.cpu.write(kBufferA, b.a, sizeof b.a);
    s.fw.cpu.write(kBufferB, b.b, sizeof b.b);
    toneBothWord(s, i, kSyncAt, tone.tail.sync, rng() % 2 == 0 ? 0u : (uint32_t)rng());
    if (rng() % 2 == 0) {
        std::ranges::generate(w.target, [&] { return drawMixTarget(rng); });
        mixBothWords(s, i, offsetof(Vc2MixWords, target), w.target);
    }
    const size_t render = installedMixRender(tone);
    if ((render == kDelayMono || render == kDelayStereo) && !(w.gain[0] >= 0.0f && w.gain[0] <= 1.0f)) {
        w.gain[0] = drawMixTarget(rng);
        mixBothWords(s, i, offsetof(Vc2MixWords, gain), w.gain[0]);
    }
}

std::string mixLineDifference(ToneSides& s, uint32_t at, const Vc2ToneMixRings::Ring& line) {
    return vc2FirstDifference(s.fw, at, line.data(), sizeof line, "a delay line");
}

// The two delay lines of a tone's number
std::string mixLinesOfNumber(ToneSides& s, uint32_t number) {
    const uint32_t a = Vc2ToneMixRings::kAAt + number * Vc2ToneMixRings::kBytes;
    const uint32_t b = Vc2ToneMixRings::kBAt + number * Vc2ToneMixRings::kBytes;
    const std::string what = mixLineDifference(s, a, s.shared.rings.a(a));
    return what.empty() ? mixLineDifference(s, b, s.shared.rings.b(b)) : what;
}

// The install, on noise in the words it must set
std::string toneMixInstallOn(ToneSides& s, int i, std::mt19937& rng) {
    Vc2ToneTail& tail = s.pool.tones[(size_t)i].tail;
    std::generate_n(tail.unread, 2, [&] { return (uint32_t)rng(); });
    s.fw.cpu.write(s.pool.addressOf(i) + kMemberPointerAt, tail.unread, 2 * sizeof(uint32_t));
    const bool kind3 = rng() % 2 == 0;
    const uint32_t mix = s.pool.addressOf(i) + kMixAt;
    return vc2BothOn(s.fw, kInstallAt, { mix, kind3 ? 1u : 0u }, "the mix's install",
                     [&] { s.parts[(size_t)i].installMix(kind3); }, [&] { return toneDifference(s, i); });
}

// A block through the mix alone, rom.4's render against the port's
std::string toneMixBlockOn(ToneSides& s, int i, std::mt19937& rng) {
    Vc2ToneBuffers& b = s.shared.buffers;
    drawMixBlockInputs(s, i, rng);
    ++s.n.mixBlocks[installedMixRender(s.pool.tones[(size_t)i])];
    const uint32_t mix = s.pool.addressOf(i) + kMixAt;
    const std::string what = vc2BothOn(s.fw, kBlockAt, { mix, kBufferA, kBufferB, kBufferC }, "the mix's render",
                                       [&] { s.parts[(size_t)i].renderMix(b.a, b.b, b.c); },
                                       [&] { return toneDifference(s, i); });
    return what.empty() ? toneMixRendered(s, i) : what;
}
} // namespace

std::string toneMixDifference(ToneSides& s, int i) {
    const Vc2MixWords& w = s.parts[(size_t)i].mix.w;
    return vc2FirstDifference(s.fw, s.pool.addressOf(i) + kMixAt, &w, sizeof w, "the mix's words");
}

std::string toneMixRingsOf(ToneSides& s, int i) {
    return mixLinesOfNumber(s, (uint32_t)s.pool.tones[(size_t)i].tail.index);
}

std::string toneMixRingsDifference(ToneSides& s) {
    std::string what;
    for (uint32_t k = 0; k < Vc2ToneMixRings::kTones && what.empty(); ++k) what = mixLinesOfNumber(s, k);
    return what;
}

void toneDrawMixBlock(Vc2ToneInput& in, std::mt19937& rng) {
    in.at(In::kMix) = rng() % 8 == 0 ? (uint32_t)rng() : rng() % 6;
    for (uint32_t k = 1; k <= kTargets; ++k) {
        in.at(In::kMix + k * sizeof(uint32_t)) = std::bit_cast<uint32_t>(drawMixTarget(rng));
    }
}

std::string toneMixPrepareRender(ToneSides& s, int i, std::mt19937& rng) {
    const bool installed = installedMixRender(s.pool.tones[(size_t)i]) < kVc2MixRoutines.size();
    const std::string what = installed ? "" : toneMixInstallOn(s, i, rng);
    drawMixBlockInputs(s, i, rng);
    return what;
}

std::string toneMixRendered(ToneSides& s, int i) {
    const auto& c = s.shared.buffers.c;
    const std::string what = vc2FirstDifference(s.fw, kBufferC, c, sizeof c, "the mix's output");
    return what.empty() ? toneMixRingsOf(s, i) : what;
}

// A note-on from a drawn block and flags, the install, then a run of blocks, each render's state carried to the next
std::string toneMixPlayOn(ToneSides& s, std::mt19937& rng) {
    const int i = s.live[rng() % s.live.size()];
    tonePrepare(s, i, rng);
    In& in = s.shared.input;
    toneDrawMixBlock(in, rng);
    s.fw.cpu.write(kToneInputAt + In::kMix, in.block(In::kMix), (kTargets + 1) * sizeof(uint32_t));
    const uint32_t flag1 = rng() % 2, flag2 = rng() % 2, mix = s.pool.addressOf(i) + kMixAt;
    ++s.n.mixPlays;
    std::string what = vc2BothOn(s.fw, kNoteOnAt, { mix, kToneInputAt + In::kMix, flag1, flag2 }, "the mix's note-on",
                                 [&] { s.parts[(size_t)i].noteOnMix(in.block(In::kMix), flag1, flag2); },
                                 [&] { return toneDifference(s, i); });
    if (what.empty()) what = toneMixRingsOf(s, i);
    if (what.empty()) what = toneMixInstallOn(s, i, rng);
    const int blocks = 1 + (int)(rng() % kMostBlocks);
    for (int k = 0; k < blocks && what.empty(); ++k) what = toneMixBlockOn(s, i, rng);
    return what;
}
#endif
