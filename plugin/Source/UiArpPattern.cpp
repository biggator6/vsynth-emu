// The arpeggio's pattern on the PLAY page (UiArpPattern.h; UI step 4)
#include "UiArpPattern.h"
#include "PluginArp.h"
#include "UiLook.h"
#include "UiParts.h"

namespace {

constexpr int kNameW = 44;
constexpr uint8_t kRest = 0, kTie = 1, kControl = 0x80, kNumber = 0x7f;   // a cell's REST and TIE; a line's bits
constexpr float kFullVelocity = 127.0f;

// A line's name: its note's, or CC and its controller's number
juce::String lineName(uint8_t line) {
    const int n = line & kNumber;
    return (line & kControl) != 0 ? "CC" + juce::String(n) : Ui::keyName(n);
}

}

ArpPatternView::ArpPatternView(VSynthEmuProcessor& p)
    : processor_(p), end_(p.apvts, { "arpEndPoint" }, [this] { repaint(); }) {
    pattern_.referTo(p.apvts.state.getPropertyAsValue("arpPattern", nullptr));
    pattern_.addListener(this);
}

ArpPatternView::~ArpPatternView() {
    pattern_.removeListener(this);
}

// The lines' names down the left; each line's cells across, a played step's cell filled by its velocity, a tie joining
// it to the step before; the steps past the End Point faint; and a line saying so when no step plays
void ArpPatternView::paint(juce::Graphics& g) {
    const XtArpPattern pattern = processor_.arpPattern();
    auto r = getLocalBounds();
    Ui::fillRounded(g, r.toFloat(), Look::field, 6.0f);
    r.reduce(6, 6);
    const auto names = r.removeFromLeft(kNameW);
    const float w = (float)r.getWidth() / (float)XtArpPattern::kSteps;
    const float h = (float)r.getHeight() / (float)XtArpPattern::kLines;
    const int end = juce::roundToInt(end_.value(0));
    for (int l = 0; l < XtArpPattern::kLines; ++l) {
        const float y = (float)r.getY() + (float)l * h;
        Ui::drawText(g, lineName(pattern.line[l]), { names.getX(), juce::roundToInt(y), kNameW, juce::roundToInt(h) },
                     { Look::mono(9.0f), Look::label });
        for (int s = 0; s < XtArpPattern::kSteps; ++s) {
            const uint8_t cell = pattern.cell[s][l];
            if (cell == kRest) continue;
            const float alpha = cell == kTie ? 0.3f : 0.3f + 0.7f * (float)(cell - 1) / kFullVelocity;
            const juce::Rectangle<float> at { (float)r.getX() + (float)s * w, y, w, h };
            Ui::fillRounded(g, cell == kTie ? at.withTrimmedTop(h * 0.35f).withTrimmedBottom(h * 0.35f)
                                            : at.reduced(1.0f, 1.0f),
                            Look::accent.withAlpha(s > end ? alpha * 0.35f : alpha), 2.0f);
        }
    }
    if (!xtArpPatternSilent(pattern)) return;
    Ui::drawText(g, "This pattern has no step to play: the arpeggio is silent", r,
                 { Look::sans(12.0f), Look::muted, juce::Justification::centred });
}
