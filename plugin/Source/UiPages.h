#pragma once
// D's pages under the page bar but SYNTH (research/UI_DESIGN.md, "The pages"; UI step 4): MODULATION, FX, PLAY and
// ZONES, each made by its row of one table (makePage), in Blocks::kPages' order; and what their cards share: a card of a
// grid of controls from a table of rows, and cards stacked down a column.
#include "PluginProcessor.h"
#include "UiCard.h"
#include "UiControlGrid.h"
#include <array>
#include <memory>
#include <vector>

namespace Pages {

inline constexpr int kMaxRows = 10;
// A card of controls: its title and its rows (each a parameter's ID and the name under its control), in a column; and
// a switch on its title line, a power dot (v453: the VC-2's Portamento, Scale Tune), none by default
struct CardRows {
    const char* title;
    std::array<Controls::KnobRow, kMaxRows> rows;
    int column;
    const char* titleSwitch = nullptr;
};

// A switch on a card's title line: a power dot on its parameter (v453)
class TitleSwitch : public juce::ToggleButton {
public:
    TitleSwitch(Controls::State& state, const juce::String& id);
    static constexpr int kWidth = 18;

private:
    Controls::Attached attached_;
};
// A box on a card's title line, on its parameter (v453: the VC-2's Vocoder Type)
class TitleBox : public juce::ComboBox {
public:
    TitleBox(Controls::State& state, const juce::String& id);

private:
    Controls::Attached attached_;
};

// A card holding a grid of controls, from a table's rows (each row's ID made a parameter's by `idOf`: as it is for the
// V-Synth's, rom.4's hex for the VC-2's, v453) or from rows made at run time
using RowId = juce::String (*)(const char* row);
juce::String rowAsId(const char* row);
std::unique_ptr<Card> gridCard(Controls::State& state, const CardRows& rows, RowId idOf = rowAsId);
std::unique_ptr<Card> gridCard(Controls::State& state, const juce::String& title, const ControlGrid::Rows& rows);
// The cards down r, each as high as its grid needs across r's width; the last given what is left when `fill`
void stack(const std::vector<Card*>& cards, juce::Rectangle<int> r, bool fill);

// The page `page` of Blocks::kPages, made for a processor; none for SYNTH, which is the path, the rack and the lower panel
std::unique_ptr<juce::Component> makePage(int page, VSynthEmuProcessor& p);
// A mode's table of pages is their makers (v453): SYNTH's none, as it is the path, the rack and the lower panel; a page
// made from the processor alone by `made`
using Maker = std::unique_ptr<juce::Component> (*)(VSynthEmuProcessor&);
std::unique_ptr<juce::Component> noPage(VSynthEmuProcessor& p);
template <class Page>
std::unique_ptr<juce::Component> made(VSynthEmuProcessor& p) {
    return std::make_unique<Page>(p);
}

}
