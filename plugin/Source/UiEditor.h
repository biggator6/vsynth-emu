#pragma once
// D, the redesigned editor (research/UI_DESIGN.md, "The redesign, D"; UI step 1): drawn at Look::width x Look::height
// and scaled whole, its aspect kept; the header, the page bar, on SYNTH the signal path, and the footer.  The rack and the
// lower panel come in steps 2 and 3, the other pages in step 4.  Not yet the plugin's editor: VSynthEmuEditorTest draws
// and checks it until step 5.
#include "PluginProcessor.h"
#include "UiFooter.h"
#include "UiHeader.h"
#include "UiLook.h"
#include "UiPageBar.h"
#include "UiPath.h"

class SynthEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit SynthEditor(VSynthEmuProcessor& p);
    ~SynthEditor() override;

    void showPage(int page);
    int page() const { return pageBar_.page(); }
    void select(int block);
    int selected() const { return path_.selected(); }
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
    Footer footer_;
    void timerCallback() override;
};
