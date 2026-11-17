// The VC-2's portamento: see Vc2PitchRamp.h (v236).  Every address is rom.4's; the tick is Vc2PitchRampTick.cpp's.
#include "Vc2PitchRamp.h"
#include "Vc2RampTables.h"
#include "XtFloat.h"
#include <array>
#include <cmath>
#include <cstring>

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

namespace {
constexpr int32_t kStarted = 1, kDone = 3;   // [FW] the states the restart sets
// [FW] 0x893b0d08: which settings word, beside the switch, turns a ramp on by its type (0 none; past 2 the switch is
// left as it was)
constexpr std::array<uint32_t Vc2RampSettings::*, 3> kGates { nullptr, &Vc2RampSettings::walkOn,
                                                               &Vc2RampSettings::formantOn };
constexpr uint32_t kLegatoWords = 4;   // [FW] 0x893b0e30 copies the switches and the time, not the timing's type

void holdSpan(Vc2RampWords& w, const Vc2RampTimePool& k) {
    if (w.span > k.spanMost) w.span = k.spanMost;
    else if (k.spanLeast > w.span) w.span = k.spanLeast;
}

// [FW] the time law the restart (0x893b0b9c…) and the legato update (0x893b0e44…) share, each on its own pool: the
// time (TIME's, or RATE's time per 1200 cents of the span); under 1 the span held and the ease whole at a time of 2;
// under 100 the span held, then under 50 the ease whole and the time doubled, else the ease (100 − time) × 0.02 at a
// time of 100; then the step, 655360 over the time, at most 65536
void stepFor(Vc2RampWords& w, const Vc2RampTimePool& k) {
    float time = w.settings.timed == 1 ? w.settings.time : div(mul(std::fabs(w.span), w.settings.time), k.perOctave);
    if (1.0f > time) {
        holdSpan(w, k);
        w.ease = 1.0f;
        time = add(1.0f, 1.0f);
    } else {
        w.ease = 0.0f;
        if (k.slow > time) {
            holdSpan(w, k);
            const bool fast = k.fast > time;
            w.ease = fast ? 1.0f : mul(sub(k.slow, time), k.easeSlope);
            time = fast ? add(time, time) : k.slow;
        }
    }
    w.step = div(k.phaseRate, time);
    if (!(k.phaseEnd > w.step)) w.step = k.phaseEnd;
}

// [FW] a ratio in cents, as the restart takes it: logf(x) / logf(2) × 1200
float centsOf(float x, const Vc2ToneShared& shared, const Vc2RampTimePool& k) {
    const float octaves = div(shared.log(x), shared.log(add(1.0f, 1.0f)));
    return mul(octaves, k.perOctave);
}
} // namespace

void Vc2PitchRamp::construct(int32_t type) {
    const Vc2RampTables& t = vc2RampTables();
    w.type = type;
    w.state = 0;
    w.value = 0.0f;
    w.on = 0;
    w.factor = 1.0f;
    w.factorPhase = t.builtPhase;
    w.factorRise = t.builtRise;
    w.factorFall = t.builtFall;
}

void Vc2PitchRamp::start(const uint32_t* settings, float from, float to, const Vc2ToneShared& shared) {
    std::memcpy(&w.settings, settings, sizeof w.settings);
    restart(from, to, shared);
}

void Vc2PitchRamp::restart(float from, float to, const Vc2ToneShared& shared) {
    const Vc2RampTables& t = vc2RampTables();
    const Vc2RampTimePool& k = t.restart;
    w.state = kStarted;
    w.fromGiven = from;
    w.toGiven = to;
    w.from = w.type != 0 ? centsOf(from, shared, k) : from;
    w.to = w.type != 0 ? centsOf(to, shared, k) : to;
    w.rising = !(w.from > w.to);
    w.span = sub(w.to, w.from);
    w.phase = 0.0f;
    stepFor(w, k);
    if (w.span == 0.0f) w.phase = k.phaseEnd;
    const float done = w.type != 0 ? 1.0f : 0.0f;
    if (!(k.phaseEnd > w.phase) || w.settings.time == 0.0f) {
        w.state = kDone;
        w.value = done;
    } else {
        const float x = add(div(w.phase, k.phaseEnd), -1.0f);
        w.value = w.type != 0 ? shared.exp2(mul(mul(x, w.span), t.restartPerCent)) : mul(x, w.span);
    }
    if ((uint32_t)w.type < kGates.size()) {
        const uint32_t Vc2RampSettings::*gate = kGates[(size_t)w.type];
        w.on = w.settings.on == 1 && (gate == nullptr || w.settings.*gate == 1) ? 1 : 0;
    }
    w.factor = 1.0f;
    w.factorPhase = 0.0f;
    w.factorDepth = 1.0f;
    w.factorStep = w.step;
    if (t.factorLeast > w.step) w.factorStep = t.factorLeast;
    else if (w.step > t.factorMost) w.factorStep = t.factorMost;
}

void Vc2PitchRamp::legato(const uint32_t* settings) {
    std::memcpy(&w.settings, settings, kLegatoWords * sizeof(uint32_t));
    stepFor(w, vc2RampTables().legato);
}

float Vc2PitchRamp::value() const {
    if (w.on != 0) return w.value;
    return w.type != 0 ? 1.0f : 0.0f;
}
