// vc2run cosm <rom.2>: COSM on rom.2's routines and rom.4's, without the port (xt_cosm holds the port to rom.2, so rom.2
// equal to rom.4 is the port equal to rom.4).  Each of the 16 types' blocks is made by rom.2's converter on an input block
// of its parameters, each drawn in its descriptor's range, and given to both images (rom.4's converters take another input
// block: research/VC2_VOCAL_DESIGNER.md, "COSM in rom.4"), then:
//   - the shared note-on (0x894edf70 / 0x893aa0e0, 0x40 bytes smaller in rom.4) with the buffer pool's slot finder hooked
//     in each (0x894ec2b0 / 0x893a8560);
//   - then 40 blocks of the type's DSP routine, mono or stereo, each image's from its own tables by block code, on a
//     signal of 32 frames at one of xt_cosm's levels, with updates (0x894ee6a0 / 0x893aa7d0, 0xc0 smaller; new envelope,
//     LFO and pitch values) and now and then a new block (0x894ee4a0 / 0x893aa5d0) between: the output, the object and the
//     four buffers compared after each call.
// The inputs are as xt_cosm makes them (plugin/Source/XtCosm.cpp).  Fails on any difference.
#include "vc2runs.h"
#include "vc2twin.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

// [FW] rom.2's converters, one a type in type order (the dispatcher 0x894de3b0's; the Speaker Simulator's a function of its
// own), and each type's parameters, which sit at +0x12, +0x14, … in its input in ID order (the IDs from the table at
// 0x897cebd8: HARDWARE_FIRMWARE.md, "COSM: the Dual Filter, and the map of the types")
struct Converter {
    uint32_t addr;
    uint16_t firstId, count;
};
constexpr Converter kConverters[] = {
    {0x894dc8b0, 0x8d0, 9},  {0x894dcbf0, 0x8d9, 9},  {0x894dcd80, 0x8e2, 8},  {0x894dcef0, 0x8ea, 1},
    {0x894dcf00, 0x8eb, 8},  {0x894dd0a0, 0x8f3, 14}, {0x894dd2c0, 0x901, 13}, {0x894dd4f0, 0x90e, 7},
    {0x894dd690, 0x915, 11}, {0x894dd870, 0x920, 8},  {0x894dda40, 0x928, 10}, {0x894ddc50, 0x932, 3},
    {0x894ddd20, 0x935, 5},  {0x894ddde0, 0x93a, 7},  {0x894ddf80, 0x941, 9},  {0x894de110, 0x94a, 11},
};
// [FW] the second descriptor bank: 8 bytes an ID from 0x55d, {centre, default, min, max} (CLAUDE.md, "Firmware facts")
constexpr uint32_t kDescriptors2 = 0x897a56d8, kFirstId2 = 0x55d;
constexpr Twin::Pair kNoteOn{0x894edf70, 0x893aa0e0}, kUpdate{0x894ee6a0, 0x893aa7d0};   // [FW]
constexpr Twin::Pair kSlots{0x894ec2b0, 0x893a8560}, kNewBlock{0x894ee4a0, 0x893aa5d0};   // [FW]
// [FW] the DSP routines by block code, 21 a table: rom.2's in .data (the chooser 0x894eef70's), rom.4's where its .data
// holds its routines' addresses (the image's 0x8991e0ec and 0x8991e140, after C startup at these)
constexpr Twin::Pair kMonoTable{0x8987c7ec, 0x8993fb50}, kStereoTable{0x8987c840, 0x8993fba4};

constexpr uint32_t kIn = kScratch, kBlk = kScratch + 0x100, kObj = kScratch + 0x200, kObjSize = 0x200;
constexpr uint32_t kEnvQ = kScratch + 0x400, kEnvC = kScratch + 0x500;
constexpr uint32_t kLfoQ = kScratch + 0x600, kLfoC = kScratch + 0x700;
constexpr uint32_t kBuf = kScratch + 0x800, kBufSamples = 64, kPast = 3;   // 32 frames L/R, and what lies after them
constexpr uint32_t kBuffers = kScratch + 0x20000, kBufferStride = 0x4000, kBufferCount = 4;
constexpr int kNoteCases = 100, kBlocks = 40;

// An input block as the dispatcher fills it: COSM flag, key, the type's parameters (each in its descriptor's range, as a
// patch can hold it: past it a DSP reads its tables past their end) and two velocities
void writeInput(Twin& t, Rng& r, const Converter& conv) {
    std::array<uint8_t, 0x100> in{};
    in[2] = (uint8_t)(r.next() & 1);
    in[3] = (uint8_t)(r.next() % 128);
    for (uint32_t k = 0; k < conv.count; ++k) {
        const uint32_t d = kDescriptors2 + 8 * (conv.firstId + k - kFirstId2);
        const int lo = (int16_t)t.vsynth.rd16(d + 4), hi = (int16_t)t.vsynth.rd16(d + 6);
        const auto v = (int16_t)(lo + (int)(r.next() % (uint32_t)(hi - lo + 1)));
        std::memcpy(&in[0x12 + 2 * k], &v, 2);
    }
    const float va = r.in(0, 1), vb = r.in(0, 1);
    std::memcpy(&in[0x34], &va, 4);
    std::memcpy(&in[0x38], &vb, 4);
    t.write(kIn, in.data(), in.size());
}

// The object and the envelopes and LFOs it reads, as xt_cosm sets them before a note-on; the buffers hold a last note's
// junk over the delay lines' length (xt_cosm's 3267 words) and 0 after it, which the Side Band Filters read as a bound:
// junk there, and their routines run to the step limit
constexpr uint32_t kBufferJunk = 3267;
void writeVoice(Twin& t, Rng& r) {
    const std::vector<uint8_t> zero(kObjSize, 0);
    t.write(kObj, zero.data(), kObjSize);
    for (const uint32_t o : {kEnvQ, kEnvC, kLfoQ, kLfoC}) t.write(o, zero.data(), 0x100);
    std::vector<float> junk(kBufferStride / 4, 0.0f);
    for (uint32_t b = 0; b < kBufferCount; ++b) {
        for (uint32_t i = 0; i < kBufferJunk; ++i) junk[i] = r.in(-1, 1);
        t.write(kBuffers + b * kBufferStride, junk.data(), kBufferStride);
    }
    std::array<float, (0x150 - 0x74) / 4> last{};   // the last note's state, which the note-on must leave nothing of
    for (float& x : last) x = r.in(-1, 1);
    t.write(kObj + 0x74, last.data(), sizeof last);
    const uint32_t ptrs[4] = {kEnvQ, kEnvC, kLfoQ, kLfoC};
    t.write(kObj + 0x150, ptrs, sizeof ptrs);
    const float head[2] = {r.in(0.03f, 4.0f), r.in(20.0f, 2000.0f)};   // pitch, root
    t.write(kObj, head, sizeof head);
    const float fade = (r.next() & 1) ? 0.0f : 1.5f;
    t.write(kObj + 0x13c, &fade, 4);
}

// New envelope and LFO values, rates and a moved pitch, as an update reads them; the pitch, which the update also takes in
// fr4 (xt_cosm's calls)
float writeModulation(Twin& t, Rng& r) {
    for (uint32_t i = 0; i < 2; ++i) {
        const uint32_t e = i ? kEnvC : kEnvQ;
        const uint32_t l = i ? kLfoC : kLfoQ;
        const float level = r.in(-1, 1);
        const float lfo = r.in(-1, 1);
        const float rate = (r.next() & 1) ? 1.0f / 32.0f : r.in(0.2f, 1.0f) / 32.0f;
        t.write(e + 0x4c, &level, 4);
        t.write(e + 0x58, &level, 4);
        t.write(e + 0x54, &rate, 4);
        t.write(l + 0x38, &lfo, 4);
    }
    const float pitch = r.in(0.03f, 4.0f);
    t.write(kObj, &pitch, 4);
    return pitch;
}

// A note as it runs: its converter, mono or stereo, and whether its first difference is said (the first of its kind)
struct CosmNote {
    size_t k;
    bool stereo, say;
};

// The output (after a DSP call), the object and the four buffers, compared: true when they agree, the first difference
// said with `what` when the note says it
bool voiceSame(Twin& t, const CosmNote& n, const char* what, bool withOut = false) {
    uint32_t base = kBuf;
    long at = withOut ? t.differs(kBuf, kBufSamples * 4) : -1;
    if (at < 0) at = t.differs(base = kObj, kObjSize);
    for (uint32_t b = 0; at < 0 && b < kBufferCount; ++b)
        at = t.differs(base = kBuffers + b * kBufferStride, kBufferStride);
    if (at < 0) return true;
    if (!n.say) return false;
    std::printf("converter %08x's type, %s, %s: ", kConverters[n.k].addr, n.stereo ? "stereo" : "mono", what);
    t.sayDiffers(base, at);
    std::printf("\n");
    return false;
}

// The converter's block from a random input, given to both; false on a fault
bool makeBlock(Twin& t, Rng& r, size_t k) {
    writeInput(t, r, kConverters[k]);
    if (!t.callVSynth(kConverters[k].addr, {kIn, kBlk})) return false;
    t.copyToVc2(kBlk, 0x100);
    return true;
}

// A block's signal in both: 32 frames, L and R each at one of the levels, and what lies after them (the Side Band
// Filters' follower reads on)
void writeSignal(Twin& t, Rng& r) {
    const float left = pickLevel(r), right = pickLevel(r);
    std::array<float, kBufSamples + kPast> x{};
    for (size_t i = 0; i < x.size(); ++i) {
        const float level = i % 2 == 0 && i < kBufSamples ? left : right;
        x[i] = r.in(-level, level);
    }
    t.write(kBuf, x.data(), sizeof x);
}

// The type's DSP routine in each image, from its own table by block code
Twin::Pair routineOf(Twin& t, uint32_t code, bool stereo) {
    const Twin::Pair table = stereo ? kStereoTable : kMonoTable;
    return {t.vsynth.rd32(table.vsynth + 4 * code), t.vc2.cpu.rd32(table.vc2 + 4 * code)};
}

// What runs before block b, as xt_cosm's stereo section has it: an update twice in 14 blocks, a new block at the ninth one
// time in two; false on a difference or a fault
bool between(Twin& t, Rng& r, const CosmNote& n, int b) {
    if (b % 14 == 3 || b % 14 == 13) {
        const float pitch = writeModulation(t, r);
        return t.call(kUpdate, {kObj}, pitch) && voiceSame(t, n, "update");
    }
    if (b % 14 != 8 || (r.next() & 1)) return true;
    return makeBlock(t, r, n.k) && t.call(kNewBlock, {kObj, kBlk}) && voiceSame(t, n, "new block");
}

// One note of a converter's type: note-on, then its DSP's blocks with what runs between; false on a difference or a fault
bool typeCase(Twin& t, Rng& r, const CosmNote& n) {
    if (!makeBlock(t, r, n.k)) return false;
    writeVoice(t, r);
    if (!t.call(kNoteOn, {kObj, kBlk, 0}) || !voiceSame(t, n, "note-on")) return false;
    const Twin::Pair dsp = routineOf(t, t.vsynth.rd32(kBlk), n.stereo);
    for (int b = 0; b < kBlocks; ++b) {
        if (!between(t, r, n, b)) return false;
        writeSignal(t, r);
        if (!t.call(dsp, {kObj + 0x58, kObj + 0x10, kObj, kBuf}) || !voiceSame(t, n, "DSP", true)) return false;
    }
    return true;
}

int cosmCases(Twin& t) {
    Rng r;
    t.hook(kSlots, [](Sh4& c) {
        for (uint32_t i = 0; i < kBufferCount; ++i) c.wr32(c.r[5] + 4 * i, kBuffers + i * kBufferStride);
        c.ret(0);
    });
    Twin::Returns all;
    for (size_t k = 0; k < std::size(kConverters); ++k)
        for (const bool stereo : {false, true}) {
            Twin::Returns n;
            for (int c = 0; c < kNoteCases; ++c, ++n.cases)
                n.differ += typeCase(t, r, CosmNote{k, stereo, n.differ == 0}) ? 0 : 1;
            char label[64];
            std::snprintf(label, sizeof label, "type of %08x (code %2u), %s", kConverters[k].addr, t.vsynth.rd32(kBlk),
                          stereo ? "stereo" : "mono  ");
            addCount(label, "notes", n, all);
        }
    return sayCount("cosm", "notes", all);
}

} // namespace

int cosm(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, noPortTables, cosmCases);
}

} // namespace vc2
