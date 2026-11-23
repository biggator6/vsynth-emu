#pragma once
// D's PLAY page (research/UI_DESIGN.md, "The pages"; UI step 4): the arpeggio's card and its pattern's down the left;
// portamento, keys, tune and tempo down the right, each a card of controls from a table (UiPlayPage.cpp).  A mode's
// (v453): the V-Synth's, or the VC-2's (UiVc2Pages.cpp), whose right column ends in a card of its own, Scale Tune's.
#include "PluginProcessor.h"
#include "UiArpPattern.h"
#include "UiCard.h"
#include "UiPages.h"
#include <functional>
#include <memory>
#include <vector>

class PlayPage : public juce::Component {
public:
    // A mode's page: its cards' table, how a row names a parameter, its pattern's source, and a card that ends the right
    // column given what is left (none for the V-Synth)
    struct Mode {
        std::vector<Pages::CardRows> cards;
        Pages::RowId idOf = Pages::rowAsId;
        ArpPatternView::Source pattern;
        std::function<std::unique_ptr<Card>()> last;
    };
    explicit PlayPage(VSynthEmuProcessor& p);   // the V-Synth's
    PlayPage(VSynthEmuProcessor& p, Mode mode);
    void resized() override;

private:
    std::vector<int> columns_;                   // each card's column, the table's
    std::vector<std::unique_ptr<Card>> cards_;   // the table's, in its order
    std::unique_ptr<Card> pattern_, last_;
};
