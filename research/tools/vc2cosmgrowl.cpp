// vc2run <rom.4> cosmgrowl [notes] [plant]: COSM type 8, the VC-2's GROWL, which takes the V-Synth's Comb Filter's place
// (research/VC2_VOCAL_DESIGNER.md, "COSM type 8, the Growl, run").  rom.4's converter (0x89398780), the COSM note-on
// (0x893aa0e0), update (0x893aa7d0) and the Growl's DSP (0x893b9b40, mono and stereo alike) on a COSM object laid as the
// COSM twin lays one (vc2twincosm.cpp), against a model read from them: the block's two fields, the update's depth and
// divider factor, and each sample's subharmonic phase, delay lines and two taps.  The model's constants are read from
// rom.4's pools.  Random notes (Intencity, Speed, pitch, a signal, the pitch moved between blocks); block, object, state,
// delay lines and output compared bit for bit.  `plant` drops the model's odd-position step on the divider, which must
// differ.  Fails on a fault or a difference (without `plant`), or no difference (with it).
#include "vc2cosmgrowllaw.h"
#include "vc2runs.h"
#include "vc2twin.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <set>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kConverter = 0x89398780, kNoteOn = 0x893aa0e0, kUpdate = 0x893aa7d0, kDsp = 0x893b9b40;   // [FW]
constexpr uint32_t kSlots = 0x893a8560, kTermsA = 0x893a2960, kTermsB = 0x893a2aa0;   // [FW] buffers; Matrix reads
constexpr uint32_t kIn = kScratch, kBlk = kScratch + 0x100, kObj = kScratch + 0x200, kObjBytes = 0x200;
constexpr uint32_t kTerms = kScratch + 0x400, kMods = kScratch + 0x500, kIo = kScratch + 0x800;
constexpr uint32_t kBuffers = kScratch + 0x20000, kBufferStride = 0x4000, kBuffersGiven = 4;
constexpr uint32_t kD = kObj + 0x58;   // [FW] the DSP's state: +4 position, +8, +0x10 the lines, +0x1c, +0x20 phase, step
constexpr uint32_t kDepthAt = 0xa4, kFactorAt = 0xa8;   // [FW] in the state, from the update
constexpr int kBlocks = 40, kDefaultNotes = 200;

std::vector<float> floatsAt(Machine& m, uint32_t at, uint32_t n) {
    std::vector<float> v(n);
    m.cpu.read(at, v.data(), n * 4);
    return v;
}

bool sameBits(float a, float b) {
    return !std::memcmp(&a, &b, 4);
}

// The converter on Intencity and Speed: the block, against the model's; false on a fault
bool growlBlock(Machine& m, int intensity, int speed, int& differ) {
    std::array<uint8_t, 0x100> in{};
    const auto i16 = (int16_t)intensity, s16 = (int16_t)speed;
    std::memcpy(&in[0x10], &i16, 2);
    std::memcpy(&in[0x12], &s16, 2);
    m.cpu.write(kIn, in.data(), in.size());
    if (!m.run(kConverter, {kIn, kBlk})) return false;
    const float want[2] = {CosmGrowl::blockIntensity(intensity), CosmGrowl::blockSpeed(speed)};
    differ += m.cpu.rd32(kBlk) != 9 || !sameBits(m.f32(kBlk + 4), want[0]) || !sameBits(m.f32(kBlk + 8), want[1]);
    for (uint32_t at = 0xc; at < 0x2c; at += 4) differ += m.cpu.rd32(kBlk + at) != 0;
    return true;
}

// A COSM object as the twin lays one: zeros, its modulation objects' pointers, the pitch at +0 and a root at +4; the
// buffers a last note's junk
void writeObject(Machine& m, Rng& r, float pitch) {
    const std::vector<uint8_t> zero(kObjBytes, 0);
    m.cpu.write(kObj, zero.data(), kObjBytes);
    m.cpu.write(kMods, zero.data(), 0x100);
    for (uint32_t k = 0; k < 4; ++k) m.cpu.wr32(kObj + 0x150 + 4 * k, kMods);
    m.setF32(kObj, pitch);
    m.setF32(kObj + 4, 440.0f);
    std::vector<float> junk(kBufferStride / 4);
    for (uint32_t b = 0; b < kBuffersGiven; ++b) {
        for (float& x : junk) x = r.in(-1, 1);
        m.cpu.write(kBuffers + b * kBufferStride, junk.data(), kBufferStride);
    }
}

// The model's state from the machine's, after the note-on and update and after each block
void readGrowlState(Machine& m, CosmGrowl& g) {
    g.pos = (int)m.cpu.rd32(kD + 4);
    g.phase = m.f32(kD + 0x1c);
    g.step = m.f32(kD + 0x20);
    g.l = floatsAt(m, m.cpu.rd32(kD + 8), kGrowlLine);
    g.r = floatsAt(m, m.cpu.rd32(kD + 0x10), kGrowlLine);
}

// The DSP's block against the model's: output, state words and both lines; how many words differ
long growlDiffers(Machine& m, const CosmGrowl& g, const std::vector<float>& io) {
    long differ = 0;
    const std::vector<float> out = floatsAt(m, kIo, 2 * kGrowlFrames);
    for (size_t k = 0; k < out.size(); ++k) differ += !sameBits(out[k], io[k]);
    differ += (int)m.cpu.rd32(kD + 4) != g.pos || !sameBits(m.f32(kD + 0x1c), g.phase) ||
              !sameBits(m.f32(kD + 0x20), g.step);
    const std::vector<float> l = floatsAt(m, m.cpu.rd32(kD + 8), kGrowlLine);
    const std::vector<float> r = floatsAt(m, m.cpu.rd32(kD + 0x10), kGrowlLine);
    for (int k = 0; k < kGrowlLine; ++k) differ += !sameBits(l[k], g.l[k]) || !sameBits(r[k], g.r[k]);
    return differ;
}

// A note's start in the firmware: the block from the settings, the object, the note-on and an update; false on a fault
bool startGrowl(Machine& m, Rng& r, int intensity, int speed, float pitch, int& fieldsDiffer) {
    if (!growlBlock(m, intensity, speed, fieldsDiffer)) return false;
    writeObject(m, r, pitch);
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
bool sayLaws(Machine& m, Rng& r, const CosmGrowlConsts& c) {
    int ignored = 0;
    if (!startGrowl(m, r, 0, 64, 1.0f, ignored)) return false;
    std::printf("Intencity 0, pitch 1, an impulse at frame 0: output");
    for (int b = 0; b < 20; ++b) {
        std::vector<float> io(2 * kGrowlFrames, 0.0f);
        if (b == 0) io[0] = io[1] = 1.0f;
        if (!runBlock(m, io)) return false;
        for (int k = 0; k < kGrowlFrames; ++k)
            if (io[2 * k] != 0.0f || io[2 * k + 1] != 0.0f)
                std::printf(" frame %d: %g, %g;", b * kGrowlFrames + k, io[2 * k], io[2 * k + 1]);
    }
    std::printf("\n");
    for (const int speed : {0, 32, 64, 96, 127})
        for (const float pitch : {0.5f, 1.0f, 2.0f}) {
            if (!startGrowl(m, r, 127, speed, pitch, ignored)) return false;
            std::set<int> dividers;
            for (int b = 0; b < 200; ++b) {
                std::vector<float> io(2 * kGrowlFrames, 0.0f);
                if (!runBlock(m, io)) return false;
                dividers.insert((int)std::lround(pitch * c.rate / m.f32(kD + 0x20)));
            }
            std::printf("Speed %3d, pitch %.1f: dividers", speed, pitch);
            for (const int n : dividers) std::printf(" %d (%.1f Hz)", n, 440.0 * pitch / n);
            std::printf("\n");
        }
    return true;
}

// One note: its start, then the DSP's blocks with the pitch moved now and then; false on a fault
bool growlNote(Machine& m, Rng& r, CosmGrowl& g, long& blocksDiffer, int& fieldsDiffer) {
    const int intensity = (int)(r.next() % 128), speed = (int)(r.next() % 128);
    float pitch = r.in(0.03f, 4.0f);
    if (!startGrowl(m, r, intensity, speed, pitch, fieldsDiffer)) return false;
    g.update(m.f32(kBlk + 4), m.f32(kBlk + 8));
    fieldsDiffer += !sameBits(m.f32(kD + kDepthAt), g.depth) || !sameBits(m.f32(kD + kFactorAt), g.factor);
    readGrowlState(m, g);
    for (int b = 0; b < kBlocks; ++b) {
        if (b % 9 == 4) m.setF32(kObj, pitch = r.in(0.03f, 4.0f));
        std::vector<float> io(2 * kGrowlFrames);
        for (float& x : io) x = r.in(-1, 1);
        m.cpu.write(kIo, io.data(), io.size() * 4);
        if (!m.run(kDsp, {kD, kObj + 0x10, kObj, kIo})) return false;
        g.block(pitch, io.data());
        blocksDiffer += growlDiffers(m, g, io) ? 1 : 0;
        readGrowlState(m, g);
    }
    return true;
}

} // namespace

int cosmgrowl(Machine& m, int argc, char** argv) {
    const std::optional<int> notes = argc > 1 ? intArg(argv[1]) : kDefaultNotes;
    const bool plant = argc > 2 && std::string(argv[2]) == "plant";
    if (!notes) {
        std::fprintf(stderr, "%s [notes] [plant]\n", argv[0]);
        return 2;
    }
    m.cpu.hook(kSlots, [](Sh4& c) {
        for (uint32_t i = 0; i < kBuffersGiven; ++i) c.wr32(c.r[5] + 4 * i, kBuffers + i * kBufferStride);
        c.ret(0);
    });
    m.cpu.hook(kTermsA, [](Sh4& c) { c.ret(kTerms); });
    m.cpu.hook(kTermsB, [](Sh4& c) { c.ret(kTerms); });
    m.cpu.write(kTerms, std::array<uint8_t, 16>{}.data(), 16);
    const CosmGrowlConsts c = cosmGrowlConstsOf(m);
    RoundToZero rz;
    CosmGrowl g{c, plant};
    Rng r;
    long blocksDiffer = 0;
    int fieldsDiffer = 0;
    for (int k = 0; k < *notes; ++k) {
        if (growlNote(m, r, g, blocksDiffer, fieldsDiffer)) continue;
        std::printf("cosmgrowl: %s\n", m.error().c_str());
        return 1;
    }
    std::printf("%d notes%s: block and update fields differing %d; DSP blocks differing %ld of %d\n", *notes,
                plant ? " (planted: no odd-position step)" : "", fieldsDiffer, blocksDiffer, *notes * kBlocks);
    if (!plant && !sayLaws(m, r, c)) {
        std::printf("cosmgrowl: %s\n", m.error().c_str());
        return 1;
    }
    const bool differs = fieldsDiffer || blocksDiffer;
    return plant ? (differs ? 0 : 1) : (differs ? 1 : 0);
}

} // namespace vc2
