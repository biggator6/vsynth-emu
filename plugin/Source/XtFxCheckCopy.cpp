// xt_fx control's copy without the chip (v205): XtExtInCopy, the input as program A hands it back computed without the
// chip, against XtFxUnit's words 4 and 5, bit for bit, under every External Input Type and Source, settled and after a
// change of Type, and at the defaults with nothing set (XtFxCheck.h).  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckUnit.h"
#include <climits>

namespace xtfxcheck {
namespace {
constexpr int kFrames = 8192;   // a setting's frames
constexpr int kSettle = 4410;   // what the chip has after a change of Type, 100 ms, before the two are compared again

// The input's next frame: noise over the whole of int32, now and then its extremes
void noiseFrame(Rng& r, int32_t* lr) {
    const uint32_t k = r.next() % 16;
    for (int side = 0; side < 2; ++side) lr[side] = k == 0 ? INT32_MAX : k == 1 ? INT32_MIN : (int32_t)r.next();
}

// The unit at Type `from` and Source `source` (from < 0: the defaults, nothing set), then `to`: the frames whose words
// differ, those of the change's kSettle aside
long copyAgainstChip(const char* rom, int from, int to, int source, uint32_t seed) {
    using Sets = std::vector<std::pair<int, int>>;
    const auto u = settledUnit(rom, xtVSynthFx(), from >= 0 ? Sets { { 0x78, from }, { 0x7c, source } } : Sets {});
    XtExtInCopy copy;
    if (from >= 0) copy.setType(from);
    XtExtInput copied;
    Rng r(seed);
    float frame[8] = {}, lr[2];
    long off = 0;
    for (int i = 0; i < 2 * kFrames; ++i) {
        if (i == kFrames && to != from) {
            u->control().set(0x78, to);
            u->control().update();
            copy.setType(to);
        }
        int32_t in[2];
        noiseFrame(r, in);
        u->pushInput(in, 1);
        u->process(frame, lr, 1);
        copy.into(copied, in, 1);
        const int32_t* c = copied.take(1);
        const bool settling = to != from && i >= kFrames && i < kFrames + kSettle;
        off += !settling && (c[0] != u->returned()[0] || c[1] != u->returned()[1]);
    }
    return off;
}
} // namespace

void copyWithoutChip(const char* rom, uint32_t seed) {
    int tried = 0, different = 0;
    const auto run = [&](int from, int to, int source) {
        const long off = copyAgainstChip(rom, from, to, source, seed + (uint32_t)tried);
        tried++;
        different += off ? 1 : 0;
        if (off && xtSwitch("XT_FX_FIRST"))
            std::printf("  copy: Type %d then %d, Source %d: %ld frames off the chip's\n", from, to, source, off);
    };
    for (int source = 0; source <= 1; ++source)
        for (int type = 0; type <= 3; ++type) run(type, (type + 1) % 4, source);
    run(-1, -1, 0);                                                              // nothing set: MONO MIX, Analog
    std::printf("the copy without the chip against the chip: %d samples, %d different\n", tried, different);
}
} // namespace xtfxcheck
#endif
