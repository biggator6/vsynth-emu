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
    // Loop: the OSC's Loop switch, the wave's Loop Mode and region
    juce::ToggleButton loopBtn_ { "Loop" };
    juce::ComboBox   loopModeBox_;
    juce::Slider     loopStartSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     loopEndSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label      loopStartLabel_ { {}, "Start" }, loopEndLabel_ { {}, "End" };
    // Portamento: switch, Mode, Type, Time, Time Velo Sens
    juce::ToggleButton portaBtn_ { "Porta" };
    juce::ComboBox   portaModeBox_, portaTypeBox_;
    juce::Slider     portaTimeSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     portaSensSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label      portaTimeLabel_ { {}, "Time" }, portaSensLabel_ { {}, "V-Sns" };
    // Key assign: Mono/Poly, Legato Switch, Octave Shift
    juce::ComboBox   monoBox_;
    juce::ToggleButton legatoBtn_ { "Legato" };
    juce::Slider     octSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label      octLabel_ { {}, "Oct" };
    // The COSM sections (TVF, Dynamic TVF, Dual, TB, Comb and Side Band Filters, Resonator, Frequency Shifter, Lo-Fi, Compressor), one at a time by the selector: switch,
    // Filter Type, Cutoff, Resonance, Cutoff KF; the depth and Env A rows;
    // the LFO (3 or 4)
    juce::ToggleButton cosmBtn_ { "On" };
    juce::ComboBox   cosmSelBox_, cosmFilterBox_, clfoWaveBox_, cosmTypeBox_;
    juce::Slider     cosmCutOwnSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     cosmResoOwnSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label      cosmTypeLabel_ { {}, "Type" }, cosmCutOwnLabel_ { {}, "C-Own" }, cosmResoOwnLabel_ { {}, "R-Own" };
    juce::Slider     cosmCutSlider_  { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     cosmResoSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     cosmKfSlider_   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider     clfoRateSlider_ { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton clfoSyncBtn_ { "Key Sync" };
    juce::Label      cosmCutLabel_ { {}, "Cut" }, cosmResoLabel_ { {}, "Reso" }, cosmKfLabel_ { {}, "KF" },
                     clfoLabel_ { {}, "LFO 3" }, clfoRateLabel_ { {}, "Rate" };
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
    // the OSC pitch, the patch's tuning, the OSC TVA, its amp envelope,
    // velocity to level, the COSM's depths and its cutoff envelope: eight
    // rows of four
    static constexpr int kLevelRows = 8, kLevelKnobs = 4 * kLevelRows;
    juce::Slider     levelSliders_[kLevelKnobs];
    juce::Label      levelLabels_[kLevelKnobs], levelRowLabels_[kLevelRows];
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
    std::unique_ptr<ButtonAtt> loopAtt_;
    std::unique_ptr<ComboAtt>  loopModeAtt_;
    std::unique_ptr<SliderAtt> loopStartAtt_, loopEndAtt_;
    std::unique_ptr<ButtonAtt> portaAtt_;
    std::unique_ptr<ComboAtt>  portaModeAtt_, portaTypeAtt_;
    std::unique_ptr<SliderAtt> portaTimeAtt_, portaSensAtt_;
    std::unique_ptr<ComboAtt>  monoAtt_;
    std::unique_ptr<ButtonAtt> legatoAtt_;
    std::unique_ptr<SliderAtt> octAtt_;
    std::unique_ptr<ButtonAtt> cosmAtt_, clfoSyncAtt_;
    std::unique_ptr<ComboAtt>  cosmFilterAtt_, clfoWaveAtt_, cosmTypeAtt_;
    std::unique_ptr<SliderAtt> cosmCutOwnAtt_, cosmResoOwnAtt_;
    std::unique_ptr<SliderAtt> cosmCutAtt_, cosmResoAtt_, cosmKfAtt_, clfoRateAtt_;
    void attachCosm(int k);             // COSM 1 (0) or 2 (1) on the COSM rows
    void attachSideBand();              // the 1st or 2nd Side Band Filter's set, by the type
    void showCosmType();                // only the selected type's rows
    void layoutCosm(juce::Rectangle<int>& xt);
    // the Dual Filter: its type (on the Own row) and a row per filter of
    // Frequency, Q, KF, LFO and Env depths
    static constexpr int kDualKnobs = 10;
    juce::ComboBox   dualTypeBox_;
    juce::Slider     dualSliders_[kDualKnobs];
    juce::Label      dualLabels_[kDualKnobs], dualRowLabels_[2];
    std::unique_ptr<ComboAtt>  dualTypeAtt_;
    std::unique_ptr<SliderAtt> dualAtts_[kDualKnobs];
    // the TB Filter: an LPF row (Cutoff, KF, Q, Q Follow, LFO, Env) and an
    // HPF row (Cutoff, KF, LFO, Env, Gain)
    static constexpr int kTbKnobs = 11;
    juce::Slider     tbSliders_[kTbKnobs];
    juce::Label      tbLabels_[kTbKnobs], tbRowLabels_[2];
    std::unique_ptr<SliderAtt> tbAtts_[kTbKnobs];
    // the Comb Filter: Octave, then Detune's row (Detune, LFO, Env) and
    // Tone's (Tone, LFO, Env)
    static constexpr int kCombKnobs = 6;
    juce::ComboBox   combOctBox_;
    juce::Slider     combSliders_[kCombKnobs];
    juce::Label      combLabels_[kCombKnobs], combRowLabels_[2];
    std::unique_ptr<ComboAtt>  combOctAtt_;
    std::unique_ptr<SliderAtt> combAtts_[kCombKnobs];
    // the Side Band Filters: Octave, then HPF (the 2nd's Q), LPF, KF, Sub
    // Detune; Detune, LFO, Own, Env; Width, KF, LFO, Own, Env
    static constexpr int kSbKnobs = 13;
    juce::ComboBox   sbOctBox_;
    juce::Slider     sbSliders_[kSbKnobs];
    juce::Label      sbLabels_[kSbKnobs], sbRowLabels_[3];
    std::unique_ptr<ComboAtt>  sbOctAtt_;
    std::unique_ptr<SliderAtt> sbAtts_[kSbKnobs];
    int cosmK_ = 0;                     // the COSM section on the rows
    juce::Label      tvfLabel_, dualLabel_; // the TVF's and Dual Filter's first rows
    // the Resonator: Filter Type, then Size's row (Size, KF, LFO, Env) and
    // Balance's (Balance, LFO, Env)
    static constexpr int kResKnobs = 7;
    juce::ComboBox   resTypeBox_;
    juce::Slider     resSliders_[kResKnobs];
    juce::Label      resLabels_[kResKnobs], resRowLabels_[2];
    std::unique_ptr<ComboAtt>  resTypeAtt_;
    std::unique_ptr<SliderAtt> resAtts_[kResKnobs];
    // the Frequency Shifter: Depth's row (Depth, KF, LFO, Env) and Balance's
    // (Balance, LFO, Env)
    static constexpr int kFsKnobs = 7;
    juce::Slider     fsSliders_[kFsKnobs];
    juce::Label      fsLabels_[kFsKnobs], fsRowLabels_[2];
    std::unique_ptr<SliderAtt> fsAtts_[kFsKnobs];
    // Lo-Fi: Bit, Rate, Balance and its depths; the Filter and its KF and
    // depths
    static constexpr int kLofiKnobs = 9;
    juce::Slider     lofiSliders_[kLofiKnobs];
    juce::Label      lofiLabels_[kLofiKnobs], lofiRowLabels_[2];
    std::unique_ptr<SliderAtt> lofiAtts_[kLofiKnobs];
    // the Compressor: Attack, Sustain, Output Level
    static constexpr int kCompKnobs = 3;
    juce::Slider     compSliders_[kCompKnobs];
    juce::Label      compLabels_[kCompKnobs], compRowLabel_;
    std::unique_ptr<SliderAtt> compAtts_[kCompKnobs];
    std::unique_ptr<SliderAtt> lfoRateAtt_, lfoDepthAtt_, lfoDelayAtt_, lfoFadeAtt_, lfoTimeAtt_, lfoFmtAtt_;
    std::unique_ptr<SliderAtt> levelAtts_[kLevelKnobs];
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
