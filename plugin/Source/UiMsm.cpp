// The Multi Step Modulator on the MODULATION page (UiMsm.h; UI step 4)
#include "UiMsm.h"
#include "PluginMsm.h"
#include "UiLook.h"
#include "XtMsm.h"

namespace {

constexpr int kTopH = Field::kHeight, kSwitchW = 84, kKeySyncW = 120, kNumberH = 14, kStepGap = 4;
constexpr int kEndPoint = 4;   // the End Point among a track's settings
// A track's eight settings' names, in the XT's order of their IDs (msmTrackFwId's 0…7)
const char* const kSettingNames[MsmTrack::kSettings] = { "On", "Loop", "Type", "Grid", "End Point", "Direction", "Zone",
                                                         "Destination" };

ControlGrid::Rows settingRows(int track) {
    ControlGrid::Rows rows;
    for (int i = 0; i < MsmTrack::kSettings; ++i)
        rows.push_back({ msmParamId(msmTrackFwId(track, i)), kSettingNames[i] });
    return rows;
}

juce::StringArray trackNames() {
    juce::StringArray names;
    for (int t = 0; t < MsmView::kTracks; ++t) names.add("TRACK " + juce::String(t + 1));
    return names;
}

}

MsmTrack::MsmTrack(Controls::State& state, int track)
    : settings_(state, settingRows(track)),
      end_(state, { msmParamId(msmTrackFwId(track, kEndPoint)) }, [this] { dimPast(); }) {
    addAndMakeVisible(settings_);
    for (int s = 0; s < kSteps; ++s) {
        const juce::String id = msmParamId(msmTrackFwId(track, kSettings + s));
        if (state.getParameter(id) == nullptr) continue;
        auto bar = std::make_unique<juce::Slider>(juce::Slider::LinearBarVertical, juce::Slider::NoTextBox);
        bar->setTitle("Step " + juce::String(s + 1));
        bar->setColour(juce::Slider::trackColourId, Look::mod);
        stepAtts_.push_back(std::make_unique<Controls::State::SliderAttachment>(state, id, *bar));
        addAndMakeVisible(*bar);
        steps_.push_back(std::move(bar));
        auto number = std::make_unique<juce::Label>();
        Look::setTextLook(*number, Look::mono(10.0f), Look::label);
        number->setText(juce::String(s + 1), juce::dontSendNotification);
        addAndMakeVisible(*number);
        numbers_.push_back(std::move(number));
    }
    dimPast();
}

// The steps after the End Point, which the track doesn't reach, drawn faint
void MsmTrack::dimPast() {
    const int end = juce::roundToInt(end_.value(0));
    for (size_t s = 0; s < steps_.size(); ++s) steps_[s]->setAlpha((int)s <= end ? 1.0f : 0.35f);
}

// The settings across the top, then the steps across the rest, each its number under it
void MsmTrack::resized() {
    auto r = getLocalBounds();
    settings_.setBounds(r.removeFromTop(settings_.heightFor(r.getWidth())));
    r.removeFromTop(ControlGrid::kGap * 2);
    auto numbers = r.removeFromBottom(kNumberH);
    const int n = juce::jmax(1, (int)steps_.size());
    const int w = (r.getWidth() - (n - 1) * kStepGap) / n;
    for (size_t s = 0; s < steps_.size(); ++s) {
        const int x = r.getX() + (int)s * (w + kStepGap);
        steps_[s]->setBounds(x, r.getY(), w, r.getHeight());
        numbers_[s]->setBounds(x, numbers.getY(), w, kNumberH);
    }
}

MsmView::MsmView(Controls::State& state)
    : tabs_(trackNames(), { Look::sans(11.0f, "Bold", 0.06f), 0, 10 }),
      keySync_(state, msmParamId(kXtMsmFirstId + 1), "Key Sync") {
    Controls::toggle(on_, state, msmParamId(kXtMsmFirstId), attached_);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &tabs_, &on_, &keySync_ }) addAndMakeVisible(c);
    for (int t = 0; t < kTracks; ++t) {
        tracks_[(size_t)t] = std::make_unique<MsmTrack>(state, t);
        addChildComponent(*tracks_[(size_t)t]);
    }
    tabs_.onChoose = [this](int t) { showTrack(t); };
    showTrack(0);
}

void MsmView::showTrack(int t) {
    tabs_.choose(t);
    for (int i = 0; i < kTracks; ++i) tracks_[(size_t)i]->setVisible(i == tabs_.chosen());
}

// The tracks' tabs, the switch and Key Sync along the top; the shown track under them
void MsmView::resized() {
    auto r = getLocalBounds();
    auto top = r.removeFromTop(kTopH);
    keySync_.setBounds(top.removeFromRight(kKeySyncW));
    top.removeFromRight(ControlGrid::kGap);
    on_.setBounds(top.removeFromRight(kSwitchW).withTrimmedTop(kTopH - 24));
    tabs_.setBounds(top.withTrimmedTop(kTopH - 24));
    r.removeFromTop(ControlGrid::kGap);
    for (const auto& t : tracks_) t->setBounds(r);
}
