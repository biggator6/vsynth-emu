// The VariPhrase players side by side, what both runs share: vc2twinplay.h.
#include "vc2twinplay.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace vc2 {

namespace {

constexpr double kRate = 44100.0;
constexpr int kHarmonics = 6, kMostStrikes = 3, kShortestLoop = 512;
constexpr double kFullScale = 32767.0, kLevel = 9000.0;

int pickInt(Rng& r, int lo, int hi) {
    return lo + (int)(r.next() % (uint32_t)(hi - lo + 1));
}

// A walk as a note's start or a move gives it: still one time in six, backward one in four
float pickWalk(Rng& r) {
    const uint32_t k = r.next() % 12;
    if (k < 2) return 0.0f;
    return k < 5 ? r.in(-1.5f, -0.2f) : r.in(0.2f, 2.0f);
}

// The sample of the wave's tone at frame i for one channel: the harmonics under the envelope since the last strike
double toneAt(const std::array<double, kHarmonics>& amp, double f0, long sinceStrike, long i, int channel) {
    const double t = (double)sinceStrike / kRate;
    const double env = std::exp(-3.0 * t) * (1.0 - std::exp(-400.0 * t));
    double v = 0;
    for (int k = 0; k < kHarmonics; ++k) {
        const double phase = 2.0 * std::numbers::pi * f0 * (k + 1) * (double)i / kRate + 0.7 * channel * k;
        v += amp[(size_t)k] * std::sin(phase);
    }
    return v * env * kLevel;
}

int16_t toSample(double v) {
    return (int16_t)std::lround(std::clamp(v, -kFullScale, kFullScale));
}

// A loop of the wave's at random: its points at least kShortestLoop apart, each end's mode 0…3
XtLoop pickLoop(Rng& r, long frames) {
    const int last = (int)frames - 1;
    const int fwdMode = pickInt(r, 0, 3), bwdMode = pickInt(r, 0, 3);
    const int fwdStart = pickInt(r, 0, last / 2), bwdStart = pickInt(r, 0, last / 2);
    const int fwdEnd = pickInt(r, fwdStart + kShortestLoop, last), bwdEnd = pickInt(r, bwdStart + kShortestLoop, last);
    return XtLoop{true, fwdMode, bwdMode, fwdStart, fwdEnd, bwdStart, bwdEnd};
}

// Where the images part after a call: the output, the player, its cache; nullptr if nowhere.  Not r0: the players
// return nothing, and leave a code address or an offset there that differs by image
const char* partsAt(Twin& t, const PlayerRun& p, bool withOut, long& at) {
    if (withOut && (at = t.differs(kBlockOut, kBlockOutBytes)) >= 0) return "the output";
    if ((at = t.differsMapped(kPlayer, p.map)) >= 0) return "the player";
    at = t.differs(kCache, kCacheBytes);
    return at >= 0 ? "its loop cache" : nullptr;
}

} // namespace

TestWave makeTestWave(Rng& r) {
    TestWave w;
    w.channels = r.next() % 3 == 0 ? 2 : 1;
    w.frames = pickInt(r, 6000, 20000);
    const double f0 = r.in(60.0f, 700.0f);
    std::array<double, kHarmonics> amp{};
    for (size_t k = 0; k < amp.size(); ++k) amp[k] = r.in(0.0f, 1.0f) / (double)(k + 1);
    std::vector<long> strikes{0};
    const int more = pickInt(r, 0, kMostStrikes - 1);
    for (int s = 0; s < more; ++s) strikes.push_back(pickInt(r, 1000, (int)w.frames - 1000));
    std::ranges::sort(strikes);
    w.pcm.resize((size_t)(w.frames * w.channels));
    size_t s = 0;
    for (long i = 0; i < w.frames; ++i) {
        while (s + 1 < strikes.size() && strikes[s + 1] <= i) ++s;
        for (int c = 0; c < w.channels; ++c)
            w.pcm[(size_t)(i * w.channels + c)] = toSample(toneAt(amp, f0, i - strikes[s], i, c));
    }
    return w;
}

TestNote makeTestNote(Rng& r, const TestWave& w) {
    const float pitch = std::exp2(r.in(-2.0f, 2.0f));
    const float walk = pickWalk(r);
    return TestNote{pitch, walk, r.next() % 3 == 0 ? pickLoop(r, w.frames) : XtLoop{}};
}

NoteMoves makeMoves(Rng& r, int blocks) {
    NoteMoves m;
    const int walks = pickInt(r, 0, 3), gains = pickInt(r, 0, 2);
    for (int k = 0; k < walks; ++k) m.walkAt.emplace_back(pickInt(r, 1, blocks - 1), pickWalk(r));
    for (int k = 0; k < gains; ++k) m.gainAt.emplace_back(pickInt(r, 1, blocks - 1), r.in(0.0f, 1.5f));
    std::ranges::sort(m.walkAt);
    std::ranges::sort(m.gainAt);
    return m;
}

void setFields(Twin& t, std::span<const SetField> fields) {
    for (const SetField& f : fields) t.vsynth.write(kPlayer + f.offset, &f.bits, f.bytes);
}

void tallyCall(Twin& t, const PlayerRun& p, bool withOut, const char* what, Twin::Returns& tally) {
    long at = -1;
    const char* where = partsAt(t, p, withOut, at);
    ++tally.cases;
    if (!where) return;
    if (tally.differ++ == 0) std::printf("%s: %s at +%#lx\n", what, where, at);
}

bool playBlocks(Twin& t, const PlayerRun& p, int blocks, const NoteMoves& moves, Twin::Returns& tally) {
    const std::vector<uint8_t> zero(kBlockOutBytes, 0);
    size_t w = 0, g = 0;
    for (int b = 0; b < blocks; ++b) {
        for (; w < moves.walkAt.size() && moves.walkAt[w].first <= b; ++w)
            t.writeF32(kPlayer + kWalk, kPlayer + kWalk, moves.walkAt[w].second);
        for (; g < moves.gainAt.size() && moves.gainAt[g].first <= b; ++g)
            t.writeF32(kPlayer + kGain, kPlayer + kGain, moves.gainAt[g].second / 65536.0f);
        t.write(kBlockOut, zero.data(), zero.size());
        if (!t.call(p.block, {kBlockOut, kPlayer})) return false;
        char what[96];
        std::snprintf(what, sizeof what, "%s, block %d", p.name, b);
        tallyCall(t, p, true, what, tally);
    }
    return true;
}

} // namespace vc2
