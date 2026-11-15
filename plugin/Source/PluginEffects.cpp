// The effects' tick, on their control thread (FxThread, PluginProcessor.cpp) or a test's: the chip and its driver, made
// the first time the Effects switch is on, then each tick what's changed sent to the driver.  Moved here from
// PluginProcessor.cpp in v172, when a driver that didn't start began to say so.  And the effects' parameters by the
// firmware's ID and the catalog, moved here as they stood in v207, to make room in PluginProcessor.cpp.
#include "PluginProcessor.h"
#include "PluginParamIds.h"

bool VSynthEmuProcessor::effectsWanted() const { return apvts.getRawParameterValue(Params::kFxOn)->load() >= 0.5f; }

// The plugin's effect parameters, each the firmware's ID.
static const struct { const char* name; int id; } kFxKnobs[] = {
    { "fxMfxOn", 0x120 }, { "fxChoOn", 0x121 }, { "fxRevOn", 0x122 },
    { "fxMfxType", 0x177 }, { "fxMfxMaster", 0x178 }, { "fxMfxCho", 0x179 }, { "fxMfxRev", 0x17a },
    { "fxChoType", 0x1fc }, { "fxChoMaster", 0x1fd }, { "fxChoRev", 0x1fe },
    { "fxRevType", 0x230 }, { "fxRevMaster", 0x231 },
    { "fxMaster", 0x3e }, { "fxGain", 0x76 }, { "fxExtInType", 0x78 }, { "fxExtInSource", 0x7c }
};

int VSynthEmuProcessor::fxKnobId(const juce::String& parameter) {
    for (const auto& k : kFxKnobs) if (parameter == k.name) return k.id;
    return -1;
}

const char* VSynthEmuProcessor::fxKnobOf(int id) {
    const auto* k = std::ranges::find_if(kFxKnobs, [id](const auto& knob) { return knob.id == id; });
    return k == std::end(kFxKnobs) ? nullptr : k->name;
}

// All of it to the driver's store, which only takes what's changed.
void VSynthEmuProcessor::fxApply(XtFxControl& c) {
    for (const auto& k : kFxKnobs) c.set(k.id, (int)std::lround(apvts.getRawParameterValue(k.name)->load()));
    c.set(0x11d, synth_.fxPatchTempo());
    c.set(0x41, synth_.fxClockSix() ? 1 : 0);
    for (int id = 0; id < kFxIds; ++id) { const int v = fxEdits_[id].load(std::memory_order_relaxed); if (v != kFxNoEdit) c.set(id, v); }
}

void VSynthEmuProcessor::setFxParam(int id, int value) { if (id >= 0 && id < kFxIds) fxEdits_[id].store(value, std::memory_order_relaxed); }

int VSynthEmuProcessor::fxParam(int id, int fallback) const {
    const int v = id >= 0 && id < kFxIds ? fxEdits_[id].load(std::memory_order_relaxed) : kFxNoEdit;
    return v == kFxNoEdit ? fallback : v;
}

const XtFxCatalog& VSynthEmuProcessor::fxCatalog() {
    if (!fxCatalog_.ok) {
        juce::String rom;
        { const std::lock_guard<std::mutex> lock(fxLock_); rom = romFile_; }
        if (rom.isNotEmpty()) fxCatalog_ = xtFxCatalog(rom.toRawUTF8());
    }
    return fxCatalog_;
}

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
