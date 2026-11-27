#pragma once
// The arpeggio pattern card's body when its pattern can be edited (v468; research/UI_DESIGN.md, "The pages"): the
// pattern (ArpPatternView) and under it the XT's Pattern Edit's own: Value, the cell at the cursor as its screen says it;
// the six buttons, 0, 32, 64, 96, 127 and Tie (Clr on a CTRL line), each writing its cell at the cursor; LINE CLEAR (the
// cursor's line) and ALL CLEAR, the choices of its Clear window; and INIT, the pattern an initialized patch has (the
// user's d-6).  What each writes is XtArpPatternEdit's; each goes into the session as a patch's pattern does.
#include "UiArpPattern.h"
#include <array>
#include <memory>

class ArpPatternEditor : public juce::Component {
public:
    ArpPatternEditor(VSynthEmuProcessor& p, ArpPatternView::Source source);
    void resized() override;

    ArpPatternView& view() { return view_; }
    juce::TextButton& valueButton(int b) { return *buttons_[(size_t)b]; }
    juce::TextButton& lineClear() { return lineClear_; }
    juce::TextButton& allClear() { return allClear_; }
    juce::TextButton& init() { return init_; }
    const juce::Label& value() const { return value_; }

private:
    ArpPatternView view_;
    juce::Label value_;
    std::array<std::unique_ptr<juce::TextButton>, kXtArpButtons> buttons_;
    juce::TextButton lineClear_ { "LINE CLEAR" }, allClear_ { "ALL CLEAR" }, init_ { "INIT" };
    void follow();   // Value's text and the buttons' names, by the cursor and its line's State
    void own(juce::TextButton& b, std::function<void(XtArpPattern&, XtArpCursor)> change);
};
