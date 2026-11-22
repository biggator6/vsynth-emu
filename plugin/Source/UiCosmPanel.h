#pragma once
// D's COSM panel (research/UI_DESIGN.md, "The rack"; UI step 2): a FacedPanel made for each COSM by its index, its faces
// COSM Type's sixteen (UiCosmPanel.cpp's kFaces): the type's own box where it has one and up to five knobs, its first
// (the rest in the lower panel's DETAILS); a display of the type's response, its curve, or its name.  Its faces are a
// mode's (v453): the V-Synth's, or the VC-2's seventeen by rom.4's COSM Type for a zone (UiVc2Rack.cpp).
#include "UiFacedPanel.h"
#include <array>
#include <functional>

class CosmPanel : public FacedPanel {
public:
    static constexpr int kKnobs = 5;
    // How a type's display draws: its response as a filter, its curve as a shaper, its name, or a wobble (the VC-2's
    // GROWL: its rate by its second knob, its depth by its first)
    enum class Display { response, curve, name, wobble };
    // What a type shows: its own box beside Type (none: nullptr), its first knobs (a row of no ID: none) and its display
    struct Face {
        const char* sub;
        std::array<Controls::KnobRow, kKnobs> knobs;
        Display display;
    };
    // A mode's COSM panel: its faces in its COSM Type's order, a COSM's own ID of a row, its switch's and Type's rows
    struct Model {
        const Face* faces = nullptr;
        int count = 0;
        std::function<juce::String(int cosm, const juce::String& row)> id;
        juce::String switchRow, typeRow;
    };
    CosmPanel(juce::AudioProcessorValueTreeState& state, int cosm);   // the V-Synth's
    CosmPanel(juce::AudioProcessorValueTreeState& state, int cosm, Model model);
    void refresh() override;   // and the display, if its first two knobs have moved
    static constexpr int kTypes = 16;   // the V-Synth's COSM Type's values, TVF to Speaker Simulator
    static int tableKnobs(int type);   // how many knobs the V-Synth's table gives a type, each must name a parameter
    int knobsOf(int face) const;       // how many its own table gives a face
    int faces() const { return model_.count; }
    void resized() override;

private:
    Model model_;
    juce::String drawn_;   // the knobs the display was drawn from
    juce::Rectangle<int> display_;
    float proportionOf(size_t knob) const;   // the face's knob's value, 0 to 1; 0 if it has none
    void paintDisplay(juce::Graphics&) const override;
};
