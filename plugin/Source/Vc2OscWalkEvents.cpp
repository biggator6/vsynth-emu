// The walk's events' ways (v327): what a crossing does at an event by its kind, forward at the next event (0x893c9040…)
// and back at the current one (0x893c9230…), each kind's way a row.  The two ways don't mirror, so each has its own
// table.  Every address is rom.4's, every offset the object's.
#include "Vc2OscWalkSteps.h"
#include <algorithm>
#include <array>
#include <span>

namespace vc2walk {
namespace {
// [FW] byte 2's way for an event of kind 2 or 3: it acts with 0x01, unless 0x02 is set, which it then clears
bool opens(Vc2WalkWords& w) {
    const uint8_t b2 = w.byte(kEventBits);
    if ((b2 & kActs) == 0) return false;
    if ((b2 & kPasses) == 0) return true;
    w.setByte(kEventBits, b2 & (uint8_t)~kPasses);
    return false;
}
// [FW] 0x893c90d6…, 0x893c92c8…: a turn's step, negated, and byte 2's 0x04 the walk's 0x01
void turnStep(Vc2WalkWords& w, const Vc2WalkWay& way) {
    negateStep(w, way);
    if ((w.byte(kEventBits) & kSets) != 0) setWalk(w, kOneSample);
}
// [FW] 0x893c9096…, 0x893c927e…: to event n, byte 2's 0x04 the walk's 0x01, the position 0, then the player's jump
void returnTo(Vc2WalkWords& w, const Vc2WalkWay& way, int32_t n) {
    w.setHalf(kIndex, n);
    if ((w.byte(kEventBits) & kSets) != 0) setWalk(w, kOneSample);
    w.setWord(kPos, 0);
    way.jump(w.v, w.p);
}
// [FW] 0x893c92c0…, 0x893c92a0…: bouncing in the forward loop, the walk turned forward there
bool unbounce(Vc2WalkWords& w, const Vc2WalkWay& way) {
    const uint8_t b3 = w.byte(kWalkBits);
    if ((b3 & kBounceFwd) == 0) return false;
    w.setByte(kWalkBits, b3 & (uint8_t)~kBounceFwd);
    turnStep(w, way);
    return true;
}
// [FW] a mark's sample's step: byte 2 with 0x05 the walk's 0x01; nothing done
bool markStep(Vc2WalkWords& w, const Vc2WalkWay&) {
    if ((w.byte(kEventBits) & kMarkBits) == kMarkBits) setWalk(w, kOneSample);
    return false;
}

// [FW] 0x893c9056…: a return, forward: the walk's 0x01 cleared; if byte 2 opens it, back from the current event to the
// last mark (by the current event's own offset each time, as rom.4 steps, not by the stride; at event 0 it stops) and
// the jump there
bool forwardReturn(Vc2WalkWords& w, const Vc2WalkWay& way) {
    clearWalk(w, kOneSample);
    if (!opens(w)) return false;
    int32_t n = w.half(kIndex);
    const uint32_t offset = offsetOf(w, n);
    uint32_t entry = (uint32_t)w.word(kTable) + offset;
    while (entryEvent(w, entry).kind != kMark && n > 0) {
        entry -= offset;
        --n;
    }
    returnTo(w, way, n);
    return true;
}
// [FW] 0x893c90b0…: a turn, forward: the walk's 0x01 cleared; turned already, turned back; else if byte 2 opens it,
// turned
bool forwardTurn(Vc2WalkWords& w, const Vc2WalkWay& way) {
    clearWalk(w, kOneSample);
    const uint8_t b3 = w.byte(kWalkBits);
    if ((b3 & kTurned) == 0 && !opens(w)) return false;
    w.setByte(kWalkBits, b3 ^ kTurned);
    turnStep(w, way);
    return true;
}

// An event's way by its kind: true when it has done (a jump, a turn), else the walk goes on to the next event
struct KindWay {
    uint8_t kind;
    bool (*act)(Vc2WalkWords& w, const Vc2WalkWay& way);
};
// [FW] 0x893c9040…: forward, a mark, a return, a turn
constexpr std::array<KindWay, 3> kForward { { { kMark, markStep }, { kReturn, forwardReturn },
                                              { kTurn, forwardTurn } } };
bool actOn(std::span<const KindWay> ways, uint8_t kind, Vc2WalkWords& w, const Vc2WalkWay& way) {
    const auto row = std::ranges::find(ways, kind, &KindWay::kind);
    return row != ways.end() && row->act(w, way);
}

// [FW] 0x893c9272…: a return met walking forward from a mark: if byte 2 opens it, the jump there
bool foundReturn(Vc2WalkWords& w, const Vc2WalkWay& way, int32_t n) {
    if (!opens(w)) return false;
    returnTo(w, way, n);
    return true;
}
// [FW] 0x893c92a0…: a turn met walking forward from a mark: bouncing in the forward loop, turned forward; else if byte
// 2 opens it, bouncing in the backward loop
bool foundTurn(Vc2WalkWords& w, const Vc2WalkWay& way, int32_t) {
    if (unbounce(w, way)) return true;
    if (!opens(w)) return false;
    setWalk(w, kBounceBack);
    turnStep(w, way);
    return true;
}
// What a mark's search does at the first return or turn it meets: true when it has done
struct FoundWay {
    uint8_t kind;
    bool (*act)(Vc2WalkWords& w, const Vc2WalkWay& way, int32_t n);
};
constexpr std::array<FoundWay, 2> kFound { { { kReturn, foundReturn }, { kTurn, foundTurn } } };

// [FW] 0x893c9236…: a mark, walking back: the walk's 0x01 cleared; with byte 2's 0x01, forward from the event after it
// (by that event's own offset each time) to the first return or turn, and its way; else bouncing in the forward loop,
// turned forward
bool backwardMark(Vc2WalkWords& w, const Vc2WalkWay& way) {
    clearWalk(w, kOneSample);
    if ((w.byte(kEventBits) & kActs) == 0) return unbounce(w, way);
    int32_t n = w.half(kIndex) + 1;
    const uint32_t offset = offsetOf(w, n);
    uint32_t entry = (uint32_t)w.word(kTable) + offset;
    const int32_t last = w.half(kLast);
    for (;;) {
        const auto* found = std::ranges::find(kFound, entryEvent(w, entry).kind, &FoundWay::kind);
        if (found != kFound.end()) return found->act(w, way, n);
        if (!(last > n)) return false;
        entry += offset;
        ++n;
    }
}
// [FW] 0x893c9230…: back, a mark, then a return and a turn alike
constexpr std::array<KindWay, 3> kBackward { { { kMark, backwardMark }, { kReturn, markStep }, { kTurn, markStep } } };
} // namespace

void forwardEvent(Vc2WalkWords& w, const Vc2WalkWay& way) {
    if (!actOn(kForward, w.byte(kNextKind), w, way)) advance(w);
}

void backwardEvent(Vc2WalkWords& w, const Vc2WalkWay& way) {
    if (!actOn(kBackward, w.byte(kKind), w, way)) retreat(w);
}
} // namespace vc2walk
