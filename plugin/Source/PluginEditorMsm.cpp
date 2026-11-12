// The MSM tab (v193): the Multi Step Modulator's Master Switch and Key Sync, and one track at a time, picked by the Track
// box as the Matrix's Controls are: the track's eight settings in a card, its 16 steps in two cards of 8, as the XT's
// knobs take them, 1–8 and 9–16 [MAN p. 83].  Every card that follows the track says which in its title (UI_DESIGN.md).
// A page is 466 high at the smallest size: the two columns are each 320 here.
#include "PluginEditor.h"
#include "PluginMsm.h"
#include "XtMsm.h"

using Ui::Page;

namespace {

// The XT's ID of track t's setting i: 0…7 its Switch … Destination, 8 on its steps
int trackId(int t, int i) { return kXtMsmFirstId + 2 + kXtMsmTrackIds * t + i; }

void itemsOf(juce::ComboBox& box, juce::AudioProcessorValueTreeState& apvts, int fwId) {
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(msmParamId(fwId))))
        box.addItemList(choice->choices, 1);
}

// Steps 1–8 (half 0) or 9–16, two to a row, in column 1
Page::Card stepsCard(MsmControls& m, int half) {
    std::vector<Page::Row> rows;
    for (size_t s = (size_t)(8 * half); s < (size_t)(8 * half + 8); s += 2) {
        const Page::Row row { { { &m.stepLabels[s], 24 }, { &m.steps[s], 0 },
                                { &m.stepLabels[s + 1], 24 }, { &m.steps[s + 1], 0 } } };
        rows.push_back(row);
    }
    Page::Card c { {}, {}, rows, 1 };
    c.titleFn = [&m, half] { return m.title(half ? " steps 9-16" : " steps 1-8"); };
    return c;
}

} // namespace

std::array<std::pair<juce::ComboBox*, int>, 4> MsmControls::trackBoxList() {
    return { { { &type, 2 }, { &grid, 3 }, { &direction, 5 }, { &dest, 7 } } };
}

std::vector<juce::Slider*> MsmControls::sliders() {
    std::vector<juce::Slider*> list { &endPoint, &zone };
    for (juce::Slider& s : steps) list.push_back(&s);
    return list;
}

std::vector<juce::Component*> MsmControls::controls() {
    std::vector<juce::Component*> list { &on, &trackOn, &loop, &keySync, &track, &type, &grid, &direction, &dest };
    for (juce::Slider* s : sliders()) list.push_back(s);
    return list;
}

std::vector<juce::Label*> MsmControls::labels() {
    std::vector<juce::Label*> list { &keySyncLabel, &trackLabel, &typeLabel, &gridLabel, &endPointLabel,
                                     &directionLabel, &zoneLabel, &destLabel };
    for (juce::Label& l : stepLabels) list.push_back(&l);
    return list;
}

void MsmControls::make(Apvts& apvts) {
    itemsOf(keySync, apvts, kXtMsmFirstId + 1);
    for (int t = 0; t < kTracks; ++t) track.addItem("Track " + juce::String(t + 1), t + 1);
    for (auto [box, i] : trackBoxList()) itemsOf(*box, apvts, trackId(0, i));
    for (juce::ToggleButton* b : { &on, &trackOn, &loop }) {
        b->setColour(juce::ToggleButton::textColourId, Ui::text);
        b->setColour(juce::ToggleButton::tickColourId, Ui::accent);
    }
    for (juce::Slider* s : sliders()) {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        Ui::styleValueSlider(*s, 40);                // "+63" whole
    }
    for (int i = 0; i < kSteps; ++i) stepLabels[(size_t)i].setText(juce::String(i + 1), juce::dontSendNotification);
}

void MsmControls::attachWhole(Apvts& apvts) {
    onAtt = std::make_unique<Apvts::ButtonAttachment>(apvts, msmParamId(kXtMsmFirstId), on);
    keySyncAtt = std::make_unique<Apvts::ComboBoxAttachment>(apvts, msmParamId(kXtMsmFirstId + 1), keySync);
}

void MsmControls::attachTrack(Apvts& apvts, int t) {
    shown = t;
    trackButtons.clear();
    trackBoxes.clear();
    trackSliders.clear();
    const auto id = [t](int i) { return msmParamId(trackId(t, i)); };
    trackButtons.push_back(std::make_unique<Apvts::ButtonAttachment>(apvts, id(0), trackOn));
    trackButtons.push_back(std::make_unique<Apvts::ButtonAttachment>(apvts, id(1), loop));
    for (auto [box, i] : trackBoxList())
        trackBoxes.push_back(std::make_unique<Apvts::ComboBoxAttachment>(apvts, id(i), *box));
    trackSliders.push_back(std::make_unique<Apvts::SliderAttachment>(apvts, id(4), endPoint));
    trackSliders.push_back(std::make_unique<Apvts::SliderAttachment>(apvts, id(6), zone));
    for (size_t s = 0; s < steps.size(); ++s)
        trackSliders.push_back(std::make_unique<Apvts::SliderAttachment>(apvts, id(8 + (int)s), steps[s]));
}

juce::String MsmControls::title(const char* what) const { return "Track " + juce::String(shown + 1) + what; }

// The controls styled and attached, on the editor so the page adopts them; the track box attaches the first track; then
// the cards: the whole and the track's settings in column 0, the steps in column 1
void VSynthEmuEditor::buildMsmPage() {
    auto& apvts = processor_.apvts;
    msm_.make(apvts);
    for (juce::Component* c : msm_.controls()) addAndMakeVisible(*c);
    for (juce::Label* l : msm_.labels()) {
        styleSmall(*l);
        addAndMakeVisible(*l);
    }
    msm_.attachWhole(apvts);
    msm_.track.onChange = [this] {
        const int t = juce::jlimit(0, MsmControls::kTracks - 1, msm_.track.getSelectedItemIndex());
        msm_.attachTrack(processor_.apvts, t);
        repaintPages();                              // the cards' titles
    };
    msm_.track.setSelectedId(1, juce::sendNotificationSync);

    Page& p = *pages_[6];
    p.add(Card { "Multi Step Modulator", { { &msm_.on, 54 } },
                 { lr(msm_.keySyncLabel, msm_.keySync), lr(msm_.trackLabel, msm_.track) } });
    Card& track = p.add(Card { {}, { { &msm_.trackOn, 54 }, { &msm_.loop, 64 } },
                               { lr(msm_.typeLabel, msm_.type), lr(msm_.gridLabel, msm_.grid),
                                 lr(msm_.endPointLabel, msm_.endPoint), lr(msm_.directionLabel, msm_.direction),
                                 lr(msm_.zoneLabel, msm_.zone), lr(msm_.destLabel, msm_.dest) } });
    track.titleFn = [this] { return msm_.title(""); };
    p.add(stepsCard(msm_, 0));
    p.add(stepsCard(msm_, 1));
}
