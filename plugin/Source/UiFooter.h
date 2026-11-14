#pragma once
// D's footer (research/UI_DESIGN.md, "The window"; UI step 1): MONO or POLY and how many zones the patch plays, then a
// keyboard of six octaves with each zone's keys under a band of its colour, from the zones' Range Upper parameters.
#include "PluginProcessor.h"

class Footer : public juce::Component {
public:
    explicit Footer(VSynthEmuProcessor& p) : processor_(p) {}
    void paint(juce::Graphics&) override;
    void refresh();   // repaints if the keys' mode, the zones or their keys have moved since they were drawn

    static constexpr int kStatusW = 184;           // as wide as the rack's Time Trip column
    static constexpr int kLowKey = 24, kWhiteKeys = 42;   // C1 and six octaves of white keys up from it
    static juce::Colour zoneColour(int zone);

private:
    VSynthEmuProcessor& processor_;
    juce::String shown_;   // what the footer said when it was drawn
    void paintZones(juce::Graphics&, juce::Rectangle<float> band, float keyW) const;
    void paintKeys(juce::Graphics&, juce::Rectangle<float> keys, float keyW) const;
};
