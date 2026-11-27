#pragma once
// The arpeggio's pattern on the PLAY page (research/UI_DESIGN.md, "The pages"; UI step 4): a patch's data, no parameter.
// Its 16 lines down, each its State (NOTE, CTRL) and its Note/CC at the left, and its 32 steps across, each cell drawn
// as the XT's screen marks it (XtArpPatternEdit: a note in the accent by its velocity, a tie a thin bar carrying the line
// on, a controller a ring, a rest empty); the steps past the End Point dimmed; a line saying so when no step plays.
// Drawn again when a patch or a session brings a pattern, or the End Point moves.  With a source to edit (v468, the
// V-Synth's), it is edited by the XT's Pattern Edit's rules: a click on a cell is the cursor, a drag up or down on it or
// the wheel its value (by 1, by 10 with Shift); a click on a State turns it, a drag or the wheel on a Note/CC moves it
// (by 1, by an octave on a NOTE line with Shift).  The buttons, Value and the clears are ArpPatternEditor's
// (UiArpPatternEdit.h).
#include "PluginProcessor.h"
#include "UiWatch.h"
#include "XtArpPatternEdit.h"
#include <functional>

class ArpPatternView : public juce::Component, private juce::Value::Listener {
public:
    // Where a mode's pattern comes from (v453): the pattern now, the session's property a patch or a session brings it
    // in by, and its End Point's ID; the V-Synth's by default.  And where an edit goes (v468): none, drawn only (the
    // VC-2's)
    struct Source {
        std::function<XtArpPattern()> pattern;
        juce::Identifier property;
        juce::String endPointId;
        std::function<void(const XtArpPattern&)> set;
    };
    explicit ArpPatternView(VSynthEmuProcessor& p);
    ArpPatternView(VSynthEmuProcessor& p, Source source);
    ~ArpPatternView() override;
    ArpPatternView(const ArpPatternView&) = delete;
    ArpPatternView& operator=(const ArpPatternView&) = delete;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    XtArpPattern pattern() const { return source_.pattern(); }
    bool editable() const { return source_.set != nullptr; }
    // An edit of the pattern as it stands, into the source (no source, nothing)
    void edit(const std::function<void(XtArpPattern&)>& change);
    XtArpCursor cursor() const { return cursor_; }
    void setCursor(XtArpCursor at);
    std::function<void()> onChange;   // the cursor moved or the pattern came in: what follows them (the editor's bar)
    // Where each part is drawn, for a click and the editor's test
    juce::Rectangle<int> cellBounds(int line, int step) const;
    juce::Rectangle<int> stateBounds(int line) const;
    juce::Rectangle<int> noteBounds(int line) const;

private:
    // What a press went down on, and what a drag from it moves
    struct Press {
        enum class On { nothing, cell, note } on = On::nothing;
        int line = 0, startY = 0, moved = 0;
    };
    Source source_;
    juce::Value pattern_;   // the processor's property, to hear a patch's or a session's come in
    Watch end_;             // the End Point
    XtArpCursor cursor_;
    Press press_;
    void valueChanged(juce::Value&) override;
    juce::Rectangle<int> grid() const;
    void nudge(Press::On on, int line, int by, bool shift);
    void paintLines(juce::Graphics& g, const XtArpPattern& pattern) const;
    void paintCells(juce::Graphics& g, const XtArpPattern& pattern) const;
};
