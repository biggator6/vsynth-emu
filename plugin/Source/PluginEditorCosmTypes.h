#pragma once
// The COSM tab's type rows: each type's own controls and their attachments, which layoutCosmType places and showCosmType
// shows by the selected type.  Moved as they stood from VSynthEmuEditor (v190), which takes them as a base, so its code
// names them as before.
#include <juce_audio_processors/juce_audio_processors.h>

struct CosmTypeControls {
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
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
    // the Limiter: Ratio, then Threshold, Attack, Release, Output Level
    static constexpr int kLimKnobs = 4;
    juce::ComboBox   limRatioBox_;
    juce::Slider     limSliders_[kLimKnobs];
    juce::Label      limLabels_[kLimKnobs], limRowLabel_;
    std::unique_ptr<ComboAtt>  limRatioAtt_;
    std::unique_ptr<SliderAtt> limAtts_[kLimKnobs];
    // Wave Shape: Type, then Level, Drive and its depths; Tone, its KF and
    // depths
    static constexpr int kWsKnobs = 8;
    juce::ComboBox   wsTypeBox_;
    juce::Slider     wsSliders_[kWsKnobs];
    juce::Label      wsLabels_[kWsKnobs], wsRowLabels_[2];
    std::unique_ptr<ComboAtt>  wsTypeAtt_;
    std::unique_ptr<SliderAtt> wsAtts_[kWsKnobs];
    // OD/Dist: the same rows as Wave Shape's, its Type OD or DS
    juce::ComboBox   odTypeBox_;
    juce::Slider     odSliders_[kWsKnobs];
    juce::Label      odLabels_[kWsKnobs], odRowLabels_[2];
    std::unique_ptr<ComboAtt>  odTypeAtt_;
    std::unique_ptr<SliderAtt> odAtts_[kWsKnobs];
    // the Amp Simulator: Type, then Gain, its depths and Level; Bass,
    // Middle, Treble
    static constexpr int kAmpKnobs = 7;
    juce::ComboBox   ampTypeBox_;
    juce::Slider     ampSliders_[kAmpKnobs];
    juce::Label      ampLabels_[kAmpKnobs], ampRowLabels_[2];
    std::unique_ptr<ComboAtt>  ampTypeAtt_;
    std::unique_ptr<SliderAtt> ampAtts_[kAmpKnobs];
    // the Speaker Simulator: Speaker Type
    juce::ComboBox   spkTypeBox_;
    juce::Label      spkRowLabel_;
    std::unique_ptr<ComboAtt>  spkTypeAtt_;
};
