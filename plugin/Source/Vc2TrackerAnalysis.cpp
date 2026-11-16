// The VC-2's pitch tracker's analysis: see Vc2TrackerAnalysis.h (v224).  Each routine's arithmetic in its own order,
// in XtFloat; every address is rom.4's.  The estimator is Vc2TrackerEstimate.cpp.
#include "Vc2TrackerAnalysis.h"
#include "XtFloat.h"
#include <algorithm>
#include <cmath>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;

constexpr int32_t kLocked = 3;      // [FW] a lock above it is locked: the onset check and the history run, taking resets
constexpr int32_t kFailing = 3;     // [FW] refused estimates from which each halves the lock
constexpr int32_t kMost = 256;      // [FW] the lock's and the failures' cap
constexpr int32_t kBoth = 2;        // [FW] follow: both directions' estimates move P

constexpr bool upMoves(int32_t follow) { return (follow & 1) == 0; }   // [FW] 0x893ee6a4: tst #1
constexpr bool downMoves(int32_t follow) { return follow > 0; }        // [FW] 0x893ee994: cmp/pl

// [FW] the two routines' own words, by what each reads and writes
constexpr Vc2TrackerDirection kUp {
    0, 1, false, &Vc2TrackerWords::sinceUp, &Vc2TrackerWords::sinceDown, &Vc2TrackerWords::up,
    &Vc2TrackerWords::down, &Vc2TrackerWords::crossingsAtUp, &Vc2TrackerWords::crossingsAtDown, upMoves,
};
constexpr Vc2TrackerDirection kDown {
    1, 0, true, &Vc2TrackerWords::sinceDown, &Vc2TrackerWords::sinceUp, &Vc2TrackerWords::down,
    &Vc2TrackerWords::up, &Vc2TrackerWords::crossingsAtDown, &Vc2TrackerWords::crossingsAtUp, downMoves,
};

// The end of a valid crossing that went on: the counters from here, the other side's pending peak dropped
void endCrossing(Vc2TrackerWords& s, const Vc2TrackerDirection& d) {
    s.sinceCrossing = s.lastSince = 0;
    s.lastUp = d.mark;
    s.*d.since = 0;
    s.*d.crossingsAt = s.crossings;
    s.crossings = 0;
    (s.*d.otherSide).pending = 0.0f;
}

// An estimate taken by a direction that follows: P toward it by the smoothing, and the band-pass's targets on it
void moveP(Vc2TrackerWords& s, const Vc2TrackerCrossingPool& k, float estimate) {
    s.fallback = s.period;
    s.estimate = estimate;
    s.period = add(mul(sub(estimate, s.period), s.smoothing), s.period);   // FMAC
    const Vc2TrackerLimits& follow = s.settings.follow;
    if (!(s.period > follow.shortest && follow.longest > s.period)) return;
    const float w = xtf::div(k.twoPi, s.period);
    s.lowpassTarget = add(w, k.band);
    s.highpassTarget = mul(w, k.half);
}

// The first crossing of a direction since the other's: estimate, take, and the follow state
void estimatePath(Vc2TrackerWords& s, const Vc2TrackerDirection& d, const Vc2TrackerCrossingPool& k, float fraction) {
    const float estimate = vc2TrackerEstimate(s, d, fraction);
    const int32_t taken = vc2TrackerTake(s, estimate);
    const int32_t follow = s.follow;
    if (taken != 0 && d.moves(follow)) {
        moveP(s, k, estimate);
        if (follow == kBoth) {   // the first to move P follows from now, or the other if the estimator handed off
            s.follow = s.handOff != 0 ? d.index ^ 1 : d.index;
            s.handOff = 0;
            return;
        }
    }
    if (s.handOff != 0 && s.follow == d.index) s.follow = d.index ^ 1;
    s.handOff = 0;
}

// A second crossing of a direction: the interval since the last valid one into the halves' history, the other side's
// peaks on; false when it doesn't count
bool historyPath(Vc2TrackerWords& s, const Vc2TrackerDirection& d, const Vc2TrackerCrossingPool& k, float fraction,
    int32_t lastSince, float peak) {
    if (!(lastSince > 0)) return false;
    Vc2TrackerSide& other = s.*d.otherSide;
    const float second = (s.*d.side).second;
    float recent = other.second;
    if (!(recent > second)) recent = second;
    if (s.lock > kLocked) return false;
    if (mul(recent, k.history) > peak) return false;
    const int32_t span = s.sinceCrossing - lastSince;
    s.*d.otherSince = span;
    s.halves[3] = s.halves[2];
    s.halves[2] = s.halves[1];
    s.halves[1] = xtf::flt(lastSince);
    s.halves[0] = xtf::flt(span);
    other.second = other.last;
    s.fraction = fraction;
    other.last = other.pending;
    return true;
}

// A crossing, previous one side of 0 and now the other: rom.4's two routines over their direction
void crossing(Vc2TrackerWords& s, const Vc2TrackerDirection& d, float previous, float now) {
    const Vc2TrackerCrossingPool& k = vc2TrackerTables().crossing[d.index];
    Vc2TrackerSide& own = s.*d.side;
    if (xtf::flt(s.*d.since) > s.longest) {   // none of this direction for the longest period: the pending peak's
        own.threshold = own.pending;
        own.pending = mul(own.pending, k.decay);
    }
    const float peak = s.peak;
    const float fallen = mul(own.threshold, k.decay);
    int32_t valid = peak > fallen ? 1 : 0;
    own.threshold = valid != 0 ? peak : fallen;
    ++s.crossings;
    s.peak = d.negate ? -now : now;
    const int32_t lastSince = s.lastSince;
    s.lastSince = s.sinceCrossing;
    float held = own.pending;
    if (peak > held) held = peak;
    const float otherLast = (s.*d.otherSide).last;
    s.recentPeak = otherLast > held ? otherLast : held;
    vc2TrackerOnset(s, s.*d.since, peak, valid);
    if (valid == 0) {   // refused: its peak waits as the pending one
        own.pending = held;
        return;
    }
    const float step = sub(now, previous);
    const float fraction = std::fabs(xtf::div(previous, step));
    own.second = own.last;
    own.last = held;
    if ((s.lastUp != 0) != (d.mark != 0)) {
        estimatePath(s, d, k, fraction);
    } else if (!historyPath(s, d, k, fraction, lastSince, peak)) {
        return;
    }
    endCrossing(s, d);
}

// Halving the lock after refused estimates: the high-pass back to the mode's, P held as the fallback
void halveLock(Vc2TrackerWords& s) {
    s.lock >>= 1;
    s.highpassTarget = s.modeHighpass;
    s.fallback = s.period;
}
} // namespace

void Vc2TrackerAnalysis::up(Vc2TrackerWords& s, float previous, float now) { crossing(s, kUp, previous, now); }

void Vc2TrackerAnalysis::down(Vc2TrackerWords& s, float previous, float now) { crossing(s, kDown, previous, now); }

void vc2TrackerOnset(Vc2TrackerWords& s, int32_t n, float peak, int32_t& valid) {
    if (!(s.lock > kLocked)) return;
    const Vc2TrackerOnsetPool& k = vc2TrackerTables().onset;
    float recent = s.up.second;
    if (!(s.up.second > s.down.second)) recent = s.down.second;
    const float since = xtf::flt(n);
    if (peak > s.settings.onsetLevel && peak > mul(recent, k.burst)) {   // an onset: the lock starts again
        s.lock = 0;
        s.failures = 0;
    } else if (mul(s.period, k.soon) > since) {   // too soon for P: refused if soon for the last candidate too
        if (mul(s.candidate, k.candidate) > since) {
            valid = 0;
            return;
        }
        s.lock >>= 1;
    }
    if (valid != 0) return;
    const float late = recent > mul(peak, 2.0f) ? k.failedLoud : k.failedQuiet;
    if (mul(s.period, late) > since) return;
    valid = 1;   // a crossing under the threshold counts when late enough for P
}

int32_t vc2TrackerTake(Vc2TrackerWords& s, float estimate) {
    const Vc2TrackerTakePool& k = vc2TrackerTables().take;
    if (!(estimate > 0.0f)) {   // refused
        s.failures += 1;
        const bool capped = s.failures > kMost;
        if (capped) s.failures = kMost;
        if (s.lock > kLocked) s.smoothing = k.half;
        if (capped || s.failures >= kFailing) halveLock(s);
        return 0;
    }
    if (estimate > mul(s.estimate, k.agreeBelow) && mul(s.estimate, k.agreeAbove) > estimate) {   // agrees
        if (s.lock > kLocked) {
            s.failures = 0;
            s.smoothing = 1.0f;
        }
        s.lock = std::min(s.lock + 1, kMost);
        return 1;
    }
    if (s.lock == 0) {
        s.smoothing = 1.0f;
        return 1;
    }
    if (mul(s.period, k.far) > estimate || estimate > mul(s.period, k.wide)) {   // far from P: unlocked, slowly
        s.smoothing = k.half;
        s.lock = 0;
    }
    return 1;
}
