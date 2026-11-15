#pragma once
// D's OSC panel (research/UI_DESIGN.md, "The rack"; UI step 2): a FacedPanel made for each OSC by its index, its faces
// OSC Type's (UiOscPanel.cpp's kFaces): the encoding or the analog waveform beside Type, four big knobs and six small.
// Besides: Load... and a line of Vari, Playback Mode and Tempo Sync for a PCM OSC; a display of what it plays (v200,
// UiWaveDisplay): the loaded wave and its loop, or the analog oscillator's own cycles at the panel's settings.
#include "PluginProcessor.h"
#include "UiFacedPanel.h"
#include "UiWaveDisplay.h"
#include <memory>
#include <vector>

class OscPanel : public FacedPanel {
public:
    OscPanel(VSynthEmuProcessor& p, int osc);
    // and the wave's name, its view, its loop or the analog cycles, or whether the host gives an input, if any has moved
    void refresh() override;
    static constexpr int kTypes = 3;         // OSC Type's values: ANALOG, PCM, EXT IN (v206)
    static constexpr int kTableKnobs = 10;   // the knobs its table gives each Type, each of which must name a parameter
    void resized() override;
    // What its EXT IN display says under the input's Type when the host gives no input, as the processor finds it on
    // every paint (v207); empty when there is one
    juce::String noInputWord() const;

    // What the display draws now, for the checks: the analog cycles, and the wave's view
    const std::vector<float>& cycles() const { return cycles_; }
    const WaveDisplay::View* view() const { return view_.get(); }

private:
    VSynthEmuProcessor& processor_;
    const int osc_;
    juce::ToggleButton vari_ { "Vari" }, sync_ { "Sync" };
    juce::ComboBox playback_;
    juce::TextButton load_ { "Load..." };
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String wave_;
    bool input_ = true;   // whether the host gave an input at the last refresh
    juce::Rectangle<int> display_;
    std::shared_ptr<const WaveDisplay::View> view_;   // the wave as drawn, the processor's at viewGen_
    int viewGen_ = -1;
    WaveDisplay::Loop loop_;
    std::vector<float> cycles_;                       // the analog cycles as drawn, made from drawn_
    WaveDisplay::AnalogSettings drawn_;

    void refreshWave();
    void refreshCycles();
    juce::String id(const juce::String& name) const;
    void faceShown() override;
    void paintDisplay(juce::Graphics&) const override;
    void chooseWave();
};
