// The effects' tick, on their control thread (FxThread, PluginProcessor.cpp) or a test's: the chip and its driver, made
// the first time the Effects switch is on, then each tick what's changed sent to the driver.  Moved here from
// PluginProcessor.cpp in v172, when a driver that didn't start began to say so.
#include "PluginProcessor.h"

void VSynthEmuProcessor::fxTick() {
    const std::lock_guard<std::mutex> lock(fxLock_);
    if (!effectsWanted() && !fxUnit_) return;
    if (!fxUnit_) {
        if (romFile_.isEmpty()) return;
        // A driver that didn't start says why, and is used no further: one whose ROM won't load has no store to set.
        const auto started = [this](XtFxUnit& unit) {
            if (unit.ok()) return true;
            setStatus("Effects didn't start: " + juce::String(unit.control().error()));
            romFile_.clear();
            return false;
        };
        auto u = std::make_unique<XtFxUnit>(romFile_.toRawUTF8());
        if (!started(*u)) return;
        fxApply(u->control());
        u->control().loadPatch();
        fxPatchPending_ = false;
        u->settle();
        if (!started(*u)) return;
        fxUnit_ = std::move(u);
        synth_.setBusSink(fxUnit_.get());
        fxRunning_ = true;
    }
    XtFxControl& c = fxUnit_->control();
    fxApply(c);
    if (fxPatchPending_.exchange(false)) c.wantPatch();
    c.tick();
    if (effectsWanted()) {    // nothing's taking the words otherwise
        c.setMatrix(synth_.fxMatrix(0), synth_.fxMatrix(1), synth_.fxMatrix(2));
        c.outsideClock(synth_.fxClockSix());
    }
    c.pump();
}
