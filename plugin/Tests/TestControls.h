#pragma once
#include "../Source/PluginProcessor.h"
#include "../Source/PluginVc2Params.h"
#include <map>

// A parameter set to a value in its own range, as a host sets one: the tests' one way to move a control.
inline void setParam(VSynthEmuProcessor& p, const juce::String& id, float value) {
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(value));
}

// The parameters' values by ID: the VC-2 mode's (v451, by their IDs' prefix), or the V-Synth's
inline std::map<juce::String, float> parameterValues(VSynthEmuProcessor& p, bool vc2) {
    std::map<juce::String, float> out;
    for (auto* base : p.getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(base);
        if (r && r->paramID.startsWith(kVc2ParamPrefix) == vc2) out[r->paramID] = r->convertFrom0to1(r->getValue());
    }
    return out;
}
