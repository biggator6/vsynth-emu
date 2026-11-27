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
const std::array<std::pair<juce::String, const char*>, 6> kPointRows { {
    { Params::kStartOffset, "Start" }, { Params::kLoop, "Loop" }, { Params::kLoopMode, "Loop Mode" },
    { Params::kLoopStart, "Loop Start" }, { Params::kLoopEnd, "Loop End" }, { Params::kPlaybackMode, "Playback" },
} };

} // namespace

WavePage::WavePage(VSynthEmuProcessor& p) : processor_(p) {
    addAndMakeVisible(waveCard_);
    auto oscs = std::make_unique<Ui::Segmented>(juce::StringArray { "OSC 1", "OSC 2" }, kOscsGroup, Look::cards);
    oscs_ = &waveCard_.toTitle(std::move(oscs), kOscsW);
    oscs_->onChange = [this](int osc) { showOsc(osc); };
    revert_ = &waveCard_.toTitle(std::make_unique<juce::TextButton>("Revert"), kButtonW);
    truncate_ = &waveCard_.toTitle(std::make_unique<juce::TextButton>("Truncate"), kButtonW);
    truncate_->onClick = [this] { views_[(size_t)osc_]->truncate(); };
    revert_->onClick = [this] { views_[(size_t)osc_]->revert(); };
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
