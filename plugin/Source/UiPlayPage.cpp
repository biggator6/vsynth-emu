// D's PLAY page (UiPlayPage.h; UI step 4)
#include "UiPlayPage.h"
#include "UiLook.h"
#include "UiPages.h"

namespace {

constexpr int kLeftW = 640;   // the arpeggio's column; the other cards take the rest
constexpr int kColumns = 2;

// Each card: its title, its controls (a parameter's ID and its name) and its column, the arpeggio's first
constexpr Pages::CardRows kCards[] = {
    { "ARPEGGIO",
      { { { "arpSwitch", "Arpeggio" }, { "arpHold", "Hold" }, { "arpMotif", "Motif" }, { "arpOctave", "Octaves" },
          { "arpKeyVelocity", "Velocity" }, { "arpDuration", "Duration" }, { "arpGrid", "Grid" },
          { "arpEndPoint", "End Point" }, { "arpShuffleRate", "Shuffle" }, { "arpShuffleRes", "Resolution" } } },
      0 },
    { "PORTAMENTO",
      { { { "portamento", "Portamento" }, { "portaMode", "Mode" }, { "portaType", "Type" }, { "portaTime", "Time" },
          { "portaVeloSens", "Velo Sens" } } },
      1 },
    { "KEYS", { { { "monoPoly", "Voices" }, { "legatoSwitch", "Legato" }, { "octaveShift", "Octave" } } }, 1 },
    { "TUNE",
      { { { "patchCoarse", "Coarse" }, { "patchFine", "Fine" }, { "bendRangeUp", "Bend Up" },
          { "bendRangeDown", "Bend Down" } } },
      1 },
    { "TEMPO", { { { "clockSource", "Clock" }, { "patchTempo", "Tempo" } } }, 1 },
};

}

PlayPage::PlayPage(VSynthEmuProcessor& p) {
    for (const Pages::CardRows& rows : kCards) {
        cards_.push_back(Pages::gridCard(p.apvts, rows));
        addAndMakeVisible(*cards_.back());
    }
    pattern_ = std::make_unique<Card>("ARPEGGIO PATTERN");
    pattern_->setBody(std::make_unique<ArpPatternView>(p));
    addAndMakeVisible(*pattern_);
}

// Each column's cards down it, the arpeggio's pattern last in the first, given what is left
void PlayPage::resized() {
    auto r = getLocalBounds().reduced(Look::inset);
    const juce::Rectangle<int> columns[kColumns] = { r.removeFromLeft(kLeftW), r.withTrimmedLeft(Look::gap) };
    for (int col = 0; col < kColumns; ++col) {
        std::vector<Card*> down;
        for (size_t i = 0; i < cards_.size(); ++i)
            if (kCards[i].column == col) down.push_back(cards_[i].get());
        const bool first = col == 0;
        if (first) down.push_back(pattern_.get());
        Pages::stack(down, columns[col], first);
    }
}
