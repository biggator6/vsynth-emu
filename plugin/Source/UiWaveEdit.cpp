// The WAVE page's wave (UiWaveEdit.h, v479)
#include "UiWaveEdit.h"
#include "PluginParamIds.h"
#include "PluginStartOffset.h"
#include "UiLook.h"
#include "UiParts.h"
#include "UiWaveDisplay.h"
#include <array>
#include <cmath>

namespace {

enum Watched : size_t { kStart, kLoop, kLoopMode, kLoopStart, kLoopEnd, kNone };
constexpr float kStrip = 16.0f, kRulerH = 18.0f, kReach = 8.0f, kHandleW = 9.0f;
constexpr float kZoomPerDelta = 4.0f;   // the wheel's delta to the power of two the window narrows by
constexpr float kPanPerDelta = 0.5f;    // and a sideways delta to the share of the window it moves by
constexpr float kFaint = 0.45f, kBwdAlpha = 0.08f;
constexpr int kRulerMarks = 6;

// In Watched's order, the OSC's own (Params::oscId)
juce::StringArray watched(int osc) {
    juce::StringArray ids;
    for (const juce::String& id : { Params::kStartOffset, Params::kLoop, Params::kLoopMode, Params::kLoopStart,
                                    Params::kLoopEnd })
        ids.add(Params::oscId(osc, id));
    return ids;
}

// The watched parameter each point is, in Point's order (none, start, the forward loop's ends, the backward's); kNone
// for the backward loop's, which is the session's edit
constexpr std::array<size_t, 6> kWatchedOf { kNone, kStart, kLoopStart, kLoopEnd, kNone, kNone };
size_t watchedOf(WaveEditView::Point p) {
    return kWatchedOf[(size_t)p];
}

long within(long f, long lo, long hi) {
    return std::clamp(f, lo, std::max(lo, hi));
}

} // namespace

WaveEditView::WaveEditView(VSynthEmuProcessor& p, int osc)
    : processor_(p), osc_(osc), watch_(p.apvts, watched(osc), [this] { repaint(); }) {
    refresh();
}

void WaveEditView::refresh() {
    if (const int gen = processor_.waveViewGen(osc_); gen != viewGen_) {
        viewGen_ = gen;
        view_ = processor_.waveView(osc_);
        const long frames = view_ && processor_.hasFileLoaded(osc_) ? view_->frames : 0;
        if (frames != frames_) {
            frames_ = frames;
            first_ = 0;
            span_ = frames;
        }
        madeSpan_ = -1;
        repaint();
    }
    if (dragging_ == Point::bwdStart || dragging_ == Point::bwdEnd) return;
    const PluginWaveEdit bwd = PluginWaveEdits::backward(processor_, osc_);
    if (bwd == bwd_) return;
    bwd_ = bwd;
    repaint();
}

long WaveEditView::at(Point p) const {
    const long last = std::max(0L, frames_ - 1);
    switch (p) {
        case Point::start: return std::min((long)watch_.value(kStart), last);
        case Point::fwdStart: return waveLoopFrame(watch_.value(kLoopStart), frames_);
        case Point::fwdEnd: return waveLoopFrame(watch_.value(kLoopEnd), frames_);
        case Point::bwdStart: return bwd_.backward ? bwd_.bwdStart : at(Point::fwdStart);
        case Point::bwdEnd: return bwd_.backward ? bwd_.bwdEnd : at(Point::fwdEnd);
        case Point::none: break;
    }
    return 0;
}

juce::Rectangle<float> WaveEditView::wave() const {
    return getLocalBounds().toFloat().withTrimmedTop(kStrip).withTrimmedBottom(kStrip + kRulerH);
}

float WaveEditView::xOf(long frame) const {
    const auto w = wave();
    return span_ > 0 ? w.getX() + w.getWidth() * (float)((double)(frame - first_) / (double)span_) : w.getX();
}

long WaveEditView::frameAt(float x) const {
    const auto w = wave();
    if (span_ <= 0 || w.getWidth() <= 0.0f) return 0;
    const double f = (double)first_ + (double)(x - w.getX()) / (double)w.getWidth() * (double)span_;
    return std::clamp((long)std::lround(f), 0L, std::max(0L, frames_ - 1));
}

juce::Point<float> WaveEditView::handle(Point p) const {
    const auto w = wave();
    const float x = xOf(at(p));
    if (p == Point::fwdStart || p == Point::fwdEnd) return { x, w.getY() - kStrip / 2.0f };
    if (p == Point::bwdStart || p == Point::bwdEnd) return { x, w.getBottom() + kStrip / 2.0f };
    return { x, w.getCentreY() };
}

// The point a press takes: in the strip above the wave the forward loop's ends, below it the backward loop's, on the
// wave the Start Offset's line; the nearest in reach, the later of two on one another (which can be drawn off it)
WaveEditView::Point WaveEditView::nearest(juce::Point<float> pos) const {
    if (frames_ <= 0) return Point::none;
    const auto w = wave();
    std::array<Point, 2> strip { Point::start, Point::start };
    if (pos.y < w.getY()) strip = { Point::fwdStart, Point::fwdEnd };
    if (pos.y > w.getBottom()) strip = { Point::bwdStart, Point::bwdEnd };
    Point best = Point::none;
    float reach = kReach;
    for (const Point p : strip) {
        const float d = std::abs(xOf(at(p)) - pos.x);
        if (d > reach) continue;
        reach = d;
        best = p;
    }
    return best;
}

// A point moved to `frame`, held where the XT holds it: the Start Offset in the wave; a loop's end held to its start
// and the wave, 16 frames apart at the least, none on a wave whose last frame is under 16 (the Loop screens' setters,
// 0x89382990 and 0x89382aa0: clamped, not refused; HARDWARE_FIRMWARE.md {#wave-truncate-read})
void WaveEditView::place(Point p, long frame) {
    const long last = std::max(0L, frames_ - 1);
    const long shortest = last >= kWaveLoopShortest ? kWaveLoopShortest : 0;
    const auto loopEnd = [&](size_t i, long f) {
        if (auto* prm = watch_.parameter(i)) watch_.dragTo(i, waveLoopPercentFor(*prm, (int)f, frames_));
    };
    const long fwdLeast = std::min(last, at(Point::fwdStart) + shortest);
    const long bwdLeast = std::min(last, (long)bwd_.bwdStart + shortest);
    switch (p) {
        case Point::start: watch_.dragTo(kStart, (float)within(frame, 0, last)); break;
        case Point::fwdStart: loopEnd(kLoopStart, within(frame, 0, at(Point::fwdEnd) - shortest)); break;
        case Point::fwdEnd: loopEnd(kLoopEnd, within(frame, fwdLeast, last)); break;
        case Point::bwdStart: bwd_.bwdStart = (int)within(frame, 0, bwd_.bwdEnd - shortest); break;
        case Point::bwdEnd: bwd_.bwdEnd = (int)within(frame, bwdLeast, last); break;
        case Point::none: break;
    }
    repaint();
}

void WaveEditView::mouseDown(const juce::MouseEvent& e) {
    dragging_ = nearest(e.position);
    scrolling_ = dragging_ == Point::none && frames_ > 0;
    grabbedFirst_ = first_;
    grabbedX_ = e.position.x;
    if (const size_t i = watchedOf(dragging_); i != kNone) watch_.begin(i);
    const bool bwd = dragging_ == Point::bwdStart || dragging_ == Point::bwdEnd;
    if (bwd && !bwd_.backward)   // a backward loop of its own from here, the forward loop's until now
        bwd_ = { true, (int)watch_.value(kLoopMode), (int)at(Point::fwdStart), (int)at(Point::fwdEnd) };
}

void WaveEditView::mouseDrag(const juce::MouseEvent& e) {
    if (dragging_ != Point::none) return place(dragging_, frameAt(e.position.x));
    if (!scrolling_ || span_ <= 0) return;
    const double moved = (double)(e.position.x - grabbedX_) / (double)wave().getWidth() * (double)span_;
    first_ = within(grabbedFirst_ - (long)std::lround(moved), 0, frames_ - span_);
    repaint();
}

void WaveEditView::mouseUp(const juce::MouseEvent&) {
    if (const size_t i = watchedOf(dragging_); i != kNone) watch_.end(i);
    else if (dragging_ != Point::none) PluginWaveEdits::setBackward(processor_, osc_, bwd_);
    dragging_ = Point::none;
    scrolling_ = false;
}

// The wheel narrows or widens the window about the frame under the pointer, to a frame a column at most; sideways, it
// moves the window
void WaveEditView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (frames_ <= 0) return;
    const auto w = wave();
    if (wheel.deltaX != 0.0f)
        first_ = within(first_ - (long)std::lround(wheel.deltaX * kPanPerDelta * (float)span_), 0, frames_ - span_);
    if (wheel.deltaY != 0.0f) {
        const long least = std::min(frames_, std::max(1L, (long)w.getWidth()));
        const long pinned = frameAt(e.position.x);
        const double share = (double)(e.position.x - w.getX()) / (double)w.getWidth();
        const double wider = std::pow(2.0, -(double)wheel.deltaY * kZoomPerDelta);
        span_ = std::clamp((long)std::lround((double)span_ * wider), least, frames_);
        first_ = within(pinned - (long)std::lround(share * (double)span_), 0, frames_ - span_);
    }
    repaint();
}

void WaveEditView::makeColumns() {
    if (madeFirst_ == first_ && madeSpan_ == span_) return;
    madeFirst_ = first_;
    madeSpan_ = span_;
    columns_ = view_ ? view_->window(first_, span_, std::max(1, (int)wave().getWidth())) : decltype(columns_) {};
}

void WaveEditView::resized() {
    madeSpan_ = -1;
}

void WaveEditView::paint(juce::Graphics& g) {
    Ui::fillRounded(g, getLocalBounds().toFloat(), Look::field, 6.0f);
    if (frames_ <= 0) {
        Ui::drawText(g, "No wave loaded", getLocalBounds().reduced(10), { Look::sans(11.0f), Look::label });
        return;
    }
    makeColumns();
    const auto w = wave();
    const auto share = [&](long f) { return (xOf(f) - w.getX()) / w.getWidth(); };
    if (bwd_.backward) {   // the backward loop's span, fainter than the forward's
        const float x0 = xOf(at(Point::bwdStart)), x1 = std::max(x0 + 1.0f, xOf(at(Point::bwdEnd)));
        const auto span = juce::Rectangle<float>::leftTopRightBottom(x0, w.getY(), x1, w.getBottom());
        Ui::fill(g, span.toNearestInt(), Look::mod.withAlpha(kBwdAlpha));
    }
    PluginWaveView shown;
    shown.columns = columns_;
    const WaveDisplay::Loop loop { watch_.value(kLoop) >= 0.5f, share(at(Point::fwdStart)), share(at(Point::fwdEnd)) };
    WaveDisplay::paintWave(g, w, shown, loop);
    paintPoints(g);
    paintRuler(g);
}

// The forward loop's ends above the wave, the backward's below, fainter; the Start Offset's line across it
void WaveEditView::paintPoints(juce::Graphics& g) const {
    const auto w = wave();
    for (const Point p : { Point::fwdStart, Point::fwdEnd, Point::bwdStart, Point::bwdEnd }) {
        const bool fwd = p == Point::fwdStart || p == Point::fwdEnd;
        const auto colour = fwd ? Look::mod : Look::mod.withMultipliedAlpha(kFaint);
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
