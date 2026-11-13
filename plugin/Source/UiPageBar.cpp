// D's page bar (UiPageBar.h; UI step 1)
#include "UiPageBar.h"
#include "UiBlocks.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {
constexpr int kLeft = 16, kTabPad = 14, kMark = 2;
const char* const kHint = "Click a block on the path or in the rack: its envelopes, LFO and routes open below";

juce::Font tabFont() { return Look::sans(12.0f, "Bold", 0.1f); }
}

void PageBar::setPage(int page) {
    page_ = juce::jlimit(0, (int)Blocks::kPages.size() - 1, page);
    repaint();
}

std::vector<juce::Rectangle<int>> PageBar::tabs() const {
    std::vector<juce::Rectangle<int>> r;
    int x = kLeft;
    for (const char* name : Blocks::kPages) {
        const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(tabFont(), name)) + 2 * kTabPad;
        r.push_back({ x, 0, w, getHeight() });
        x += w + 4;
    }
    return r;
}

void PageBar::paint(juce::Graphics& g) {
    g.fillAll(Look::ground);
    Ui::fill(g, getLocalBounds().removeFromBottom(1), Look::divider);
    const auto r = tabs();
    for (size_t i = 0; i < r.size(); ++i) {
        const bool open = (int)i == page_;
        Ui::drawText(g, Blocks::kPages[i], r[i],
                     { tabFont(), open ? Look::text : Look::label, juce::Justification::centred, false });
        if (open) Ui::fill(g, r[i].withTop(r[i].getBottom() - kMark), Look::accent);
    }
    if (page_ != Blocks::kSynthPage || r.empty()) return;
    Ui::drawText(g, kHint, getLocalBounds().withLeft(r.back().getRight() + 24).withTrimmedRight(kLeft),
                 { Look::sans(11.0f), Look::muted, juce::Justification::centredRight });
}

void PageBar::mouseDown(const juce::MouseEvent& e) {
    const auto r = tabs();
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i].contains(e.getPosition())) {
            setPage((int)i);
            if (onPage) onPage((int)i);
        }
}
