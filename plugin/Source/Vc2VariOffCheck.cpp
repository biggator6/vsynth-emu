// xt_vc2osc's Vari off part (v261): vc2OscVariOffMono and vc2OscVariOffStereo against rom.4's own 0x893c63c0 and 0x893c66e0
// on an object and a wave in the interpreter's RAM, before the voice part hooks anything.  Each case a wave of drawn
// samples (now and then a full-scale one) and an object: the words the renders read drawn about a loop, the rest noise;
// then blocks of either render, the words drawn again one block in eight; after each the output and the object's words
// +0x70…+0x130.  Not drawn: a loop's length of 0 or less (rom.4's wrap never ends), a mode past 2 (rom.4 jumps into its
// table), a pitch of 0 or less (its notes have none).  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"
#include "Vc2OscVariOffRender.h"
#include <cstring>
#include <memory>
#include <vector>

namespace {
constexpr uint32_t kMono = 0x893c63c0, kStereo = 0x893c66e0;   // [FW]
constexpr uint32_t kObjectAt = 0x8C090000, kOutAt = 0x8C091000, kWaveAt = 0x8C0A0000;   // in Vc2Fw's RAM
constexpr uint32_t kFirst = 0x70, kBytes = 0xc0;   // the object's words compared
constexpr uint32_t kHalfwords = 16384;
constexpr int kOutWords = 64, kBlocks = 60, kFirstFrame = 1024;
// [FW] the object's words the renders read, as Vc2OscVariOffRender.cpp names them
constexpr uint32_t kPitchAt = 0x70, kTargetAt = 0x80, kGainAt = 0x84, kFracAt = 0xec, kPosAt = 0xf0, kSamplesAt = 0xf4;
constexpr uint32_t kLastAt = 0xf8, kStartAt = 0xfc, kEndAt = 0x100, kSpanAt = 0x104, kRateAt = 0x108, kModeAt = 0x10c;
constexpr uint32_t kFoldAt = 0x124, kFadeAt = 0x128, kTurnAt = 0x12c;

struct VariCounts {
    long blocks = 0, mono = 0, stereo = 0, folded = 0, silent = 0, loops = 0, turns = 0, ending = 0;
};

struct VariRig {
    Vc2Fw& fw;
    std::vector<int16_t> wave = std::vector<int16_t>(kHalfwords);
    Vc2OscVoice voice {};
    VariCounts n {};
};

uint32_t objectWordOf(const VariRig& s, uint32_t at) { return s.voice.u32(Vc2OscVoice::kObject + at); }
void putObjectWord(VariRig& s, uint32_t at, uint32_t word) { s.voice.setU32(Vc2OscVoice::kObject + at, word); }
void putObjectReal(VariRig& s, uint32_t at, float v) { s.voice.setF32(Vc2OscVoice::kObject + at, v); }

void drawVariWave(VariRig& s, std::mt19937& rng) {
    constexpr int16_t kEdges[2] = { -32768, 32767 };
    for (int16_t& h : s.wave) h = rng() % 16 == 0 ? kEdges[rng() % 2] : (int16_t)rng();
    s.fw.cpu.write(kWaveAt, s.wave.data(), s.wave.size() * sizeof(int16_t));
}

// The words: noise under them all; a loop of 1…600 frames; a last sample past its end three times in four, else inside it,
// where the hold is; the two positions about the loop and its way back; a mode, a fold one time in three
void drawVariWords(VariRig& s, std::mt19937& rng) {
    for (uint32_t at = kFirst; at < kFirst + kBytes; at += 4) putObjectWord(s, at, (uint32_t)rng());
    const int32_t span = 1 + (int32_t)(rng() % 600);
    const int32_t start = kFirstFrame + (int32_t)(rng() % 1024);
    const int32_t end = start + span;
    const int32_t past = end + (int32_t)(rng() % 64);
    const int32_t last = rng() % 4 != 0 ? past : start + (int32_t)(rng() % (uint32_t)span);
    const auto nearLoop = [&] { return start - 32 + (int32_t)(rng() % (uint32_t)(2 * span + 64)); };
    constexpr uint32_t kModes[8] = { 0, 1, 1, 1, 2, 2, 2, 2 };
    putObjectWord(s, kPosAt, (uint32_t)nearLoop());
    putObjectWord(s, kTurnAt, (uint32_t)nearLoop());
    putObjectWord(s, kLastAt, (uint32_t)last);
    putObjectWord(s, kStartAt, (uint32_t)start);
    putObjectWord(s, kEndAt, (uint32_t)end);
    putObjectWord(s, kSpanAt, (uint32_t)span);
    putObjectWord(s, kSamplesAt, kWaveAt);
    putObjectWord(s, kModeAt, kModes[rng() % 8]);
    putObjectWord(s, kFoldAt, rng() % 3 == 0 ? (uint32_t)rng() | 1u : 0u);
    putObjectReal(s, kPitchAt, 0.05f + 6.0f * vc2Unit(rng) * vc2Unit(rng));
    putObjectReal(s, kRateAt, 0.5f + 1.5f * vc2Unit(rng));
    for (const uint32_t at : { kTargetAt, kGainAt, kFracAt, kFadeAt }) putObjectReal(s, at, vc2Unit(rng));
}

void tallyVariBlock(VariRig& s, bool stereo) {
    const uint32_t mode = objectWordOf(s, kModeAt);
    s.n.stereo += stereo;
    s.n.mono += !stereo;
    s.n.folded += stereo && objectWordOf(s, kFoldAt) != 0;
    s.n.silent += mode == 0;
    s.n.loops += mode == 1;
    s.n.turns += mode == 2;
    s.n.ending += objectWordOf(s, kEndAt) > objectWordOf(s, kLastAt);
}

// One block of a render on both sides, the output noise first
std::string variBlock(VariRig& s, std::mt19937& rng) {
    uint32_t noise[kOutWords];
    for (uint32_t& w : noise) w = (uint32_t)rng();
    float out[kOutWords];
    std::memcpy(out, noise, sizeof out);
    s.fw.cpu.write(kOutAt, noise, sizeof noise);
    const uint8_t* mine = s.voice.at(Vc2OscVoice::kObject + kFirst);
    s.fw.cpu.write(kObjectAt + kFirst, mine, kBytes);
    const bool stereo = rng() % 2 == 0;
    tallyVariBlock(s, stereo);
    const Vc2OscSamples wave { s.wave.data(), kWaveAt, kHalfwords };
    return vc2BothOn(
        s.fw, stereo ? kStereo : kMono, { kOutAt, kObjectAt }, "Vari off's render",
        [&] { (stereo ? vc2OscVariOffStereo : vc2OscVariOffMono)(s.voice, out, wave); },
        [&] {
            return vc2RegionsDifference(s.fw, { { kOutAt, out, sizeof out, "the output" },
                                                { kObjectAt + kFirst, mine, kBytes, "the object" } });
        });
}

std::string variCase(VariRig& s, std::mt19937& rng) {
    drawVariWave(s, rng);
    drawVariWords(s, rng);
    std::string what;
    for (int k = 0; k < kBlocks && what.empty(); ++k, ++s.n.blocks) {
        if (rng() % 8 == 0) drawVariWords(s, rng);
        what = variBlock(s, rng);
    }
    return what;
}
} // namespace

void oscVariOffPart(Vc2Fw& fw, const Vc2Run& run, Vc2Tally& tally) {
    auto s = std::make_unique<VariRig>(VariRig { fw });
    vc2RunCases(run, tally, "", s->n.blocks, [&](std::mt19937& rng, bool) { return variCase(*s, rng); });
    const VariCounts& n = s->n;
    std::printf("vari off: %d cases, %ld blocks (%ld mono, %ld stereo, %ld of them folded; %ld silent, %ld loops, %ld "
                "turning; %ld with the loop past the last sample)\n", run.cases, n.blocks, n.mono, n.stereo, n.folded,
                n.silent, n.loops, n.turns, n.ending);
}
#endif
