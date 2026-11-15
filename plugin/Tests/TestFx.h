#pragma once
// A reference's effects, started as the processor's are (PluginEffects.cpp's fxTick: its settings, the Patch Tempo, the
// patch, then the chip played till it has all of it) and ticked each block as its control thread ticks them, so XtSynth
// through it is the processor's sound to the bit.  PluginTest's TEST_FX's, made one with VSynthEmuExtInTest's (v207).
#include "../Source/XtFxControl.h"
#include "../Source/XtSynth.h"
#include <map>
#include <memory>

struct RefEffects {
    static constexpr int kPatchTempo = 0x11d, kClockSource = 0x41;   // [FW] the system's IDs the driver reads

    explicit RefEffects(const char* rom) : unit(std::make_unique<XtFxUnit>(rom)) {}
    XtFxControl& control() { return unit->control(); }
    // After its settings: the reference's Patch Tempo, the patch, and the chip played till it has all the driver sent
    void start(const XtSynth& ref) {
        control().set(kPatchTempo, ref.fxPatchTempo());
        control().loadPatch();
        unit->settle();
    }
    // A block's tick: what changed in it by ID, then what the processor's fxApply and fxTick send each time round
    void tick(const XtSynth& ref, const std::map<int, int>& now = {}) {
        XtFxControl& c = control();
        for (const auto& [id, value] : now) c.set(id, value);
        c.set(kPatchTempo, ref.fxPatchTempo());
        c.set(kClockSource, ref.fxClockSix() ? 1 : 0);
        c.tick();
        c.setMatrix(ref.fxMatrix(0), ref.fxMatrix(1), ref.fxMatrix(2));
        c.outsideClock(ref.fxClockSix());
        c.pump();
    }

    std::unique_ptr<XtFxUnit> unit;
};
