#pragma once
// D's header (research/UI_DESIGN.md, "The window"; UI step 1): the wordmark and the build under it, the mode switch
// (V-SYNTH and VC-2, v453: the session's mode, d-15), the patch box (UiPatchBox.h, its own from v601: the patch's name,
// what it misses, its project to browse, Browse…), the tempo, the Effects switch, the ROM's state and the ROM… button.
// In VC-2 mode the patch box is a .p02's and loads one, the tempo the VC-2's Patch Tempo, and a MIC chip beside the patch
// box says whether the vocoder is heard, and why not (research/UI_DESIGN_VC2.md, "6. The mic's state").
#include "PluginMode.h"
#include "PluginProcessor.h"
#include "UiPatchBox.h"
#include <functional>
#include <memory>

class Header : public juce::Component {
public:
    explicit Header(VSynthEmuProcessor& p);
    std::function<void(PluginMode)> onMode;   // a mode's button pressed
    std::function<void()> onMic;              // the MIC chip pressed: the VOCODER page
    void setMode(PluginMode mode);            // what the header shows for a mode; no call
    PluginMode mode() const { return mode_; }
    // The patch's name and what it misses, the tempo, the mic and the ROM's state, as they are now (the editor's timer)
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    PluginMode mode_ = PluginMode::vsynth;
    juce::Label build_, tempo_, romState_;
    PatchBox patch_;
    juce::TextButton synthMode_ { "V-SYNTH" }, vc2Mode_ { "VC-2" }, rom_ { "ROM..." };
    juce::TextButton mic_ { "MIC" };
    juce::ToggleButton effects_ { "Effects" };
    juce::AudioProcessorValueTreeState::ButtonAttachment effectsAtt_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> patchBox_, tempoBox_;
    void chooseRom();
};
