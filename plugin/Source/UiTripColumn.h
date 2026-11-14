#pragma once
// D's Time Trip column (research/UI_DESIGN.md, "The rack"; UI step 2, the XT's pad from v202): the pad (UiTripPad.h),
// sending the XT's pad controllers into the synth; under it Hold, which the session keeps, and the Time Trip switch and
// Beat Keep of the OSC it follows (OSC 1 or OSC 2).
#include "PluginProcessor.h"
#include "UiControls.h"
#include "UiTripPad.h"
#include <array>

class TripColumn : public juce::Component {
public:
    explicit TripColumn(VSynthEmuProcessor& p);
    static constexpr int kOscs = 2;
    static constexpr const char* kPadHold = "padHold";   // the session's state property
    void follow(int osc);   // whose switches show under the pad
    TripPad& pad() { return pad_; }
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    Controls::Attached attached_;
    TripPad pad_;
    std::array<juce::TextButton, kOscs> follow_;
    std::array<juce::ToggleButton, kOscs> trip_, beat_;
    juce::ToggleButton hold_ { "Pad Hold" };   // the XT's name (CONTROLLERS.md), apart from the arpeggio's Hold
    juce::Label name_;
};
