// xt_vc2hireso's second part (v214): HIRESO's rate filters against rom.4's own on a 68-byte filter object in the
// interpreter's RAM.  A kind for each routine and the table its callers give it: the analysis's decimators on its table
// (0x893efd20 32→16, 0x893efdb0 16→8, 0x895613f4), the carrier's (0x893efc80 with its mix, and 0x893efdb0 again, on
// 0x895616d8), the interpolators (0x893efef0 8→16 into a buffer that holds something, 0x893efe40 16→32 into frames, on
// 0x89561700); the stereo path's four (v218: 0x893eff90, 0x893f0160 on the decimators' table, 0x893f0350, 0x893f0240 on
// the interpolators').  Each case clears the object by 0x893efc30 and sets its table, then blocks of Vc2Signal's input, the mic's
// at its scale and the carrier's at the bus's; after every block the outputs and the object's 16 states word for word.
// XT_VC2HIRESO_FIRST=1 says where the first block to differ does, XT_VC2HIRESO_STOP=1 stops there.  XT_VC2HIRESO_MAIN
// only.
#ifdef XT_VC2HIRESO_MAIN
#include "Vc2HiresoCheck.h"
#include "Vc2HiresoRate.h"
#include "XtSwitches.h"
#include <cstdio>

namespace {
constexpr uint32_t kClear = 0x893efc30;                                                  // [FW]
constexpr uint32_t kAnalysisTable = 0x895613f4, kDecimatorTable = 0x895616d8, kInterpolatorTable = 0x89561700;   // [FW]
constexpr uint32_t kObjAt = 0x8C030000, kInAt = 0x8C031000, kMidAt = 0x8C032000, kOutAt = 0x8C033000;   // Vc2Fw's RAM
constexpr int kBlocks = 100;

struct Buffers {
    float in[64] {}, mid[64] {}, out[64] {};
};

// A kind: its routine, the table rom.4's callers give it and the port's copy, its buffers' sizes (the floats it reads,
// reads or writes beside, and writes or adds into) and scale, and the port's routine on them.  Its arguments are the
// object, `in`, then `mid` when it has one, then `out`.
struct Kind {
    const char* name;
    uint32_t routine, table;
    const float* (*portTable)();
    int inWords, midWords, outWords;
    float scale;
    void (*port)(Vc2RateFilter&, Buffers&);
};

const float* analysisTable() { return vc2HiresoTables().analysisRate; }
const float* decimatorTable() { return vc2HiresoTables().decimator; }
const float* interpolatorTable() { return vc2HiresoTables().interpolator; }

constexpr Kind kKinds[] = {
    { "the analysis's 32→16", 0x893efd20, kAnalysisTable, analysisTable, 32, 0, 16, 1.0f,
      [](Vc2RateFilter& f, Buffers& b) { vc2Decimate(f, b.in, b.out, 16); } },
    { "the analysis's 16→8", 0x893efdb0, kAnalysisTable, analysisTable, 16, 0, 8, 1.0f,
      [](Vc2RateFilter& f, Buffers& b) { vc2Decimate(f, b.in, b.out, 8); } },
    { "the carrier's 32→16", 0x893efc80, kDecimatorTable, decimatorTable, 64, 32, 16, kVc2Bus,
      [](Vc2RateFilter& f, Buffers& b) { vc2DecimateFrames(f, b.in, b.mid, b.out); } },
    { "the carrier's 16→8", 0x893efdb0, kDecimatorTable, decimatorTable, 16, 0, 8, kVc2Bus,
      [](Vc2RateFilter& f, Buffers& b) { vc2Decimate(f, b.in, b.out, 8); } },
    { "8→16", 0x893efef0, kInterpolatorTable, interpolatorTable, 8, 0, 16, kVc2Bus,
      [](Vc2RateFilter& f, Buffers& b) { vc2InterpolateAdd(f, b.in, b.out); } },
    { "16→32", 0x893efe40, kInterpolatorTable, interpolatorTable, 16, 32, 64, kVc2Bus,
      [](Vc2RateFilter& f, Buffers& b) { vc2InterpolateFrames(f, b.in, b.mid, b.out); } },
    // the stereo path's (v218), on the tables slot 3 gives them, left and right interleaved
    { "the stereo carrier's 32→16", 0x893eff90, kDecimatorTable, decimatorTable, 64, 64, 32, kVc2Bus,
      [](Vc2RateFilter& f, Buffers& b) { vc2DecimateFrames(f, b.in, b.mid, b.out, 2); } },
    { "the stereo 16→8", 0x893f0160, kDecimatorTable, decimatorTable, 32, 0, 16, kVc2Bus,
      [](Vc2RateFilter& f, Buffers& b) { vc2Decimate(f, b.in, b.out, 8, 2); } },
    { "the stereo 8→16", 0x893f0350, kInterpolatorTable, interpolatorTable, 16, 0, 32, kVc2Bus,
      [](Vc2RateFilter& f, Buffers& b) { vc2InterpolateAdd(f, b.in, b.out, 2); } },
    { "the stereo 16→32", 0x893f0240, kInterpolatorTable, interpolatorTable, 32, 64, 64, kVc2Bus,
      [](Vc2RateFilter& f, Buffers& b) { vc2InterpolateFrames(f, b.in, b.mid, b.out, 2); } },
};

// A block's inputs: the signal's samples at the kind's scale in `in`, and something in the buffers it reads beside or
// adds into, as a caller's would hold
void draw(const Kind& k, Vc2Signal& signal, Buffers& b) {
    float x[kVc2Block];
    for (int at = 0; at < k.inWords; at += kVc2Block) {
        signal.block(x);
        for (int i = 0; i < kVc2Block && at + i < k.inWords; ++i) b.in[at + i] = x[i] * k.scale;
    }
    signal.block(x);
    for (int i = 0; i < 64; ++i) b.mid[i] = b.out[i] = x[i % kVc2Block] * k.scale * (i % 2 == 0 ? 0.5f : -0.25f);
}

// One block of a kind on both, then the first word that differs: the outputs, the buffer beside, the states
std::string rateBlockOn(Vc2Fw& fw, const Kind& k, Vc2RateFilter& f, Buffers& b) {
    fw.cpu.write(kInAt, b.in, sizeof b.in);
    fw.cpu.write(kMidAt, b.mid, sizeof b.mid);
    fw.cpu.write(kOutAt, b.out, sizeof b.out);
    if (!fw.call(k.routine, { kObjAt, kInAt, k.midWords == 0 ? kOutAt : kMidAt, kOutAt }))
        return "the routine faults: " + fw.cpu.error();
    k.port(f, b);
    std::string what = vc2FirstDifference(fw, kOutAt, b.out, sizeof(float) * (size_t)k.outWords, "the output");
    if (what.empty()) what = vc2FirstDifference(fw, kMidAt, b.mid, sizeof(float) * (size_t)k.midWords, "the buffer");
    if (what.empty()) what = vc2FirstDifference(fw, kObjAt + 4, f.s, sizeof f.s, "the states");
    return what;
}

// A case of a kind: the object cleared and given its table, then its blocks
std::string rateCaseOf(Vc2Fw& fw, const Kind& k, Vc2RateFilter& f, Vc2Signal& signal, long& blocks) {
    fw.cpu.wr32(kObjAt, k.table);
    if (!fw.call(kClear, { kObjAt })) return "the clear faults: " + fw.cpu.error();
    f.c = k.portTable();
    vc2RateClear(f);
    std::string what = vc2FirstDifference(fw, kObjAt + 4, f.s, sizeof f.s, "the cleared states");
    Buffers b;
    for (int n = 0; n < kBlocks && what.empty(); ++n, ++blocks) {
        draw(k, signal, b);
        what = rateBlockOn(fw, k, f, b);
    }
    return what;
}
} // namespace

long vc2HiresoRateCases(Vc2Fw& fw, const Vc2Run& run) {
    std::mt19937 rng((uint32_t)run.seed);
    const bool first = xtSwitch("XT_VC2HIRESO_FIRST") != nullptr;
    long blocks = 0, different = 0;
    Vc2RateFilter f;   // one through the cases, as rom.4's object at one address is: only the clear starts it again
    for (int c = 0; c < run.cases; ++c) {
        const Kind& k = kKinds[(size_t)c % std::size(kKinds)];
        Vc2Signal signal { std::mt19937(rng()), c % 8 == 7 };
        const std::string what = rateCaseOf(fw, k, f, signal, blocks);
        if (what.empty()) continue;
        if (!different++ && first) std::printf("  rate case %d, %s, block %ld: %s\n", c, k.name, blocks, what.c_str());
        if (xtSwitch("XT_VC2HIRESO_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("hireso rate: %d cases (%zu kinds), %ld blocks, %ld different\n", run.cases, std::size(kKinds), blocks,
                different);
    return different;
}
#endif
