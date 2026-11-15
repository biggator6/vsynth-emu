#pragma once
// D, the plugin's editor from v196 (research/UI_DESIGN.md; UI steps 1 to 5): drawn at Look::width x Look::height and
// scaled whole, its aspect kept; the header, the page bar, on SYNTH the signal path, the rack and the lower panel, on
// each other page its own (UiPages.h), and the footer.  VSynthEmuEditorTest draws and checks it.
#include "PluginProcessor.h"
#include "UiBlocks.h"
#include "UiFooter.h"
#include "UiHeader.h"
#include "UiLook.h"
#include "UiLowerPanel.h"
#include "UiPageBar.h"
#include "UiPath.h"
#include "UiRack.h"
#include <array>
#include <memory>

class SynthEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit SynthEditor(VSynthEmuProcessor& p);
    ~SynthEditor() override;

    void showPage(int page);
    void showEffect(int effect);   // the FX page at an effect's own page, as a click on it on the path
    int page() const { return pageBar_.chosen(); }
    void select(int block);
    int selected() const { return path_.selected(); }
    LowerPanel& lower() { return lower_; }
    juce::Component* pageView(int page) { return pages_[(size_t)page].get(); }   // none for SYNTH
    juce::Component& drawing() { return drawing_; }   // what the window scales; the checks walk it
    float scale() const { return (float)getWidth() / (float)Look::width; }
    // The footer's keyboard shown, or folded away with the window shorter by its height at the scale it is drawn (v201,
    // the user's choice); kept in the session's state, so the editor opens as it was left
    void showKeyboard(bool shown);
    bool keyboardShown() const { return footer_.isVisible(); }
    static constexpr const char* kKeyboardShown = "keyboardShown";   // the state's property

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    Look::Feel feel_;
    juce::Component drawing_;
    Header header_;
    PageBar pageBar_;
    PathBar path_;
    Rack rack_;
    LowerPanel lower_;
    Footer footer_;
    std::array<std::unique_ptr<juce::Component>, Blocks::kPages.size()> pages_;
    void timerCallback() override;
};
