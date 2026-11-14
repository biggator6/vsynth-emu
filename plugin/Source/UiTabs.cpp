// Names to choose from (UiTabs.h; UI step 3; the page bar's, from UI step 1)
#include "UiTabs.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {
constexpr int kMark = 2, kSpace = 4;   // the chosen one's underline; the space between names
}

Tabs::Tabs(juce::StringArray names, Style style)
    : style_(std::move(style)), names_(std::move(names)) {}

void Tabs::choose(int tab) {
    const int now = juce::jlimit(0, juce::jmax(0, names_.size() - 1), tab);
    if (now == chosen_) return;
    chosen_ = now;
    repaint();
}

std::vector<juce::Rectangle<int>> Tabs::tabs() const {
    std::vector<juce::Rectangle<int>> r;
    int x = style_.left;
    for (const juce::String& name : names_) {
        const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(style_.font, name)) + 2 * style_.pad;
        r.push_back({ x, 0, w, getHeight() });
        x += w + kSpace;
    }
    return r;
}

void Tabs::paintTab(juce::Graphics& g, int tab, juce::Rectangle<int> r) const {
    const bool open = tab == chosen_;
    const Ui::TextStyle look { style_.font, open ? Look::text : Look::label, juce::Justification::centred, false };
    Ui::drawText(g, names_[tab], r, look);
    if (open) Ui::fill(g, r.withTop(r.getBottom() - kMark), Look::accent);
}

void Tabs::paint(juce::Graphics& g) {
    const auto r = tabs();
    for (size_t i = 0; i < r.size(); ++i) paintTab(g, (int)i, r[i]);
}

void Tabs::mouseDown(const juce::MouseEvent& e) {
    const auto r = tabs();
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i].contains(e.getPosition())) {
            choose((int)i);
            if (onChoose) onChoose((int)i);
        }
}
