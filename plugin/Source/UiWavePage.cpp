// D's WAVE page (UiWavePage.h, v479)
#include "UiWavePage.h"
#include "PluginParamIds.h"
#include "PluginStartOffset.h"
#include "PluginWaveEdit.h"
#include "UiLook.h"
#include "UiPages.h"

namespace {

constexpr int kInfoH = 18, kViewH = 340, kOscsW = 150, kButtonW = 76;
constexpr int kOscsGroup = 1005;   // the OSC buttons' radio group, after the FX page's 1004
// The points' card: each row an OSC's own parameter, made for each OSC by index (Params::oscId), and its name
const std::array<std::pair<juce::String, const char*>, 8> kPointRows { {
    { Params::kStartOffset, "Start" }, { Params::kLoop, "Loop" }, { Params::kLoopMode, "Loop Mode" },
    { Params::kLoopStart, "Loop Start" }, { Params::kLoopEnd, "Loop End" }, { Params::kPlaybackMode, "Playback" },
    { Params::kEncodeType, "Encode" }, { Params::kEncodeDepth, "Depth" },   // v481: the events' depth, as the XT's screen
} };

// The title line's buttons in WavePage::Button's order, each what it does to the shown OSC's view
using Act = void (*)(WaveEditView&);
constexpr std::array<const char*, WavePage::kButtons> kButtonNames { "Revert", "Truncate", "Delete", "Add",
                                                                    "Next Event", "Prev Event" };
constexpr std::array<Act, WavePage::kButtons> kButtonActs {
    [](WaveEditView& v) { v.revert(); },
    [](WaveEditView& v) { v.truncate(); },
    [](WaveEditView& v) { v.deleteEvent(); },
    [](WaveEditView& v) { v.addEvent(); },
    [](WaveEditView& v) { v.toEvent(true); },
    [](WaveEditView& v) { v.toEvent(false); },
};

} // namespace

WavePage::WavePage(VSynthEmuProcessor& p) : processor_(p) {
    addAndMakeVisible(waveCard_);
    auto oscs = std::make_unique<Ui::Segmented>(juce::StringArray { "OSC 1", "OSC 2" }, kOscsGroup, Look::cards);
    oscs_ = &waveCard_.toTitle(std::move(oscs), kOscsW);
    oscs_->onChange = [this](int osc) { showOsc(osc); };
    for (size_t b = 0; b < kButtons; ++b) {   // from the right, after OSC 1 and OSC 2
        buttons_[b] = &waveCard_.toTitle(std::make_unique<juce::TextButton>(kButtonNames[b]), kButtonW);
        buttons_[b]->onClick = [this, act = kButtonActs[b]] { act(*views_[(size_t)osc_]); };
    }
    info_.setFont(Look::sans(11.0f));
    info_.setColour(juce::Label::textColourId, Look::label);
    waveCard_.addAndMakeVisible(info_);
    for (int osc = 0; osc < kOscs; ++osc) {
        views_[(size_t)osc] = std::make_unique<WaveEditView>(p, osc);
        waveCard_.addChildComponent(*views_[(size_t)osc]);
        ControlGrid::Rows rows;
        for (const auto& [row, name] : kPointRows) rows.push_back({ Params::oscId(osc, row), name });
        points_[(size_t)osc] = Pages::gridCard(p.apvts, "OSC " + juce::String(osc + 1) + " POINTS", rows);
        addChildComponent(*points_[(size_t)osc]);
    }
    showOsc(0);
}

void WavePage::showOsc(int osc) {
    osc_ = juce::jlimit(0, kOscs - 1, osc);
    oscs_->setIndex(osc_);
    for (int k = 0; k < kOscs; ++k) {
        views_[(size_t)k]->setVisible(k == osc_);
        points_[(size_t)k]->setVisible(k == osc_);
    }
    refresh();
}

void WavePage::refresh() {
    views_[(size_t)osc_]->refresh();
    const juce::String now = says();
    if (now != info_.getText()) info_.setText(now, juce::dontSendNotification);
}

// What the wave is: its name, its length in time and samples, and what the session keeps of it besides its file (d-8)
juce::String WavePage::says() const {
    const juce::String osc = "OSC " + juce::String(osc_ + 1);
    if (!processor_.hasFileLoaded(osc_)) return osc + ": no wave loaded (Load... on its panel)";
    const long frames = views_[(size_t)osc_]->frames();
    const juce::String length = startOffsetText((int)std::min<long>(frames, kStartOffsetMax));
    juce::String s = processor_.getLoadedFileName(osc_) + "    " + length + ", " + juce::String(frames) + " samples";
    const PluginWaveEdit edit = PluginWaveEdits::edit(processor_, osc_);
    juce::StringArray kept;   // what isn't its file's (d-8)
    if (edit.cut) kept.add("its cut (Revert for the file's)");
    if (edit.backward) kept.add("its own backward loop");
    if (!edit.events.empty()) kept.add(juce::String((int)edit.events.size()) + " event edits");   // v481
    if (!kept.isEmpty()) s << "    the session keeps " << kept.joinIntoString(", ");
    return s;
}

void WavePage::resized() {
    auto r = getLocalBounds().reduced(Look::inset);
    waveCard_.setBounds(r.removeFromTop(waveCard_.heightFor(kInfoH + Look::gap + kViewH)));
    auto body = waveCard_.getLocalBounds().reduced(Card::kPad).withTrimmedTop(Card::kTitleH + Card::kGap);
    info_.setBounds(body.removeFromTop(kInfoH));
    body.removeFromTop(Look::gap);
    for (auto& v : views_) v->setBounds(body);
    r.removeFromTop(Look::gap);
    for (auto& c : points_) Pages::stack({ c.get() }, r, false);
}
