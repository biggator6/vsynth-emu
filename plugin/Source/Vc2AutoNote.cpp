// The VC-2's Auto Note and the pitch source: see Vc2AutoNote.h (v226).  Each routine's arithmetic in its own order, in
// XtFloat; every address is rom.4's.
#include "Vc2AutoNote.h"
#include "Vc2AutoNoteTables.h"
#include "XtFloat.h"

namespace {
using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

constexpr int32_t kOff = 0, kWhileNoKey = 1;   // [FW] the On Switch's 0 and 1; 2 plays whatever the keys do
constexpr int32_t kStates = 3;                 // [FW] the setter's 0…2
} // namespace

void Vc2AutoNote::init() {
    const Vc2AutoNoteTables& t = vc2AutoNoteTables();
    g.a440Period = t.a440Period;
    g.unreadRate = t.unreadRate;
    g.c4Period = t.c4Period;
}

void Vc2AutoNote::clear() {
    w.note = -1;
    w.period = 0.0f;
    w.release = 0;
    w.ratio = 1.0f;
}

void Vc2AutoNote::block(Vc2Tracker& tracker, const float* in, int n, const Vc2PitchSource& source,
                        Vc2AutoNoteHost& host) {
    const float tracked = tracker.block(in, n);
    if (w.release != 0) {   // the note-off goes even for no note, and then the release stays asked for
        host.noteOff(w.note);
        if (w.note >= 0) clear();
    }
    if (w.onSwitch == kOff) return;
    if (w.onSwitch == kWhileNoKey && source.others > 0) {   // a key held: Auto Note gives way
        if (w.note >= 0) clear();
        return;
    }
    if (w.period > 0.0f) {   // sounding: C4 bent to the sung pitch
        w.ratio = div(w.period, tracked);
        return;
    }
    if (w.detector != 0) return;
    if (host.noteOn(kC4, 1.0f)) {
        w.note = kC4;
        w.period = g.c4Period;
    }
}

void Vc2AutoNote::setSwitch(int32_t on) {
    if (on < 0 || on >= kStates || on == w.onSwitch) return;
    if (w.period > 0.0f) w.release = 1;
    w.onSwitch = on;
}

void Vc2AutoNote::askRelease() {
    if (w.detector == 0 && w.period > 0.0f) w.release = 1;
}

float Vc2AutoNote::bend(const Vc2Tracker& tracker, float depth) {
    const Vc2AutoNoteTables& t = vc2AutoNoteTables();
    const float perC4 = div(1.0f, g.c4Period);
    if (g.bendStarted == 0) {
        g.bendStarted = 1;
        g.smoothed = g.c4Period;
    }
    if (depth == 0.0f) return 1.0f;
    const float moved = sub(tracker.period(), g.smoothed);
    g.smoothed = add(mul(moved, t.smoothing), g.smoothed);   // FMAC
    float held = moved;
    if (moved > t.most) {
        held = t.most;
    } else if (t.least > held) {
        held = t.least;
    }
    return sub(1.0f, mul(mul(depth, held), perC4));
}

float Vc2AutoNote::ratioFor(const Vc2Tracker& tracker, const Vc2PitchSource& source) {
    return source.note >= 0 ? w.ratio : bend(tracker, source.depth);
}
