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

    // ── The XT strip: firmware image, encode type and depth, key, the OSC ───
    juce::TextButton romButton_ { "ROM..." };
    juce::Label      statusLabel_;
    juce::ComboBox   encodeBox_;
    juce::Slider     keySlider_     { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Slider     depthSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     energySlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    // the VariPhrase OSC: Time Offset, Time KF, Formant KF (Time and Formant
    // themselves are the TIME and FORMANT knobs while the XT plays)
    juce::ComboBox   timeOffsetBox_;
    juce::Slider     oscTkfSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     oscFkfSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    // amp envelope, the XT's 0..127 knobs
    juce::Slider     attackSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     decaySlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     sustainSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     releaseSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     aSensSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     dSensSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     rSensSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     kfSlider_      { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    // pitch envelope
    juce::Slider     pAttackSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     pDecaySlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     pSustainSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     pReleaseSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     pDepthSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    // time and formant envelopes
    juce::Slider     tAttackSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     tDecaySlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     tSustainSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     tReleaseSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     tDepthSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     fAttackSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     fDecaySlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     fSustainSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     fReleaseSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     fDepthSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    // LFO
    juce::ComboBox   lfoWaveBox_, lfoModeBox_, lfoOffsetBox_;
    juce::ToggleButton lfoSyncBtn_ { "Key Sync" };
    juce::Slider     lfoRateSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     lfoDepthSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     lfoDelaySlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     lfoFadeSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     lfoTimeSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     lfoFmtSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label encodeLabel_ { {}, "Encode" }, keyLabel_ { {}, "Key" }, depthLabel_ { {}, "Depth" },
                energyLabel_ { {}, "Energy" }, attackLabel_ { {}, "A" }, decayLabel_ { {}, "D" },
                sustainLabel_ { {}, "S" }, releaseLabel_ { {}, "R" }, aSensLabel_ { {}, "A-Sns" },
                dSensLabel_ { {}, "D-Sns" }, rSensLabel_ { {}, "R-Sns" }, kfLabel_ { {}, "T-KF" },
                pAttackLabel_ { {}, "PA" }, pDecayLabel_ { {}, "PD" }, pSustainLabel_ { {}, "PS" },
                pReleaseLabel_ { {}, "PR" }, pDepthLabel_ { {}, "PDep" }, lfoLabel_ { {}, "LFO" },
                lfoRateLabel_ { {}, "Rate" }, lfoDepthLabel_ { {}, "Pitch" }, lfoDelayLabel_ { {}, "Delay" },
                lfoFadeLabel_ { {}, "Fade" }, lfoTimeLabel_ { {}, "Time" }, lfoFmtLabel_ { {}, "Fmt" },
                lfoDepthsLabel_ { {}, "LFO Dp" }, offsetLabel_ { {}, "Offset" }, oscTkfLabel_ { {}, "Time KF" },
                oscFkfLabel_ { {}, "Fmt KF" },
                tAttackLabel_ { {}, "TA" }, tDecayLabel_ { {}, "TD" }, tSustainLabel_ { {}, "TS" },
                tReleaseLabel_ { {}, "TR" }, tDepthLabel_ { {}, "TDep" },
                fAttackLabel_ { {}, "FA" }, fDecayLabel_ { {}, "FD" }, fSustainLabel_ { {}, "FS" },
                fReleaseLabel_ { {}, "FR" }, fDepthLabel_ { {}, "FDep" };
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
    std::unique_ptr<SliderAtt> keyAtt_, depthAtt_, energyAtt_, attackAtt_, decayAtt_, sustainAtt_, releaseAtt_;
    std::unique_ptr<SliderAtt> aSensAtt_, dSensAtt_, rSensAtt_, kfAtt_;
    std::unique_ptr<SliderAtt> pAttackAtt_, pDecayAtt_, pSustainAtt_, pReleaseAtt_, pDepthAtt_;
    std::unique_ptr<SliderAtt> tAttackAtt_, tDecayAtt_, tSustainAtt_, tReleaseAtt_, tDepthAtt_;
    std::unique_ptr<SliderAtt> fAttackAtt_, fDecayAtt_, fSustainAtt_, fReleaseAtt_, fDepthAtt_;
    std::unique_ptr<SliderAtt> oscTkfAtt_, oscFkfAtt_;
    std::unique_ptr<ComboAtt>  timeOffsetAtt_;
    std::unique_ptr<SliderAtt> lfoRateAtt_, lfoDepthAtt_, lfoDelayAtt_, lfoFadeAtt_, lfoTimeAtt_, lfoFmtAtt_;
    std::unique_ptr<ComboAtt>  lfoWaveAtt_, lfoModeAtt_, lfoOffsetAtt_;
    std::unique_ptr<ButtonAtt> lfoSyncAtt_;
    std::unique_ptr<ComboAtt>  encodeAtt_;

    // TIME and FORMANT: the XT's Time and Formant (−63..+63) with a ROM, the
    // engine's stretch and semitones without
    bool mainKnobsXt_ = false;
    void attachMainKnobs(bool xt);

    void comboBoxChanged(juce::ComboBox* box) override;
    void styleSlider(juce::Slider& s, juce::Label& l);
    void openFileChooser();
    void openRomChooser();
    void timerCallback() override;
    void styleSmall(juce::Label& l);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSynthEmuEditor)
};
