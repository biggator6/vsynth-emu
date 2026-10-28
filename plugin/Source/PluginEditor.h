#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class VSynthEmuEditor : public juce::AudioProcessorEditor,
                        private juce::ComboBox::Listener,
                        private juce::Timer
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

    // ── The XT strip: firmware image, encode type and depth, key, energy ────
    juce::TextButton romButton_ { "ROM..." };
    juce::Label      statusLabel_;
    juce::ComboBox   encodeBox_;
    juce::Slider     keySlider_     { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Slider     depthSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     energySlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     releaseSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label encodeLabel_ { {}, "Encode" }, keyLabel_ { {}, "Key" }, depthLabel_ { {}, "Depth" },
                energyLabel_ { {}, "Energy" }, releaseLabel_ { {}, "Release" };
    std::unique_ptr<juce::FileChooser> romChooser_;

    // ── Algorithm selector ───────────────────────────────────────────────────
    juce::ComboBox algoBox_;
    juce::Label    algoLabel_ { {}, "Algorithm" };

    // ── APVTS attachments ────────────────────────────────────────────────────
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAtt> pitchAtt_, timeAtt_, formantAtt_;
    std::unique_ptr<ButtonAtt> robotAtt_;
    std::unique_ptr<SliderAtt> keyAtt_, depthAtt_, energyAtt_, releaseAtt_;
    std::unique_ptr<ComboAtt>  encodeAtt_;

    void comboBoxChanged(juce::ComboBox* box) override;
    void styleSlider(juce::Slider& s, juce::Label& l);
    void openFileChooser();
    void openRomChooser();
    void timerCallback() override;
    void styleSmall(juce::Label& l);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSynthEmuEditor)
};
