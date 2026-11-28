// The WAVE page's wave drawn (UiWaveEdit.h; moved out of UiWaveEdit.cpp in v480 as it neared the kit's line limit):
// the backward loop's span and the Edit region's, the wave and the forward loop's span, the points' handles and the
// Start Offset's line, the time ruler
#include "UiWaveEdit.h"
#include "PluginStartOffset.h"
#include "UiLook.h"
#include "UiParts.h"
#include "UiWaveDisplay.h"
#include <array>

namespace {
constexpr float kHandleW = 9.0f, kHandleRadius = 2.0f, kLineW = 2.0f;
constexpr float kFaint = 0.45f, kBwdAlpha = 0.08f, kEditAlpha = 0.07f;
constexpr int kRulerMarks = 6;
}

// v484: each mark's colour, the one place the view's drawing and the page's key take it from
juce::Colour WaveEditView::colourOf(Mark m) {
    switch (m) {
        case Mark::loop: return Look::mod;
        case Mark::backward: return Look::mod.withMultipliedAlpha(kFaint);
        case Mark::edit: return Look::text;
        case Mark::start: return Look::accent;   // as WaveDisplay::paintStartLine draws it, on the rack's display too
        case Mark::event: return Look::text;
        case Mark::current: return Look::label;
        case Mark::playing: return Look::ready;   // v485: green, nothing else on the wave is
        case Mark::kMarks: break;
    }
    return Look::text;
}

WaveEditView::Mark WaveEditView::markOf(Point p) {
    switch (p) {
        case Point::fwdStart:
        case Point::fwdEnd: return Mark::loop;
        case Point::bwdStart:
        case Point::bwdEnd: return Mark::backward;
        case Point::editStart:
        case Point::editEnd: return Mark::edit;
        case Point::current: return Mark::current;
        case Point::start:
        case Point::none: break;
    }
    return Mark::start;
}

const char* WaveEditView::nameOf(Mark m) {
    static constexpr std::array<const char*, (size_t)Mark::kMarks> kNames {
        "Loop Start, End", "Backward loop", "Edit region: Truncate keeps it", "Start Offset", "Event", "Current",
        "Playing"
    };
    return m == Mark::kMarks ? "" : kNames[(size_t)m];
}

// A handle a rounded bar the strip's height, an event the XT's ▼, Start and Current a line
void WaveEditView::paintMark(juce::Graphics& g, Mark m, juce::Point<float> centre) {
    const juce::Colour c = colourOf(m);
    if (m == Mark::loop || m == Mark::backward || m == Mark::edit) {
        Ui::fillRounded(g, juce::Rectangle<float>(kHandleW, kStrip - 4.0f).withCentre(centre), c, kHandleRadius);
        return;
    }
    if (m == Mark::event) {
        juce::Path mark;
        const float top = centre.y - kMarkH / 2.0f;
        mark.addTriangle(centre.x - kMarkW / 2.0f, top, centre.x + kMarkW / 2.0f, top, centre.x, top + kMarkH);
        Ui::fillPath(g, mark, c);
        return;
    }
    const bool centred = m == Mark::start || m == Mark::playing;   // on its frame; Current from it, as on the wave
    const float w = centred ? kLineW : 1.0f;
    const float left = centred ? centre.x - w / 2.0f : centre.x;
    const float h = kStrip - 4.0f;
    Ui::fill(g, juce::Rectangle<float>(left, centre.y - h / 2.0f, w, h).toNearestInt(), c);
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
        Ui::fill(g, spanOf(Point::bwdStart, Point::bwdEnd), colourOf(Mark::backward).withAlpha(kBwdAlpha));
    Ui::fill(g, spanOf(Point::editStart, Point::editEnd), colourOf(Mark::edit).withAlpha(kEditAlpha));
    const auto share = [&](long f) { return (xOf(f) - w.getX()) / w.getWidth(); };
    PluginWaveView shown;
    shown.columns = columns_;
    const WaveDisplay::Loop loop { watch_.value(kLoop) >= 0.5f, share(at(Point::fwdStart)), share(at(Point::fwdEnd)) };
    WaveDisplay::paintWave(g, w, shown, loop);
    paintEvents(g);   // v481: the events' marks and Current (UiWaveEditEvents.cpp)
    if (playing_ >= 0 && playing_ < frames_) {   // v485: the playhead, the wave's height
        const juce::Rectangle<float> line(xOf(playing_) - kLineW / 2.0f, w.getY(), kLineW, w.getHeight());
        Ui::fill(g, line.toNearestInt(), colourOf(Mark::playing));
    }
    paintPoints(g);
    paintRuler(g);
}

// The forward loop's ends above the wave, the backward's below, fainter, the Edit region's in the ruler; the Start
// Offset's line across the wave
void WaveEditView::paintPoints(juce::Graphics& g) const {
    const auto w = wave();
    for (const Point p : { Point::fwdStart, Point::fwdEnd, Point::bwdStart, Point::bwdEnd, Point::editStart,
                           Point::editEnd })
        paintMark(g, markOf(p), handle(p));
    const float x = xOf(at(Point::start));
    WaveDisplay::paintStartLine(g, w, x);   // the rack's display draws the same line while it's held
    Ui::drawText(g, "START " + startOffsetText((int)at(Point::start)),
                 juce::Rectangle<float>(x + 4.0f, w.getY() + 2.0f, 140.0f, 14.0f).toNearestInt(),
                 { Look::sans(10.0f), colourOf(Mark::start) });
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
