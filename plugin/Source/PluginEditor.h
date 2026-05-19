#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class VSynthEmuEditor : public juce::AudioProcessorEditor,
                        private juce::ComboBox::Listener
{
public:
    explicit VSynthEmuEditor(VSynthEmuProcessor& p);
    ~VSynthEmuEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VSynthEmuProcessor& processor_;

    // ── File loading ─────────────────────────────────────────────────────────
    juce::TextButton loadButton_     { "Load WAV..." };
    juce::Label      fileNameLabel_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    // ── Sliders ──────────────────────────────────────────────────────────────
    juce::Slider pitchSlider_   { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider timeSlider_    { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider formantSlider_ { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::ToggleButton robotBtn_{ "Robot" };

    juce::Label pitchLabel_   { {}, "PITCH" };
    juce::Label timeLabel_    { {}, "TIME" };
    juce::Label formantLabel_ { {}, "FORMANT" };

    // ── Algorithm selector ───────────────────────────────────────────────────
    juce::ComboBox algoBox_;
    juce::Label    algoLabel_ { {}, "Algorithm" };

    // ── APVTS attachments ────────────────────────────────────────────────────
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAtt> pitchAtt_, timeAtt_, formantAtt_;
    std::unique_ptr<ButtonAtt> robotAtt_;

    void comboBoxChanged(juce::ComboBox* box) override;
    void styleSlider(juce::Slider& s, juce::Label& l);
    void openFileChooser();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSynthEmuEditor)
};
