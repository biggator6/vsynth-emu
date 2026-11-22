#pragma once
// D's Time Trip column (research/UI_DESIGN.md, "The rack"; UI step 2, the XT's pad from v202): the pad (UiTripPad.h),
// sending the XT's pad controllers into the synth; under it Hold, which the session keeps, and the Time Trip switch and
// Beat Keep of the OSC it follows (OSC 1 or OSC 2).  A mode's (v453): the VC-2's keeps Pad Mode and Pad Hold as the
// patch's parameters, a row under the pad, and its pad sends the pair Pad Mode names (research/UI_DESIGN_VC2.md, d-10).
#include "PluginProcessor.h"
#include "UiControls.h"
#include "UiTripPad.h"
#include <array>
#include <functional>

class TripColumn : public juce::Component {
public:
    static constexpr int kOscs = 2;
    // What a mode's column is made from: each OSC's Time Trip switch and Beat Keep, Pad Hold's parameter and Pad Mode's
    // (none: the V-Synth's, Hold the editor's own, which the session keeps), and the controllers its pad sends
    struct Ids {
        std::function<juce::String(int osc)> trip, beat;
        juce::String holdId, modeId;
        TripPad::Assigns assigns;
    };
    static Ids vsynth();

    explicit TripColumn(VSynthEmuProcessor& p, const Ids& ids = vsynth());
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
    juce::ComboBox mode_;                      // the VC-2's Pad Mode, beside Pad Hold
    juce::Label name_;
    void holdOwn();   // Hold as the editor's own, the session's property
};
