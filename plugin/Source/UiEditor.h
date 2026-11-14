#pragma once
// D, the redesigned editor (research/UI_DESIGN.md, "The redesign, D"; UI steps 1 to 3): drawn at Look::width x
// Look::height and scaled whole, its aspect kept; the header, the page bar, on SYNTH the signal path, the rack and the
// lower panel, and the footer.  The other pages come in step 4.  Not yet the plugin's editor: VSynthEmuEditorTest draws
// and checks it until step 5.
#include "PluginProcessor.h"
#include "UiFooter.h"
#include "UiHeader.h"
#include "UiLook.h"
#include "UiLowerPanel.h"
#include "UiPageBar.h"
#include "UiPath.h"
#include "UiRack.h"

class SynthEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit SynthEditor(VSynthEmuProcessor& p);
    ~SynthEditor() override;

    void showPage(int page);
    int page() const { return pageBar_.chosen(); }
    void select(int block);
    int selected() const { return path_.selected(); }
    LowerPanel& lower() { return lower_; }
    juce::Component& drawing() { return drawing_; }   // what the window scales; the checks walk it
    float scale() const { return (float)getWidth() / (float)Look::width; }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Look::Feel feel_;
    juce::Component drawing_;
    Header header_;
    PageBar pageBar_;
    PathBar path_;
    Rack rack_;
    LowerPanel lower_;
    Footer footer_;
    void timerCallback() override;
};
