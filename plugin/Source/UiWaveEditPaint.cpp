// The WAVE page's wave drawn (UiWaveEdit.h; moved out of UiWaveEdit.cpp in v480 as it neared the kit's line limit):
// the backward loop's span and the Edit region's, the wave and the forward loop's span, the points' handles and the
// Start Offset's line, the time ruler
#include "UiWaveEdit.h"
#include "PluginStartOffset.h"
#include "UiLook.h"
#include "UiParts.h"
#include "UiWaveDisplay.h"

namespace {
constexpr float kHandleW = 9.0f;
constexpr float kFaint = 0.45f, kBwdAlpha = 0.08f, kEditAlpha = 0.07f;
constexpr int kRulerMarks = 6;
}

void WaveEditView::paint(juce::Graphics& g) {
    Ui::fillRounded(g, getLocalBounds().toFloat(), Look::field, 6.0f);
    if (frames_ <= 0) {
        Ui::drawText(g, "No wave loaded", getLocalBounds().reduced(10), { Look::sans(11.0f), Look::label });
        return;
    }
    makeColumns();
    const auto w = wave();
    const auto spanOf = [&](Point a, Point b) {
        const float x0 = xOf(at(a)), x1 = std::max(x0 + 1.0f, xOf(at(b)));
        return juce::Rectangle<float>::leftTopRightBottom(x0, w.getY(), x1, w.getBottom()).toNearestInt();
    };
    if (bwd_.backward)   // the backward loop's span, fainter than the forward's
        Ui::fill(g, spanOf(Point::bwdStart, Point::bwdEnd), Look::mod.withAlpha(kBwdAlpha));
    Ui::fill(g, spanOf(Point::editStart, Point::editEnd), Look::text.withAlpha(kEditAlpha));   // the Edit region
    const auto share = [&](long f) { return (xOf(f) - w.getX()) / w.getWidth(); };
    PluginWaveView shown;
    shown.columns = columns_;
    const WaveDisplay::Loop loop { watch_.value(kLoop) >= 0.5f, share(at(Point::fwdStart)), share(at(Point::fwdEnd)) };
    WaveDisplay::paintWave(g, w, shown, loop);
    paintPoints(g);
    paintRuler(g);
}

// The forward loop's ends above the wave, the backward's below, fainter, the Edit region's in the ruler; the Start
// Offset's line across the wave
void WaveEditView::paintPoints(juce::Graphics& g) const {
    const auto w = wave();
    for (const Point p : { Point::fwdStart, Point::fwdEnd, Point::bwdStart, Point::bwdEnd, Point::editStart,
                           Point::editEnd }) {
        const bool fwd = p == Point::fwdStart || p == Point::fwdEnd;
        const bool edit = p == Point::editStart || p == Point::editEnd;
        const auto colour = edit ? Look::text : fwd ? Look::mod : Look::mod.withMultipliedAlpha(kFaint);
        Ui::fillRounded(g, juce::Rectangle<float>(kHandleW, kStrip - 4.0f).withCentre(handle(p)), colour, 2.0f);
    }
    const float x = xOf(at(Point::start));
    WaveDisplay::paintStartLine(g, w, x);   // the rack's display draws the same line while it's held
    Ui::drawText(g, "START " + startOffsetText((int)at(Point::start)),
                 juce::Rectangle<float>(x + 4.0f, w.getY() + 2.0f, 140.0f, 14.0f).toNearestInt(),
                 { Look::sans(10.0f), Look::accent });
}

void WaveEditView::paintRuler(juce::Graphics& g) const {
    const auto w = wave();
    const auto ruler = getLocalBounds().toFloat().removeFromBottom(kRulerH);
    for (int k = 0; k < kRulerMarks; ++k) {
        const float x = w.getX() + w.getWidth() * (float)k / (float)kRulerMarks;
        Ui::fill(g, juce::Rectangle<float>(x, ruler.getY(), 1.0f, 4.0f).toNearestInt(), Look::dim);   // its mark
        Ui::drawText(g, startOffsetText((int)frameAt(x)),
                     juce::Rectangle<float>(x + 3.0f, ruler.getY(), 90.0f, ruler.getHeight()).toNearestInt(),
                     { Look::sans(10.0f), Look::muted });
    }
}
