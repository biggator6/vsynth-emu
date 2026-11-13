// D's Time Trip column (UiTripColumn.h; UI step 2)
#include "UiTripColumn.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {
constexpr int kTitleH = 22, kSwitchH = 24, kFollowW = 30, kGrid = 3;
constexpr int kFollowGroup = 2001;   // the OSC 1/OSC 2 pair's radio group
}

void TripPad::paint(juce::Graphics& g) {
    const auto r = getLocalBounds().toFloat();
    Ui::fillRounded(g, r, Look::field, 6.0f);
    g.setColour(Look::divider);
    for (int i = 1; i < kGrid; ++i) {
        g.drawVerticalLine(juce::roundToInt(r.getWidth() * (float)i / (float)kGrid), r.getY(), r.getBottom());
        g.drawHorizontalLine(juce::roundToInt(r.getHeight() * (float)i / (float)kGrid), r.getX(), r.getRight());
    }
    const float at = (float)valueToProportionOfLength(getValue());
    const float x = r.getX() + 8.0f + at * (r.getWidth() - 16.0f);
    g.setColour(Look::mod.withAlpha(at > 0.0f ? 1.0f : 0.35f));
    g.drawLine(x, r.getY() + 6.0f, x, r.getBottom() - 6.0f, 2.0f);
    g.fillEllipse(juce::Rectangle<float>(12.0f, 12.0f).withCentre({ x, r.getCentreY() }));
    Ui::drawText(g, getTextFromValue(getValue()), getLocalBounds().reduced(8, 6).removeFromTop(14),
                 { Look::mono(10.0f), Look::muted });
}

TripColumn::TripColumn(VSynthEmuProcessor& p) {
    padAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, Params::kTripPad, pad_);
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
        robot_[i].setButtonText("Robot");
        Controls::toggle(trip_[i], p.apvts, Params::oscId(osc, Params::kTripSw), attached_);
        Controls::toggle(robot_[i], p.apvts, Params::oscId(osc, Params::kRobot), attached_);
        addAndMakeVisible(follow_[i]);
        addChildComponent(trip_[i]);
        addChildComponent(robot_[i]);
    }
    follow(0);
}

void TripColumn::follow(int osc) {
    for (int k = 0; k < kOscs; ++k) {
        const auto i = (size_t)k;
        follow_[i].setToggleState(k == osc, juce::dontSendNotification);
        trip_[i].setVisible(k == osc);
        robot_[i].setVisible(k == osc);
    }
}

void TripColumn::resized() {
    auto r = getLocalBounds().reduced(10);
    auto title = r.removeFromTop(kTitleH);
    for (int osc = kOscs - 1; osc >= 0; --osc) follow_[(size_t)osc].setBounds(title.removeFromRight(kFollowW + 12));
    name_.setBounds(title);
    auto switches = r.removeFromBottom(kSwitchH);
    r.removeFromBottom(8);
    r.removeFromTop(8);
    pad_.setBounds(r);
    for (int osc = 0; osc < kOscs; ++osc) {
        auto row = switches;
        trip_[(size_t)osc].setBounds(row.removeFromLeft(row.getWidth() / 2));
        robot_[(size_t)osc].setBounds(row);
    }
}

void TripColumn::paint(juce::Graphics& g) {
    Look::paintPanel(g, getLocalBounds(), false);
}
