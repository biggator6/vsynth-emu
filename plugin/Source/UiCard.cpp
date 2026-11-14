// A titled card of a page (UiCard.h; UI step 4)
#include "UiCard.h"
#include "UiLook.h"

Card::Card(const juce::String& title) {
    Look::panelName(name_, title);
    addAndMakeVisible(name_);
}

void Card::setHeading(const juce::String& title) {
    name_.setText(title, juce::dontSendNotification);
}

void Card::paint(juce::Graphics& g) {
    Look::paintPanel(g, getLocalBounds(), false);
}

// The title line: what it holds from the right, the name in what is left; the body under it
void Card::resized() {
    auto r = getLocalBounds().reduced(kPad);
    auto title = r.removeFromTop(kTitleH);
    for (auto& [c, w] : head_) {
        c->setBounds(title.removeFromRight(w));
        title.removeFromRight(kGap);
    }
    name_.setBounds(title);
    r.removeFromTop(kGap);
    if (body_) body_->setBounds(r);
}
