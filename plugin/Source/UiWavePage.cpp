// D's WAVE page (UiWavePage.h, v479)
#include "UiWavePage.h"
#include "PluginParamIds.h"
#include "PluginStartOffset.h"
#include "PluginWaveEdit.h"
#include "UiLook.h"
#include "UiPages.h"
#include <cmath>

namespace {

// The wave's line, the wave and the key (v484: the line taller for its buttons, the key's height out of the wave's)
constexpr int kInfoH = 22, kViewH = 312, kKeyH = 16, kOscsW = 150, kButtonW = 76;
constexpr int kZoomW = 30, kFitW = 44, kLineGap = 4, kShownW = 250;   // the line's buttons and its readout
constexpr float kSwatchW = 10.0f, kSwatchGap = 6.0f, kItemGap = 18.0f;   // the key's
constexpr double kZoomStep = 2.0;   // - widens the window twice over, + narrows it to half
constexpr int kOscsGroup = 1005;   // the OSC buttons' radio group, after the FX page's 1004
// The points' card: each row an OSC's own parameter, made for each OSC by index (Params::oscId), and its name
const std::array<std::pair<juce::String, const char*>, 8> kPointRows { {
    { Params::kStartOffset, "Start" }, { Params::kLoop, "Loop" }, { Params::kLoopMode, "Loop Mode" },
    { Params::kLoopStart, "Loop Start" }, { Params::kLoopEnd, "Loop End" }, { Params::kPlaybackMode, "Playback" },
    { Params::kEncodeType, "Encode" }, { Params::kEncodeDepth, "Depth" },   // v481: the events' depth, as the XT's screen
} };

// The page's buttons in WavePage::Button's order: each its name, its width, whether it sits at the right of the wave's
// line (the view's zoom, v484) or on the title line (from the right, after OSC 1 and OSC 2), and what it does to the
// shown OSC's view
struct ButtonDef {
    const char* name;
    int width;
    bool onLine;
    void (*act)(WaveEditView&);
};
constexpr std::array<ButtonDef, WavePage::kButtons> kButtonDefs { {
    { "Revert", kButtonW, false, [](WaveEditView& v) { v.revert(); } },
    { "Truncate", kButtonW, false, [](WaveEditView& v) { v.truncate(); } },
    { "Delete", kButtonW, false, [](WaveEditView& v) { v.deleteEvent(); } },
    { "Add", kButtonW, false, [](WaveEditView& v) { v.addEvent(); } },
    { "Next Event", kButtonW, false, [](WaveEditView& v) { v.toEvent(true); } },
    { "Prev Event", kButtonW, false, [](WaveEditView& v) { v.toEvent(false); } },
    { "-", kZoomW, true, [](WaveEditView& v) { v.zoomBy(kZoomStep); } },
    { "+", kZoomW, true, [](WaveEditView& v) { v.zoomBy(1.0 / kZoomStep); } },
    { "Fit", kFitW, true, [](WaveEditView& v) { v.fit(); } },
} };

juce::Font keyFont() {   // the key's names'
    return Look::sans(11.0f);
}

// A line's text set only when it changed: the timer's, each tick
void setText(juce::Label& l, const juce::String& now) {
    if (now != l.getText()) l.setText(now, juce::dontSendNotification);
}

} // namespace

// Left to right, each its swatch then its name, on whole pixels
std::vector<WaveKey::Item> WaveKey::items() const {
    const float mid = std::floor((float)getHeight() / 2.0f);
    std::vector<Item> out;
    float x = 0.0f;
    for (size_t m = 0; m < (size_t)WaveEditView::Mark::kMarks; ++m) {
        const auto mark = (WaveEditView::Mark)m;
        const juce::Point<float> swatch { x + kSwatchW / 2.0f, mid };
        x += kSwatchW + kSwatchGap;
        const float w = std::ceil(juce::GlyphArrangement::getStringWidth(keyFont(), WaveEditView::nameOf(mark)));
        out.push_back({ mark, swatch, { x, 0.0f, w + 2.0f, (float)getHeight() } });
        x += w + 2.0f + kItemGap;
    }
    return out;
}

void WaveKey::paint(juce::Graphics& g) {
    for (const Item& it : items()) {
        WaveEditView::paintMark(g, it.mark, it.swatch);
        Ui::drawText(g, WaveEditView::nameOf(it.mark), it.name.toNearestInt(), { keyFont(), Look::label });
    }
}

WavePage::WavePage(VSynthEmuProcessor& p) : processor_(p) {
    addAndMakeVisible(waveCard_);
    auto oscs = std::make_unique<Ui::Segmented>(juce::StringArray { "OSC 1", "OSC 2" }, kOscsGroup, Look::cards);
    oscs_ = &waveCard_.toTitle(std::move(oscs), kOscsW);
    oscs_->onChange = [this](int osc) { showOsc(osc); };
    for (size_t b = 0; b < kButtons; ++b) {
        const ButtonDef& d = kButtonDefs[b];
        auto made = std::make_unique<juce::TextButton>(d.name);
        made->onClick = [this, act = d.act] {
            act(*views_[(size_t)osc_]);
            refresh();   // the line says the window at once (v484)
        };
        buttons_[b] = made.get();
        if (!d.onLine) {
            waveCard_.toTitle(std::move(made), d.width);
            continue;
        }
        waveCard_.addAndMakeVisible(*made);
        onLine_[b] = std::move(made);
    }
    for (juce::Label* l : { &info_, &shown_ }) {
        l->setFont(Look::sans(11.0f));
        l->setColour(juce::Label::textColourId, Look::label);
        waveCard_.addAndMakeVisible(*l);
    }
    shown_.setJustificationType(juce::Justification::centredRight);
    waveCard_.addAndMakeVisible(key_);
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
    setText(info_, says());
    setText(shown_, views_[(size_t)osc_]->shown());
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
    waveCard_.setBounds(r.removeFromTop(waveCard_.heightFor(kInfoH + kViewH + kKeyH + 2 * Look::gap)));
    auto body = waveCard_.getLocalBounds().reduced(Card::kPad).withTrimmedTop(Card::kTitleH + Card::kGap);
    auto line = body.removeFromTop(kInfoH);
    for (size_t b = kButtons; b-- > 0;) {   // the zoom's from the right: -, + and Fit as the table has them
        if (!onLine_[b]) continue;
        onLine_[b]->setBounds(line.removeFromRight(kButtonDefs[b].width));
        line.removeFromRight(kLineGap);
    }
    shown_.setBounds(line.removeFromRight(kShownW));
    info_.setBounds(line);
    body.removeFromTop(Look::gap);
    key_.setBounds(body.removeFromBottom(kKeyH));
    body.removeFromBottom(Look::gap);
    for (auto& v : views_) v->setBounds(body);
    r.removeFromTop(Look::gap);
    for (auto& c : points_) Pages::stack({ c.get() }, r, false);
}
