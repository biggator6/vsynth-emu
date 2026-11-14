// D's page bar (UiPageBar.h; UI step 1)
#include "UiPageBar.h"
#include "UiBlocks.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {
const char* const kHint = "Click a block on the path or in the rack: its envelopes, LFO and routes open below";
constexpr int kKeyboardW = 100, kSwitchH = 24, kRightGap = 16;

juce::StringArray pageNames() {
    juce::StringArray names;
    for (const char* name : Blocks::kPages) names.add(name);
    return names;
}
}

PageBar::PageBar()
    : Tabs(pageNames(), { Look::sans(12.0f, "Bold", 0.1f), 16, 14 }) {
    setComponentID("pages");
    keyboard_.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(keyboard_);
}

void PageBar::resized() {
    auto r = getLocalBounds().withTrimmedRight(kRightGap);
    keyboard_.setBounds(r.removeFromRight(kKeyboardW).withSizeKeepingCentre(kKeyboardW, kSwitchH));
}

void PageBar::paint(juce::Graphics& g) {
    g.fillAll(Look::ground);
    Ui::fill(g, getLocalBounds().removeFromBottom(1), Look::divider);
    Tabs::paint(g);
    const auto r = tabs();
    if (chosen() != Blocks::kSynthPage || r.empty()) return;
    const auto hint = getLocalBounds().withLeft(r.back().getRight() + 24).withRight(keyboard_.getX() - kRightGap);
    Ui::drawText(g, kHint, hint, { Look::sans(11.0f), Look::muted, juce::Justification::centredRight });
}
