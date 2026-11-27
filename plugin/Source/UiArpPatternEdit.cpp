// The arpeggio pattern card's body when it can be edited (UiArpPatternEdit.h; v468)
#include "UiArpPatternEdit.h"
#include "UiLook.h"

namespace {
constexpr int kBarH = 24, kBarGap = 6;   // the bar under the pattern, and the space above it
constexpr int kValueW = 92, kButtonW = 38, kClearW = 84, kInitW = 52, kGap = 4, kGroupGap = 14;

// What the bar's clears and INIT do to the pattern, the cursor where it is
void lineCleared(XtArpPattern& p, XtArpCursor at) {
    xtArpClearLine(p, at.line);
}
void allCleared(XtArpPattern& p, XtArpCursor) {
    xtArpClearAll(p);
}
void initialized(XtArpPattern& p, XtArpCursor) {
    p = xtArpInitPattern();
}
}

ArpPatternEditor::ArpPatternEditor(VSynthEmuProcessor& p, ArpPatternView::Source source) : view_(p, std::move(source)) {
    addAndMakeVisible(view_);
    Look::setTextLook(value_, Look::mono(11.0f), Look::text, juce::Justification::centredLeft);
    addAndMakeVisible(value_);
    for (int b = 0; b < kXtArpButtons; ++b) {
        buttons_[(size_t)b] = std::make_unique<juce::TextButton>(xtArpButtonName(b, false));
        own(*buttons_[(size_t)b], [b](XtArpPattern& pattern, XtArpCursor at) { xtArpPress(pattern, at, b); });
    }
    own(lineClear_, lineCleared);
    own(allClear_, allCleared);
    own(init_, initialized);
    view_.onChange = [this] { follow(); };
    follow();
}

// A button of the editor's own, no parameter's (the editor's test moves none through it): its change made to the
// pattern at the cursor, into the session
void ArpPatternEditor::own(juce::TextButton& b, std::function<void(XtArpPattern&, XtArpCursor)> change) {
    b.getProperties().set(Look::editorsOwn, true);
    b.onClick = [this, change = std::move(change)] {
        const XtArpCursor at = view_.cursor();
        view_.edit([&](XtArpPattern& pattern) { change(pattern, at); });
    };
    addAndMakeVisible(b);
}

void ArpPatternEditor::follow() {
    const XtArpPattern pattern = view_.pattern();
    const XtArpCursor at = view_.cursor();
    value_.setText("Value : " + juce::String(xtArpValueText(pattern, at)), juce::dontSendNotification);
    const bool ctrl = xtArpIsCtrl(pattern, at.line);
    for (int b = 0; b < kXtArpButtons; ++b)
        buttons_[(size_t)b]->setButtonText(xtArpButtonName(b, ctrl));
}

// The pattern over the bar: Value and the six buttons from the left, the clears and INIT from the right
void ArpPatternEditor::resized() {
    auto r = getLocalBounds();
    auto bar = r.removeFromBottom(kBarH);
    r.removeFromBottom(kBarGap);
    view_.setBounds(r);
    value_.setBounds(bar.removeFromLeft(kValueW));
    for (auto& b : buttons_) {
        b->setBounds(bar.removeFromLeft(kButtonW));
        bar.removeFromLeft(kGap);
    }
    bar.removeFromLeft(kGroupGap);
    init_.setBounds(bar.removeFromRight(kInitW));
    bar.removeFromRight(kGap);
    allClear_.setBounds(bar.removeFromRight(kClearW));
    bar.removeFromRight(kGap);
    lineClear_.setBounds(bar.removeFromRight(kClearW));
}
