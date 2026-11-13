// vc2run cosm <rom.2>: COSM on rom.2's routines and rom.4's, without the port (xt_cosm holds the port to rom.2, so rom.2
// equal to rom.4 is the port equal to rom.4).  Each of the 15 types' blocks is made by rom.2's converter on a random input
// block and given to both images (rom.4's converters take another input block: research/VC2_VOCAL_DESIGNER.md, "COSM in
// rom.4"), then:
//   - the shared note-on (0x894edf70 / 0x893aa0e0, 0x40 bytes smaller in rom.4) with the buffer pool's slot finder hooked
//     in each (0x894ec2b0 / 0x893a8560), then updates (0x894ee6a0 / 0x893aa7d0, 0xc0 smaller) with new envelope, LFO and
//     pitch values: the object and the four buffers compared after each call;
//   - for the TVF and Dynamic TVF, their filters (0x89502890 / 0x893c01d0, 0x89502920 / 0x893c0260) block by block.
// The inputs are as xt_cosm makes them (plugin/Source/XtCosm.cpp).  Fails on any difference.
#include "vc2runs.h"
#include "vc2twin.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

// [FW] rom.2's converters, in its dispatcher's switch order (0x894de3b0): one a type
constexpr uint32_t kConvVSynth[] = {0x894dc8b0, 0x894dcbf0, 0x894dcd80, 0x894dcf00, 0x894dd0a0, 0x894dd2c0, 0x894dd4f0,
                                    0x894dd690, 0x894dd870, 0x894dda40, 0x894ddc50, 0x894ddd20, 0x894ddde0, 0x894ddf80,
                                    0x894de110};
constexpr size_t kConverters = std::size(kConvVSynth);
constexpr Twin::Pair kNoteOn{0x894edf70, 0x893aa0e0}, kUpdate{0x894ee6a0, 0x893aa7d0};   // [FW]
constexpr Twin::Pair kSlots{0x894ec2b0, 0x893a8560};                                       // [FW]
constexpr Twin::Pair kTvf{0x89502890, 0x893c01d0}, kDtvf{0x89502920, 0x893c0260};         // [FW]
constexpr uint32_t kTvfCode = 13, kDtvfCode = 14;   // [FW] the block codes those filters run (xt_cosm)

// A word a converter takes as an index, held in its range (0…count−1) as xt_cosm holds it: past it a table is read past
// its end, where each image holds its own neighbours (a filter's kernel table, from which both jump to junk; the octave
// table, which rom.4 copies for 198 bytes).  The TVFs' filter type; the Side Band Filters' and Comb's octave; the
// Compressor's threshold, ratio, attack, release and level; Lo-Fi's bit and rate reductions.
struct RangedWord {
    uint32_t converter, offset, count;
};
constexpr RangedWord kRanged[] = {
    {0x894dd870, 0x12, 12}, {0x894dda40, 0x12, 12}, {0x894dd0a0, 0x1a, 3}, {0x894dd2c0, 0x18, 3},
    {0x894dd4f0, 0x12, 5},  {0x894ddd20, 0x12, 128}, {0x894ddd20, 0x14, 4}, {0x894ddd20, 0x16, 128},
    {0x894ddd20, 0x18, 128}, {0x894ddd20, 0x1a, 128}, {0x894ddf80, 0x12, 16}, {0x894ddf80, 0x14, 6},
};

constexpr uint32_t kIn = kScratch, kBlk = kScratch + 0x100, kObj = kScratch + 0x200, kObjSize = 0x200;
constexpr uint32_t kEnvQ = kScratch + 0x400, kEnvC = kScratch + 0x500;
constexpr uint32_t kLfoQ = kScratch + 0x600, kLfoC = kScratch + 0x700;
constexpr uint32_t kBuf = kScratch + 0x800, kBufSamples = 64;
constexpr uint32_t kBuffers = kScratch + 0x20000, kBufferStride = 0x4000, kBufferCount = 4;
constexpr int kNoteCases = 200, kUpdates = 6, kFilterBlocks = 40;

struct Rng {
    uint32_t s = 12345;
    uint32_t next() {
        s = s * 1103515245u + 12345u;
        return (s >> 8) & 0xffff;
    }
    float in(float lo, float hi) { return lo + (hi - lo) * (float)next() / 65535.0f; }
};

// An input block as the dispatcher fills it: COSM flag, key, the type's words (any of −64…127) and two velocities
void writeInput(Twin& t, Rng& r) {
    std::array<uint8_t, 0x100> in{};
    in[2] = (uint8_t)(r.next() & 1);
    in[3] = (uint8_t)(r.next() % 128);
    for (uint32_t k = 0; k < 17; ++k) {
        const int16_t v = (int16_t)((int)(r.next() % 192) - 64);
        std::memcpy(&in[0x12 + 2 * k], &v, 2);
    }
    const float va = r.in(0, 1), vb = r.in(0, 1);
    std::memcpy(&in[0x34], &va, 4);
    std::memcpy(&in[0x38], &vb, 4);
    t.write(kIn, in.data(), in.size());
}

// The object and the envelopes and LFOs it reads, as xt_cosm sets them before a note-on; the buffers hold junk
void writeVoice(Twin& t, Rng& r) {
    const std::vector<uint8_t> zero(kObjSize, 0);
    t.write(kObj, zero.data(), kObjSize);
    for (const uint32_t o : {kEnvQ, kEnvC, kLfoQ, kLfoC}) t.write(o, zero.data(), 0x100);
    std::vector<float> junk(kBufferStride / 4);
    for (uint32_t b = 0; b < kBufferCount; ++b) {
        for (float& x : junk) x = r.in(-1, 1);
        t.write(kBuffers + b * kBufferStride, junk.data(), kBufferStride);
    }
    const uint32_t ptrs[4] = {kEnvQ, kEnvC, kLfoQ, kLfoC};
    t.write(kObj + 0x150, ptrs, sizeof ptrs);
    const float head[2] = {r.in(0.03f, 4.0f), r.in(20.0f, 2000.0f)};   // pitch, root
    t.write(kObj, head, sizeof head);
    const float fade = (r.next() & 1) ? 0.0f : 1.5f;
    t.write(kObj + 0x13c, &fade, 4);
}

// New envelope and LFO values, rates and a moved pitch, as an update reads them
void writeModulation(Twin& t, Rng& r) {
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
}

// The object and the four buffers, compared: true when they agree, the first difference said with `what`
bool voiceSame(Twin& t, const char* what, size_t k) {
    long at = t.differs(kObj, kObjSize);
    uint32_t base = kObj;
    for (uint32_t b = 0; at < 0 && b < kBufferCount; ++b) {
        base = kBuffers + b * kBufferStride;
        at = t.differs(base, kBufferStride);
    }
    if (at < 0) return true;
    std::printf("converter %08x's type, %s: ", kConvVSynth[k], what);
    t.sayDiffers(base, at);
    std::printf("\n");
    return false;
}

// One converter's type: note-on, the filter blocks for the TVFs, updates; false on a difference or a fault
bool typeCase(Twin& t, Rng& r, size_t k) {
    writeInput(t, r);
    for (const RangedWord& s : kRanged) {
        if (s.converter != kConvVSynth[k]) continue;
        const uint16_t v = (uint16_t)(r.next() % s.count);
        t.write(kIn + s.offset, &v, 2);
    }
    if (!t.callVSynth(kConvVSynth[k], {kIn, kBlk})) return false;
    t.copyToVc2(kBlk, 0x100);
    writeVoice(t, r);
    if (!t.call(kNoteOn, {kObj, kBlk, 0}) || !voiceSame(t, "note-on", k)) return false;
    const uint32_t code = t.vsynth.rd32(kBlk);
    const bool tvf = code == kTvfCode || code == kDtvfCode;
    std::vector<float> sig(kBufSamples);
    for (int b = 0; b < (tvf ? kFilterBlocks : 0); ++b) {
        for (float& x : sig) x = r.in(-1, 1);
        t.write(kBuf, sig.data(), sig.size() * 4);
        if (!t.call(code == kTvfCode ? kTvf : kDtvf, {kObj + 0x58, kObj + 0x10, kObj, kBuf})) return false;
        if (t.differs(kBuf, kBufSamples * 4) >= 0 || !voiceSame(t, "filter", k)) return false;
    }
    for (int u = 0; u < kUpdates; ++u) {
        writeModulation(t, r);
        if (!t.call(kUpdate, {kObj}) || !voiceSame(t, "update", k)) return false;
    }
    return true;
}

int cosmCases(Twin& t) {
    Rng r;
    t.hook(kSlots, [](Sh4& c) {
        for (uint32_t i = 0; i < kBufferCount; ++i) c.wr32(c.r[5] + 4 * i, kBuffers + i * kBufferStride);
        c.ret(0);
    });
    long voices = 0, voiceDiffer = 0;
    for (size_t k = 0; k < kConverters; ++k) {
        long d = 0;
        for (int n = 0; n < kNoteCases; ++n, ++voices) d += typeCase(t, r, k) ? 0 : 1;
        std::printf("type of %08x (code %u): %d notes, %ld different\n", kConvVSynth[k], t.vsynth.rd32(kBlk),
                    kNoteCases, d);
        voiceDiffer += d;
    }
    std::printf("cosm: %ld notes, %ld different\n", voices, voiceDiffer);
    return (int)voiceDiffer;
}

bool noTables(const char*) {
    return true;   // the run has no port to load: rom.2 against rom.4 alone
}

} // namespace

int cosm(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, noTables, cosmCases);
}

} // namespace vc2
