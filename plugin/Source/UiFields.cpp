// A bar and a field, each with its name (UiFields.h; UI step 3)
#include "UiFields.h"
#include "UiLook.h"

namespace {
constexpr int kNameH = 13, kBarH = 18, kBoxH = 22;
}

Bar::Bar(Controls::State& state, const juce::String& id, const juce::String& name, juce::Colour fill)
    : attachment_(state, id, bar_) {
    bar_.setTitle(name);
    bar_.setColour(juce::Slider::trackColourId, fill);
    bar_.setSliderSnapsToMousePosition(false);
    Look::fieldName(name_, name);
    addAndMakeVisible(bar_);
    addAndMakeVisible(name_);
}

void Bar::setNameWidth(int w) {
    nameW_ = w;
    resized();
}

void Bar::resized() {
    auto r = getLocalBounds();
    if (nameW_ > 0) {
        name_.setBounds(r.removeFromLeft(nameW_));
        bar_.setBounds(r.withSizeKeepingCentre(r.getWidth(), juce::jmin(kBarH, r.getHeight())));
        return;
    }
    name_.setBounds(r.removeFromTop(kNameH));
    r.removeFromTop(2);
    bar_.setBounds(r.removeFromTop(kBarH));
}

Field::Field(Controls::State& state, const juce::String& id, const juce::String& name) {
    Look::fieldName(name_, name);
    if (Controls::box(box_, state, id, attached_)) addAndMakeVisible(box_);
    addAndMakeVisible(name_);
}

void Field::resized() {
    auto r = getLocalBounds();
    name_.setBounds(r.removeFromTop(kNameH));
    r.removeFromTop(1);
    box_.setBounds(r.removeFromTop(kBoxH));
}
