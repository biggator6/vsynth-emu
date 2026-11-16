// The VC-2's pitch tracker's estimator, 0x893eeb90: see Vc2TrackerAnalysis.h (v224).  A candidate period is the
// last two half-intervals; it is trusted, or replaced by a multiple of one, by the side peaks, the levels, the noise
// count and how far the halves agree.  Its arithmetic in rom.4's order, in XtFloat.
#include "Vc2TrackerAnalysis.h"
#include "XtFloat.h"
#include <cmath>
#include <optional>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;

constexpr int32_t kLocked = 3;          // [FW] a lock above it tries the locked shortcut
constexpr int32_t kLockedLong = 30;     // [FW] a lock above it may keep a candidate whose halves disagree
constexpr int32_t kBoth = 2;            // [FW] follow, set by an unlocked estimate
constexpr int32_t kNoisyRising = 7;     // [FW] the noise count above which a falling up side refuses
constexpr int32_t kNoisy = 10;          // [FW] the noise count above which an unlocked estimate refuses
constexpr int32_t kOffNoise = 4;        // [FW] the noise count from which a candidate off P refuses
constexpr int32_t kLooseNoise = 4;      // [FW] the noise count above which the halves may disagree more
constexpr int32_t kTwice = 3;           // [FW] the other side's crossings above which a candidate past 2P refuses

using Result = std::optional<float>;    // a value: the estimator returns it; none: it goes on

// The estimator's words while it runs, its registers: the halves before the shift, the candidate, the last
struct Vc2TrackerEstimation {
    Vc2TrackerWords& s;
    const Vc2TrackerEstimatorPool& k;
    float half = 0, h0 = 0, h1 = 0, h2 = 0;   // the new half-interval, then the history as it stood
    float cand = 0, last = 0;                 // this candidate, the last one
    bool rising = false, falling = false;     // the up side's last peak above its second; the down side's below
    float f = 0;                              // how far the halves may disagree

    const Vc2TrackerSettings& settings() const { return s.settings; }
    bool quieter(float level) const { return level > s.recentPeak; }   // the recent peaks under a level
};

// The half-interval since the last valid crossing into the history, the candidate and its limits
Result start(Vc2TrackerEstimation& e, float fraction) {
    Vc2TrackerWords& s = e.s;
    e.h0 = s.halves[0];
    e.h1 = s.halves[1];
    e.h2 = s.halves[2];
    const float lastFraction = s.fraction;
    s.fraction = fraction;
    e.half = sub(add(xtf::flt(s.sinceCrossing), fraction), lastFraction);
    s.halves[3] = e.h2;
    s.halves[2] = e.h1;
    s.halves[1] = e.h0;
    s.halves[0] = e.half;
    if (e.quieter(s.settings.crossingLevel)) {
        s.lock = 0;
        return 0.0f;
    }
    e.cand = add(e.half, e.h0);
    e.last = s.candidate;
    s.candidate = e.cand;
    if (s.shortest > e.cand || e.cand > s.longest) return 0.0f;
    return std::nullopt;
}

// Which way the sides' last peaks go, and a last frame far louder than the recent peaks
Result sides(Vc2TrackerEstimation& e) {
    Vc2TrackerWords& s = e.s;
    if (!(s.down.second > s.down.last)) {
        if (s.up.last > s.up.second) e.rising = true;
    } else {
        if (s.up.second > s.up.last && s.noise > kNoisyRising) return 0.0f;
        e.falling = true;
    }
    if (e.rising) return std::nullopt;
    const float k = e.quieter(e.settings().levels[3]) ? 2.0f : e.k.loudFrame;
    if (s.lastFramePeak > mul(s.recentPeak, k)) {
        s.lock = 0;
        return 0.0f;
    }
    return std::nullopt;
}

// Locked: a candidate near P is taken; the last one, if near, hands off, and nearer still is taken instead
Result locked(Vc2TrackerEstimation& e) {
    Vc2TrackerWords& s = e.s;
    const float above = mul(s.period, e.k.lockedAbove);
    if (above > e.cand && e.cand > mul(s.period, e.k.lockedBelow)) return e.cand;
    if (!(above > e.last && e.last > mul(s.period, e.k.lockedBelow))) return std::nullopt;
    s.handOff = 1;
    if (!(mul(s.period, e.k.handAbove) > e.last && e.last > mul(s.period, e.k.handBelow))) return std::nullopt;
    s.candidate = 1.0f;
    return e.last;
}

// Unlocked and quiet: the counts and the sides must say this direction's crossings are the period's
Result unlocked(Vc2TrackerEstimation& e, const Vc2TrackerDirection& d) {
    Vc2TrackerWords& s = e.s;
    const Vc2TrackerSettings& t = e.settings();
    s.follow = kBoth;
    if (s.noise > kNoisy) return 0.0f;
    if (!e.quieter(t.levels[4])) return std::nullopt;
    if (e.quieter(t.levels[1])) {
        if (e.falling) return 0.0f;
        if (e.cand > s.period && e.cand > t.periods[1]) return 0.0f;
    }
    if (s.*d.crossingsAt != s.crossings) return 0.0f;
    if (mul((s.*d.otherSide).second, e.k.side[d.index]) > (s.*d.side).last) return 0.0f;
    if (e.cand > mul(s.period, 2.0f) && s.*d.otherCrossingsAt > kTwice) return 0.0f;
    return std::nullopt;
}

// Unlocked, a candidate off P: refused unless the levels and the periods allow it; far off, the halves loosen
Result offP(Vc2TrackerEstimation& e) {
    Vc2TrackerWords& s = e.s;
    const Vc2TrackerSettings& t = e.settings();
    if (s.noise >= kOffNoise) return 0.0f;
    if (e.quieter(t.levels[1])) {
        if (!e.rising) return 0.0f;
        if (t.periods[0] > e.cand && (s.period > e.cand || e.quieter(t.levels[0]))) return 0.0f;
        if (e.cand > t.periods[1] && (e.cand > s.period || e.quieter(t.levels[0]))) return 0.0f;
    }
    if (e.cand > mul(s.period, 2.0f) || mul(s.period, e.k.underHalf) > e.cand) e.f = e.k.loose;
    return std::nullopt;
}

// Unlocked: how far the halves may disagree, by the levels, the sides and how far the candidate is from P
Result middle(Vc2TrackerEstimation& e) {
    Vc2TrackerWords& s = e.s;
    const float period = s.period;
    if (!e.quieter(e.settings().levels[2])) {   // loud
        if (period > e.cand) {
            e.f = e.k.below;
        } else if (e.cand > mul(period, e.k.above)) {
            e.f = e.k.off;
        }
        return std::nullopt;
    }
    if (s.down.last > s.up.last ? s.up.second > s.down.second : s.down.second > s.up.second) return 0.0f;
    e.f = e.k.off;
    if (e.cand > mul(period, e.k.above) || mul(period, e.k.below) > e.cand) return offP(e);
    return std::nullopt;
}

// Unlocked: a noisy count loosens the halves; a candidate far from P needs both sides' last peaks holding
Result near(Vc2TrackerEstimation& e) {
    Vc2TrackerWords& s = e.s;
    if (s.noise > kLooseNoise) e.f = e.k.loose;
    if (mul(s.period, e.k.nearAbove) > e.cand && e.cand > mul(s.period, e.k.nearBelow)) return std::nullopt;
    if (mul(s.down.second, e.k.loose) > s.down.last) return 0.0f;
    if (mul(s.up.second, e.k.loose) > s.up.last) return 0.0f;
    return std::nullopt;
}

// Unlocked, the halves disagreeing: the candidate if near P, else whichever of it and twice a half is nearest
float nearestMultiple(const Vc2TrackerEstimation& e) {
    const Vc2TrackerWords& s = e.s;
    if (!e.rising || !(s.recentPeak > e.settings().levels[0])) return 0.0f;
    const float period = s.period;
    const float cand = add(e.half, e.h0);
    if (mul(period, e.k.unlockedAbove) > cand && cand > mul(period, e.k.agree)) return cand;
    const float apart = std::fabs(sub(period, cand));
    const float twiceOld = mul(e.h0, 2.0f);
    const float twiceNew = mul(e.half, 2.0f);
    const float apartOld = std::fabs(sub(period, twiceOld));
    const float apartNew = std::fabs(sub(period, twiceNew));
    if (apartOld > apart) return apartNew > apart ? cand : twiceNew;
    return apartNew > apartOld ? twiceOld : twiceNew;
}

// The halves' agreement: the candidate when they agree; else, locked long, the candidate if it and the last agree
// with P; unlocked, the nearest multiple
float agreement(const Vc2TrackerEstimation& e) {
    const Vc2TrackerWords& s = e.s;
    if (mul(e.half, e.f) > e.h1 || mul(e.h1, e.f) > e.half) return 0.0f;
    if (!(mul(e.h0, e.f) > e.h2) && !(mul(e.h2, e.f) > e.h0)) return e.cand;
    if (s.lock == 0) return nearestMultiple(e);
    if (!(s.lock > kLockedLong)) return 0.0f;
    const float k = e.k.agree;
    const float candidate = s.candidate;
    if (!(e.last > mul(s.period, k)) || !(s.period > mul(e.last, k))) return 0.0f;
    if (!(e.last > mul(candidate, k)) || !(candidate > mul(e.last, k))) return 0.0f;
    return candidate;
}
} // namespace

float vc2TrackerEstimate(Vc2TrackerWords& s, const Vc2TrackerDirection& d, float fraction) {
    Vc2TrackerEstimation e { s, vc2TrackerTables().estimator };
    if (Result r = start(e, fraction)) return *r;
    if (Result r = sides(e)) return *r;
    if (s.lock > kLocked) {
        if (Result r = locked(e)) return *r;
    }
    e.f = e.k.start;
    if (s.lock != 0) return agreement(e);
    if (Result r = unlocked(e, d)) return *r;
    if (Result r = middle(e)) return *r;
    if (Result r = near(e)) return *r;
    return agreement(e);
}
