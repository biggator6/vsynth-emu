#pragma once
// Two controls made from a parameter's ID with their name (UI step 3), beside the knob (UiKnob.h):
// - a bar: the parameter as a bar filled from its zero to its value, the parameter's own text of its value on it (as
//   Look::Feel draws a LinearBar), its name over it or, given a width for it, beside it: the envelopes' and the routes'
//   control, where knobs would not fit;
// - a field: a choice's box, its name over it.
#include "UiControls.h"

class Bar : public juce::Component {
public:
    Bar(Controls::State& state, const juce::String& id, const juce::String& name, juce::Colour fill);
    static constexpr int kHeight = 34;      // with its name over the bar
    static constexpr int kRowHeight = 20;   // with its name beside it
    void setNameWidth(int w);               // 0: the name over the bar
    juce::Slider& slider() { return bar_; }
    void resized() override;

private:
    juce::Slider bar_ { juce::Slider::LinearBar, juce::Slider::NoTextBox };
    juce::Label name_;
    Controls::State::SliderAttachment attachment_;
    int nameW_ = 0;
};

class Field : public juce::Component {
public:
    Field(Controls::State& state, const juce::String& id, const juce::String& name);
    static constexpr int kHeight = 36;
    bool attached() const { return !attached_.boxes.empty(); }   // false: the ID named no choice
    juce::ComboBox& box() { return box_; }
    void resized() override;

private:
    Controls::Attached attached_;
    juce::ComboBox box_;
    juce::Label name_;
};

