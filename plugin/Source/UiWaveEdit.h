#pragma once
// The WAVE page's wave (v479; research/UI_DESIGN.md, "The pages", WAVE; the user's choice of 2026-11-27): an OSC's
// loaded wave in XT samples (44.1 kHz, the frames every point is counted in), each column its lowest and highest
// sample, a time ruler under it in Start Offset's text; on it its points, each dragged: the Start Offset's line (the
// OSC's parameter, d-8a), the forward loop's ends above the wave (the OSC's Loop Start and End, set to the sample by
// waveLoopPercentFor) and the backward loop's below it (the session's edit, PluginWaveEdits, given to the synth when the
// drag ends), and (v480) the Edit region's ends in the ruler, which Truncate keeps.  A loop is at least
// kWaveLoopShortest long, its start never past its end, as on the XT.  The wheel zooms about the pointer, to a frame a
// column at most; a drag away from the points scrolls the wave.  Its drawing is UiWaveEditPaint.cpp's.
#include "PluginProcessor.h"
#include "PluginWaveEdit.h"
#include "UiWatch.h"
#include <memory>
#include <vector>

class WaveEditView : public juce::Component {
public:
    WaveEditView(VSynthEmuProcessor& p, int osc);
    enum class Point { none, start, fwdStart, fwdEnd, bwdStart, bwdEnd, editStart, editEnd, current };

    void refresh();                          // the editor's timer: drawn again when the wave, a point or the window moved
    long frames() const { return frames_; }  // the wave's, as the XT holds it; 0 for none
    long first() const { return first_; }    // the window shown: `span` frames from `first`
    long span() const { return span_; }
    long at(Point p) const;                  // where a point is, in frames, as it plays
    juce::Point<float> handle(Point p) const;   // where its handle is drawn: a press there takes it
    float xOf(long frame) const;
    long frameAt(float x) const;
    // v480: Truncate on the Edit region (PluginWaveEdits::truncate, the forward loop's ends by waveLoopAfterCut through
    // the OSC's parameters, as a drag sets them), false where the XT refuses; Revert, the wave as its file has it
    bool truncate();
    void revert();
    // v481 (UiWaveEditEvents.cpp; the XT's Encode screen, d-9): Current, set by a click on the wave; the wave's events as
    // it plays them; Current to the event before or after it (the screen's 0x89516a90, 0x89516a00); an event added or
    // deleted there (PluginWaveEdits::editEvent, the XT's own routines then judging it)
    void setCurrent(long frame);
    const std::vector<long>& events() const { return events_; }
    void toEvent(bool after);
    void addEvent();
    void deleteEvent();

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum Watched : size_t { kStart, kLoop, kLoopMode, kLoopStart, kLoopEnd, kNone };
    static constexpr float kStrip = 16.0f, kRulerH = 18.0f;   // the handles' strips over and under the wave, the ruler

    VSynthEmuProcessor& processor_;
    const int osc_;
    Watch watch_;                                 // Start Offset, Loop, Loop Mode, Loop Start, Loop End
    std::shared_ptr<const PluginWaveView> view_;
    int viewGen_ = -1;
    long frames_ = 0, first_ = 0, span_ = 0;
    PluginWaveEdit bwd_;                          // the backward loop as drawn: the synth's, or the one being dragged
    long editFrom_ = 0, editTo_ = 0;              // the Edit region, both ends kept (the XT's Edit Start and End)
    long current_ = 0;                            // the XT's Current (v481)
    std::vector<long> events_;                    // the wave's events as it plays them, in frames
    Point dragging_ = Point::none;
    bool scrolling_ = false;
    long grabbedFirst_ = 0;
    float grabbedX_ = 0.0f;
    std::vector<PluginWaveView::Column> columns_;  // the window's, made again when it moves
    long madeFirst_ = -1, madeSpan_ = -1;

    static size_t watchedOf(Point p);             // the watched parameter a point is, kNone for the page's own
    juce::Rectangle<float> wave() const;          // the wave's area, between the handles' strips, over the ruler
    Point nearest(juce::Point<float> at) const;
    void place(Point p, long frame);
    void setLoopEnd(size_t i, long frame, long frames);
    void makeColumns();
    void paintPoints(juce::Graphics&) const;
    void paintRuler(juce::Graphics&) const;
    void paintEvents(juce::Graphics&) const;
};
