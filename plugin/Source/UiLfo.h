#pragma once
// A block's LFO (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): its shape, drawn from its waveform,
// rate, delay, fade and offset; in SELECTED with its name, Key Sync, its boxes (Waveform, Fade Mode, Offset) and its
// knobs (Rate, Delay, Fade); alone, a click opening its block, in LFOs' tile.  Its parameters are its kind's LFO prefix's
// (UiBlockKinds.h) made the instance's own.
#include "UiBlockKinds.h"
#include "UiFields.h"
#include "UiWatch.h"
#include <functional>
#include <memory>
#include <vector>

class LfoShape : public juce::Component {
public:
    LfoShape(Controls::State& state, const Blocks::Block& b);
    bool complete() const { return watch_.complete(); }   // its six parameters were all found
    std::function<void()> onClick;                         // a click on it as a tile
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    Watch watch_;   // Waveform, Rate, Delay, Fade, Fade Mode, Offset
};

class LfoView : public juce::Component {
public:
    LfoView(Controls::State& state, const Blocks::Block& b);
    LfoShape& shape() { return shape_; }
    void resized() override;

private:
    Controls::Attached attached_;
    juce::Label name_;
    juce::ToggleButton sync_ { "Key Sync" };
    LfoShape shape_;
    Field wave_, fadeMode_, offset_;
    std::vector<std::unique_ptr<Knob>> knobs_;   // Rate, Delay, Fade
};
