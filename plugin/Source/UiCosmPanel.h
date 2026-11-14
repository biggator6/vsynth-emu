#pragma once
// D's COSM panel (research/UI_DESIGN.md, "The rack"; UI step 2): a FacedPanel made for each COSM by its index, its faces
// COSM Type's sixteen (UiCosmPanel.cpp's kFaces): the type's own box where it has one and up to five knobs, its first
// (the rest in the lower panel's DETAILS); a display of the type's response, its curve, or its name.
#include "UiFacedPanel.h"

class CosmPanel : public FacedPanel {
public:
    CosmPanel(juce::AudioProcessorValueTreeState& state, int cosm);
    void refresh() override;   // and the display, if its first two knobs have moved
    static constexpr int kTypes = 16;   // COSM Type's values, TVF to Speaker Simulator
    static int tableKnobs(int type);   // how many knobs its table gives a type, each of which must name a parameter
    void resized() override;

private:
    juce::String drawn_;   // the knobs the display was drawn from
    juce::Rectangle<int> display_;
    float proportionOf(size_t knob) const;   // the face's knob's value, 0 to 1; 0 if it has none
    void paintDisplay(juce::Graphics&) const override;
};
