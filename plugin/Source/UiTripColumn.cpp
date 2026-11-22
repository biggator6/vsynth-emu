// D's Time Trip column (UiTripColumn.h; UI step 2, its pad and switches from v202; a mode's, v453)
#include "UiTripColumn.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {
constexpr int kTitleH = 22, kSwitchH = 24, kFollowW = 30, kRowGap = 6;
constexpr int kFollowGroup = 2001;   // the OSC 1/OSC 2 pair's radio group
}

TripColumn::Ids TripColumn::vsynth() {
    return { [](int osc) { return Params::oscId(osc, Params::kTripSw); },
             [](int osc) { return Params::oscId(osc, Params::kBeatKeep); }, {}, {}, {} };
}

TripColumn::TripColumn(VSynthEmuProcessor& p, const Ids& ids) : processor_(p), pad_(p, ids.assigns) {
    addAndMakeVisible(pad_);
    Look::panelName(name_, "TIME TRIP");
    addAndMakeVisible(name_);
    for (int osc = 0; osc < kOscs; ++osc) {
        const auto i = (size_t)osc;
        follow_[i].setButtonText("OSC " + juce::String(osc + 1));
        follow_[i].setClickingTogglesState(true);
        follow_[i].setRadioGroupId(kFollowGroup);
        follow_[i].onClick = [this, osc] { follow(osc); };
        trip_[i].setButtonText("Trip");
        beat_[i].setButtonText("Beat Keep");
        Controls::toggle(trip_[i], p.apvts, ids.trip(osc), attached_);
        Controls::toggle(beat_[i], p.apvts, ids.beat(osc), attached_);
        addAndMakeVisible(follow_[i]);
        addChildComponent(trip_[i]);
        addChildComponent(beat_[i]);
    }
    if (ids.holdId.isEmpty()) {
        holdOwn();
    } else {   // the VC-2's: Pad Hold and Pad Mode are the patch's (d-10)
        Controls::toggle(hold_, p.apvts, ids.holdId, attached_);
        hold_.onStateChange = [this] { pad_.setHold(hold_.getToggleState()); };
        pad_.setHold(hold_.getToggleState());
        if (Controls::box(mode_, p.apvts, ids.modeId, attached_)) addAndMakeVisible(mode_);
    }
    addAndMakeVisible(hold_);
    follow(0);
}

// Hold: the editor's, not a parameter (the XT keeps it in the patch, which the plugin doesn't read); the session keeps it
void TripColumn::holdOwn() {
    hold_.getProperties().set(Look::editorsOwn, true);
    hold_.setToggleState(processor_.apvts.state.getProperty(kPadHold, false), juce::dontSendNotification);
    pad_.setHold(hold_.getToggleState());
    hold_.onClick = [this] {
        processor_.apvts.state.setProperty(kPadHold, hold_.getToggleState(), nullptr);
        pad_.setHold(hold_.getToggleState());
    };
}

void TripColumn::follow(int osc) {
    for (int k = 0; k < kOscs; ++k) {
        const auto i = (size_t)k;
        follow_[i].setToggleState(k == osc, juce::dontSendNotification);
        trip_[i].setVisible(k == osc);
        beat_[i].setVisible(k == osc);
    }
}

// The title and the OSC it follows; the pad; the VC-2's Pad Mode; Pad Hold; then that OSC's Time Trip switch and Beat
// Keep, a row each (side by side, Beat Keep's name was cut)
void TripColumn::resized() {
    auto r = getLocalBounds().reduced(10);
    auto title = r.removeFromTop(kTitleH);
    for (int osc = kOscs - 1; osc >= 0; --osc) follow_[(size_t)osc].setBounds(title.removeFromRight(kFollowW + 12));
    name_.setBounds(title);
    const auto beat = r.removeFromBottom(kSwitchH);
    const auto trip = r.removeFromBottom(kSwitchH);
    r.removeFromBottom(kRowGap);
    hold_.setBounds(r.removeFromBottom(kSwitchH));
    r.removeFromBottom(kRowGap);
    if (mode_.isVisible()) {   // the VC-2's Pad Mode, a row of its own over Pad Hold (TIME-TRIP won't fit beside it)
        mode_.setBounds(r.removeFromBottom(kSwitchH));
        r.removeFromBottom(kRowGap);
    }
    r.removeFromTop(8);
    pad_.setBounds(r);
    for (int osc = 0; osc < kOscs; ++osc) {
        trip_[(size_t)osc].setBounds(trip);
        beat_[(size_t)osc].setBounds(beat);
    }
}

void TripColumn::paint(juce::Graphics& g) {
    Look::paintPanel(g, getLocalBounds(), false);
}
