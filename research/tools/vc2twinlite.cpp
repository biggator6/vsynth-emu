// vc2run lite <rom.2>: LITE's player (the voice's kind 1, Vari on; rom.2's 0x8950a7f0 mono, 0x8950a7fc stereo) on rom.2's
// and rom.4's routines, without the port (xt_tone holds XtLitePlayer to rom.2, so rom.2 equal to rom.4 is the port equal to
// rom.4).  Its note-on runs inside the voice's (0x8950b5b0, at 0x8950b8d0 on the voice's registers), so each note's state
// is set in rom.2's player as XtLitePlayer's note-on leaves it (the fields XtLitePlayer.h lists), copied into rom.4's
// layout, and each image's block routine run on it 120 times, compared as the band and SOLO runs compare
// (vc2twinplay.cpp).  Notes that walk forward throughout are counted apart from those whose walk is ever 0 or backward:
// rom.4's backward kernels read the wave the other way up (research/VC2_VOCAL_DESIGNER.md).  Fails on any difference.
#include "vc2runs.h"
#include "vc2twinplay.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace vc2 {

namespace {

constexpr int kWaves = 12, kNotes = 6, kBlocks = 120;
// The wave between zeroed margins written to both machines: the heads read past either end (below the start walking
// backward), which on the XT is whatever lies there and here must be the same in both
constexpr uint32_t kMargin = 0x10000, kWaveAt = kSamples + kMargin;
constexpr float kUnity = 1.0f / 65536.0f;              // the gain the players scale int16 by, as xtFwBandPlay sets it
constexpr float kFilterPerHz = 3.5619449e-5f;           // [FW] the filter's rate per Hz, π/2 / 44100 (XtLitePlayer)
constexpr double kPeriodHz = 88200.0;                   // [FW] the period: 88200 over the root (XtLitePlayer)
// [FW] LITE's loop modes: its routine tables (0x8950a870, 0x8950a890; rom.4's 0x893c6f00, 0x893c6f20) hold three a way,
// the fourth entry 0 in both images, so mode 3 jumps to 0
constexpr int kLiteModes = 3;
constexpr uint32_t kNoBound = 0x7fffffff, kEventStride = 0xb4;
// [FW] what the beat (0x8950c270) reads: the rates, and the cached loop events at +0x6c, each event's position at +4, 0xb4
// apart (forward start, forward end, backward end, backward start, ×16; with no loop, no bounds), as XtLitePlayer has them
constexpr SetField kLiteCommon[] = {u32At(0x48, 44100), u32At(0x4c, 44100), u32At(0x6c, kCache)};
constexpr Twin::Pair kMono{0x8950a7f0, 0x893c6e80}, kStereo{0x8950a7fc, 0x893c6e8c};   // [FW] the kind 1 installs'

void writeLiteCache(Twin& t, const XtLoop& l) {
    const std::vector<uint8_t> zero(kCacheBytes, 0);
    t.write(kCache, zero.data(), zero.size());
    const uint32_t on[4] = {(uint32_t)l.fwdStart * 16, (uint32_t)l.fwdEnd * 16, (uint32_t)l.bwdEnd * 16,
                            (uint32_t)l.bwdStart * 16};
    const uint32_t off[4] = {0, kNoBound, kNoBound, 0};
    for (uint32_t k = 0; k < 4; ++k) t.write(kCache + kEventStride * k + 4, l.on ? &on[k] : &off[k], 4);
}

uint32_t modeOf(const XtLoop& l, int mode) {
    return l.on ? (uint32_t)(mode % kLiteModes) : 0u;
}

// Where the walk starts: at the first sample walking forward, else at the last half period's start below the end
float walkStart(int32_t first, int32_t last, float walk, float period) {
    if (walk > 0.0f) return (float)first;
    const float half = period / 2.0f;
    return std::trunc((float)last / half) * half;
}

// [FW] LITE's state (XtLitePlayer.h): the rates, the loops' modes, the samples, the heads, the filter, the fractions, the
// walk's position, the windows, the ends, the loops as floats, the period and its inverse, the position in 1/16 sample
void setLite(Twin& t, Rng& r, const TestWave& w, const TestNote& n) {
    const XtLoop& l = n.loop;
    const int32_t last = (int32_t)w.frames - 1;
    const int32_t first = r.next() % 3 == 0 ? (int32_t)(r.next() % (uint32_t)(last / 4)) : 0;
    const float root = r.in(80.0f, 800.0f), formant = r.in(0.5f, 2.0f);
    const float period = (float)(kPeriodHz / root), frac = formant - std::trunc(formant);
    const float walkAt = walkStart(first, last, n.walk, period);
    const SetField f[] = {
        u8At(0, 0x80u | (l.on ? 1u : 0u)), f32At(0x70, n.pitch), f32At(0x74, formant), f32At(kWalk, n.walk),
        f32At(kGain, kUnity), f32At(0x84, kUnity), u32At(0xe8, modeOf(l, l.fwdMode)), u32At(0xec, modeOf(l, l.bwdMode)),
        u32At(0xf0, kWaveAt), u32At(0xf4, (uint32_t)walkAt), u32At(0xf8, (uint32_t)walkAt),
        f32At(0xfc, root * n.pitch * kFilterPerHz), f32At(0x108, frac), f32At(0x10c, frac), f32At(0x110, walkAt),
        f32At(0x11c, period / 2.0f), f32At(0x120, period / 2.0f), f32At(0x124, (float)last),
        f32At(0x128, (float)l.fwdStart), f32At(0x12c, (float)l.fwdEnd), f32At(0x130, (float)(l.fwdEnd - l.fwdStart)),
        f32At(0x134, 1.0f), f32At(0x138, (float)first), f32At(0x13c, (float)l.bwdEnd),
        f32At(0x140, (float)l.bwdStart), f32At(0x144, (float)(l.bwdEnd - l.bwdStart)), f32At(0x148, 1.0f),
        f32At(0x14c, period), f32At(0x150, 1.0f / period), u32At(0x38, (uint32_t)walkAt << 4),
    };
    setFields(t, f);
}

// Whether a note walks forward throughout: its start and every move above 0 (the block takes the forward kernels only then)
bool forwardOnly(const TestNote& n, const NoteMoves& m) {
    return n.walk > 0.0f && std::ranges::all_of(m.walkAt, [](const auto& at) { return at.second > 0.0f; });
}

// One note: its state set in rom.2's player and copied, then its blocks, counted by its walk; false on a fault
bool liteNote(Twin& t, Rng& r, const TestWave& w, Twin::Returns (&counts)[2]) {
    const TestNote n = makeTestNote(r, w);
    const NoteMoves moves = makeMoves(r, kBlocks);
    const bool forward = forwardOnly(n, moves);
    const std::vector<uint8_t> zero(kPlayerBytes, 0);
    t.vsynth.write(kPlayer, zero.data(), zero.size());
    setFields(t, kLiteCommon);
    writeLiteCache(t, n.loop);
    setLite(t, r, w, n);
    t.copyMapped(kPlayer, kPlayerBytes + kOscInsert.bytes, kOscMap);
    char name[64];
    std::snprintf(name, sizeof name, "LITE, %s, walk %s", w.channels == 2 ? "stereo" : "mono",
                  forward ? "forward" : "back or still");
    const PlayerRun run{w.channels == 2 ? kStereo : kMono, kOscMap, name};
    return playBlocks(t, run, kBlocks, moves, counts[forward ? 0 : 1]);
}

// The cases once through, the same each time; their differences, or −1 on a fault
long litePass(Twin& t, const char* label) {
    Rng r;
    Twin::Returns all, counts[2];
    const std::vector<uint8_t> margin(kMargin, 0);
    for (int k = 0; k < kWaves; ++k) {
        const TestWave w = makeTestWave(r);
        t.write(kSamples, margin.data(), margin.size());
        t.write(kWaveAt, w.pcm.data(), w.pcm.size() * 2);
        t.write(kWaveAt + (uint32_t)(w.pcm.size() * 2), margin.data(), margin.size());
        for (int j = 0; j < kNotes; ++j)
            if (!liteNote(t, r, w, counts)) return -1;
    }
    std::printf("%s\n", label);
    addCount("LITE, walk forward", "blocks", counts[0], all);
    addCount("LITE, walk back or still", "blocks", counts[1], all);
    return sayCount("lite", "blocks", all);
}

// [FW] rom.2's backward kernels (0x8950b1d4 mono, 0x8950b41a stereo) read a sample negated: FNEG, then FADD onto the
// difference; rom.4's (0x893c7864, 0x893c7aa6) FSUB with the sample as it is. A pair for each head, and each channel's
constexpr uint32_t kBackwardFneg[] = {0x8950b28a, 0x8950b2b6, 0x8950b504, 0x8950b51c, 0x8950b54a, 0x8950b562};
constexpr uint32_t kBackwardFadd[] = {0x8950b290, 0x8950b2bc, 0x8950b50a, 0x8950b522, 0x8950b550, 0x8950b568};
constexpr uint16_t kNop = 0x0009, kFaddToFsub = 0x0001;   // FADD FRm,FRn is 0xFnm0, FSUB 0xFnm1

using Fnegs = std::array<uint16_t, std::size(kBackwardFneg)>;

// rom.2's backward reads made rom.4's in rom.2's machine (its code, not the image); the FNEGs as they were
Fnegs backwardAsRom4(Twin& t) {
    Fnegs was{};
    for (size_t k = 0; k < was.size(); ++k) {
        was[k] = t.vsynth.rd16(kBackwardFneg[k]);
        t.vsynth.wr16(kBackwardFneg[k], kNop);
        t.vsynth.wr16(kBackwardFadd[k], (uint16_t)(t.vsynth.rd16(kBackwardFadd[k]) ^ kFaddToFsub));
    }
    return was;
}

// And back as they were
void backwardAsRom2(Twin& t, const Fnegs& was) {
    for (size_t k = 0; k < was.size(); ++k) {
        t.vsynth.wr16(kBackwardFneg[k], was[k]);
        t.vsynth.wr16(kBackwardFadd[k], (uint16_t)(t.vsynth.rd16(kBackwardFadd[k]) ^ kFaddToFsub));
    }
}

// The cases as the images are, which fail the run on any difference; then again with rom.2's backward reads as rom.4's,
// which shows whether they are all the difference
int liteCases(Twin& t) {
    const long asIs = litePass(t, "as the images are:");
    if (asIs < 0) return -1;
    const Fnegs was = backwardAsRom4(t);
    const long patched = litePass(t, "rom.2's six backward reads as rom.4's:");
    backwardAsRom2(t, was);
    return patched < 0 ? -1 : (int)asIs;
}

} // namespace

int lite(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, noPortTables, liteCases);
}

} // namespace vc2
