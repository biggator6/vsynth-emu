// xt_vc2libm <rom.4> [cases] [seed]: Vc2LibMath against rom.4's own routines in the interpreter, every answer bit for
// bit, NaNs' too.  logf and powf by their wrappers (0x895065f4, 0x89506e54), whose errno is held to what the answer
// says, and the two routines under powf by their own entries.  Each routine in turn: its edges (every pair of them for
// powf); [cases] draws aimed at its branches (Vc2LibMathDraws.h); a sweep of the float patterns, every 2^kSweepShift-th
// from an offset the seed draws (powf's over x, then over y).  XT_VC2LIBM_FIRST says where the first difference is,
// XT_VC2LIBM_STOP stops there.  XT_VC2LIBM_MAIN only.
#ifdef XT_VC2LIBM_MAIN
#include "Vc2CheckRig.h"
#include "Vc2CheckRom2.h"
#include "Vc2LibMath.h"
#include "Vc2LibMathDraws.h"
#include "Vc2LibMathTables.h"
#include "Vc2ToneMathCheck.h"
#include "XtOsc.h"

namespace {
constexpr uint32_t kLog = 0x895065f4, kPow = 0x89506e54, kExp = 0x89509928, kLdexp = 0x8950ae24;   // [FW]
constexpr uint32_t kErrnoAt = 0x89add990, kErrnoNan = 1101, kErrnoInfinite = 1100;                 // [FW]
constexpr int kSweepShift = 12;
constexpr uint64_t kPatterns = 1ull << 32;

// A call of a routine: its arguments, and the port's answer to them
struct LibCall {
    uint32_t routine;
    float x, y;
    int32_t n;
    float mine;
};

struct LibRig {
    Vc2Fw& fw;
    Vc2Tally tally { "XT_VC2LIBM_FIRST", "XT_VC2LIBM_STOP" };
    long calls = 0, notNumbers = 0, infinite = 0, zeros = 0, negative = 0, errnos = 0;
    bool stopped = false;
};

uint32_t errnoFor(float answer) {
    if (answer != answer) return kErrnoNan;
    return std::isinf(answer) ? kErrnoInfinite : 0;
}

std::string saidOf(const LibCall& c, uint32_t theirs, const char* what) {
    char t[160];
    std::snprintf(t, sizeof t, "0x%08x (x %08x, y %08x, n %d): %s %08x, against %08x", c.routine,
                  std::bit_cast<uint32_t>(c.x), std::bit_cast<uint32_t>(c.y), c.n, what,
                  std::bit_cast<uint32_t>(c.mine), theirs);
    return t;
}

// What differs between rom.4's answer and the port's, the wrapper's errno with it; empty when nothing does
std::string differenceOf(LibRig& r, const LibCall& c) {
    r.fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(c.x);
    r.fw.cpu.fb[0][5] = std::bit_cast<uint32_t>(c.y);
    r.fw.cpu.wr32(kErrnoAt, 0);
    if (!r.fw.call(c.routine, { (uint32_t)c.n })) return "it faults: " + r.fw.cpu.error();
    const uint32_t theirs = r.fw.cpu.fb[0][0];
    if (theirs != std::bit_cast<uint32_t>(c.mine)) return saidOf(c, theirs, "the answer");
    if (c.routine != kLog && c.routine != kPow) return "";
    const uint32_t errnoWord = r.fw.cpu.rd32(kErrnoAt);
    r.errnos += errnoWord != 0;
    return errnoWord == errnoFor(c.mine) ? "" : saidOf(c, errnoWord, "errno for the answer");
}

void onBoth(LibRig& r, const LibCall& c) {
    if (r.stopped) return;
    ++r.calls;
    r.notNumbers += c.mine != c.mine;
    r.infinite += std::isinf(c.mine);
    r.zeros += c.mine == 0.0f;
    r.negative += c.mine < 0.0f;
    r.stopped = vc2CaseOutcome(r.tally, differenceOf(r, c), (int)r.calls, 0);
}

// Every 2^kSweepShift-th float pattern from an offset below that, each through `one`
template <class One>
void sweepPatterns(LibRig& r, std::mt19937& rng, One one) {
    const uint32_t offset = rng() & ((1u << kSweepShift) - 1);
    for (uint64_t p = offset; p < kPatterns && !r.stopped; p += 1u << kSweepShift)
        one(std::bit_cast<float>((uint32_t)p));
}

// A routine of one float: its edges, its draws and the sweep
template <class Draw, class One>
void checkOfOneFloat(LibRig& r, const Vc2Run& run, std::mt19937& rng, Draw draw, One one) {
    for (uint32_t edge : libEdges()) one(std::bit_cast<float>(edge));
    for (int c = 0; c < run.cases && !r.stopped; ++c) one(draw(rng));
    sweepPatterns(r, rng, one);
}

void checkLdexp(LibRig& r, const Vc2Run& run, std::mt19937& rng) {
    const auto one = [&](float x, int32_t n) { onBoth(r, { kLdexp, x, 0.0f, n, vc2LibLdexp(x, n) }); };
    for (uint32_t edge : libEdges())
        for (int32_t n : libScales()) one(std::bit_cast<float>(edge), n);
    for (int c = 0; c < run.cases && !r.stopped; ++c) {
        const LibScaled draw = libDrawLdexp(rng);
        one(draw.x, draw.n);
    }
    sweepPatterns(r, rng, [&](float x) { one(x, libDrawScale(rng, x)); });
}

void checkPow(LibRig& r, const Vc2Run& run, std::mt19937& rng) {
    const auto one = [&](float x, float y) { onBoth(r, { kPow, x, y, 0, vc2LibPow(x, y) }); };
    for (uint32_t x : libEdges())
        for (uint32_t y : libEdges()) one(std::bit_cast<float>(x), std::bit_cast<float>(y));
    for (int c = 0; c < run.cases && !r.stopped; ++c) {
        const LibPower draw = libDrawPow(rng);
        one(draw.x, draw.y);
    }
    sweepPatterns(r, rng, [&](float x) { one(x, libDrawPower(rng)); });
    sweepPatterns(r, rng, [&](float y) { one(libDrawBase(rng), y); });
}

// rom.4's tables, and (v320) XtOsc's from the rom.2 beside it, the second part's velocity curve's and exp2's
bool libLoadTables(const char* path) { return vc2LoadLibMath(path) && xtOscLoadTables(vc2Rom2Of(path).c_str()); }
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2libm", libLoadTables);
    if (!run) return 1;
    LibRig r { fw };
    std::mt19937 rng((uint32_t)run->seed);
    checkOfOneFloat(r, *run, rng, libDrawLog, [&](float x) { onBoth(r, { kLog, x, 0.0f, 0, vc2LibLog(x) }); });
    checkOfOneFloat(r, *run, rng, libDrawExp, [&](float x) { onBoth(r, { kExp, x, 0.0f, 0, vc2LibExp(x) }); });
    checkLdexp(r, *run, rng);
    checkPow(r, *run, rng);
    std::printf("calls: %ld, answers NaN %ld, infinite %ld, zero %ld, negative %ld; errno written %ld times\n", r.calls,
                r.notNumbers, r.infinite, r.zeros, r.negative, r.errnos);
    if (!r.stopped) vc2ToneMathPart(fw, *run, r.tally);   // v320
    std::printf("xt_vc2libm: %ld different\n", r.tally.different);
    return 0;
}
#endif
