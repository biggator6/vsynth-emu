// VSynthEmuEditorTest's checks of the WAVE page's zoom and key (EditorTest.h; v484, the user's word of 2026-11-28):
// - Fit shows the whole wave and the line says so; + narrows the window to half about its middle, the middle's frame
//   kept, and - widens it back; + no closer than a frame a column; the line says the window each time;
// - the key names every mark, each name whole, and each mark is drawn in the colour the view draws it with, and the
//   forward loop's handles on the wave in the key's colour (the view drawn into an image); the marks to tell apart
//   each their own colour.
// The wheel zooms through the same routine as the buttons (WaveEditView::zoomAbout; wavePageChecks' wheel check).
#include "WavePageTest.h"
#include <array>

namespace {

using Point = WaveEditView::Point;
using Mark = WaveEditView::Mark;
constexpr int kWaveSeconds = 2;
constexpr long kWaveFrames = kWaveSeconds * 44100L;
constexpr int kPresses = 30;   // more than a 2-second wave's halvings to a frame a column
constexpr float kColumnSlack = 0.01f;   // a frame a column at the closest: the window as many frames as the wave's columns
const juce::String kWhole = "0.00 ms to 2.000 s of 2.000 s";

void press(WavePage& page, WavePage::Button b) {
    page.button(b).onClick();
}

void zooms(WavePage& page, WaveEditView& v, TestChecks& check) {
    press(page, WavePage::kFit);
    check(v.first() == 0 && v.span() == kWaveFrames, "Fit doesn't show the whole wave");
    check(page.shownText() == kWhole, "the line says " + page.shownText().toStdString());
    press(page, WavePage::kZoomIn);
    check(v.span() == kWaveFrames / 2, "+ doesn't halve the window: " + std::to_string(v.span()));
    check(std::abs(v.first() + v.span() / 2 - kWaveFrames / 2) <= 1, "+ moved the window's middle");
    check(page.shownText() == v.shown() && page.shownText() != kWhole, "the line doesn't say the narrowed window");
    press(page, WavePage::kZoomOut);
    check(v.first() == 0 && v.span() == kWaveFrames, "- doesn't widen the window back");
    for (int i = 0; i < kPresses; ++i) press(page, WavePage::kZoomIn);
    const float column = v.xOf(v.first() + 1) - v.xOf(v.first());
    check(std::abs(column - 1.0f) < kColumnSlack, "+ stops at " + std::to_string(column) + " columns a frame");
    const long closest = v.span();
    press(page, WavePage::kZoomIn);
    check(v.span() == closest, "+ went past a frame a column");
    press(page, WavePage::kFit);
    check(v.first() == 0 && v.span() == kWaveFrames && page.shownText() == kWhole,
          "Fit doesn't bring the wave back");
}

// The marks told apart by colour: the loop's handles, the Edit region's, Start, Current and the playhead (v485) each
// their own (an event's ▼ shares the Edit region's white; the backward loop is the loop's, faint)
void coloursApart(TestChecks& check) {
    const std::array<Mark, 5> apart { Mark::loop, Mark::edit, Mark::start, Mark::current, Mark::playing };
    for (size_t i = 0; i < apart.size(); ++i)
        for (size_t j = i + 1; j < apart.size(); ++j)
            check(WaveEditView::colourOf(apart[i]) != WaveEditView::colourOf(apart[j]),
                  std::string("the key's ") + WaveEditView::nameOf(apart[i]) + " and " +
                      WaveEditView::nameOf(apart[j]) + " are one colour");
}

bool drawnIn(const juce::Image& shot, juce::Point<float> at, juce::Colour c) {
    const auto p = at.toInt();
    return shot.getBounds().contains(p) && shot.getPixelAt(p.x, p.y) == c;
}

void keyDrawn(WavePage& page, WaveEditView& v, TestChecks& check) {
    WaveKey& key = page.key();
    const auto items = key.items();
    check(key.isVisible() && items.size() == (size_t)Mark::kMarks, "the key doesn't show every mark");
    const juce::Image shot = key.createComponentSnapshot(key.getLocalBounds());
    for (const WaveKey::Item& it : items) {
        const std::string name = WaveEditView::nameOf(it.mark);
        check(!name.empty() && it.name.getRight() <= (float)key.getWidth(), "the key's '" + name + "' is cut");
        if (it.mark == Mark::backward) continue;   // faint: its colour over the card, not alone
        check(drawnIn(shot, it.swatch, WaveEditView::colourOf(it.mark)), "the key's '" + name + "' isn't its colour");
    }
    const juce::Image wave = v.createComponentSnapshot(v.getLocalBounds());
    for (const Point p : { Point::fwdStart, Point::fwdEnd })
        check(drawnIn(wave, v.handle(p), WaveEditView::colourOf(Mark::loop)),
              "the forward loop's handle isn't the key's Loop colour");
}

} // namespace

void waveZoomChecks(SynthEditor& ed, VSynthEmuProcessor& p, const juce::File& folder, TestChecks& check) {
    auto* page = WavePageTest::pageWithWave(ed, p, folder, kWaveSeconds, check);
    if (page == nullptr) return;
    zooms(*page, page->view(0), check);
    keyDrawn(*page, page->view(0), check);
    coloursApart(check);
}
