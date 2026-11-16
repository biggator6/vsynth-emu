// xt_vc2in <rom.4> [cases] [seed]: Vc2Unvoiced against rom.4's own input and output stages in Sh4Interp (v208, v209).
// Each case resets both, gives both the same settings, then blocks of an input that changes: silence, noise at levels
// from almost nothing to full scale, sines from 50 Hz to 15 kHz, a loud block cut to silence, now and then new
// settings; every eighth case noise faded into silence until the filters give 0.  The host flushes denormals, as the
// SH-4 does.  Each block goes into both mic rings where 0x893ed410 says, the input stage runs on it, and the output stage
// (0x893ed510 or 0x893ed690, a case's channels) on the same engine frames, with a case's gain, slot 7's scale and the
// level's term from a generator of their own, so the input's cases stay as v208 drew them.  After each stage the whole
// region (0x899d9978…0x899d9a80) against the port's words, the flags' ring and the voiced query (0x893ed3e0), then the
// frames and rom.4's two .data words; at a case's end the mic ring and the allpasses' buffers.  XT_VC2IN_FIRST=1 says
// where the first case to differ first does.  The interpreter, the input and the compare are Vc2CheckRig.h's (v210).
// XT_VC2IN_MAIN only.
#ifdef XT_VC2IN_MAIN
#include "Vc2CheckRig.h"
#include "Vc2CheckWords.h"
#include "Vc2Unvoiced.h"
#include "XtSwitches.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <random>

namespace {
constexpr uint32_t kReset = 0x893ecf60, kSettings = 0x893ed030, kInput = 0x893ed040, kVoiced = 0x893ed3e0;   // [FW]
constexpr uint32_t kMicAt = 0x893ed410, kMono = 0x893ed510, kStereo = 0x893ed690;                         // [FW]
constexpr uint32_t kLevelMod = 0x89997298;                                                                // [FW]
constexpr uint32_t kFramesAt = 0x8C012000;
constexpr int kBlocks = 400;
constexpr int kFadeEvery = 8;   // every eighth case fades (Vc2Signal's fades)
constexpr int kFrameWords = 2 * Vc2Unvoiced::kBlock;

// The output stage's arguments: the consonant gain × 2^31, as the block driver forms it; slot 7's scale, often 1; the
// level's ÷127 term, half the time 0; now and then new.  The engine's frames, at the bus's int32 scale.
struct Output {
    std::mt19937 rng;
    int channels = 1;
    float gain = 0.0f, flagScale = 0.0f, levelMod = 0.0f;
    float uniform() { return (float)rng() / 4294967296.0f; }
    void draw() {
        gain = (float)(rng() % 128) / 127.0f * 2147483648.0f;
        flagScale = rng() % 4 == 0 ? 1.0f : 1.5f * uniform();
        levelMod = rng() % 2 == 0 ? 0.0f : (float)((int)(rng() % 255) - 127) / 127.0f;
    }
    void frames(float* f) {
        for (int i = 0; i < kFrameWords; ++i) f[i] = (2.0f * uniform() - 1.0f) * 1073741824.0f;
    }
};

// Both given the same settings: Unvoice Detect + 24, Spread and Sound Level as the converter forms them
bool settings(Vc2Fw& fw, Vc2Unvoiced& port, std::mt19937& rng) {
    const int detect = 24 + (int)(rng() % 128), spread = (int)(rng() % 2);
    const float level = (float)(rng() % 128) / 127.0f;
    fw.cpu.wr32(kVc2Args, (uint32_t)detect);
    fw.cpu.wr32(kVc2Args + 4, (uint32_t)spread);
    fw.cpu.wr32(kVc2Args + 8, std::bit_cast<uint32_t>(level));
    port.settings(detect, spread, level);
    return fw.call(kSettings, { kVc2Args });
}

// One block of the mic into both rings and the input stage on both, then the first word that differs
std::string blockOn(Vc2Fw& fw, Vc2Unvoiced& port, const float* x) {
    uint32_t at = 0;
    if (!fw.call(kMicAt, {}, &at) || at != kVc2MicRingAt + 4 * (uint32_t)port.s.micIndex)
        return "the mic ring's write point";
    fw.cpu.write(at, x, sizeof(float) * Vc2Unvoiced::kBlock);
    std::copy_n(x, Vc2Unvoiced::kBlock, port.micBlock());
    if (!fw.call(kInput, { at })) return "the firmware faults: " + fw.cpu.error();
    port.block(port.micBlock());
    std::string what = vc2UnvoicedDifference(fw, port);
    uint32_t voiced = 0;
    if (what.empty() && (!fw.call(kVoiced, {}, &voiced) || (voiced != 0) != port.voiced())) what = "the voiced query";
    return what;
}

// The output stage on both, on the same frames, then the first word that differs: the frames, the region, the level
// and the allpasses' index
std::string consonantsOn(Vc2Fw& fw, Vc2Unvoiced& port, Output& out) {
    float frames[kFrameWords];
    out.frames(frames);
    fw.cpu.write(kFramesAt, frames, sizeof frames);
    fw.cpu.wr32(kLevelMod, std::bit_cast<uint32_t>(out.levelMod));
    fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(out.gain);
    fw.cpu.fb[0][5] = std::bit_cast<uint32_t>(out.flagScale);
    if (!fw.call(out.channels == 1 ? kMono : kStereo, { kFramesAt }))
        return "the output stage faults: " + fw.cpu.error();
    port.consonants(frames, out.channels, out.gain, out.flagScale, out.levelMod);
    std::string what = vc2FirstDifference(fw, kFramesAt, frames, sizeof frames, "the frames");
    if (what.empty()) what = vc2UnvoicedDifference(fw, port);
    return what;
}

// The memory the blocks show only through the frames, after a reset and at a case's end (vc2UnvoicedBuffersDifference,
// Vc2CheckWords.cpp): a reset that left the ring would reach the sound only if the gate opened before 14 blocks had
// written it over, which the cases seldom do
std::string buffersDifference(Vc2Fw& fw, const Vc2Unvoiced& port) { return vc2UnvoicedBuffersDifference(fw, port); }

// The flag the last block wrote
int lastFlag(const Vc2Unvoiced& port) {
    return port.flags[(size_t)((port.s.ringIndex + Vc2Unvoiced::kRing - 1) % Vc2Unvoiced::kRing)];
}

struct Counts {
    long blocks = 0, unvoiced = 0, zeroPeak = 0, notANumber = 0, stereo = 0, spread = 0, modded = 0;
    long inDifferent = 0, outDifferent = 0;
};
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2in", vc2LoadUnvoiced);
    if (!run) return 1;
    const int cases = run->cases, seed = run->seed;
    std::mt19937 rng((uint32_t)seed);
    Counts n;
    const bool first = xtSwitch("XT_VC2IN_FIRST") != nullptr;
    Vc2Unvoiced port;   // one through the cases, as rom.4's region is: its reset leaves the second biquad as it was
    for (int c = 0; c < cases; ++c) {
        const bool fades = c % kFadeEvery == kFadeEvery - 1;
        Vc2Signal input { std::mt19937(rng()), fades };
        Output out { std::mt19937((uint32_t)seed * 2654435761u + (uint32_t)c) };
        out.channels = 1 + (int)(out.rng() % 2);
        out.draw();
        std::string what = fw.call(kReset, {}) ? "" : "the reset faults", said;
        port.reset();
        if (what.empty()) said = buffersDifference(fw, port);
        if (what.empty() && !settings(fw, port, rng)) what = "the settings fault";
        float x[Vc2Unvoiced::kBlock];
        for (int k = 0; k < (fades ? kVc2FadeBlocks : kBlocks) && what.empty() && said.empty(); ++k, ++n.blocks) {
            if (rng() % 64 == 0 && !settings(fw, port, rng)) what = "the settings fault";
            input.block(x);
            if (what.empty()) what = blockOn(fw, port, x);
            if (out.rng() % 32 == 0) out.draw();
            if (what.empty()) said = consonantsOn(fw, port, out);
            n.unvoiced += lastFlag(port);
            n.zeroPeak += port.s.peaks[0] == 0.0f ? 1 : 0;      // the filters' output exactly 0 through a block
            n.notANumber += std::isnan(port.s.total) ? 1 : 0;   // the count over a peak of 0, the decaying peak still high
            n.stereo += out.channels == 2 ? 1 : 0;
            n.spread += port.s.spread != 0 ? 1 : 0;
            n.modded += out.levelMod != 0.0f ? 1 : 0;
        }
        if (what.empty() && said.empty()) said = buffersDifference(fw, port);
        if (what.empty() && said.empty()) continue;
        if (n.inDifferent + n.outDifferent == 0 && first)
            std::printf("  case %d, block %ld: %s\n", c, n.blocks, (what.empty() ? said : what).c_str());
        ++(what.empty() ? n.outDifferent : n.inDifferent);
        if (xtSwitch("XT_VC2IN_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("unvoiced: %d cases, %ld blocks (%ld called unvoiced, %ld with a peak of 0, %ld with the vote not a "
                "number), %ld different\n", cases, n.blocks, n.unvoiced, n.zeroPeak, n.notANumber, n.inDifferent);
    std::printf("consonants: %ld blocks (%ld stereo, %ld with Spread, %ld with the level's term), %ld different\n",
                n.blocks, n.stereo, n.spread, n.modded, n.outDifferent);
    std::printf("xt_vc2in: %ld different\n", n.inDifferent + n.outDifferent);
    return 0;
}
#endif
