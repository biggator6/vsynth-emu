// What an OSC panel's display draws (UiWaveDisplay.h; v200)
#include "UiWaveDisplay.h"
#include "UiLook.h"
#include "UiParts.h"
#include "XtOsc.h"
#include <array>
#include <cmath>
#include <memory>

namespace {

constexpr int kKey = 33;                  // A1, 55 Hz: three octaves under A4, the analog converter's ratio 1
constexpr float kRatio = 0.125f;          // that key's pitch ratio
constexpr int kKeyfollow = 1000;          // Pitch KF 100%, as xtPitch10 takes it
constexpr double kXtRate = 44100.0, kA4 = 440.0;
constexpr int kCycles = 3;
constexpr int kBlock = 32;                // the frames a process() call makes
constexpr int kSettleBlocks = 64;         // about 46 ms: what Impact's start and the gain's glide leave
constexpr float kLoopAlpha = 0.14f;

float yOf(juce::Rectangle<float> area, float s) {
    return area.getCentreY() - 0.5f * area.getHeight() * juce::jlimit(-1.0f, 1.0f, s);
}

} // namespace

namespace WaveDisplay {

AnalogSettings analogSettings(const XtSynth::Controls& c, int osc) {
    const XtSynth::OscControls& o = osc ? c.osc2 : c;
    return { o.ana, o.waveGain, osc != 0 };
}

std::vector<float> analogCycles(const AnalogSettings& s) {
    if (!xtAnalogTablesLoaded()) return {};
    const int frames = (int)std::lround(kCycles * kXtRate / (kA4 * kRatio));
    const int pitch10 = xtPitch10(kKey, 0, kKeyfollow, 0);
    const XtAnalogBlock block = xtAnalogBlock(s.ana, kKey, pitch10, s.waveGain, 1.0f, 1.0f, s.secondOsc);
    auto shared = std::make_unique<XtAnalogShared>();   // FEEDBACK-OSC's lines and SUPER-SAW's counter, this drawing's own
    XtAnalogOsc osc;
    osc.noteOn(block, kRatio, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, *shared);
    std::array<float, 2 * kBlock> lr {};
    for (int b = 0; b < kSettleBlocks; ++b) osc.process(lr.data());
    std::vector<float> out;
    while ((int)out.size() < frames) {
        osc.process(lr.data());
        for (int i = 0; i < kBlock && (int)out.size() < frames; ++i) out.push_back(lr[(size_t)(2 * i)]);
    }
    osc.release();
    float peak = 0.0f;
    for (float s : out) peak = std::max(peak, std::abs(s));
    if (peak > 0.0f)
        for (float& s : out) s /= peak;
    return out;
}

void paintWave(juce::Graphics& g, juce::Rectangle<float> area, const View& view, const Loop& loop) {
    if (loop.on) {
        const float x0 = area.getX() + area.getWidth() * loop.start, x1 = area.getX() + area.getWidth() * loop.end;
        const juce::Rectangle<float> span { x0, area.getY(), std::max(1.0f, x1 - x0), area.getHeight() };
        Ui::fillRounded(g, span, Look::mod.withAlpha(kLoopAlpha), 2.0f);
    }
    const size_t n = view.columns.size();
    if (n == 0) return;
    const float w = area.getWidth() / (float)n;
    juce::Path bars;
    for (size_t i = 0; i < n; ++i) {
        const float top = yOf(area, view.columns[i].high), bottom = yOf(area, view.columns[i].low);
        const float x = area.getX() + w * (float)i;
        bars.addRectangle(x, top, std::max(w, 1.0f), std::max(1.0f, bottom - top));
    }
    Ui::fillPath(g, bars, Look::accent);
}

void paintCycles(juce::Graphics& g, juce::Rectangle<float> area, const std::vector<float>& samples) {
    if (samples.size() < 2) return;
    std::vector<juce::Point<float>> points;
    const float last = (float)(samples.size() - 1);
    for (size_t i = 0; i < samples.size(); ++i)
        points.push_back({ area.getX() + area.getWidth() * (float)i / last, yOf(area, samples[i]) });
    Ui::stroke(g, Ui::polyline(points), Look::accent, juce::PathStrokeType(2.0f));
}

} // namespace WaveDisplay
