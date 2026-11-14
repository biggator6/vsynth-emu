// D's page bar (UiPageBar.h; UI step 1)
#include "UiPageBar.h"
#include "UiBlocks.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {
const char* const kHint = "Click a block on the path or in the rack: its envelopes, LFO and routes open below";

juce::StringArray pageNames() {
    juce::StringArray names;
    for (const char* name : Blocks::kPages) names.add(name);
    return names;
}
}

PageBar::PageBar()
    : Tabs(pageNames(), { Look::sans(12.0f, "Bold", 0.1f), 16, 14 }) {
    setComponentID("pages");
}

void PageBar::paint(juce::Graphics& g) {
    g.fillAll(Look::ground);
    Ui::fill(g, getLocalBounds().removeFromBottom(1), Look::divider);
    Tabs::paint(g);
    const auto r = tabs();
    if (chosen() != Blocks::kSynthPage || r.empty()) return;
    Ui::drawText(g, kHint, getLocalBounds().withLeft(r.back().getRight() + 24).withTrimmedRight(16),
                 { Look::sans(11.0f), Look::muted, juce::Justification::centredRight });
}
