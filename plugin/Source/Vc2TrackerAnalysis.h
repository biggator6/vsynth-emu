#pragma once
// The VC-2's pitch tracker's analysis (v224): what each zero crossing of the band-passed signal does, on the frame's
// state (Vc2TrackerWords), word for word.  rom.4's two crossing routines (up 0x893ee570, down 0x893ee7d0), mirror
// images of each other, are one routine over a direction (Vc2TrackerDirection): a side's peak threshold, the onset
// check (0x893eea50) that says whether the crossing counts, its fraction of a sample, and then, the first crossing
// of its direction since the other's, the estimator (0x893eeb90) and taking its estimate (0x893ef110), which move P
// and the band-pass's targets; a second of the same direction instead keeps the half-intervals' history.  Bit for
// bit: xt_vc2tracker, the whole tracker against rom.4's module in Sh4Interp.  HARDWARE_FIRMWARE.md, "The pitch
// tracker's analysis".  Audio thread.
#include "Vc2Tracker.h"
#include <cstdint>

// What an up crossing and a down crossing differ in: their own words and the other direction's
struct Vc2TrackerDirection {
    int32_t index;                              // 0 up, 1 down: the estimator's argument, the follow state's value
    int32_t mark;                               // what lastUp reads after it: up 1, down 0
    bool negate;                                // the peak since starts from −now (down) or now (up)
    int32_t Vc2TrackerWords::* since;           // samples since the last crossing of this direction
    int32_t Vc2TrackerWords::* otherSince;
    Vc2TrackerSide Vc2TrackerWords::* side;
    Vc2TrackerSide Vc2TrackerWords::* otherSide;
    int32_t Vc2TrackerWords::* crossingsAt;     // the count of crossings at this direction's last valid one
    int32_t Vc2TrackerWords::* otherCrossingsAt;
    bool (*moves)(int32_t follow);              // whether this direction's estimates move P, by the follow state
};

class Vc2TrackerAnalysis final : public Vc2TrackerCrossings {
public:
    void up(Vc2TrackerWords& s, float previous, float now) override;     // 0x893ee570
    void down(Vc2TrackerWords& s, float previous, float now) override;   // 0x893ee7d0
};

// 0x893eea50: whether a crossing n samples after the last of its direction, after a peak since of `peak`, counts;
// it may clear or halve the lock
void vc2TrackerOnset(Vc2TrackerWords& s, int32_t n, float peak, int32_t& valid);

// 0x893eeb90: a period from the half-intervals, or 0 when none is trusted; moves the halves, the fraction and the
// last candidate, and may clear the lock, set follow to both or raise the hand-off
float vc2TrackerEstimate(Vc2TrackerWords& s, const Vc2TrackerDirection& d, float fraction);

// 0x893ef110: whether an estimate is taken, with the lock, failures and smoothing it leaves
int32_t vc2TrackerTake(Vc2TrackerWords& s, float estimate);
