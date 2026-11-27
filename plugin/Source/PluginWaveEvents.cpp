// A loaded wave's events edited on the WAVE page, the XT's own Encode screen's adds and deletes (PluginWaveEdit.h,
// v481; HARDWARE_FIRMWARE.md {#wave-edit-read}): kept in the session in order at the Encode Depth they were made at, run
// by the XT's own routines in the wave's encode (XtFwEvents.h), each one the XT refuses dropped and said
#include "PluginParamIds.h"
#include "PluginProcessor.h"
#include "PluginWaveEdit.h"

namespace {
constexpr int kSixteenths = 16;   // a vpdt's positions are in 1/16 sample
}

bool waveEventsStand(const PluginWaveEdit& edit, int depth) {
    return !edit.events.empty() && edit.eventsDepth == depth;
}

void PluginWaveEdits::editEvent(VSynthEmuProcessor& p, int osc, const XtFwEventEdit& edit) {
    const int depth = juce::roundToInt(p.apvts.getRawParameterValue(Params::oscId(osc, Params::kEncodeDepth))->load());
    {
        const juce::ScopedLock lock(p.bufferLock_);
        PluginWaveEdit& e = p.wave_[osc].edit;
        if (e.eventsDepth != depth) e.events.clear();   // made at another depth: the XT's are gone with it
        e.eventsDepth = depth;
        e.events.push_back(edit);
    }
    p.reencode(osc);
}

std::vector<long> PluginWaveEdits::events(const VSynthEmuProcessor& p, int osc) {
    std::shared_ptr<const XtEncoded> e;
    {
        const juce::ScopedLock lock(p.bufferLock_);
        e = p.wave_[osc].encoded;
    }
    std::vector<long> frames;
    if (!e) return frames;
    for (const int32_t at : xtVpdtEvents(e->vpdt, e->type, e->channels)) frames.push_back(at / kSixteenths);
    return frames;
}

juce::String PluginWaveEdits::eventsDone(VSynthEmuProcessor& p, int osc, const PluginWaveEdit& given, int depth,
                                         const std::vector<int>& results) {
    const juce::ScopedLock lock(p.bufferLock_);
    PluginWaveEdit& e = p.wave_[osc].edit;
    if (e.events != given.events) return {};   // edited again since: the next encode judges them
    if (!waveEventsStand(given, depth)) {      // the depth moved off theirs: the XT discards them
        e.events.clear();
        e.eventsDepth = -1;
        return {};
    }
    if (results.size() != given.events.size()) return {};   // the encode didn't reach the detection
    std::vector<XtFwEventEdit> taken;
    juce::String said;
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i] == kEventDone) taken.push_back(given.events[i]);
        else if (said.isEmpty()) said = xtFwEventMessage(results[i]);
    }
    e.events = std::move(taken);
    return said;
}
