// vc2run growl: the VOCODER OSC's growl branch (the carrier's render 0x893ebfa0 with its +0x3c above 0, from 0x893ec240),
// run on a carrier its own init made (0x893ebed0) and its own update set (0x893ec4d0, which puts P+0x20 at +0x3c), after
// the carriers' rate setter (0x893ec6a0).  A hook before the saw's filter (0x893ec310) takes the saw as the growl leaves
// it, and one where the growl's divider is chosen (0x893ec29a) takes the divider; vc2growllaw.cpp's model of the branch
// is held to the first, bit for bit.
//   growl [blocks]   pitches 55 Hz…10 kHz, growl amounts 0.05…1: samples different from the model, and per pitch the
//                    dividers the firmware chose
#include "vc2growllaw.h"
#include "vc2runs.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kSetRate = 0x893ec6a0;   // [FW] the carriers' rate: 0x899d9964 = 880/44100, 0x899d9960 = 44100/440
constexpr uint32_t kInit = 0x893ebed0;      // [FW] (carrier, P): fields from P, the phases, the noise's state
constexpr uint32_t kUpdate = 0x893ec4d0;    // [FW] (carrier, P): gain and pitch targets, P+0x20 to +0x3c, the balance
constexpr uint32_t kRender = 0x893ebfa0;    // [FW] (carrier, out): 32 samples, a float pair each
constexpr uint32_t kSawIn = 0x893ec310;     // [FW] growl branch: fr7 is the saw before × 0.5 and the filter
constexpr uint32_t kDivider = 0x893ec29a;   // [FW] growl branch: r6 is the divider just chosen
// [FW] the init's random start phase (0x893f31a0) asks the OS (0x893018d0 jumps to its RAM, 0x880080d0, not mapped
// here) for a number; it is answered with a fixed one, as the model reads the carrier's state after the init
constexpr uint32_t kOsRandom = 0x893018d0;
constexpr uint32_t kFixedRandom = 12345;
constexpr float kA4 = 440.0f;               // P+4 is the pitch as a ratio to 440 Hz (by kSetRate's 880/44100)
constexpr int kBlock = 32;

struct Tally {
    long samples = 0, differ = 0;
    std::map<uint32_t, long> dividers;      // this pitch's: divider, growl cycles
};

// One pitch and growl amount: the carrier made and updated, `blocks` rendered, each sample against the model
void growlCase(Machine& m, float hz, float growl, int blocks, Tally& t) {
    const uint32_t carrier = m.alloc(0x80), p = m.alloc(0x80), out = m.alloc(8 * kBlock);
    m.setF32(p + 0x00, 1.0f);       // gain
    m.setF32(p + 0x04, hz / kA4);
    m.setF32(p + 0x20, growl);
    m.setF32(p + 0x2c, 1.0f);       // the saw–noise balance at its centre
    if (!m.run(kInit, {carrier, p}) || !m.run(kUpdate, {carrier, p})) return;
    GrowlState s = growlStateOf(m, carrier);
    std::vector<float> saw;
    m.cpu.hook(kSawIn, [&saw](Sh4& c) {
        float f = 0;
        std::memcpy(&f, &c.fb[0][7], 4);
        saw.push_back(f);
    });
    m.cpu.hook(kDivider, [&t](Sh4& c) { ++t.dividers[c.r[6]]; });
    for (int b = 0; b < blocks; ++b) {
        saw.clear();
        if (!m.run(kRender, {carrier, out})) return;
        for (size_t n = 0; n < (size_t)kBlock && n < saw.size(); ++n) {
            const float want = growlStep(s);
            ++t.samples;
            if (std::memcmp(&want, &saw[n], 4) != 0 && ++t.differ <= 5)
                std::printf("%g Hz growl %g block %d sample %zu: firmware %.9g, model %.9g\n", hz, growl, b, n, saw[n],
                            want);
        }
    }
    m.cpu.unhook(kSawIn);
    m.cpu.unhook(kDivider);
}

} // namespace

int growl(Machine& m, int argc, char** argv) {
    int blocks = 200;
    if (argc >= 2) {
        const char* end = argv[1] + std::strlen(argv[1]);
        if (std::from_chars(argv[1], end, blocks).ptr != end || blocks <= 0) {
            std::fprintf(stderr, "growl [blocks]: blocks is a count\n");
            return 2;
        }
    }
    m.cpu.hook(kOsRandom, [](Sh4& c) { c.ret(kFixedRandom); });
    if (!m.run(kSetRate, {})) return 1;
    long samples = 0, differ = 0;
    for (const float hz : {55.0f, 110.0f, 220.0f, 440.0f, 880.0f, 1760.0f, 2600.0f, 4000.0f, 8000.0f, 10000.0f}) {
        Tally t;
        for (const float g : {0.05f, 0.3f, 0.7f, 1.0f}) growlCase(m, hz, g, blocks, t);
        std::printf("%6g Hz, dividers (growl cycles):", hz);
        for (const auto& [divider, cycles] : t.dividers) std::printf(" %u (%ld)", divider, cycles);
        std::printf("\n");
        samples += t.samples;
        differ += t.differ;
    }
    std::printf("growl: %ld samples, %ld different from the model\n", samples, differ);
    return differ == 0 && samples > 0 ? 0 : 1;
}

} // namespace vc2
