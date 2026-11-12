// The Play tab: portamento, key assign, tuning and tempo.  Its page moved as it stood from buildLayout (v190), which
// calls it in its place; lr and rowOfLevel, the usual rows, were buildLayout's lambdas.  And the Arpeggio card (v190),
// under Keys: the arpeggio's ten settings, two to a row.
#include "PluginEditor.h"
#include <array>
#include <utility>

using Ui::Page;

Ui::Page::Row VSynthEmuEditor::lr(juce::Component& l, juce::Component& c, int w) {
    return Page::Row{ { Page::Item{ &l, Ui::labelW }, Page::Item{ &c, w } } };
}

Ui::Page::Row VSynthEmuEditor::rowOfLevel(int i) { return lr(levelLabels_[i], levelSliders_[i]); }

void VSynthEmuEditor::buildPlayPage() {
    // ── Play: portamento, key assign, tuning and tempo ──
    {
        Page& p = *pages_[5];
        Card porta { "Portamento" };
        porta.head = { { &portaBtn_, 54 } };
        porta.rows = { lr(*newLabel("Mode"), portaModeBox_), lr(*newLabel("Type"), portaTypeBox_),
                       lr(portaTimeLabel_, portaTimeSlider_), lr(portaSensLabel_, portaSensSlider_) };
        p.add(porta);
        Card keys { "Keys" };
        keys.head = { { &legatoBtn_, 74 } };
        keys.rows = { lr(*newLabel("Voices"), *monoSeg_, 140), lr(octLabel_, octSlider_) };
        p.add(keys);
        Card tune { "Tune" };
        tune.col = 1;
        for (int i : { 4, 5, 6, 7 }) tune.rows.push_back(rowOfLevel(i));
        p.add(tune);
        Card tempo { "Tempo" };
        tempo.col = 1;
        tempo.rows = { lr(clockLabel_, clockBox_), lr(patchTempoLabel_, patchTempoSlider_), lr(tripLabel_, tripSlider_) };
        p.add(tempo);
        addArpCards(p);
    }
}

// The arpeggio's controls, styled as the editor's others and each attached to its parameter (PluginArp.cpp), on the
// editor so the page adopts them; then its two cards, one a column under Keys and Tempo: each column 426 high of a
// page's 442 at the smallest size
void VSynthEmuEditor::addArpCards(Ui::Page& p) {
    using Apvts = juce::AudioProcessorValueTreeState;
    auto& apvts = processor_.apvts;
    const std::array<std::pair<juce::ToggleButton*, const char*>, 2> toggles { {
        { &arp_.on, "arpSwitch" }, { &arp_.hold, "arpHold" } } };
    for (auto [b, id] : toggles) {
        b->setColour(juce::ToggleButton::textColourId, Ui::text);
        b->setColour(juce::ToggleButton::tickColourId, Ui::accent);
        arp_.buttonAtts.push_back(std::make_unique<Apvts::ButtonAttachment>(apvts, id, *b));
        addAndMakeVisible(*b);
    }
    const std::array<std::pair<juce::ComboBox*, const char*>, 3> boxes { {
        { &arp_.motif, "arpMotif" }, { &arp_.grid, "arpGrid" }, { &arp_.resolution, "arpShuffleRes" } } };
    for (auto [box, id] : boxes) {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
            box->addItemList(choice->choices, 1);
        arp_.comboAtts.push_back(std::make_unique<Apvts::ComboBoxAttachment>(apvts, id, *box));
        addAndMakeVisible(*box);
    }
    const std::array<std::pair<juce::Slider*, const char*>, 5> sliders { {
        { &arp_.duration, "arpDuration" }, { &arp_.endPoint, "arpEndPoint" }, { &arp_.shuffle, "arpShuffleRate" },
        { &arp_.velocity, "arpKeyVelocity" }, { &arp_.octave, "arpOctave" } } };
    for (auto [s, id] : sliders) {
        Ui::styleValueSlider(*s, 56);   // "REAL" whole, as the Original Key's box
        arp_.sliderAtts.push_back(std::make_unique<Apvts::SliderAttachment>(apvts, id, *s));
        addAndMakeVisible(*s);
    }
    for (juce::Label* l : { &arp_.motifLabel, &arp_.octaveLabel, &arp_.velocityLabel, &arp_.durationLabel,
                            &arp_.gridLabel, &arp_.endPointLabel, &arp_.shuffleLabel }) {
        styleSmall(*l);
        addAndMakeVisible(*l);
    }
    arp_.pattern.referTo(apvts.state.getPropertyAsValue("arpPattern", nullptr));
    arp_.onPattern = [this] { repaintPages(); };

    Card arp { "Arpeggio" };
    arp.head = { { &arp_.on, 54 }, { &arp_.hold, 64 } };
    arp.rows = { lr(arp_.motifLabel, arp_.motif), lr(arp_.octaveLabel, arp_.octave),
                 lr(arp_.velocityLabel, arp_.velocity), lr(arp_.durationLabel, arp_.duration) };
    p.add(arp);
    Card pattern { "Arpeggio pattern" };
    pattern.col = 1;
    pattern.titleFn = [this] {   // why it's quiet, when that's why
        const bool silent = xtArpPatternSilent(processor_.arpPattern());
        return silent ? juce::String("Arpeggio pattern: empty") : juce::String("Arpeggio pattern");
    };
    const Page::Row shuffle { { { &arp_.shuffleLabel, Ui::labelW }, { &arp_.shuffle, 0 }, { &arp_.resolution, 84 } } };
    pattern.rows = { lr(arp_.gridLabel, arp_.grid), lr(arp_.endPointLabel, arp_.endPoint), shuffle };
    p.add(pattern);
}
