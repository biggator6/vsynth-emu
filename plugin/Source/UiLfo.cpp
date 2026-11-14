// A block's LFO (UiLfo.h; UI step 3)
#include "UiLfo.h"
#include "UiLook.h"
#include "UiParts.h"
#include <array>
#include <cmath>
#include <numbers>

namespace {

constexpr int kTitleH = 22, kGap = 6, kDial = 30, kSyncW = 96, kSteps = 96;
constexpr float kMaxCycles = 4.0f, kMarks = 0.3f;   // cycles drawn at the top rate; the width a full delay or fade takes
constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
// The parameters' names after the kind's LFO prefix, in Watch's order, and where each is in it
const char* const kNames[] = { "Wave", "Rate", "Delay", "Fade", "FadeMode", "Offset" };
constexpr size_t kWave = 0, kRate = 1, kDelay = 2, kFade = 3, kFadeMode = 4, kOffset = 5;
// The levels a random waveform is drawn through: a drawing's, not the LFO's (its values are the firmware's own)
constexpr std::array<float, 8> kLevels { 0.2f, -0.6f, 0.9f, -0.2f, 0.5f, -0.9f, 0.7f, -0.4f };

float level(int i) {
    return kLevels[(size_t)(i % (int)kLevels.size())];
}

// Each waveform as drawn, -1 to 1 at a phase (0 to 1) of a cycle, in the Waveform choice's order: SIN, TRI, SAW, SQR,
// RND, TRP, S&H, CHAOS
float sine(float p, int) { return std::sin(kTwoPi * p); }
float triangle(float p, int) { return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f; }
float saw(float p, int) { return 2.0f * p - 1.0f; }
float square(float p, int) { return p < 0.5f ? 1.0f : -1.0f; }
float random(float p, int cycle) {
    const float t = 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * p);
    return level(cycle) + t * (level(cycle + 1) - level(cycle));
}
float trapezoid(float p, int c) { return juce::jlimit(-1.0f, 1.0f, 2.0f * triangle(p, c)); }
float sampleHold(float p, int cycle) { return level(2 * cycle + (p < 0.5f ? 0 : 1)); }
float chaos(float p, int cycle) {
    return 0.6f * std::sin(kTwoPi * p) + 0.4f * std::sin(kTwoPi * 2.7f * p + (float)cycle);
}
using Wave = float (*)(float, int);
constexpr Wave kWaves[] = { sine, triangle, saw, square, random, trapezoid, sampleHold, chaos };

// The amplitude each Fade Mode draws at x (0 to 1 across), given how far into its fade x is: in Fade Mode's order,
// ON << (silent for the delay, then fading in), ON >> (whole for the delay, then fading out), and the two after a
// key's release, drawn whole
float fadesIn(float x, float delay, float into) { return x < delay ? 0.0f : into; }
float fadesOut(float x, float delay, float into) { return x < delay ? 1.0f : 1.0f - into; }
float whole(float, float, float) { return 1.0f; }
using Fade = float (*)(float, float, float);
constexpr Fade kFades[] = { fadesIn, fadesOut, whole, whole };

template <class T, size_t N>
T pick(const T (&table)[N], float value) {
    return table[(size_t)juce::jlimit(0, (int)N - 1, juce::roundToInt(value))];
}

juce::String lfoParam(const Blocks::Block& b, const char* name) {
    return Blocks::blockParam(b, juce::String(Blocks::kindOf(b).lfo) + name);
}

juce::StringArray lfoIds(const Blocks::Block& b) {
    juce::StringArray ids;
    for (const char* name : kNames) ids.add(lfoParam(b, name));
    return ids;
}

}

LfoShape::LfoShape(Controls::State& state, const Blocks::Block& b)
    : watch_(state, lfoIds(b), [this] { repaint(); }) {
    setTitle(Blocks::lfoNameOf(b));
}

// The wave across the width: as many cycles as its rate gives, its delay and fade by its Fade Mode, its offset lifting
// or lowering it
void LfoShape::paint(juce::Graphics& g) {
    Ui::fillRounded(g, getLocalBounds().toFloat(), Look::field, 6.0f);
    const auto a = getLocalBounds().toFloat().reduced(6.0f);
    g.setColour(Look::divider);
    g.drawHorizontalLine(juce::roundToInt(a.getCentreY()), a.getX(), a.getRight());
    const Wave wave = pick(kWaves, watch_.value(kWave));
    const Fade fadeOf = pick(kFades, watch_.value(kFadeMode));
    const float cycles = 1.0f + (kMaxCycles - 1.0f) * watch_.proportion(kRate);
    const float delay = kMarks * watch_.proportion(kDelay), fade = kMarks * watch_.proportion(kFade);
    const float offset = (watch_.proportion(kOffset) - 0.5f) * 2.0f;
    std::vector<juce::Point<float>> points;
    for (int i = 0; i <= kSteps; ++i) {
        const float x = (float)i / (float)kSteps;
        const float into = juce::jlimit(0.0f, 1.0f, fade > 0.0f ? (x - delay) / fade : 1.0f);
        const float t = x * cycles;
        const float v = 0.5f * (fadeOf(x, delay, into) * wave(t - std::floor(t), (int)t) + offset);
        points.push_back({ a.getX() + x * a.getWidth(), a.getCentreY() - v * a.getHeight() * 0.9f });
    }
    Ui::stroke(g, Ui::polyline(points), Look::mod, juce::PathStrokeType(1.6f));
}

void LfoShape::mouseDown(const juce::MouseEvent&) {
    if (onClick) onClick();
}

LfoView::LfoView(Controls::State& state, const Blocks::Block& b)
    : shape_(state, b), wave_(state, lfoParam(b, "Wave"), "Wave"),
      fadeMode_(state, lfoParam(b, "FadeMode"), "Fade Mode"), offset_(state, lfoParam(b, "Offset"), "Offset") {
    shape_.setInterceptsMouseClicks(false, false);
    Look::panelName(name_, Blocks::lfoNameOf(b));
    Controls::toggle(sync_, state, lfoParam(b, "KeySync"), attached_);
    const std::initializer_list<juce::Component*> parts = { &name_, &sync_, &shape_, &wave_, &fadeMode_, &offset_ };
    for (juce::Component* c : parts) addAndMakeVisible(c);
    const Controls::KnobRow knobs[] = { { "Rate", "Rate" }, { "Delay", "Delay" }, { "Fade", "Fade" } };
    for (const Controls::KnobRow& row : knobs)
        if (auto k = Controls::knob(state, lfoParam(b, row.id), row.name, kDial)) {
            addAndMakeVisible(*k);
            knobs_.push_back(std::move(k));
        }
}

// The name and Key Sync, the shape, the three boxes, the three knobs; the shape takes what is left
void LfoView::resized() {
    auto r = getLocalBounds();
    auto title = r.removeFromTop(kTitleH);
    sync_.setBounds(title.removeFromRight(kSyncW));
    name_.setBounds(title);
    r.removeFromTop(kGap);
    auto knobs = r.removeFromBottom(Knob::heightFor(kDial));
    r.removeFromBottom(kGap);
    auto fields = r.removeFromBottom(Field::kHeight);
    r.removeFromBottom(kGap);
    shape_.setBounds(r);
    const int fw = (fields.getWidth() - 2 * kGap) / 3;
    for (Field* f : { &wave_, &fadeMode_, &offset_ }) {
        f->setBounds(fields.removeFromLeft(fw));
        fields.removeFromLeft(kGap);
    }
    const int kw = knobs.getWidth() / juce::jmax(1, (int)knobs_.size());
    for (const auto& k : knobs_) k->setBounds(knobs.removeFromLeft(kw));
}
