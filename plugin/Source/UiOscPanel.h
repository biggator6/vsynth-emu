#pragma once
// D's OSC panel (research/UI_DESIGN.md, "The rack"; UI step 2): a FacedPanel made for each OSC by its index, its faces
// OSC Type's (UiOscPanel.cpp's kFaces): the encoding or the analog waveform beside Type, four big knobs and six small.
// Besides: Load... and a line of Vari, Playback Mode and Tempo Sync for a PCM OSC; a display of the wave's name or the
// analog waveform.
#include "PluginProcessor.h"
#include "UiFacedPanel.h"

class OscPanel : public FacedPanel {
public:
    OscPanel(VSynthEmuProcessor& p, int osc);
    void refresh() override;   // and the wave's name, if it has moved
    static constexpr int kTypes = 2;         // OSC Type's values: ANALOG, PCM
    static constexpr int kTableKnobs = 10;   // the knobs its table gives each Type, each of which must name a parameter
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    const int osc_;
    juce::ToggleButton vari_ { "Vari" }, sync_ { "Sync" };
    juce::ComboBox playback_;
    juce::TextButton load_ { "Load..." };
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String wave_;
    juce::Rectangle<int> display_;

    juce::String id(const juce::String& name) const;
    void faceShown() override;
    void paintDisplay(juce::Graphics&) const override;
    void chooseWave();
};
