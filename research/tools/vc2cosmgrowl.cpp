// vc2run <rom.4> cosmgrowl: COSM type 8, the VC-2's GROWL, which takes the V-Synth's Comb Filter's place
// (research/VC2_VOCAL_DESIGNER.md, "COSM type 8, the Growl, run"): its laws as the firmware shows them, on rom.4's
// converter (0x89398780), the COSM note-on (0x893aa0e0) and update (0x893aa7d0) and the Growl's DSP (0x893b9b40, mono
// and stereo alike), on a COSM object laid as the COSM twin lays one (vc2twincosm.cpp): an impulse's output at
// Intencity 0, and at five Speeds and three pitches the dividers the phase is given.  Its model of the routines
// (vc2cosmgrowllaw.cpp), held to them bit for bit, went into the plugin at v234: the Growl is
// plugin/Source/Vc2CosmGrowl.cpp's, held to rom.4's own routines word for word by xt_vc2cosm (Vc2CosmCheck.cpp).
// Fails on a fault.
#include "vc2runs.h"
#include "vc2twin.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kConverter = 0x89398780, kNoteOn = 0x893aa0e0, kUpdate = 0x893aa7d0, kDsp = 0x893b9b40;   // [FW]
constexpr uint32_t kSlots = 0x893a8560, kTermsA = 0x893a2960, kTermsB = 0x893a2aa0;   // [FW] buffers; Matrix reads
constexpr uint32_t kRate = 0x893b9c9c;   // [FW] the DSP's pool: the phase's step a pitch, over the divider
constexpr uint32_t kIn = kScratch, kBlk = kScratch + 0x100, kObj = kScratch + 0x200, kObjBytes = 0x200;
constexpr uint32_t kTerms = kScratch + 0x400, kMods = kScratch + 0x500, kIo = kScratch + 0x800;
constexpr uint32_t kBuffers = kScratch + 0x20000, kBufferStride = 0x4000, kBuffersGiven = 4;
constexpr uint32_t kD = kObj + 0x58;   // [FW] the DSP's state: +0x20 the phase's step
constexpr int kFrames = 32, kLawBlocks = 200;

// A COSM object as the twin lays one: zeros, its modulation objects' pointers, the pitch at +0 and a root at +4
void writeObject(Machine& m, float pitch) {
    const std::vector<uint8_t> zero(kObjBytes, 0);
    m.cpu.write(kObj, zero.data(), kObjBytes);
    m.cpu.write(kMods, zero.data(), 0x100);
    for (uint32_t k = 0; k < 4; ++k) m.cpu.wr32(kObj + 0x150 + 4 * k, kMods);
    m.setF32(kObj, pitch);
    m.setF32(kObj + 4, 440.0f);
}

// A note's start in the firmware: the block from Intencity and Speed, the object, the note-on and an update; false on a
// fault
bool startGrowl(Machine& m, int intensity, int speed, float pitch) {
    std::array<uint8_t, 0x100> in{};
    const auto i16 = (int16_t)intensity, s16 = (int16_t)speed;
    std::memcpy(&in[0x10], &i16, 2);
    std::memcpy(&in[0x12], &s16, 2);
    m.cpu.write(kIn, in.data(), in.size());
    if (!m.run(kConverter, {kIn, kBlk})) return false;
    writeObject(m, pitch);
    return m.run(kNoteOn, {kObj, kBlk, 0}) && runF(m, kUpdate, {kObj}, pitch);
}

// One DSP block on the given frames, which come back as its output; false on a fault
bool runBlock(Machine& m, std::vector<float>& io) {
    m.cpu.write(kIo, io.data(), io.size() * 4);
    if (!m.run(kDsp, {kD, kObj + 0x10, kObj, kIo})) return false;
    m.cpu.read(kIo, io.data(), io.size() * 4);
    return true;
}

// The laws as the firmware shows them: at Intencity 0, an impulse's output; at five Speeds and three pitches, the
// dividers the phase is given (from the step after each block: n = pitch · the rate / step) and the growl, 440·pitch/n
bool sayLaws(Machine& m) {
    const float rate = m.f32(kRate);
    if (!startGrowl(m, 0, 64, 1.0f)) return false;
    std::printf("Intencity 0, pitch 1, an impulse at frame 0: output");
    for (int b = 0; b < 20; ++b) {
        std::vector<float> io(2 * kFrames, 0.0f);
        if (b == 0) io[0] = io[1] = 1.0f;
        if (!runBlock(m, io)) return false;
        for (int k = 0; k < kFrames; ++k)
            if (io[2 * k] != 0.0f || io[2 * k + 1] != 0.0f)
                std::printf(" frame %d: %g, %g;", b * kFrames + k, io[2 * k], io[2 * k + 1]);
    }
    std::printf("\n");
    for (const int speed : {0, 32, 64, 96, 127})
        for (const float pitch : {0.5f, 1.0f, 2.0f}) {
            if (!startGrowl(m, 127, speed, pitch)) return false;
            std::set<int> dividers;
            for (int b = 0; b < kLawBlocks; ++b) {
                std::vector<float> io(2 * kFrames, 0.0f);
                if (!runBlock(m, io)) return false;
                dividers.insert((int)std::lround(pitch * rate / m.f32(kD + 0x20)));
            }
            std::printf("Speed %3d, pitch %.1f: dividers", speed, pitch);
            for (const int n : dividers) std::printf(" %d (%.1f Hz)", n, 440.0 * pitch / n);
            std::printf("\n");
        }
    return true;
}

} // namespace

int cosmgrowl(Machine& m, int, char**) {
    m.cpu.hook(kSlots, [](Sh4& c) {
        for (uint32_t i = 0; i < kBuffersGiven; ++i) c.wr32(c.r[5] + 4 * i, kBuffers + i * kBufferStride);
        c.ret(0);
    });
    m.cpu.hook(kTermsA, [](Sh4& c) { c.ret(kTerms); });
    m.cpu.hook(kTermsB, [](Sh4& c) { c.ret(kTerms); });
    m.cpu.write(kTerms, std::array<uint8_t, 16>{}.data(), 16);
    if (sayLaws(m)) return 0;
    std::printf("cosmgrowl: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
