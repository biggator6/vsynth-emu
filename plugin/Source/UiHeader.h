#pragma once
// D's header (research/UI_DESIGN.md, "The window"; UI step 1): the wordmark and the build under it, the mode switch
// (V-SYNTH; VC-2 greyed until the VC-2 mode is in), the patch box (the patch's name, Browse… to load one), the tempo,
// the Effects switch, the ROM's state and the ROM… button.
#include "PluginProcessor.h"
#include <memory>

class Header : public juce::Component {
public:
    explicit Header(VSynthEmuProcessor& p);
    void refresh();   // the patch's name, the tempo and the ROM's state, as they are now (the editor's timer)
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    juce::Label build_, patchName_, tempo_, romState_;
    juce::TextButton synthMode_ { "V-SYNTH" }, vc2Mode_ { "VC-2" }, browse_ { "Browse..." }, rom_ { "ROM..." };
    juce::ToggleButton effects_ { "Effects" };
    juce::AudioProcessorValueTreeState::ButtonAttachment effectsAtt_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> patchBox_, tempoBox_;
    void choosePatch();
    void chooseRom();
};
