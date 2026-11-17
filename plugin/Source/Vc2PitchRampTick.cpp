// The VC-2's portamento's tick: see Vc2PitchRamp.h (v236).  rom.4's 0x893b0f30(ramp, n): nothing while off; type 0's
// gain factor first; then by the state, a start's first tick arming the glide, the glide, its end.  Every address is
// rom.4's.
#include "Vc2PitchRamp.h"
#include "Vc2RampTables.h"
#include "XtFloat.h"
#include <algorithm>

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

namespace {
constexpr int32_t kHalf = 4;   // [FW] 0x893b0fcc: the rows from 4 on take the second half's scale
constexpr int32_t kLastRow = 7;
enum : int32_t { kIdle = 0, kStarted = 1, kGliding = 2, kDone = 3 };   // [FW] the tick's two jump tables
} // namespace

// [FW] 0x893b0f9c…0x893b103c: the factor's phase on by its step × n, held to 65536; x its fraction, its row the
// eighth x is in; the factor 1 + (2^−24 + step × 1e−4) × depth × ease × scale × (base + curve × (x − at)²), the scale
// the second half's above row 4 and the first half's negated below.  From 65536 on, the factor is 1
void Vc2PitchRamp::stepFactor(int32_t n, const Vc2RampTables& t) {
    if (!(t.factorEnd > w.factorPhase)) {
        w.factor = 1.0f;
        return;
    }
    float p = add(mul(w.factorStep, xtf::flt(n)), w.factorPhase);
    w.factorPhase = p;
    if (p > t.factorEnd) {
        w.factorPhase = t.factorEnd;
        p = t.factorEnd;
    }
    const float x = mul(p, t.perPhase);
    const int32_t row = xtf::ftrc(mul(x, t.rowsPerX));
    const float scale = row >= kHalf ? w.factorRise : -w.factorFall;
    // A row past the table: rom.4 reads the words around it.  Only a negative count or phase reaches one, which the
    // tone never gives (its count is the blocks gone by, its phase from 0) and the check never draws
    const Vc2RampRow& r = t.rows[(size_t)std::clamp(row, 0, kLastRow)];
    const float d = sub(x, r.at);
    const float k = mul(mul(mul(add(mul(w.factorStep, t.factorPerStep), t.factorFloor), w.factorDepth), w.ease), scale);
    const float bump = add(mul(r.curve, mul(d, d)), r.base);
    w.factor = add(mul(k, bump), 1.0f);
}

// [FW] 0x893b10a0…: the phase on by the step × n; at its end the glide is done (0, a ratio's 1); else x = phase/65536 − 1
// gives type 0 span × ((1 − ease) x − ease x⁴), the others exp2(x × span / 1200)
void Vc2PitchRamp::glide(int32_t n, const Vc2ToneShared& shared, const Vc2RampTables& t) {
    w.phase = add(mul(w.step, xtf::flt(n)), w.phase);
    if (!(t.glideEnd > w.phase)) {
        w.state = kDone;
        w.value = w.type != 0 ? 1.0f : 0.0f;
        return;
    }
    const float x = add(div(w.phase, t.glideEnd), -1.0f);
    if (w.type != 0) {
        w.value = shared.exp2(mul(mul(x, w.span), t.tickPerCent));
        return;
    }
    const float x2 = mul(x, x);
    w.value = mul(sub(mul(sub(1.0f, w.ease), x), mul(mul(x2, x2), w.ease)), w.span);
}

void Vc2PitchRamp::tick(int32_t n, const Vc2ToneShared& shared) {
    if (w.on == 0) return;
    const int32_t n16 = (int16_t)n;
    const Vc2RampTables& t = vc2RampTables();
    if (w.type == 0) stepFactor(n16, t);
    switch (w.state) {
        case kIdle: return;
        case kStarted: w.state = kGliding; return;
        case kGliding: glide(n16, shared, t); return;
        case kDone: w.value = w.type != 0 ? 1.0f : 0.0f; return;
        default:   // [FW] 0x893b1140: a state past 3 is done
            w.state = kDone;
            w.value = w.type != 0 ? 1.0f : 0.0f;
            return;
    }
}
