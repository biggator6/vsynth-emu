#pragma once
// The map of the controllers learnt (v463; research/NEXT_ISSUES.md item 1, "a map of the controls"; research/UI_DESIGN.md,
// "MIDI learn"): a row for each, by controller, its CC, its parameter's name and Forget; under them the parameter
// waiting to learn, if one is, and Forget all.  Opened from any control's menu (UiMidiLearn.h) in a call-out at it,
// and made again whenever the map changes (PluginMidiLearn::changes, on a timer).  Its list scrolls past twelve rows.
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

class VSynthEmuProcessor;

class MidiMapView : public juce::Component, private juce::Timer {
public:
    explicit MidiMapView(VSynthEmuProcessor& p);
    ~MidiMapView() override;
    static void open(VSynthEmuProcessor& p, juce::Component& at);   // in a call-out at a control
    int rows() const { return (int)rows_.size(); }
    juce::String rowText(int i) const;      // "CC n" and the parameter's name, as the row shows them
    juce::String waitingText() const;       // the line under the rows
    void forgetRow(int i);                  // as the row's Forget does
    void forgetAll();                       // as Forget all does
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    struct MapRow {
        juce::Label cc, name;
        juce::TextButton forget { "Forget" };
    };
    static constexpr int kWidth = 340, kRowH = 22, kTitleH = 26, kFootH = 54, kMostShown = 12;
    VSynthEmuProcessor& processor_;
    std::vector<std::unique_ptr<MapRow>> rows_;
    juce::Label title_, waiting_;
    juce::TextButton forgetAll_ { "Forget all" };
    juce::Viewport view_;
    juce::Component list_;
    uint32_t seen_ = 0;          // the map's changes and its learning line as rebuild last saw them
    juce::String learning_;
    void rebuild();
    void timerCallback() override;
};
