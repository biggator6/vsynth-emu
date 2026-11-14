#pragma once
// D's page bar (research/UI_DESIGN.md, "The window"; UI step 1): Blocks::kPages' names as tabs (UiTabs.h), the open one
// underlined in the accent, a click opening another; on SYNTH, a line saying what a click on a block does.
#include "UiTabs.h"

class PageBar : public Tabs {
public:
    PageBar();
    void paint(juce::Graphics&) override;
};
