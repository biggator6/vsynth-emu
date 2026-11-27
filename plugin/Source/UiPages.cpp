// D's pages and what their cards share (UiPages.h; UI step 4)
#include "UiPages.h"
#include "FxPanel.h"
#include "UiBlocks.h"
#include "UiLook.h"
#include "UiModPage.h"
#include "UiPlayPage.h"
#include "UiWavePage.h"
#include "UiZonesPage.h"

namespace Pages {

std::unique_ptr<Card> gridCard(Controls::State& state, const juce::String& title, const ControlGrid::Rows& rows) {
    auto card = std::make_unique<Card>(title);
    card->setBody(std::make_unique<ControlGrid>(state, rows));
    return card;
}

juce::String rowAsId(const char* row) {
    return row;
}

TitleSwitch::TitleSwitch(Controls::State& state, const juce::String& id) {
    getProperties().set(Look::power, true);
    Controls::toggle(*this, state, id, attached_);
}

TitleBox::TitleBox(Controls::State& state, const juce::String& id) {
    Controls::box(*this, state, id, attached_);
}

std::unique_ptr<Card> gridCard(Controls::State& state, const CardRows& rows, RowId idOf) {
    ControlGrid::Rows made;
    for (const Controls::KnobRow& row : rows.rows)
        if (row.id != nullptr) made.push_back({ idOf(row.id), row.name });
    auto card = gridCard(state, rows.title, made);
    if (rows.titleSwitch != nullptr)
        card->toTitle(std::make_unique<TitleSwitch>(state, idOf(rows.titleSwitch)), TitleSwitch::kWidth);
    return card;
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

std::unique_ptr<juce::Component> noPage(VSynthEmuProcessor&) {
    return nullptr;
}

namespace {

std::unique_ptr<juce::Component> effects(VSynthEmuProcessor& p) {
    return std::make_unique<FxPanel>(p, Look::cards);
}

// Each page's maker, in Blocks::kPages' order: SYNTH, MODULATION, FX, PLAY, ZONES, WAVE
constexpr std::array<Maker, Blocks::kPages.size()> kMakers { noPage, made<ModulationPage>, effects, made<PlayPage>,
                                                             made<ZonesPage>, made<WavePage> };

}

std::unique_ptr<juce::Component> makePage(int page, VSynthEmuProcessor& p) {
    return kMakers[(size_t)page](p);
}

}
