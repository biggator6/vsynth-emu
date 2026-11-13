#pragma once
// A knob made from a parameter's ID: the dial on the parameter, its name under it and the parameter's own text of its
// value under that (research/UI_DESIGN.md, "The look"; UI step 1).  Its width is its owner's; its height is heightFor.
#include <juce_audio_processors/juce_audio_processors.h>

class Knob : public juce::Component {
public:
    Knob(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int dialSize);

    static int heightFor(int dialSize) { return dialSize + 2 * kTextH + 1; }
    // The modulation's reach as proportions of the range, drawn as a ring; none if lo == hi
    void setModulation(float lo, float hi);
    juce::Slider& dial() { return dial_; }
    void resized() override;

private:
    static constexpr int kTextH = 13;
    juce::Slider dial_ { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Label name_, value_;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment_;
    int size_;
    void showValue();
};
