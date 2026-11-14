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
// A card of controls: its title and its rows (each a parameter's ID and the name under its control), in a column
struct CardRows {
    const char* title;
    std::array<Controls::KnobRow, kMaxRows> rows;
    int column;
};

// A card holding a grid of controls, from a table's rows or from rows made at run time
std::unique_ptr<Card> gridCard(Controls::State& state, const CardRows& rows);
std::unique_ptr<Card> gridCard(Controls::State& state, const juce::String& title, const ControlGrid::Rows& rows);
// The cards down r, each as high as its grid needs across r's width; the last given what is left when `fill`
void stack(const std::vector<Card*>& cards, juce::Rectangle<int> r, bool fill);

// The page `page` of Blocks::kPages, made for a processor; none for SYNTH, which is the path, the rack and the lower panel
std::unique_ptr<juce::Component> makePage(int page, VSynthEmuProcessor& p);

}
