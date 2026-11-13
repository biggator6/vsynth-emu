#pragma once
// D's controls made straight from a parameter's ID (UI step 2): a box holding a choice's values, attached once it has
// them; a switch; a knob (UiKnob.h).  A panel keeps what these give it for as long as it shows the controls.
#include "UiKnob.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace Controls {

using State = juce::AudioProcessorValueTreeState;

// A knob in a panel's table: its parameter's ID (OSC 1's or COSM 1's, which the panel makes its own instance's) and
// the name under it
struct KnobRow {
    const char* id;
    const char* name;
};

// What a panel's controls are attached by; each made with its control, dropped with it
struct Attached {
    std::vector<std::unique_ptr<State::ComboBoxAttachment>> boxes;
    std::vector<std::unique_ptr<State::ButtonAttachment>> switches;
};

// The box given the choice's values, then attached to it; false if the ID names no choice
bool box(juce::ComboBox& b, State& state, const juce::String& id, Attached& into);
// The switch attached to the parameter; false if the ID names none
bool toggle(juce::Button& b, State& state, const juce::String& id, Attached& into);
// A knob on the parameter, or none if the ID names none (a table row's mistake, which the editor's test reports)
std::unique_ptr<Knob> knob(State& state, const juce::String& id, const juce::String& name, int dialSize);

}
