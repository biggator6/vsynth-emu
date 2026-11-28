#pragma once
// D's controls made straight from a parameter's ID (UI step 2): a box holding a choice's values, attached once it has
// them; a switch; a knob (UiKnob.h); and which face a Type parameter shows (UI step 3).  A panel keeps what these give it
// for as long as it shows the controls.
#include "UiKnob.h"
#include "UiMidiLearn.h"
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

// A control attached to a parameter, and marked to learn MIDI for it (v463, UiMidiLearn.h): every maker's way to attach
template <class Attachment, class Control>
std::unique_ptr<Attachment> attached(Control& control, State& state, const juce::String& id) {
    MidiLearn::attach(control, id);
    return std::make_unique<Attachment>(state, id, control);
}

// What a panel's controls are attached by; each made with its control, dropped with it
struct Attached {
    std::vector<std::unique_ptr<State::ComboBoxAttachment>> boxes;
    std::vector<std::unique_ptr<State::ButtonAttachment>> switches;
};

// A control's kind (v453): the V-Synth's by its parameter's type (a choice a box, a bool a switch, any other a knob), the
// VC-2's by rom.4's value string (d-8: Vc2Text::controlKind); none of either a knob
enum class Kind { knob, box, toggle };
Kind kindFor(State& state, const juce::String& id);
// A value's text as the editor shows it (v453): a VC-2 parameter's on its string's scale where rom.4's routine can't
// write it (d-9: Vc2Text::editorText), any other its parameter's own
juce::String editorValueText(State& state, const juce::String& id, int value);
// A parameter's value now as it counts it, rounded; 0 if the ID names none (v453)
int paramValue(State& state, const juce::String& id);
// A parameter set to a value as a control sets it, a whole gesture (the VC-2's zone chips, v453)
void setValue(State& state, const juce::String& id, int value);
// A parameter's value now in the editor's text, empty if the ID names none (the path's settings, the routes' names)
juce::String currentText(State& state, const juce::String& id);
// A slider attached to a parameter, its value shown in the editor's text of it (a VC-2 parameter's; any other's as is)
void showEditorText(juce::Slider& s, State& state, const juce::String& id);

// The box given the parameter's values, then attached to it: a choice's, or a VC-2 parameter's each in the editor's text
// once rom.4's tables are in (v453); false if the ID names neither
bool box(juce::ComboBox& b, State& state, const juce::String& id, Attached& into);
// The switch attached to the parameter; false if the ID names none
bool toggle(juce::Button& b, State& state, const juce::String& id, Attached& into);
// A knob on the parameter, or none if the ID names none (a table row's mistake, which the editor's test reports)
std::unique_ptr<Knob> knob(State& state, const juce::String& id, const juce::String& name, int dialSize);
// The face a Type parameter shows: its value, within the faces there are; the first with no Type
int faceOf(State& state, const juce::String& typeId, int faces);

}
