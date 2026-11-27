// Upright bars across (UiBars.h; v453, moved out of UiMsm.cpp's track)
#include "UiBars.h"
#include "UiLook.h"

namespace {
constexpr int kNameH = 14, kGap = 4;
}

UprightBars::UprightBars(Controls::State& state, const ControlGrid::Rows& rows) {
    for (const auto& [id, name] : rows) {
        if (state.getParameter(id) == nullptr) continue;
        auto bar = std::make_unique<LearnSlider>(juce::Slider::LinearBarVertical, juce::Slider::NoTextBox);
        bar->setTitle(name);
        bar->setColour(juce::Slider::trackColourId, Look::mod);
        atts_.push_back(Controls::attached<Controls::State::SliderAttachment>(*bar, state, id));
        Controls::showEditorText(*bar, state, id);
        addAndMakeVisible(*bar);
        bars_.push_back(std::move(bar));
        auto label = std::make_unique<juce::Label>();
        Look::setTextLook(*label, Look::mono(10.0f), Look::label);
        label->setText(name, juce::dontSendNotification);
        addAndMakeVisible(*label);
        names_.push_back(std::move(label));
    }
}

// The bars across, each its name under it
void UprightBars::resized() {
    auto r = getLocalBounds();
    auto names = r.removeFromBottom(kNameH);
    const int n = juce::jmax(1, (int)bars_.size());
    const int w = (r.getWidth() - (n - 1) * kGap) / n;
    for (size_t i = 0; i < bars_.size(); ++i) {
        const int x = r.getX() + (int)i * (w + kGap);
        bars_[i]->setBounds(x, r.getY(), w, r.getHeight());
        names_[i]->setBounds(x, names.getY(), w, kNameH);
    }
}
