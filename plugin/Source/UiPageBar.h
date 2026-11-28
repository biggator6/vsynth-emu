#pragma once
// D's page bar (research/UI_DESIGN.md, "The window"; UI step 1): Blocks::kPages' names as tabs (UiTabs.h), the open one
// underlined in the accent, a click opening another; on SYNTH, a line saying what a click on a block does.  At its right
// end the Keyboard switch (v201), which the editor gives folding the footer away and back.
#include "UiTabs.h"

class PageBar : public Tabs {
public:
    PageBar();
    void showVc2(bool vc2);   // the VC-2 mode's pages' names (Blocks::kVc2Pages, v453), or the V-Synth's
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::Button& keyboardSwitch() { return keyboard_; }
    // A line in the accent on every page in the hint's place, MIDI learn's while it waits (v463); none, the hint again
    void showLine(const juce::String& line);
    const juce::String& line() const { return line_; }

private:
    juce::ToggleButton keyboard_ { "Keyboard" };
    juce::String line_;
};
