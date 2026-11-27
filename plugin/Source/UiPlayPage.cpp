// D's PLAY page (UiPlayPage.h; UI step 4; a mode's, v453)
#include "UiPlayPage.h"
#include "UiLook.h"

namespace {

constexpr int kLeftW = 640;   // the arpeggio's column; the other cards take the rest
constexpr int kColumns = 2;

// Each card: its title, its controls (a parameter's ID and its name) and its column, the arpeggio's first
const Pages::CardRows kCards[] = {
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

// The V-Synth's: its pattern edited into the session (v468)
PlayPage::Mode vsynthPlay(VSynthEmuProcessor& p) {
    return { { std::begin(kCards), std::end(kCards) }, Pages::rowAsId,
             { [&p] { return p.arpPattern(); }, "arpPattern", "arpEndPoint",
               [&p](const XtArpPattern& edited) { p.setArpPattern(edited); } },
             {} };
}

}

PlayPage::PlayPage(VSynthEmuProcessor& p) : PlayPage(p, vsynthPlay(p)) {}

PlayPage::PlayPage(VSynthEmuProcessor& p, Mode mode) {
    for (const Pages::CardRows& rows : mode.cards) {
        cards_.push_back(Pages::gridCard(p.apvts, rows, mode.idOf));
        columns_.push_back(rows.column);
        addAndMakeVisible(*cards_.back());
    }
    pattern_ = std::make_unique<Card>("ARPEGGIO PATTERN");
    if (mode.pattern.set) editor_ = &pattern_->setBody(std::make_unique<ArpPatternEditor>(p, mode.pattern));
    else pattern_->setBody(std::make_unique<ArpPatternView>(p, mode.pattern));   // drawn only (the VC-2's)
    addAndMakeVisible(*pattern_);
    if (mode.last) {
        last_ = mode.last();
        addAndMakeVisible(*last_);
    }
}

// Each column's cards down it, the arpeggio's pattern last in the first, and a mode's last card in the second, each
// given what is left
void PlayPage::resized() {
    auto r = getLocalBounds().reduced(Look::inset);
    const juce::Rectangle<int> columns[kColumns] = { r.removeFromLeft(kLeftW), r.withTrimmedLeft(Look::gap) };
    for (int col = 0; col < kColumns; ++col) {
        std::vector<Card*> down;
        for (size_t i = 0; i < cards_.size(); ++i)
            if (columns_[i] == col) down.push_back(cards_[i].get());
        const bool first = col == 0;
        if (first) down.push_back(pattern_.get());
        else if (last_) down.push_back(last_.get());
        Pages::stack(down, columns[col], first || last_ != nullptr);
    }
}
