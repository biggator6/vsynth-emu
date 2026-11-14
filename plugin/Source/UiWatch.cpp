// Parameters a drawing follows (UiWatch.h; UI step 3)
#include "UiWatch.h"
#include <algorithm>

Watch::Watch(juce::AudioProcessorValueTreeState& state, const juce::StringArray& ids, std::function<void()> moved)
    : values_((size_t)ids.size(), 0.0f), moved_(std::move(moved)) {
    for (int i = 0; i < ids.size(); ++i) {
        auto* p = state.getParameter(ids[i]);
        params_.push_back(p);
        if (p == nullptr) {
            atts_.push_back(nullptr);
            continue;
        }
        const auto at = (size_t)i;
        atts_.push_back(std::make_unique<juce::ParameterAttachment>(*p, [this, at](float v) {
            values_[at] = v;
            if (moved_) moved_();
        }));
        atts_.back()->sendInitialUpdate();
    }
}

bool Watch::complete() const {
    return std::all_of(params_.begin(), params_.end(), [](auto* p) { return p != nullptr; });
}

float Watch::value(size_t i) const {
    return i < values_.size() ? values_[i] : 0.0f;
}

float Watch::proportion(size_t i) const {
    return i < params_.size() && params_[i] != nullptr ? params_[i]->convertTo0to1(values_[i]) : 0.0f;
}

void Watch::begin(size_t i) {
    if (i < atts_.size() && atts_[i]) atts_[i]->beginGesture();
}

void Watch::drag(size_t i, float proportion) {
    if (i >= atts_.size() || !atts_[i]) return;
    const float to = params_[i]->convertFrom0to1(juce::jlimit(0.0f, 1.0f, proportion));
    atts_[i]->setValueAsPartOfGesture(to);
}

void Watch::end(size_t i) {
    if (i < atts_.size() && atts_[i]) atts_[i]->endGesture();
}
