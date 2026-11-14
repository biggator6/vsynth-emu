#pragma once
// D's PLAY page (research/UI_DESIGN.md, "The pages"; UI step 4): the arpeggio's card and its pattern's down the left;
// portamento, keys, tune and tempo down the right, each a card of controls from a table (UiPlayPage.cpp).
#include "PluginProcessor.h"
#include "UiArpPattern.h"
#include "UiCard.h"
#include <memory>
#include <vector>

class PlayPage : public juce::Component {
public:
    explicit PlayPage(VSynthEmuProcessor& p);
    void resized() override;

private:
    std::vector<std::unique_ptr<Card>> cards_;   // the table's, in its order
    std::unique_ptr<Card> pattern_;
};
