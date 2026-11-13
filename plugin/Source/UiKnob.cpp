// A knob made from a parameter's ID (UiKnob.h; UI step 1)
#include "UiKnob.h"
#include "UiLook.h"

namespace {
constexpr int kBigDial = 40;   // from this size a knob's name and value are 11, else 10 (UI_DESIGN.md, "The look")
}

Knob::Knob(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name, int dialSize)
    : attachment_(state, id, dial_), size_(dialSize) {
    const float text = dialSize >= kBigDial ? 11.0f : 10.0f;
    dial_.setRotaryParameters(Look::sweepStart, Look::sweepEnd, true);
    dial_.setTitle(name);
    Look::setTextLook(name_, Look::sans(text, "Semibold"), Look::label);
    Look::setTextLook(value_, Look::mono(text), Look::text);
    name_.setText(name, juce::dontSendNotification);
    dial_.onValueChange = [this] { showValue(); };
    showValue();
    addAndMakeVisible(dial_);
    addAndMakeVisible(name_);
    addAndMakeVisible(value_);
}

void Knob::setModulation(float lo, float hi) {
    auto& props = dial_.getProperties();
    if (juce::exactlyEqual(lo, hi)) {
        props.remove(Look::modLo);
        props.remove(Look::modHi);
    } else {
        props.set(Look::modLo, juce::jmin(lo, hi));
        props.set(Look::modHi, juce::jmax(lo, hi));
    }
    dial_.repaint();
}

void Knob::showValue() {
    value_.setText(dial_.getTextFromValue(dial_.getValue()), juce::dontSendNotification);
}

void Knob::resized() {
    auto r = getLocalBounds();
    dial_.setBounds(r.removeFromTop(size_).withSizeKeepingCentre(size_, size_));
    r.removeFromTop(1);
    name_.setBounds(r.removeFromTop(kTextH));
    value_.setBounds(r.removeFromTop(kTextH));
}
