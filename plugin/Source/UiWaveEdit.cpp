// The WAVE page's wave (UiWaveEdit.h, v479; the Edit region and Truncate, v480); its drawing is UiWaveEditPaint.cpp's
#include "UiWaveEdit.h"
#include "PluginParamIds.h"
#include "PluginStartOffset.h"
#include <array>
#include <cmath>

namespace {

constexpr float kReach = 8.0f;
constexpr float kZoomPerDelta = 4.0f;   // the wheel's delta to the power of two the window narrows by
constexpr float kPanPerDelta = 0.5f;    // and a sideways delta to the share of the window it moves by

// In Watched's order, the OSC's own (Params::oscId)
juce::StringArray watched(int osc) {
    juce::StringArray ids;
    for (const juce::String& id : { Params::kStartOffset, Params::kLoop, Params::kLoopMode, Params::kLoopStart,
                                    Params::kLoopEnd })
        ids.add(Params::oscId(osc, id));
    return ids;
}

long within(long f, long lo, long hi) {
    return std::clamp(f, lo, std::max(lo, hi));
}

} // namespace

// In Point's order (none, start, the forward loop's ends, the backward's, the Edit region's, Current); none for the
// backward loop's (the session's edit), the Edit region's and Current (the page's own, as the XT's are its RAM's)
size_t WaveEditView::watchedOf(Point p) {
    static constexpr std::array<size_t, 9> kWatchedOf { kNone, kStart, kLoopStart, kLoopEnd, kNone,
                                                        kNone, kNone,  kNone,      kNone };
    return kWatchedOf[(size_t)p];
}

WaveEditView::WaveEditView(VSynthEmuProcessor& p, int osc)
    : processor_(p), osc_(osc), watch_(p.apvts, watched(osc), [this] { repaint(); }) {
    refresh();
}

// A new wave, or the wave cut: the window all of it, the Edit region the forward loop's, as the XT's edit screen puts
// it when it opens (0x89380b60)
void WaveEditView::refresh() {
    if (const int gen = processor_.waveViewGen(osc_); gen != viewGen_) {
        viewGen_ = gen;
        view_ = processor_.waveView(osc_);
        const long frames = view_ && processor_.hasFileLoaded(osc_) ? view_->frames : 0;
        if (frames != frames_) {
            frames_ = frames;
            first_ = 0;
            span_ = frames;
            editFrom_ = at(Point::fwdStart);
            editTo_ = at(Point::fwdEnd);
        }
        events_ = PluginWaveEdits::events(processor_, osc_);   // v481: as the wave last encoded plays them
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
        case Point::editStart: return editFrom_;
        case Point::editEnd: return editTo_;
        case Point::current: return std::min(current_, last);
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
    if (p == Point::editStart || p == Point::editEnd) return { x, (float)getHeight() - kRulerH / 2.0f };
    return { x, w.getCentreY() };
}

// The point a press takes: in the strip above the wave the forward loop's ends, below it the backward loop's, in the
// ruler the Edit region's, on the wave the Start Offset's line; the nearest in reach, the later of two on one another
// (which can be drawn off it)
WaveEditView::Point WaveEditView::nearest(juce::Point<float> pos) const {
    if (frames_ <= 0) return Point::none;
    const auto w = wave();
    std::array<Point, 2> strip { Point::start, Point::start };
    if (pos.y < w.getY()) strip = { Point::fwdStart, Point::fwdEnd };
    if (pos.y > w.getBottom()) strip = { Point::bwdStart, Point::bwdEnd };
    if (pos.y > w.getBottom() + kStrip) strip = { Point::editStart, Point::editEnd };
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

void WaveEditView::setLoopEnd(size_t i, long frame, long frames) {
    if (auto* prm = watch_.parameter(i)) watch_.dragTo(i, waveLoopPercentFor(*prm, (int)frame, frames));
}

// A point moved to `frame`, held where the XT holds it: the Start Offset in the wave; a loop's end held to its start
// and the wave, 16 frames apart at the least, none on a wave whose last frame is under 16 (the Loop screens' setters,
// 0x89382990 and 0x89382aa0: clamped, not refused; HARDWARE_FIRMWARE.md {#wave-truncate-read}); the Edit region's
// ends held to each other with no gap (0x89380a00)
void WaveEditView::place(Point p, long frame) {
    const long last = std::max(0L, frames_ - 1);
    const long shortest = last >= kWaveLoopShortest ? kWaveLoopShortest : 0;
    const long fwdLeast = std::min(last, at(Point::fwdStart) + shortest);
    const long bwdLeast = std::min(last, (long)bwd_.bwdStart + shortest);
    switch (p) {
        case Point::start: watch_.dragTo(kStart, (float)within(frame, 0, last)); break;
        case Point::fwdStart: setLoopEnd(kLoopStart, within(frame, 0, at(Point::fwdEnd) - shortest), frames_); break;
        case Point::fwdEnd: setLoopEnd(kLoopEnd, within(frame, fwdLeast, last), frames_); break;
        case Point::bwdStart: bwd_.bwdStart = (int)within(frame, 0, bwd_.bwdEnd - shortest); break;
        case Point::bwdEnd: bwd_.bwdEnd = (int)within(frame, bwdLeast, last); break;
        case Point::editStart: editFrom_ = within(frame, 0, editTo_); break;
        case Point::editEnd: editTo_ = within(frame, editFrom_, last); break;
        case Point::none: break;
    }
    repaint();
}

bool WaveEditView::truncate() {
    if (frames_ <= 0 || editFrom_ >= editTo_) return false;
    const auto [start, end] = waveLoopAfterCut(at(Point::fwdStart), at(Point::fwdEnd), editFrom_, editTo_);
    const int mode = bwd_.backward ? bwd_.bwdMode : (int)watch_.value(kLoopMode);
    const long kept = editTo_ - editFrom_ + 1;
    if (!PluginWaveEdits::truncate(processor_, osc_, editFrom_, editTo_, mode)) return false;
    for (const auto& [i, frame] : { std::pair<size_t, long> { kLoopStart, start }, { kLoopEnd, end } }) {
        watch_.begin(i);
        setLoopEnd(i, frame, kept);
        watch_.end(i);
    }
    return true;
}

void WaveEditView::revert() {
    PluginWaveEdits::revert(processor_, osc_);
}

void WaveEditView::mouseDown(const juce::MouseEvent& e) {
    dragging_ = nearest(e.position);
    scrolling_ = dragging_ == Point::none && frames_ > 0;
    grabbedFirst_ = first_;
    grabbedX_ = e.position.x;
    if (const size_t i = watchedOf(dragging_); i != kNone) watch_.begin(i);
    const bool bwd = dragging_ == Point::bwdStart || dragging_ == Point::bwdEnd;
    if (bwd && !bwd_.backward)   // a backward loop of its own from here, the forward loop's until now
        waveEditBackward(bwd_, (int)watch_.value(kLoopMode), (int)at(Point::fwdStart), (int)at(Point::fwdEnd));
}

void WaveEditView::mouseDrag(const juce::MouseEvent& e) {
    if (dragging_ != Point::none) return place(dragging_, frameAt(e.position.x));
    if (!scrolling_ || span_ <= 0) return;
    const double moved = (double)(e.position.x - grabbedX_) / (double)wave().getWidth() * (double)span_;
    first_ = within(grabbedFirst_ - (long)std::lround(moved), 0, frames_ - span_);
    repaint();
}

void WaveEditView::mouseUp(const juce::MouseEvent& e) {
    const bool bwd = dragging_ == Point::bwdStart || dragging_ == Point::bwdEnd;
    if (const size_t i = watchedOf(dragging_); i != kNone) watch_.end(i);
    else if (bwd) PluginWaveEdits::setBackward(processor_, osc_, bwd_);
    else if (scrolling_ && !e.mouseWasDraggedSinceMouseDown()) setCurrent(frameAt(e.position.x));   // a click: Current
    dragging_ = Point::none;
    scrolling_ = false;
}

// The wheel narrows or widens the window about the frame under the pointer; sideways, it moves the window
void WaveEditView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (frames_ <= 0) return;
    if (wheel.deltaX != 0.0f)
        first_ = within(first_ - (long)std::lround(wheel.deltaX * kPanPerDelta * (float)span_), 0, frames_ - span_);
    if (wheel.deltaY != 0.0f) zoomAbout(e.position.x, std::pow(2.0, -(double)wheel.deltaY * kZoomPerDelta));
    repaint();
}

// The window `wider` times as wide about the frame at x, which stays under it, to a frame a column at most and the
// whole wave at the widest: the wheel's, and the page's - and + about the middle (v484)
void WaveEditView::zoomAbout(float x, double wider) {
    if (frames_ <= 0) return;
    const auto w = wave();
    const long least = std::min(frames_, std::max(1L, (long)w.getWidth()));
    const long pinned = frameAt(x);
    const double share = (double)(x - w.getX()) / (double)w.getWidth();
    span_ = std::clamp((long)std::lround((double)span_ * wider), least, frames_);
    first_ = within(pinned - (long)std::lround(share * (double)span_), 0, frames_ - span_);
    repaint();
}

void WaveEditView::zoomBy(double wider) {
    zoomAbout(wave().getCentreX(), wider);
}

void WaveEditView::fit() {
    first_ = 0;
    span_ = frames_;
    repaint();
}

juce::String WaveEditView::shown() const {
    if (frames_ <= 0) return {};
    return startOffsetText((int)first_) + " to " + startOffsetText((int)(first_ + span_)) + " of " +
           startOffsetText((int)frames_);
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
