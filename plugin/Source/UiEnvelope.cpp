// A block's envelopes (UiEnvelope.h; UI step 3)
#include "UiEnvelope.h"
#include "UiLook.h"
#include "UiParts.h"
#include <array>

namespace {

using Point = EnvCurve::Point;
constexpr int kTabsH = 22, kGap = 6, kPerRow = 6, kRowGap = 4;
constexpr float kQuarters = 4.0f, kHoldEnd = 3.0f, kGrab = 9.0f, kDot = 4.5f;

// An envelope's parameters by the names its prefix takes, the first four its curve's; a bar for each one it has
constexpr Controls::KnobRow kBars[] = {
    { "Attack", "Attack" },   { "Decay", "Decay" },        { "Sustain", "Sustain" },
    { "Release", "Release" }, { "Depth", "Depth" },        { "TimeKF", "Time KF" },
    { "VelSens", "Velo" },    { "VelCurve", "V-Curve" },   { "AttackSens", "A-Velo" },
    { "DecaySens", "D-Velo" }, { "ReleaseSens", "R-Velo" },
};
constexpr size_t kAttack = 0, kDecay = 1, kSustain = 2, kRelease = 3;
// The time each point's drag sets, in Point's order (the decay's height sets Sustain besides)
constexpr size_t kTimeOf[EnvCurve::kPoints] = { kAttack, kDecay, kRelease };

juce::StringArray curveIds(const Blocks::Block& b, const Blocks::EnvName& env) {
    juce::StringArray ids;
    for (size_t i = 0; i <= kRelease; ++i) ids.add(Blocks::blockParam(b, juce::String(env.prefix) + kBars[i].id));
    return ids;
}

juce::StringArray envNames(const Blocks::Block& b) {
    juce::StringArray names;
    for (const Blocks::EnvName& env : Blocks::kindOf(b).envs)
        if (env.name != nullptr) names.add(env.name);
    return names;
}

}

EnvCurve::EnvCurve(Controls::State& state, const Blocks::Block& b, const Blocks::EnvName& env, bool editable)
    : watch_(state, curveIds(b, env), [this] { repaint(); }), editable_(editable) {
    setTitle(Blocks::blockName(b) + " " + env.name);
}

juce::Rectangle<float> EnvCurve::area() const {
    const float in = editable_ ? 10.0f : 4.0f;
    return getLocalBounds().toFloat().reduced(in, in);
}

juce::Point<float> EnvCurve::pointAt(Point p) const {
    const auto a = area();
    const float q = a.getWidth() / kQuarters;
    const float xa = a.getX() + watch_.proportion(kAttack) * q;
    const float xd = xa + watch_.proportion(kDecay) * q;
    const float xr = a.getX() + (kHoldEnd + watch_.proportion(kRelease)) * q;
    const juce::Point<float> attackAt { xa, a.getY() };
    const juce::Point<float> decayAt { xd, a.getBottom() - watch_.proportion(kSustain) * a.getHeight() };
    const juce::Point<float> releaseAt { xr, a.getBottom() };
    const std::array<juce::Point<float>, kPoints> at { attackAt, decayAt, releaseAt };
    return at[(size_t)p];
}

juce::Path EnvCurve::shape() const {
    const auto a = area();
    const auto decay = pointAt(Point::decay);
    const juce::Point<float> holdEnd { a.getX() + kHoldEnd * a.getWidth() / kQuarters, decay.y };
    return Ui::polyline({ a.getBottomLeft(), pointAt(Point::attack), decay, holdEnd, pointAt(Point::release) });
}

void EnvCurve::paint(juce::Graphics& g) {
    Ui::fillRounded(g, getLocalBounds().toFloat(), Look::field, 6.0f);
    const auto a = area();
    g.setColour(Look::divider);
    for (int i = 1; i < (int)kQuarters; ++i)
        g.drawVerticalLine(juce::roundToInt(a.getX() + a.getWidth() * (float)i / kQuarters), a.getY(), a.getBottom());
    juce::Path fill = shape();
    fill.lineTo(a.getBottomLeft());
    g.setColour(Look::accent.withAlpha(0.12f));
    g.fillPath(fill);
    Ui::stroke(g, shape(), Look::accent, juce::PathStrokeType(editable_ ? 2.0f : 1.5f));
    if (!editable_) return;
    for (int i = 0; i < kPoints; ++i) {
        Ui::dot(g, pointAt((Point)i), 2 * kDot, i == dragging_ ? Look::accent : Look::text);
    }
}

void EnvCurve::mouseDown(const juce::MouseEvent& e) {
    if (!editable_) {
        if (onClick) onClick();
        return;
    }
    // The nearest point in reach; of points on one another (a decay at 0 puts its point on the attack's), the later,
    // which can be drawn off the earlier, where the earlier would carry the later with it
    dragging_ = -1;
    float nearest = kGrab;
    for (int i = 0; i < kPoints; ++i) {
        const float d = pointAt((Point)i).getDistanceFrom(e.position);
        if (d > nearest) continue;
        nearest = d;
        dragging_ = i;
    }
    if (dragging_ < 0) return;
    watch_.begin(kTimeOf[dragging_]);
    if ((Point)dragging_ == Point::decay) watch_.begin(kSustain);
    repaint();
}

// A point dragged: its time from where its segment starts, over a quarter of the width; the decay's height its sustain
void EnvCurve::mouseDrag(const juce::MouseEvent& e) {
    if (dragging_ < 0) return;
    const auto a = area();
    const float q = a.getWidth() / kQuarters;
    const std::array<float, kPoints> starts { a.getX(), pointAt(Point::attack).x, a.getX() + kHoldEnd * q };
    watch_.drag(kTimeOf[dragging_], (e.position.x - starts[(size_t)dragging_]) / q);
    if ((Point)dragging_ == Point::decay) watch_.drag(kSustain, (a.getBottom() - e.position.y) / a.getHeight());
}

void EnvCurve::mouseUp(const juce::MouseEvent&) {
    if (dragging_ < 0) return;
    watch_.end(kTimeOf[dragging_]);
    if ((Point)dragging_ == Point::decay) watch_.end(kSustain);
    dragging_ = -1;
    repaint();
}

EnvelopeView::EnvelopeView(Controls::State& state, const Blocks::Block& b)
    : tabs_(envNames(b), { Look::sans(11.0f, "Bold", 0.06f), 0, 10 }) {
    for (const Blocks::EnvName& env : Blocks::kindOf(b).envs) {
        if (env.name == nullptr) continue;
        Envelope e;
        e.curve = std::make_unique<EnvCurve>(state, b, env, true);
        addChildComponent(*e.curve);
        for (const Controls::KnobRow& row : kBars) {
            const juce::String id = Blocks::blockParam(b, juce::String(env.prefix) + row.id);
            if (state.getParameter(id) == nullptr) continue;
            e.bars.push_back(std::make_unique<Bar>(state, id, row.name, Look::accent));
            addChildComponent(*e.bars.back());
        }
        envs_.push_back(std::move(e));
    }
    tabs_.onChoose = [this](int env) { showEnvelope(env); };
    addAndMakeVisible(tabs_);
    showEnvelope(0);
}

void EnvelopeView::showEnvelope(int env) {
    tabs_.choose(env);
    for (size_t i = 0; i < envs_.size(); ++i) {
        const bool shown = (int)i == tabs_.chosen();
        envs_[i].curve->setVisible(shown);
        for (const auto& bar : envs_[i].bars) bar->setVisible(shown);
    }
}

// The tabs over the curve, then the bars in rows of six under it; the curve takes what is left
void EnvelopeView::resized() {
    auto r = getLocalBounds();
    tabs_.setBounds(r.removeFromTop(kTabsH));
    r.removeFromTop(kGap);
    for (Envelope& e : envs_) {
        const int rows = ((int)e.bars.size() + kPerRow - 1) / kPerRow;
        auto bars = r.withTop(r.getBottom() - rows * Bar::kHeight - (rows - 1) * kRowGap);
        e.curve->setBounds(r.withBottom(bars.getY() - kGap));
        const int w = (bars.getWidth() - (kPerRow - 1) * kGap) / kPerRow;
        for (size_t i = 0; i < e.bars.size(); ++i) {
            const int col = (int)i % kPerRow, row = (int)i / kPerRow;
            e.bars[i]->setBounds(bars.getX() + col * (w + kGap), bars.getY() + row * (Bar::kHeight + kRowGap), w,
                                 Bar::kHeight);
        }
    }
}
