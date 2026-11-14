// D's pages and what their cards share (UiPages.h; UI step 4)
#include "UiPages.h"
#include "FxPanel.h"
#include "UiBlocks.h"
#include "UiLook.h"
#include "UiModPage.h"
#include "UiPlayPage.h"
#include "UiZonesPage.h"

namespace Pages {

std::unique_ptr<Card> gridCard(Controls::State& state, const juce::String& title, const ControlGrid::Rows& rows) {
    auto card = std::make_unique<Card>(title);
    card->setBody(std::make_unique<ControlGrid>(state, rows));
    return card;
}

std::unique_ptr<Card> gridCard(Controls::State& state, const CardRows& rows) {
    ControlGrid::Rows made;
    for (const Controls::KnobRow& row : rows.rows)
        if (row.id != nullptr) made.push_back({ row.id, row.name });
    return gridCard(state, rows.title, made);
}

void stack(const std::vector<Card*>& cards, juce::Rectangle<int> r, bool fill) {
    for (size_t i = 0; i < cards.size(); ++i) {
        Card& c = *cards[i];
        auto* grid = dynamic_cast<ControlGrid*>(c.body());
        const int body = grid != nullptr ? grid->heightFor(c.bodyWidthFor(r.getWidth())) : 0;
        const bool last = i + 1 == cards.size();
        c.setBounds(fill && last ? r : r.removeFromTop(c.heightFor(body)));
        r.removeFromTop(Look::gap);
    }
}

namespace {

std::unique_ptr<juce::Component> noPage(VSynthEmuProcessor&) {
    return nullptr;
}
std::unique_ptr<juce::Component> modulation(VSynthEmuProcessor& p) {
    return std::make_unique<ModulationPage>(p);
}
std::unique_ptr<juce::Component> effects(VSynthEmuProcessor& p) {
    return std::make_unique<FxPanel>(p, Look::cards);
}
std::unique_ptr<juce::Component> play(VSynthEmuProcessor& p) {
    return std::make_unique<PlayPage>(p);
}
std::unique_ptr<juce::Component> zones(VSynthEmuProcessor& p) {
    return std::make_unique<ZonesPage>(p);
}

// Each page's maker, in Blocks::kPages' order: SYNTH, MODULATION, FX, PLAY, ZONES
using Maker = std::unique_ptr<juce::Component> (*)(VSynthEmuProcessor&);
constexpr std::array<Maker, Blocks::kPages.size()> kMakers { noPage, modulation, effects, play, zones };

}

std::unique_ptr<juce::Component> makePage(int page, VSynthEmuProcessor& p) {
    return kMakers[(size_t)page](p);
}

}
