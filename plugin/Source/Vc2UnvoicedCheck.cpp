// xt_vc2in <rom.4> [cases] [seed]: Vc2Unvoiced against rom.4's own unvoiced detector in Sh4Interp (v208).  Each case
// resets both, gives both the same settings, then blocks of an input that changes: silence, noise at levels from
// almost nothing to full scale, sines from 50 Hz to 15 kHz, a loud block cut to silence, now and then new settings;
// every eighth case noise faded into silence until the filters give 0.  The host flushes denormals, as the SH-4 does.
// After every block the whole of rom.4's region (0x899d9978…0x899d9a80) against the port's words, the flags' ring and
// the voiced query (0x893ed3e0).  XT_VC2IN_FIRST=1 says where the first case to differ first does.  XT_VC2IN_MAIN only.
#ifdef XT_VC2IN_MAIN
#include "Sh4Interp.h"
#include "Vc2Unvoiced.h"
#include "XtArgs.h"
#include "XtFloat.h"
#include "XtSwitches.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <random>

namespace {
constexpr uint32_t kReset = 0x893ecf60, kSettings = 0x893ed030, kInput = 0x893ed040, kVoiced = 0x893ed3e0;   // [FW]
constexpr uint32_t kWords = 0x899d9978, kFlags = 0x8a0f87dc;                                                 // [FW]
constexpr uint32_t kSp = 0x8C0E0000, kArgs = 0x8C010000, kBlockAt = 0x8C011000;
constexpr uint64_t kSteps = 4000000;   // the reset clears 400,000 bytes of the output stage's buffers
constexpr int kBlocks = 400, kRate = 44100;
// Every eighth case fades: noise, then silence to its end, long enough for the filters' output to reach exactly 0 (758
// to 1,095 blocks of it after noise at 1e-5 to full scale, measured with the flush on), which no shorter silence does
constexpr int kFadeEvery = 8, kFadeBlocks = 1600;

struct Fw {
    Sh4 cpu;
    bool call(uint32_t f, std::initializer_list<uint32_t> args, uint32_t* r0 = nullptr) {
        uint32_t r = 0;
        const bool ok = cpu.call(f, args, kSp, &r, kSteps);
        if (r0) *r0 = r;
        return ok;
    }
};

// The input, a block at a time: a kind and its level held for a stretch of blocks
struct Signal {
    std::mt19937 rng;
    bool fades = false;   // noise, then silence to the case's end
    int kind = 0, left = 0, drawn = 0;
    float level = 0.0f, hz = 0.0f;
    double phase = 0.0;
    float uniform() { return (float)rng() / 4294967296.0f; }
    void draw() {
        kind = (int)(rng() % 6);
        // a long silence (kind 5): the filters' states falling for thousands of samples, though not to 0
        left = kind == 5 ? 150 + (int)(rng() % 200) : 1 + (int)(rng() % 40);
        level = std::pow(10.0f, -5.0f * uniform());
        hz = 50.0f * std::pow(300.0f, uniform());
        if (fades) kind = drawn == 0 ? 1 : 0;
        if (fades && drawn > 0) left = kFadeBlocks;
        ++drawn;
    }
    void block(float* x) {
        if (--left <= 0) draw();
        for (int i = 0; i < Vc2Unvoiced::kBlock; ++i) {
            const float sine = (float)std::sin(phase);
            phase += 2.0 * std::numbers::pi * hz / kRate;
            const float noise = 2.0f * uniform() - 1.0f;
            const float kinds[6] = { 0.0f, noise, sine, 0.5f * (noise + sine), left == 1 ? 0.0f : noise, 0.0f };
            x[i] = level * kinds[kind];
        }
    }
};

// Both given the same settings: Unvoice Detect + 24, Spread and Sound Level as the converter forms them
bool settings(Fw& fw, Vc2Unvoiced& port, std::mt19937& rng) {
    const int detect = 24 + (int)(rng() % 128), spread = (int)(rng() % 2);
    const float level = (float)(rng() % 128) / 127.0f;
    uint32_t levelBits;
    std::memcpy(&levelBits, &level, 4);
    fw.cpu.wr32(kArgs, (uint32_t)detect);
    fw.cpu.wr32(kArgs + 4, (uint32_t)spread);
    fw.cpu.wr32(kArgs + 8, levelBits);
    port.settings(detect, spread, level);
    return fw.call(kSettings, { kArgs });
}

// One block on both, then the first word that differs, as text; empty when none does
std::string blockOn(Fw& fw, Vc2Unvoiced& port, const float* x) {
    fw.cpu.write(kBlockAt, x, sizeof(float) * Vc2Unvoiced::kBlock);
    if (!fw.call(kInput, { kBlockAt })) return "the firmware faults: " + fw.cpu.error();
    port.block(x);
    uint32_t words[sizeof(Vc2UnvoicedWords) / 4], mine[sizeof(Vc2UnvoicedWords) / 4];
    fw.cpu.read(kWords, words, sizeof words);
    std::memcpy(mine, &port.s, sizeof mine);
    char b[120];
    for (uint32_t i = 0; i < sizeof words / 4; ++i)
        if (words[i] != mine[i]) {
            std::snprintf(b, sizeof b, "word 0x%08x: %08x, against %08x", kWords + 4 * i, mine[i], words[i]);
            return b;
        }
    for (uint32_t i = 0; i < Vc2Unvoiced::kRing; ++i)
        if ((int32_t)fw.cpu.rd32(kFlags + 4 * i) != port.flags[i]) return "the flags' ring at " + std::to_string(i);
    uint32_t voiced = 0;
    if (!fw.call(kVoiced, {}, &voiced) || (voiced != 0) != port.voiced()) return "the voiced query";
    return "";
}

// The flag the last block wrote
int lastFlag(const Vc2Unvoiced& port) {
    return port.flags[(size_t)((port.s.ringIndex + Vc2Unvoiced::kRing - 1) % Vc2Unvoiced::kRing)];
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "xt_vc2in <rom.4> [cases] [seed]\n");
        return 2;
    }
    xtf::flushDenormals();
    const int cases = argc > 2 ? intArg(argv[2]).value_or(-1) : 40;
    const int seed = argc > 3 ? intArg(argv[3]).value_or(-1) : 2024;
    Fw fw;
    if (cases < 0 || seed < 0 || !fw.cpu.loadRom(argv[1]) || !vc2LoadUnvoiced(argv[1])) {
        std::fprintf(stderr, "xt_vc2in: no rom.4 at %s, or an argument that isn't a number\n", argv[1]);
        return 1;
    }
    fw.cpu.mapRam(0x8C000000, 0x00100000);   // the stack, the arguments and the block, as xt_vc2params has them
    std::mt19937 rng((uint32_t)seed);
    long blocks = 0, unvoiced = 0, zeroPeak = 0, notANumber = 0, different = 0;
    const bool first = xtSwitch("XT_VC2IN_FIRST") != nullptr;
    Vc2Unvoiced port;   // one through the cases, as rom.4's region is: its reset leaves the second biquad as it was
    for (int c = 0; c < cases; ++c) {
        const bool fades = c % kFadeEvery == kFadeEvery - 1;
        Signal input { std::mt19937(rng()), fades };
        std::string what = fw.call(kReset, {}) ? "" : "the reset faults";
        port.reset();
        if (what.empty() && !settings(fw, port, rng)) what = "the settings fault";
        float x[Vc2Unvoiced::kBlock];
        for (int k = 0; k < (fades ? kFadeBlocks : kBlocks) && what.empty(); ++k, ++blocks) {
            if (rng() % 64 == 0 && !settings(fw, port, rng)) what = "the settings fault";
            input.block(x);
            if (what.empty()) what = blockOn(fw, port, x);
            unvoiced += lastFlag(port);
            zeroPeak += port.s.peaks[0] == 0.0f ? 1 : 0;      // the filters' output exactly 0 through a block
            notANumber += std::isnan(port.s.total) ? 1 : 0;   // the count over a peak of 0, the decaying peak still high
        }
        if (what.empty()) continue;
        if (!different++ && first) std::printf("  case %d, block %ld: %s\n", c, blocks, what.c_str());
        if (xtSwitch("XT_VC2IN_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("unvoiced: %d cases, %ld blocks (%ld called unvoiced, %ld with a peak of 0, %ld with the vote not a "
                "number), %ld different\n", cases, blocks, unvoiced, zeroPeak, notANumber, different);
    std::printf("xt_vc2in: %ld different\n", different);
    return 0;
}
#endif
