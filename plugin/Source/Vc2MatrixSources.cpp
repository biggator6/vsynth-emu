// rom.4's Matrix sources: see Vc2MatrixSources.h (v245).  Every address is rom.4's.
#include "Vc2MatrixSources.h"

namespace {
enum class Vc2MatrixSourceKind { Off, Controller, Bend, Aftertouch, Assign, Bipolar, Part, Keyfollow, Velocity };
using enum Vc2MatrixSourceKind;

// [FW] each routine, what it reads and, for an assignment, which of the seven
struct Vc2MatrixSourceRow {
    uint32_t routine;
    Vc2MatrixSourceKind kind;
    int assign = 0;
};
constexpr Vc2MatrixSourceRow kRows[] { { 0x893a1de0, Off }, { 0x893a1df0, Controller }, { 0x893a1e10, Bend },
    { 0x893a1e30, Aftertouch }, { 0x893a1e60, Assign, 0 }, { 0x893a1ef0, Assign, 1 }, { 0x893a1f30, Assign, 2 },
    { 0x893a1f70, Assign, 3 }, { 0x893a1fb0, Part }, { 0x893a1fd0, Keyfollow }, { 0x893a1ff0, Velocity },
    { 0x893a2000, Assign, 4 }, { 0x893a2040, Assign, 5 }, { 0x893a2080, Bipolar, 2 }, { 0x893a2130, Bipolar, 3 },
    { 0x893a21f0, Assign, 6 } };
// [FW] the choice's cases: a parameter's routine; 0 none, else 1…95 a controller by number, from 96 none
constexpr std::pair<int32_t, uint32_t> kChoices[] { { 96, 0x893a1e10 }, { 97, 0x893a1e30 }, { 103, 0x893a1e60 },
    { 104, 0x893a1ef0 }, { 100, 0x893a1f30 }, { 101, 0x893a1f70 }, { 102, 0x893a1fb0 }, { 108, 0x893a1fd0 },
    { 107, 0x893a1ff0 }, { 105, 0x893a2000 }, { 106, 0x893a2040 }, { 98, 0x893a2080 }, { 99, 0x893a2130 },
    { 109, 0x893a21f0 } };
constexpr uint32_t kOff = 0x893a1de0, kController = 0x893a1df0;
constexpr int32_t kFirstNone = 96, kBendCentre = 8192, kNone = 32;   // [FW] past the controllers; a bend's 0; no assign
constexpr int32_t kKeyStep = 122, kKeyAt60 = 7320;                   // [FW] the keyfollow: 122 a key, 0 at 60
constexpr int kBipolarCentre = 64, kBipolarSpan = 127, kBipolarSteps = 63, kPartTop = 127;   // [FW]
constexpr int kControllersAt = 0x84, kPolyAt = 4;                    // [FW] in the channel's state

// The curve's word as a signed one (mov.w)
int32_t matrixCurve(int index) { return static_cast<int16_t>(vc2MatrixCurveWord(index)); }

// A byte of the channel's state at `at`, as rom.4 indexes it (an assignment's number signed)
uint8_t channelByte(const Vc2MatrixChannel& c, int at) { return reinterpret_cast<const uint8_t*>(&c)[at]; }

// An assignment's controller value, or none (its number 0 or 32): the assignment byte read signed
bool assignedValue(const Vc2MatrixSourceState& s, int assign, uint8_t& value) {
    const auto number = static_cast<int8_t>(s.assigns[(size_t)assign]);
    if (number == 0 || number == kNone) return false;
    value = channelByte(*s.channel, kControllersAt + number);
    return true;
}

// The bipolar reading: 64 and up, the curve at (v − 64) × 127 / 63; below, less the curve's unsigned word at
// (64 − v) × 127 / 63; 0 at 0
int32_t bipolarValue(uint8_t v) {
    if (v == 0) return 0;
    if (v >= kBipolarCentre) return matrixCurve((v - kBipolarCentre) * kBipolarSpan / kBipolarSteps);
    return -static_cast<int32_t>(vc2MatrixCurveWord((kBipolarCentre - v) * kBipolarSpan / kBipolarSteps));
}

const Vc2MatrixSourceRow* sourceRowOf(uint32_t routine) {
    for (const Vc2MatrixSourceRow& r : kRows)
        if (r.routine == routine) return &r;
    return nullptr;
}
} // namespace

int32_t Vc2MatrixSourceRoutines::value(uint32_t routine, uint8_t byte) {
    const Vc2MatrixSourceRow* row = sourceRowOf(routine);
    if (row == nullptr) return 0;
    const Vc2MatrixSourceState& s = state_;
    const Vc2MatrixChannel& c = *s.channel;
    uint8_t v = 0;
    switch (row->kind) {
    case Off: return 0;
    case Controller: return matrixCurve(channelByte(c, kControllersAt + byte));
    case Bend: return static_cast<int32_t>(c.bend) - kBendCentre;
    case Aftertouch: {
        const uint8_t poly = channelByte(c, kPolyAt + s.m->head[2]);
        return matrixCurve(c.aftertouch > poly ? c.aftertouch : poly);
    }
    case Assign: return assignedValue(s, row->assign, v) ? matrixCurve(v) : 0;
    case Bipolar: return assignedValue(s, row->assign, v) ? bipolarValue(v) : 0;
    case Part: return matrixCurve(static_cast<uint8_t>(kPartTop - s.partByte));
    case Keyfollow: return kKeyStep * s.m->head[1] - kKeyAt60;
    case Velocity: return matrixCurve(s.m->head[0]);
    }
    return 0;
}

uint32_t vc2MatrixSourceRoutine(int32_t source) {
    if (source == 0) return kOff;
    for (const auto& [s, routine] : kChoices)
        if (s == source) return routine;
    return source >= kFirstNone ? kOff : kController;
}
