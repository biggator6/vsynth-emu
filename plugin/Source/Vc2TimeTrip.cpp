// The VC-2 part's Time Trip: see Vc2TimeTrip.h (v306).  Each routine's reads and writes in rom.4's order; the steps'
// products and shifts in its 32-bit arithmetic, its division the library's (signed, toward zero).
#include "Vc2TimeTrip.h"
#include "Vc2PartSettingsTables.h"
#include "XtFloat.h"

namespace {
using T = Vc2PartSettingsTables;
constexpr int8_t kCentre = 64;                      // [FW] the pad's centre; a made trip's two bytes
constexpr int8_t kNoRadius = -1;                    // [FW]
constexpr int16_t kNoAngle = -1;                    // [FW]
constexpr int kMadeRate = 4, kInitRate = 2;         // [FW] the constructor's rate index, the records'
constexpr uint32_t kRecordMask = 0xffff;            // [FW] the records' mask
constexpr uint8_t kRest = 127;                      // [FW] the part's level at rest
constexpr int32_t kTurn = 512, kHalfTurn = 256;     // [FW] a whole turn of the angle, half of one
constexpr int32_t kHeld = 64;                       // [FW] the most a turn against the way is held
constexpr int kFraction = 9;                        // [FW] the steps' fixed point
constexpr int32_t kChunk = 127;                     // [FW] what each broadcast after the first takes off

// What the set and the rate's index 0 share
void stop(Vc2TripObject& t) {
    t.moving = 0;
    t.turning = 0;
    t.x = kCentre;
    t.y = kCentre;
    t.radius = kNoRadius;
    t.angle = kNoAngle;
    t.lastRadius = kNoRadius;
    t.lastAngle = kNoAngle;
    t.byte14 = 0;
    t.byte15 = 0;
}

// A quarter of the pad: the table's row from X or Y, each with its sign, and the angle's base (the last quarter's 0
// kept 0)
struct Quarter {
    bool rowOfX;
    int32_t rowSign, columnSign, base;
    bool zeroKept;
};
// [FW] by X past the centre, then Y
constexpr Quarter kQuarters[4] = {
    { true, -1, -1, 64, false },
    { false, 1, -1, 96, false },
    { false, -1, 1, 32, false },
    { true, 1, 1, 128, true },
};

// The angle quantized to the rate's steps: v × steps in the fixed point, back, over the steps
int32_t quantized(int32_t v, int32_t steps) {
    const int32_t fixed = (int32_t)((uint32_t)v * (uint32_t)steps) >> kFraction;
    return (int32_t)((uint32_t)fixed << kFraction) / steps;
}

// While turning, a turn's way, the carry taken off the angle from; 0 with no steps: past the most held (the carry
// dropped), or against the way with the carry not with it (the turn held as the carry)
int8_t keepTurning(Vc2TripObject& t, Vc2TripScratch& s) {
    const int32_t way = s.turn < 0 ? -1 : 1;
    if (s.turn * way > kHeld) {
        t.carry = 0;
        return 0;
    }
    if (t.turning == -way && t.carry * way <= 0) {
        t.carry = (int16_t)s.turn;
        return 0;
    }
    s.from -= t.carry;
    return (int8_t)way;
}

// Not turning yet: at angle 0, by the half the angle from is in, else by the turn's way
int8_t startTurning(const Vc2TripScratch& s) {
    if (s.to == 0) return s.from >= kHalfTurn ? 1 : -1;
    return s.turn >= 0 ? 1 : -1;
}
} // namespace

void vc2TripConstruct(Vc2TripObject& t, uint32_t part, uint32_t mask) {
    const T& tables = vc2PartSettingsTables();
    t.part = part;
    t.mask = mask;
    t.turning = 0;
    t.carry = 0;
    t.offset = tables.tripOffsets[kMadeRate];
    t.steps = tables.tripSteps[kMadeRate];
    t.byte14 = kCentre;
    t.byte15 = kCentre;
    t.lastRadius = kNoRadius;
    t.lastAngle = kNoAngle;
    t.x = kCentre;
    t.y = kCentre;
    t.radius = kNoRadius;
    t.angle = kNoAngle;
}

void vc2TripRate(Vc2TripObject& t, int32_t index) {
    if (index >= T::kTripRates) return;
    const T& tables = vc2PartSettingsTables();
    t.steps = tables.tripSteps[(size_t)index];
    t.offset = tables.tripOffsets[(size_t)index];
    if (index == 0) stop(t);
}

void vc2TripSet(Vc2TripObject& t, int32_t on, int8_t x, int8_t y) {
    if (on == 0) {
        stop(t);
        return;
    }
    if (t.steps <= 0) return;
    t.moving = 1;
    t.x = x;
    t.y = y;
}

void vc2TripPosition(Vc2TripObject& t) {
    const int32_t dx = t.x - kCentre, dy = t.y - kCentre;
    const Quarter& q = kQuarters[2 * (dx > 0) + (dy > 0)];
    const int32_t row = q.rowSign * (q.rowOfX ? dx : dy), column = q.columnSign * (q.rowOfX ? dy : dx);
    const auto& entry = vc2PartSettingsTables().tripPolar[(size_t)(T::kTripSide * row + column)];
    const int32_t angle = q.zeroKept && entry[1] == 0 ? 0 : q.base - entry[1];
    t.radius = entry[0];
    t.angle = (int16_t)(uint16_t)((uint32_t)angle << 2);
}

int32_t vc2TripSteps(Vc2TripObject& t, Vc2TripScratch& s) {
    s.from = t.lastAngle;
    s.to = t.angle;
    if (s.from == kNoAngle || s.from == s.to) return 0;
    s.turn = s.to - s.from;
    if (s.turn > kHalfTurn) s.turn -= kTurn;
    if (s.turn < -kHalfTurn) s.turn += kTurn;
    const int8_t way = t.turning == 0 ? startTurning(s) : keepTurning(t, s);
    if (way == 0) return 0;
    t.turning = way;
    t.carry = 0;
    const int32_t steps = t.steps;
    s.from = quantized(way < 0 ? s.from + t.offset + 1 : s.from, steps);
    const auto distance = s.from >= s.to ? (uint32_t)s.from - (uint32_t)s.to : (uint32_t)s.to - (uint32_t)s.from;
    s.turn = (int32_t)((distance + kTurn) & (kTurn - 1));
    if (s.turn > kHalfTurn) s.turn = kTurn - s.turn;
    const int32_t count = (int32_t)((uint32_t)s.turn * (uint32_t)steps) >> kFraction;
    return way < 0 ? -count : count;
}

void vc2TripBroadcast(int32_t count, uint32_t part, uint32_t mask, Vc2TripParts& parts) {
    for (uint32_t tone = parts.firstTone(); tone != 0; tone = parts.nextTone(tone))
        parts.stepTone(tone, count, part, mask);
}

void vc2TripPlace(Vc2TripObject& t, int8_t x, int8_t y) {
    t.x = x;
    t.y = y;
    vc2TripPosition(t);
}

int8_t vc2TripMove(Vc2TripObject& t, int8_t x, int8_t y, Vc2TripScratch& s, Vc2TripParts& parts) {
    vc2TripPlace(t, x, y);
    if (t.moving != 0) {
        const int32_t first = vc2TripSteps(t, s);
        const int32_t toward = first < 0 ? kChunk : -kChunk;
        for (int32_t count = first; first < 0 ? count < 0 : count > 0; count += toward)
            vc2TripBroadcast(count, t.part, t.mask, parts);
    }
    t.lastRadius = t.radius;
    t.lastAngle = t.angle;
    return t.radius;
}

void Vc2TripRecords::staticInit() {
    const float scale = vc2PartSettingsTables().tripScale;
    for (int k = 0; k < kParts; ++k) {
        Vc2TripRecord& r = records[(size_t)k];
        vc2TripConstruct(r.front.trip, (uint32_t)k, kRecordMask);
        r.part = (uint8_t)k;
        r.front.x = 0;
        r.front.y = 0;
        r.front.level = kRest;
        r.front.byte22.fill(0);
        r.rate = kInitRate;
        vc2TripRate(r.front.trip, kInitRate);
        r.perRate = xtf::div(scale, (float)r.rate);
    }
}
