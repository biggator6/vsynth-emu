#pragma once
#include "../Source/PluginProcessor.h"

// A parameter set to a value in its own range, as a host sets one: the tests' one way to move a control.
inline void setParam(VSynthEmuProcessor& p, const juce::String& id, float value) {
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(value));
}
