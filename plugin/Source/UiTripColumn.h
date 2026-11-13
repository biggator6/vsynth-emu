#pragma once
// D's Time Trip column (research/UI_DESIGN.md, "The rack"; UI step 2): the pad, which sets the Time Trip parameter by
// where it is pressed across it (the X-Y pad's controllers come in a later step), whose OSC's switches show under it
// (OSC 1 or OSC 2), and that OSC's Time Trip switch and Robot.
#include "PluginProcessor.h"
#include "UiControls.h"
#include <array>

// The pad: a slider on the Time Trip parameter across its width, drawn as the pad (OFF at its left edge)
class TripPad : public juce::Slider {
public:
    TripPad() : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox) { setTitle("Time Trip"); }
    void paint(juce::Graphics&) override;
};

class TripColumn : public juce::Component {
public:
    explicit TripColumn(VSynthEmuProcessor& p);
    static constexpr int kOscs = 2;
    void follow(int osc);   // whose switches show under the pad
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Controls::Attached attached_;
    TripPad pad_;
    std::array<juce::TextButton, kOscs> follow_;
    std::array<juce::ToggleButton, kOscs> trip_, robot_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> padAtt_;
    juce::Label name_;
};
