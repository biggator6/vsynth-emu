#pragma once
// The arpeggio's pattern on the PLAY page (research/UI_DESIGN.md, "The pages"; UI step 4): a patch's data, no parameter,
// drawn: its 16 lines down, each named at the left (a note, or a controller), and its 32 steps across, a step's cell as
// its velocity (the accent, fainter for softer), a tie carrying the line on, a rest empty; the steps past the End Point
// dimmed.  It says so when no step plays.  Drawn again when a patch or a session brings a pattern, or the End Point
// moves.
#include "PluginProcessor.h"
#include "UiWatch.h"

class ArpPatternView : public juce::Component, private juce::Value::Listener {
public:
    explicit ArpPatternView(VSynthEmuProcessor& p);
    ~ArpPatternView() override;
    ArpPatternView(const ArpPatternView&) = delete;
    ArpPatternView& operator=(const ArpPatternView&) = delete;
    void paint(juce::Graphics&) override;

private:
    VSynthEmuProcessor& processor_;
    juce::Value pattern_;   // the processor's "arpPattern", to hear a patch's or a session's come in
    Watch end_;             // the End Point
    void valueChanged(juce::Value&) override { repaint(); }
};
