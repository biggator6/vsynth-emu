#pragma once
// Upright bars across, each a parameter to drag, filled in `mod` from its range's zero, its name under it (v453, moved out
// of the MSM's track as it was: its 16 steps, research/UI_DESIGN.md, "The pages"; and the VC-2's Scale Tune, twelve
// bars a key, research/UI_DESIGN_VC2.md, "PLAY", d-7).  A row of no parameter makes no bar.
#include "UiControlGrid.h"
#include <memory>
#include <vector>

class UprightBars : public juce::Component {
public:
    UprightBars(Controls::State& state, const ControlGrid::Rows& rows);
    int made() const { return (int)bars_.size(); }
    juce::Slider& bar(size_t i) { return *bars_[i]; }
    void resized() override;

private:
    std::vector<std::unique_ptr<juce::Slider>> bars_;
    std::vector<std::unique_ptr<Controls::State::SliderAttachment>> atts_;
    std::vector<std::unique_ptr<juce::Label>> names_;
};
