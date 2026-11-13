#pragma once
// D's page bar (research/UI_DESIGN.md, "The window"; UI step 1): Blocks::kPages' names, the open one underlined in the
// accent, a click opening another; on SYNTH, a line saying what a click on a block does.
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

class PageBar : public juce::Component {
public:
    PageBar() { setComponentID("pages"); }
    std::function<void(int page)> onPage;
    void setPage(int page);
    int page() const { return page_; }
    std::vector<juce::Rectangle<int>> tabs() const;   // where each page's name is, in Blocks::kPages' order
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    int page_ = 0;
};
