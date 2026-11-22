// The players' event walk: see Vc2OscWalk.h (v327): its entries and events, the loops' part of a crossing (its two
// ways, rom.4's code twice over, one routine over a row, each loop mode's way a row), the next crossing, and mode 1's
// two event jumps.  The events' ways are Vc2OscWalkEvents.cpp's.  Every address is rom.4's, every offset the object's
// (from the voice's +0x15c) unless a record's.
#include "Vc2OscWalkSteps.h"
#include <algorithm>
#include <array>

int16_t Vc2WalkWords::memoryHalf(uint32_t address) const {
    constexpr uint32_t kHigh = 2, kOdd = 1, kHalfBits = 16;   // little-endian: the high half 2 bytes on
    const uint32_t word = memory(address & ~(kHigh | kOdd));
    return (int16_t)((address & kHigh) != 0 ? word >> kHalfBits : word);
}

namespace vc2walk {
namespace {
// [FW] an event's entry: its flags' half word (the kind its 0x0b), its position 4 on; the end, 16 past the last
// event's position
constexpr uint32_t kKindBits = 0x0b, kPosAfter = 4;
constexpr int32_t kPastLast = 16;
} // namespace

// [FW] the table + (number × stride + first) half words, as the SH-4 forms it (MULU.W on the number's and the stride's
// low halves, both bytes sign-extended)
uint32_t offsetOf(const Vc2WalkWords& w, int32_t number) {
    const uint32_t stride = (uint16_t)(int16_t)(int8_t)w.byte(kStride);
    const uint32_t first = (uint32_t)(int32_t)(int8_t)w.byte(kFirst);
    return 2 * ((uint32_t)(uint16_t)number * stride + first);
}

Event entryEvent(const Vc2WalkWords& w, uint32_t entry) {
    return { (uint8_t)(w.memoryHalf(entry) & kKindBits), (int32_t)w.memory(entry + kPosAfter) };
}

// [FW] the current event (+0xc) loaded, its kind (+0x5) and position (+0x24); its entry
uint32_t load(Vc2WalkWords& w) {
    const uint32_t entry = (uint32_t)w.word(kTable) + offsetOf(w, w.half(kIndex));
    const Event e = entryEvent(w, entry);
    w.setByte(kKind, e.kind);
    w.setWord(kPrev, e.pos);
    return entry;
}

// [FW] the event after the current one the next, its kind (+0x6) and position (+0x28)
void readNext(Vc2WalkWords& w) {
    const Event e = entryEvent(w, (uint32_t)w.word(kTable) + offsetOf(w, w.half(kIndex) + 1));
    w.setByte(kNextKind, e.kind);
    w.setWord(kNext, e.pos);
}

// [FW] the next event current and the one after it next; past the last, the current event's position the next's, the
// next 16 past the last event's, the end 1
void advance(Vc2WalkWords& w) {
    if (w.half(kLast) > w.half(kIndex)) {
        w.setHalf(kIndex, w.half(kIndex) + 1);
        load(w);
        if (w.half(kLast) > w.half(kIndex)) {
            readNext(w);
            return;
        }
    }
    w.setWord(kPrev, w.word(kNext));
    w.setWord(kNext, vc2WalkPlus(w.word(kFinal), kPastLast));
    w.setByte(kEnded, 1);
}

// [FW] the event before the current one current and the one after it next; before the first, the end −1
void retreat(Vc2WalkWords& w) {
    const int32_t n = w.half(kIndex);
    if (n <= 0) {
        w.setByte(kEnded, kBefore);
        return;
    }
    w.setHalf(kIndex, n - 1);
    load(w);
    readNext(w);
}

void setWalk(Vc2WalkWords& w, uint8_t bits) { w.setByte(kWalkBits, w.byte(kWalkBits) | bits); }
void clearWalk(Vc2WalkWords& w, uint8_t bits) { w.setByte(kWalkBits, w.byte(kWalkBits) & (uint8_t)~bits); }
void negateStep(Vc2WalkWords& w, const Vc2WalkWay& way) {
    if (!way.keepsSign) w.setWord(kStep, vc2WalkPlus(0, -w.word(kStep)));
}

namespace {
// [FW] the loops' record: four cached events of 0xb4 bytes (an event's number, the loop's mode, the position ×16, its
// state), the forward loop's start and end, the backward's end and start; a loop's last carries its mode
constexpr uint32_t kCachedBytes = 0xb4, kCachedMode = 2, kCachedPos = 4;
enum Cached : uint32_t { kFwdStart, kFwdEnd, kBackEnd, kBackStart };

uint32_t cachedAt(const Vc2WalkWords& w, Cached c) { return (uint32_t)w.word(kRecord) + kCachedBytes * c; }
int32_t cachedPos(const Vc2WalkWords& w, Cached c) { return (int32_t)w.memory(cachedAt(w, c) + kCachedPos); }
int16_t cachedMode(const Vc2WalkWords& w, Cached c) { return w.memoryHalf(cachedAt(w, c) + kCachedMode); }

// A crossing's two ways at the loops: where a bounce ends and its bit, and the other loop's bit it leaves there; the
// loop whose last point this is (forward: the forward loop's end, back: the backward's start), its bit and rate, where
// its One Way and its hold go, the bit its Alternate sets; whether a mode past 3 keeps the loop's bit and rate; then
// the event that is crossed and its way
struct Crossing {
    Cached bounceEnd;
    uint8_t bounce, left;
    Cached loopLast;
    uint8_t inLoop;
    uint32_t rate;
    Cached oneWayTo, holdTo;
    uint8_t turn;
    bool otherModeKeeps;
    uint32_t event;
    void (*atEvent)(Vc2WalkWords& w, const Vc2WalkWay& way);
};
// [FW] 0x893c8f50… forward, 0x893c9140… back
constexpr Crossing kCrossFwd { kBackEnd, kBounceBack, kInBack, kFwdEnd, kInFwd, kFwdRate, kFwdStart, kFwdStart,
                               kBounceFwd, true, kNext, forwardEvent };
constexpr Crossing kCrossBack { kFwdStart, kBounceFwd, kInFwd, kBackStart, kInBack, kBackRate, kBackEnd, kBackStart,
                                kBounceBack, false, kPrev, backwardEvent };

void leave(Vc2WalkWords& w, uint8_t inLoop) {   // [FW] out of a loop: its bit cleared, the rate factor 1
    clearWalk(w, inLoop);
    w.setReal(kRate, 1.0f);
}

// [FW] a loop mode's way at the loop's last point: One Way, the position less the crossing, the jump to the loop's
// other end; Alternate, the turn; hold, held, the step and position 0, the jump to the hold's event
void oneWay(Vc2WalkWords& w, const Crossing& c, const Vc2WalkWay& way, int32_t x) {
    w.setWord(kPos, vc2WalkPlus(w.word(kPos), -x));
    way.jumpCached(w.v, w.p, cachedAt(w, c.oneWayTo));
}
void alternate(Vc2WalkWords& w, const Crossing& c, const Vc2WalkWay& way, int32_t) {
    setWalk(w, c.turn);
    negateStep(w, way);
}
void hold(Vc2WalkWords& w, const Crossing& c, const Vc2WalkWay& way, int32_t) {
    setWalk(w, kHeld);
    w.setWord(kStep, 0);
    w.setWord(kPos, 0);
    way.jumpCached(w.v, w.p, cachedAt(w, c.holdTo));
}
struct ModeWay {
    int16_t mode;
    void (*act)(Vc2WalkWords& w, const Crossing& c, const Vc2WalkWay& way, int32_t x);
};
constexpr std::array<ModeWay, 3> kModes { { { 1, oneWay }, { 2, alternate }, { 3, hold } } };   // [FW]

// [FW] a loop's last point with the loops on: in the loop, its rate, the end 0; then its mode's way; true when done
bool looped(Vc2WalkWords& w, const Crossing& c, const Vc2WalkWay& way, int32_t x) {
    setWalk(w, c.inLoop);
    w.setByte(kEnded, 0);
    w.setReal(kRate, w.real(c.rate));
    const auto* mode = std::ranges::find(kModes, cachedMode(w, c.loopLast), &ModeWay::mode);
    if (mode != kModes.end()) {
        mode->act(w, c, way, x);
        return true;
    }
    if (!c.otherModeKeeps) leave(w, c.inLoop);
    return false;
}

// [FW] the loops' part of a crossing at x: a bounce's end turns the walk round, or leaves the other loop; a loop's last
// point, unless bouncing, leaves the loop (the loops off) or loops; true when done
bool atLoops(Vc2WalkWords& w, const Crossing& c, const Vc2WalkWay& way, int32_t x) {
    if (cachedPos(w, c.bounceEnd) == x) {
        const uint8_t b3 = w.byte(kWalkBits);
        if ((b3 & c.bounce) != 0) {
            negateStep(w, way);
            w.setByte(kWalkBits, b3 & (uint8_t)~c.bounce);
            return true;
        }
        if ((b3 & c.left) != 0) leave(w, c.left);
    }
    if (cachedPos(w, c.loopLast) != x || (w.byte(kWalkBits) & c.bounce) != 0) return false;
    if ((w.byte(kFlags) & kLoopsOn) == 0) {
        leave(w, c.inLoop);
        return false;
    }
    return looped(w, c, way, x);
}

// [FW] a crossing at +0x2c: the loops' part, then at its event (forward the next, back the current) the event's way
void crossed(Vc2OscVoice& v, Vc2OscParts& p, const Vc2WalkWay& way, const Crossing& c) {
    Vc2WalkWords w { v, p };
    const int32_t x = w.word(kCross);
    if (atLoops(w, c, way, x) || w.word(c.event) != x) return;
    c.atEvent(w, way);
}

// The next crossing's two ways: the event it starts from, the bounce that leaves only the far point, the far and near
// loop points; which way is ahead
struct Ahead {
    uint32_t event;
    uint8_t bounce;
    Cached far, near;
    bool forward;
};
constexpr Ahead kAheadFwd { kNext, kBounceBack, kBackEnd, kFwdEnd, true };       // [FW] 0x893c9386…
constexpr Ahead kAheadBack { kPrev, kBounceFwd, kFwdStart, kBackStart, false };  // [FW] 0x893c93f0…
bool past(const Ahead& a, int32_t x, int32_t y) { return a.forward ? x > y : y > x; }   // x past y, the way it walks

// [FW] 0x893ca1b2…: the walk at `pos`, its fraction 0; the next event's kind and position read `stride` half words
// past `from`, or, the current the last, none and 16 past the last event's position
void jumpTail(Vc2WalkWords& w, int32_t pos, uint32_t from) {
    w.setWord(kPos, pos);
    w.setWord(kFrac, 0);
    const uint32_t next = from + 2 * (uint32_t)(int32_t)(int8_t)w.byte(kStride);
    const Event none { 0, vc2WalkPlus(w.word(kFinal), kPastLast) };
    const Event e = w.half(kLast) > w.half(kIndex) ? entryEvent(w, next) : none;
    w.setByte(kNextKind, e.kind);
    w.setWord(kNext, e.pos);
}
// [FW] 0x893ca190: mode 1's jump to the current event: loaded, the walk at its position; the next read from its entry's
// +8 (rom.4's −8 is taken off r4, which nothing reads after it, where 0x893ca1a0 takes it off r6)
void modeOneJump(Vc2OscVoice& v, Vc2OscParts& p) {
    constexpr uint32_t kPastEntry = 8;   // [FW] where 0x893c8f00 leaves r6
    Vc2WalkWords w { v, p };
    const uint32_t entry = load(w);
    jumpTail(w, w.word(kPrev), entry + kPastEntry);
}
// [FW] 0x893ca1a0: mode 1's jump to a loop's cached event: its number goes to the caller's r4 + 0xc, not the object's
// (from mode 1's render r4 is its mask 0xfffff: on the XT a half word to an odd address, an address error; the port
// writes it nowhere); the current event loaded, the walk at the cached position, the next read from its entry
void modeOneCached(Vc2OscVoice& v, Vc2OscParts& p, uint32_t cached) {
    Vc2WalkWords w { v, p };
    const uint32_t entry = load(w);
    jumpTail(w, (int32_t)w.memory(cached + kCachedPos), entry);
}
} // namespace
} // namespace vc2walk

namespace {
// [FW] the start's seek for index 0, 0x893cc830, from v329 the voice's own (vc2OscSeekModeOne, Vc2OscWalkStart.cpp)
// with the beat's reset it ends in, by the voice's seeks' table
void modeOneSeek(Vc2OscVoice& v, Vc2OscParts& p, int32_t at) {
    constexpr uint32_t kModeOneSeek = 0x893cc830;
    v.seekBy(kModeOneSeek, at, p);
}
} // namespace

const Vc2WalkWay kVc2ModeOneWalk { vc2walk::modeOneJump, vc2walk::modeOneCached, false, modeOneSeek,
                                   vc2walk::modeOneJump };

// [FW] 0x893c9380: the next crossing (+0x2c): from the next event (forward) or the current (back); bouncing, nearer
// the bounce's far end; else nearer the far point if it is ahead of the position, then nearer the near point if its
// mode is above 0 and the loops are on or it is ahead too
void vc2WalkCrossing(Vc2OscVoice& v, Vc2OscParts& p) {
    using namespace vc2walk;
    Vc2WalkWords w { v, p };
    const Ahead& a = w.word(kStep) >= 0 ? kAheadFwd : kAheadBack;
    const int32_t pos = w.word(kPos);
    int32_t x = w.word(a.event);
    const int32_t far = cachedPos(w, a.far);
    if ((w.byte(kWalkBits) & a.bounce) != 0) {
        if (past(a, x, far)) x = far;
        w.setWord(kCross, x);
        return;
    }
    if (past(a, far, pos) && past(a, x, far)) x = far;
    const int32_t near = cachedPos(w, a.near);
    const bool ahead = (w.byte(kFlags) & kLoopsOn) != 0 || past(a, near, pos);
    if (cachedMode(w, a.near) > 0 && ahead && past(a, x, near)) x = near;
    w.setWord(kCross, x);
}

void vc2WalkForward(Vc2OscVoice& v, Vc2OscParts& p, const Vc2WalkWay& way) {
    vc2walk::crossed(v, p, way, vc2walk::kCrossFwd);
}

void vc2WalkBackward(Vc2OscVoice& v, Vc2OscParts& p, const Vc2WalkWay& way) {
    vc2walk::crossed(v, p, way, vc2walk::kCrossBack);
}
