#pragma once
// D's OSC panel (research/UI_DESIGN.md, "The rack"; UI step 2): a FacedPanel made for each OSC by its index, its faces
// OSC Type's (UiOscPanel.cpp's kFaces): the encoding or the analog waveform beside Type, four big knobs and six small.
// Besides: Load... and a line of Vari, Playback Mode and Tempo Sync for a PCM OSC; a display of what it plays (v200,
// UiWaveDisplay): the loaded wave and its loop, or the analog oscillator's own cycles at the panel's settings.  Its faces
// and what it draws are a mode's (v453): the V-Synth's, or the VC-2's for a zone (UiVc2Rack.cpp).
#include "PluginProcessor.h"
#include "UiFacedPanel.h"
#include "UiWaveDisplay.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

class OscPanel : public FacedPanel, private juce::AudioProcessorParameter::Listener {
public:
    static constexpr int kBig = 4, kSmall = 6;
    // What a Type shows: the box beside Type (none: empty), the big knobs and the small, as a mode's rows
    struct Face {
        const char* sub;
        std::array<Controls::KnobRow, kBig> big;
        std::array<Controls::KnobRow, kSmall> small;
    };
    // A mode's OSC panel: its faces in OSC Type's order, which is PCM (its line and its wave), EXT IN and VOCODER (-1:
    // none); an OSC's own ID of a row; its switch's and Type's rows, the PCM line's (Vari, Playback Mode, Tempo Sync) and
    // External In Type's ID; whether a PCM OSC loads a file (the V-Synth's); the analog cycles' settings now; and what a
    // display draws that isn't D's (a PCM wave it doesn't load, the VOCODER OSC's carrier)
    struct Model {
        const Face* faces = nullptr;
        int count = 0, pcm = 1, extIn = 2, vocoder = -1;
        std::function<juce::String(int osc, const juce::String& row)> id;
        juce::String switchRow, typeRow;
        std::array<juce::String, 3> line {};
        juce::String extInType;
        bool loads = true;
        std::function<WaveDisplay::AnalogSettings(int osc)> analog;
        std::function<void(juce::Graphics&, juce::Rectangle<int>, int osc, int face)> draw;
    };
    OscPanel(VSynthEmuProcessor& p, int osc);   // the V-Synth's
    OscPanel(VSynthEmuProcessor& p, int osc, Model model);
    ~OscPanel() override;
    // and the wave's name, its view, its loop or the analog cycles, or whether the host gives an input, if any has moved
    void refresh() override;
    static constexpr int kTypes = 3;         // the V-Synth's OSC Type's values: ANALOG, PCM, EXT IN (v206)
    static constexpr int kTableKnobs = 10;   // the knobs its table gives each Type, each of which must name a parameter
    int faces() const { return model_.count; }
    void resized() override;
    // What its EXT IN display says under the input's Type when the host gives no input, as the processor finds it on
    // every paint (v207); empty when there is one
    juce::String noInputWord() const;

    // What the display draws now, for the checks: the analog cycles, and the wave's view
    const std::vector<float>& cycles() const { return cycles_; }
    const WaveDisplay::View* view() const { return view_.get(); }
    // Whether the display draws the Start Offset's line: while that parameter's gesture runs (its knob held, a drag on
    // the WAVE page, a host's), the user's item 5 (v479)
    bool drawsStartLine() const { return lineAt_ >= 0; }

private:
    VSynthEmuProcessor& processor_;
    const int osc_;
    Model model_;
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
    juce::String shown_;                              // what a mode's own drawing was made from
    juce::RangedAudioParameter* start_ = nullptr;     // its Start Offset (a PCM OSC that loads its wave, v479)
    std::atomic<bool> startHeld_ { false };           // its gesture running, as the parameter says on any thread
    int lineAt_ = -1;                                 // the line as drawn: the Start Offset, or -1 for none
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { startHeld_.store(starting); }

    void refreshWave();
    void refreshCycles();
    juce::String id(const juce::String& name) const;
    std::vector<juce::Component*> pcmLine();
    void faceShown() override;
    void paintDisplay(juce::Graphics&) const override;
    void paintExtIn(juce::Graphics&) const;
    void chooseWave();
};
