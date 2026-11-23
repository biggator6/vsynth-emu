// SOLO's set-ups and seek on the OSC voice (v334): rom.4's 0x893cc2e0 (type 1) and 0x893cc310 (type 6), which
// VariPhrase's start runs on the object (rom.2's SOLO note-on 0x8950fba0, XtSoloVoice::noteOn's), and the start's seek
// for SOLO 0x893cc630 (rom.2's 0x8950fed0).  They read the wave's records through the object's head (+0x44), as mode
// 1's set-up does (Vc2OscModeOneStart.cpp).  HARDWARE_FIRMWARE.md, "The VC-2's SOLO".  Every address is rom.4's,
// every offset the object's.  Audio thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscSolo.h"
#include "Vc2OscTables.h"
#include "XtFloat.h"
#include <array>

namespace vc2solo {
namespace {
using xtf::add;
using xtf::flt;
using xtf::mul;
constexpr uint32_t kSamplesFrom = 0x4;   // [FW] the head's +4, the samples
constexpr uint32_t kFwdEndPos = 0xb4 + 0x4;   // [FW] the forward loop's end in the cached events (the second's position)
constexpr int kAgeShift = 8, kSampleShift = 4;   // [FW] ×16 into 1/4096; ×16 into a sample
// [FW] the grains' words the set-ups clear: the cutoff, the coefficient, each channel's two states
constexpr std::array<uint32_t, 6> kCleared { kCutoff, kCoef, kLowLeft, kBandLeft, kLowRight, kBandRight };

// [FW] the grains from where the walk's start left the anchor: both ages and the timer its way into the record (1/4096),
// both starts the record's; the windows' places by their ages (B's from ½); both grains' weights and periods the
// parameters'
void startGrains(Vc2WalkWords& w) {
    const Vc2OscSoloPool& k = vc2OscTables().solo;
    const int32_t age = (int32_t)((uint32_t)w.word(kAnchor) - (uint32_t)w.word(kRecStart)) << kAgeShift;
    for (uint32_t at : { kTimer, kGrainA.age, kGrainB.age }) w.setWord(at, age);
    for (uint32_t at : { kGrainA.start, kGrainB.start }) w.setWord(at, w.word(kRecStart));
    const float step = w.real(kWindowStep);
    w.setReal(kGrainA.window, mul(mul(flt(w.word(kGrainA.age)), step), k.setUpWindow));
    w.setReal(kGrainB.window, add(mul(mul(flt(w.word(kGrainB.age)), step), k.setUpWindow), k.setUpHalf));
    for (const Vc2OscSoloGrain& g : { kGrainA, kGrainB }) {
        w.setWord(g.weight, w.word(kWeight));
        w.setWord(g.period, w.word(kRecLength));
        w.setWord(g.tap, w.word(kTap));
    }
}
} // namespace
} // namespace vc2solo

// [FW] 0x893cc2e0 and 0x893cc310: byte 3 0 (type 1) or 0x80 (type 6); the end 0, the gain's rate 1/64, the toggle 0,
// the rate the pitch, the samples from the head; the table, cached events, layout and walk's start with SOLO's row
// (vc2WalkSetUp); the smoothed period the record's length, the grains' parameters, the grains; the last sample the
// range's end's (the last event's where the forward loop ends there) less 1; the filter cleared, its Q; the loops'
// rates (0x893ccf50) and the beat's start (0x893cbf40)
void vc2OscSoloSetUp(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared, bool typeSix) {
    using namespace vc2solo;
    using namespace vc2walk;
    const Vc2OscSoloPool& k = vc2OscTables().solo;
    Vc2WalkWords w { v, p };
    w.setByte(kWalkBits, typeSix ? kTypeSix : 0);
    w.setByte(kEnded, 0);
    w.setReal(kGainRate, k.setUpGain);
    w.setByte(kToggle, 0);
    w.setReal(kRateNow, w.real(kPitch));
    w.setWord(kSamples, (int32_t)vc2OscHeadWord(v, p, kSamplesFrom));
    vc2WalkSetUp(v, p, kVc2SoloLayout, kVc2SoloWalk);
    w.setReal(kSmooth, flt(w.word(kRecLength)));
    grainParams(w);
    startGrains(w);
    const bool loopEnds = (int32_t)w.memory((uint32_t)w.word(vc2walk::kRecord) + kFwdEndPos) == w.word(kRangeEnd);
    w.setWord(kLastSample, (w.word(loopEnds ? kFinal : kRangeEnd) >> kSampleShift) - 1);
    for (uint32_t at : kCleared) w.setReal(at, 0.0f);
    w.setReal(kQ, k.setUpQ);
    vc2OscLoopRates(v, p, shared);
    v.beatStart();
}

// [FW] 0x893cc630: the walk's seek with SOLO's row (vc2WalkSeek), in samples from the event 0; then in samples the
// segments forward while the next starts at or before the position and before the last event, each with its first
// record, and the records forward while their end is at or before it; the anchor the record's start.  The beat's
// reset (0x893cbfe0) comes before the segments in rom.4, which read and write nothing of it; the voice's seekBy resets
// it after
void vc2OscSeekSolo(Vc2OscVoice& v, Vc2OscParts& p, int32_t at) {
    using namespace vc2solo;
    using namespace vc2walk;
    Vc2WalkWords w { v, p };
    const bool inSamples = (w.byte(kFlags) & kInSamples) != 0;
    if (inSamples) w.setHalf(kIndex, 0);
    vc2WalkSeek(v, p, at, kVc2SoloWalk);
    if (!inSamples) return;
    const int32_t to = w.word(kPos);
    while (!(w.word(kNextSegment) > to) && !(w.word(kNextSegment) >= w.word(kFinal))) {
        const int32_t next = w.word(kNextSegment);
        w.setWord(kSegment, next);
        w.setWord(kRecEnd, next);
        w.setWord(kNextRecord, w.word(kNextFirst));
        segmentEntry(w, (uint32_t)w.word(kEntryAfter));
        loadNext(w);
    }
    while (!(w.word(kRecEnd) > to)) loadNext(w);
    w.setWord(kAnchor, w.word(kRecStart));
}
