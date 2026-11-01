#include "PluginEditor.h"

static constexpr int kFileStripH = 44;   // height of the load-file strip at the top
static constexpr int kXtStripH   = 1030;  // height of the XT strip below it

// The OSC pitch, patch tuning, level, velocity and COSM rows: parameter and
// label, four per row, and the rows' names
static const char* const kLevelIds[32][2] = {
    { "oscCoarse", "Crs" },        { "oscFine", "Fine" },           { "randomPitch", "Rnd" },        { "pitchKF", "P-KF" },
    { "patchCoarse", "Crs" },      { "patchFine", "Fine" },         { "bendRangeUp", "B-Up" },       { "bendRangeDown", "B-Dn" },
    { "oscLevel", "Lvl" },         { "oscLevelKF", "KF" },          { "oscLevelLfoDepth", "LFO" },   { "waveGain", "Gain" },
    { "oscEnvAttack", "A" },       { "oscEnvDecay", "D" },          { "oscEnvSustain", "S" },        { "oscEnvRelease", "R" },
    { "oscEnvAttackSens", "A-Sns" }, { "oscEnvDecaySens", "D-Sns" }, { "oscEnvReleaseSens", "R-Sns" }, { "oscEnvTimeKF", "T-KF" },
    { "ampVelSens", "Sens" },      { "ampVelCurve", "Curve" },      { "oscVelSens", "O Sens" },      { "oscVelCurve", "O Curve" },
    { "cosmCutoffEnv", "C-Env" },  { "cosmCutoffLfo", "C-LFO" },    { "cosmResoEnv", "R-Env" },      { "cosmResoLfo", "R-LFO" },
    { "cosmEnvAAttack", "A" },     { "cosmEnvADecay", "D" },        { "cosmEnvASustain", "S" },      { "cosmEnvARelease", "R" },
};
static const char* const kLevelRowNames[8] = { "OSC Pitch", "Tune", "OSC TVA", "O Env", "O Env", "Velocity", "COSM Mod", "COSM Env" };
// The Dual Filter's rows: the id suffixes after "cosmDual1"/"cosmDual2", and labels
static const char* const kDualIds[5][2] = { { "Freq", "Frq" }, { "Q", "Q" }, { "KF", "KF" }, { "Lfo", "LFO" }, { "Env", "Env" } };
// The TB Filter's rows (6 LPF, 5 HPF): the id suffixes after "cosm"/"cosm2", and labels
static const char* const kTbIds[11][2] = { { "TbCutoff", "Cut" }, { "TbKF", "KF" }, { "TbQ", "Q" }, { "TbQFollow", "QF" }, { "TbLfo", "LFO" },
                                           { "TbEnv", "Env" }, { "TbHpf", "Cut" }, { "TbHpfKF", "KF" }, { "TbHpfLfo", "LFO" }, { "TbHpfEnv", "Env" },
                                           { "TbGain", "Gain" } };
// The Comb Filter's rows (Detune, Tone): the id suffixes after "cosm"/"cosm2", and labels
static const char* const kCombIds[6][2] = { { "CombDetune", "Det" }, { "CombDetuneLfo", "LFO" }, { "CombDetuneEnv", "Env" },
                                            { "CombTone", "Tone" }, { "CombToneLfo", "LFO" }, { "CombToneEnv", "Env" } };
// The Side Band Filters' rows (HPF or Q, LPF, KF, Sub; Detune; Width): the id
// suffixes after "cosmSb1"/"cosmSb2" (the 2nd's Q for the 1st's Hpf, and no
// SubDetune), and labels
static const char* const kSbIds[13][2] = { { "Hpf", "HPF" }, { "Lpf", "LPF" }, { "LpfKF", "KF" }, { "SubDetune", "Sub" },
                                           { "Detune", "Det" }, { "DetuneLfo", "LFO" }, { "DetuneOwn", "Own" }, { "DetuneEnv", "Env" },
                                           { "Width", "Wid" }, { "WidthKF", "KF" }, { "WidthLfo", "LFO" }, { "WidthOwn", "Own" },
                                           { "WidthEnv", "Env" } };
static constexpr int kAlgoStripH = 52;   // height of the algorithm strip at the bottom

VSynthEmuEditor::VSynthEmuEditor(VSynthEmuProcessor& p)
    : AudioProcessorEditor(&p), processor_(p)
{
    setSize(560, 1330);
    setResizable(true, true);
    setResizeLimits(480, 780, 960, 1470);

    // ── File strip ────────────────────────────────────────────────────────────
    loadButton_.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xFF00BFA5));
    loadButton_.setColour(juce::TextButton::textColourOffId,  juce::Colours::black);
    loadButton_.onClick = [this] { openFileChooser(); };
    addAndMakeVisible(loadButton_);

    fileNameLabel_.setText(
        processor_.hasFileLoaded() ? processor_.getLoadedFileName() : "No file loaded",
        juce::dontSendNotification);
    fileNameLabel_.setFont(juce::Font(13.0f));
    fileNameLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    fileNameLabel_.setJustificationType(juce::Justification::centredLeft);
    fileNameLabel_.setMinimumHorizontalScale(0.5f);
    addAndMakeVisible(fileNameLabel_);

    // ── XT strip ──────────────────────────────────────────────────────────────
    romButton_.onClick = [this] { openRomChooser(); };
    addAndMakeVisible(romButton_);
    statusLabel_.setFont(juce::Font(12.0f));
    statusLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel_.setMinimumHorizontalScale(0.5f);
    addAndMakeVisible(statusLabel_);
    encodeBox_.addItemList({ "Auto", "SOLO", "BACKING", "ENSEMBLE" }, 1);
    addAndMakeVisible(encodeBox_);
    encodeAtt_  = std::make_unique<ComboAtt>(processor_.apvts, "encodeType", encodeBox_);
    for (juce::Slider* sl : { &keySlider_, &depthSlider_, &energySlider_, &attackSlider_, &decaySlider_, &sustainSlider_, &releaseSlider_,
                              &aSensSlider_, &dSensSlider_, &rSensSlider_, &kfSlider_, &oscTkfSlider_, &oscFkfSlider_,
                              &pAttackSlider_, &pDecaySlider_, &pSustainSlider_, &pReleaseSlider_, &pDepthSlider_,
                              &tAttackSlider_, &tDecaySlider_, &tSustainSlider_, &tReleaseSlider_, &tDepthSlider_,
                              &fAttackSlider_, &fDecaySlider_, &fSustainSlider_, &fReleaseSlider_, &fDepthSlider_,
                              &lfoRateSlider_, &lfoDepthSlider_, &lfoDelaySlider_, &lfoFadeSlider_, &lfoTimeSlider_, &lfoFmtSlider_,
                              &loopStartSlider_, &loopEndSlider_, &portaTimeSlider_, &portaSensSlider_, &octSlider_,
                              &cosmCutSlider_, &cosmResoSlider_, &cosmKfSlider_, &clfoRateSlider_, &cosmCutOwnSlider_,
                              &cosmResoOwnSlider_ }) {
        sl->setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl->setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        sl->setTextBoxStyle(sl->getTextBoxPosition(), false, sl == &keySlider_ ? 56 : 44, 20);
        addAndMakeVisible(*sl);
    }
    keyAtt_     = std::make_unique<SliderAtt>(processor_.apvts, "rootKey",     keySlider_);
    depthAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "encodeDepth", depthSlider_);
    energyAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "energy",      energySlider_);
    attackAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "ampAttack",   attackSlider_);
    decayAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "ampDecay",    decaySlider_);
    sustainAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "ampSustain",  sustainSlider_);
    releaseAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "ampRelease",  releaseSlider_);
    aSensAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "ampAttackSens",  aSensSlider_);
    dSensAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "ampDecaySens",   dSensSlider_);
    rSensAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "ampReleaseSens", rSensSlider_);
    kfAtt_      = std::make_unique<SliderAtt>(processor_.apvts, "ampTimeKF",      kfSlider_);
    pAttackAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "pitchEnvAttack",  pAttackSlider_);
    pDecayAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "pitchEnvDecay",   pDecaySlider_);
    pSustainAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "pitchEnvSustain", pSustainSlider_);
    pReleaseAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "pitchEnvRelease", pReleaseSlider_);
    pDepthAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "pitchEnvDepth",   pDepthSlider_);
    tAttackAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "timeEnvAttack",   tAttackSlider_);
    tDecayAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "timeEnvDecay",    tDecaySlider_);
    tSustainAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "timeEnvSustain",  tSustainSlider_);
    tReleaseAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "timeEnvRelease",  tReleaseSlider_);
    tDepthAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "timeEnvDepth",    tDepthSlider_);
    fAttackAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "formantEnvAttack",  fAttackSlider_);
    fDecayAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "formantEnvDecay",   fDecaySlider_);
    fSustainAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "formantEnvSustain", fSustainSlider_);
    fReleaseAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "formantEnvRelease", fReleaseSlider_);
    fDepthAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "formantEnvDepth",   fDepthSlider_);
    oscTkfAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "oscTimeKF",    oscTkfSlider_);
    oscFkfAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "oscFormantKF", oscFkfSlider_);
    timeOffsetBox_.addItemList({ "BWD", "ZERO", "FWD" }, 1);
    addAndMakeVisible(timeOffsetBox_);
    timeOffsetAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "oscTimeOffset", timeOffsetBox_);
    loopModeBox_.addItemList({ "One Shot", "One Way", "Alternate" }, 1);
    loopBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    loopBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    addAndMakeVisible(loopBtn_);
    addAndMakeVisible(loopModeBox_);
    loopAtt_      = std::make_unique<ButtonAtt>(processor_.apvts, "loop", loopBtn_);
    loopModeAtt_  = std::make_unique<ComboAtt>(processor_.apvts, "loopMode", loopModeBox_);
    loopStartAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "loopStart", loopStartSlider_);
    loopEndAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "loopEnd", loopEndSlider_);
    portaModeBox_.addItemList({ "NORMAL", "LEGATO" }, 1);
    portaTypeBox_.addItemList({ "RATE", "TIME" }, 1);
    portaBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    portaBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    for (juce::Component* cpt : { (juce::Component*)&portaBtn_, (juce::Component*)&portaModeBox_, (juce::Component*)&portaTypeBox_ })
        addAndMakeVisible(*cpt);
    portaAtt_     = std::make_unique<ButtonAtt>(processor_.apvts, "portamento", portaBtn_);
    portaModeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "portaMode", portaModeBox_);
    portaTypeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "portaType", portaTypeBox_);
    portaTimeAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "portaTime", portaTimeSlider_);
    portaSensAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "portaVeloSens", portaSensSlider_);
    monoBox_.addItemList({ "MONO", "POLY" }, 1);
    legatoBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    legatoBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    addAndMakeVisible(monoBox_);
    addAndMakeVisible(legatoBtn_);
    monoAtt_   = std::make_unique<ComboAtt>(processor_.apvts, "monoPoly", monoBox_);
    legatoAtt_ = std::make_unique<ButtonAtt>(processor_.apvts, "legatoSwitch", legatoBtn_);
    octAtt_    = std::make_unique<SliderAtt>(processor_.apvts, "octaveShift", octSlider_);
    cosmFilterBox_.addItemList({ "LP24", "BP24", "HP24", "NOTCH24", "PEAK24", "LP12", "BP12", "HP12", "NOTCH12", "PEAK12", "LP6", "HP6" }, 1);
    clfoWaveBox_.addItemList({ "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 1);
    for (juce::ToggleButton* b : { &cosmBtn_, &clfoSyncBtn_ }) {
        b->setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
        b->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    }
    cosmTypeBox_.addItemList({ "TVF", "Dynamic TVF", "Dual Filter", "TB Filter", "Comb Filter", "1st Side Band", "2nd Side Band" }, 1);
    cosmTypeBox_.onChange = [this] { attachSideBand(); };
    for (juce::Component* cpt : { (juce::Component*)&cosmSelBox_, (juce::Component*)&cosmBtn_, (juce::Component*)&cosmFilterBox_,
                                  (juce::Component*)&clfoWaveBox_, (juce::Component*)&clfoSyncBtn_, (juce::Component*)&cosmTypeBox_ })
        addAndMakeVisible(*cpt);
    cosmSelBox_.addItemList({ "COSM 1", "COSM 2" }, 1);
    cosmSelBox_.setSelectedId(1, juce::dontSendNotification);
    cosmSelBox_.onChange = [this] { attachCosm(cosmSelBox_.getSelectedId() == 2 ? 1 : 0); };
    lfoWaveBox_.addItemList({ "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 1);
    lfoModeBox_.addItemList({ "ON <<", "ON >>", "OFF <<", "OFF >>" }, 1);
    lfoOffsetBox_.addItemList({ "-100", "-50", "0", "+50", "+100" }, 1);
    for (juce::Component* cpt : { (juce::Component*)&lfoWaveBox_, (juce::Component*)&lfoModeBox_,
                                   (juce::Component*)&lfoOffsetBox_, (juce::Component*)&lfoSyncBtn_ }) addAndMakeVisible(*cpt);
    lfoWaveAtt_   = std::make_unique<ComboAtt>(processor_.apvts, "lfoWave", lfoWaveBox_);
    lfoModeAtt_   = std::make_unique<ComboAtt>(processor_.apvts, "lfoFadeMode", lfoModeBox_);
    lfoOffsetAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "lfoOffset", lfoOffsetBox_);
    lfoSyncAtt_   = std::make_unique<ButtonAtt>(processor_.apvts, "lfoKeySync", lfoSyncBtn_);
    lfoRateAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "lfoRate", lfoRateSlider_);
    lfoDepthAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "lfoPitchDepth", lfoDepthSlider_);
    lfoDelayAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "lfoDelay", lfoDelaySlider_);
    lfoFadeAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "lfoFade", lfoFadeSlider_);
    lfoTimeAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "lfoTimeDepth", lfoTimeSlider_);
    lfoFmtAtt_    = std::make_unique<SliderAtt>(processor_.apvts, "lfoFormantDepth", lfoFmtSlider_);
    for (int i = 0; i < kLevelKnobs; ++i) {
        juce::Slider& sl = levelSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        levelAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, kLevelIds[i][0], sl);
        levelLabels_[i].setText(kLevelIds[i][1], juce::dontSendNotification);
        styleSmall(levelLabels_[i]);
    }
    for (int r = 0; r < kLevelRows; ++r) {
        levelRowLabels_[r].setText(kLevelRowNames[r], juce::dontSendNotification);
        styleSmall(levelRowLabels_[r]);
    }
    dualTypeBox_.addItemList({ "LPF/HPF", "LPF->HPF", "BPF/BPF" }, 1);
    addAndMakeVisible(dualTypeBox_);
    for (int i = 0; i < kDualKnobs; ++i) {
        juce::Slider& sl = dualSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        dualLabels_[i].setText(kDualIds[i % 5][1], juce::dontSendNotification);
        styleSmall(dualLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        dualRowLabels_[r].setText(r ? "Filter 2" : "Filter 1", juce::dontSendNotification);
        styleSmall(dualRowLabels_[r]);
    }
    for (int i = 0; i < kTbKnobs; ++i) {
        juce::Slider& sl = tbSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        tbLabels_[i].setText(kTbIds[i][1], juce::dontSendNotification);
        styleSmall(tbLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        tbRowLabels_[r].setText(r ? "TB HPF" : "TB LPF", juce::dontSendNotification);
        styleSmall(tbRowLabels_[r]);
    }
    combOctBox_.addItemList({ "-2", "-1", "0", "+1", "+2" }, 1);
    addAndMakeVisible(combOctBox_);
    for (int i = 0; i < kCombKnobs; ++i) {
        juce::Slider& sl = combSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        combLabels_[i].setText(kCombIds[i][1], juce::dontSendNotification);
        styleSmall(combLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        combRowLabels_[r].setText(r ? "Comb T" : "Comb", juce::dontSendNotification);
        styleSmall(combRowLabels_[r]);
    }
    sbOctBox_.addItemList({ "-2", "-1", "0" }, 1);
    addAndMakeVisible(sbOctBox_);
    for (int i = 0; i < kSbKnobs; ++i) {
        juce::Slider& sl = sbSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        sbLabels_[i].setText(kSbIds[i][1], juce::dontSendNotification);
        styleSmall(sbLabels_[i]);
    }
    for (int r = 0; r < 3; ++r) {
        sbRowLabels_[r].setText(r == 0 ? "SB" : r == 1 ? "SB Det" : "SB Wid", juce::dontSendNotification);
        styleSmall(sbRowLabels_[r]);
    }
    attachCosm(0);
    for (juce::Label* l : { &encodeLabel_, &keyLabel_, &depthLabel_, &energyLabel_, &attackLabel_, &decayLabel_,
                            &sustainLabel_, &releaseLabel_, &aSensLabel_, &dSensLabel_, &rSensLabel_, &kfLabel_,
                            &pAttackLabel_, &pDecayLabel_, &pSustainLabel_, &pReleaseLabel_, &pDepthLabel_,
                            &tAttackLabel_, &tDecayLabel_, &tSustainLabel_, &tReleaseLabel_, &tDepthLabel_,
                            &fAttackLabel_, &fDecayLabel_, &fSustainLabel_, &fReleaseLabel_, &fDepthLabel_,
                            &offsetLabel_, &oscTkfLabel_, &oscFkfLabel_,
                            &lfoLabel_, &lfoRateLabel_, &lfoDepthLabel_, &lfoDelayLabel_, &lfoFadeLabel_,
                            &lfoDepthsLabel_, &lfoTimeLabel_, &lfoFmtLabel_, &loopStartLabel_, &loopEndLabel_,
                            &portaTimeLabel_, &portaSensLabel_, &octLabel_, &cosmCutLabel_, &cosmResoLabel_, &cosmKfLabel_,
                            &clfoLabel_, &clfoRateLabel_, &cosmTypeLabel_, &cosmCutOwnLabel_, &cosmResoOwnLabel_ }) styleSmall(*l);
    timerCallback();
    startTimerHz(4);

    // ── Knobs ─────────────────────────────────────────────────────────────────
    styleSlider(pitchSlider_,   pitchLabel_);
    styleSlider(timeSlider_,    timeLabel_);
    styleSlider(formantSlider_, formantLabel_);

    attachMainKnobs(processor_.hasRom());
    robotAtt_   = std::make_unique<ButtonAtt>(processor_.apvts, "robot",        robotBtn_);

    robotBtn_.setColour(juce::ToggleButton::textColourId,     juce::Colours::lightblue);
    robotBtn_.setColour(juce::ToggleButton::tickColourId,     juce::Colour(0xFF00BFA5));
    addAndMakeVisible(robotBtn_);

    // ── Algorithm selector ────────────────────────────────────────────────────
    algoBox_.addItem("Passthrough (baseline)",         1);
    algoBox_.addItem("Phase Vocoder",                  2);
    algoBox_.addItem("Sinusoidal + Residual (TODO)",   3);
    algoBox_.addItem("LPC Source-Filter (TODO)",       4);
    algoBox_.addItem("Hybrid (TODO)",                  5);
    algoBox_.setSelectedId(1, juce::dontSendNotification);
    algoBox_.addListener(this);

    algoLabel_.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    algoLabel_.setFont(juce::Font(13.0f));

    addAndMakeVisible(algoBox_);
    addAndMakeVisible(algoLabel_);
}

VSynthEmuEditor::~VSynthEmuEditor() {
    stopTimer();
    algoBox_.removeListener(this);
}

void VSynthEmuEditor::attachMainKnobs(bool xt) {
    pitchAtt_.reset();
    timeAtt_.reset();
    formantAtt_.reset();
    // with the XT, the OSC's Pitch, Time and Formant knobs (Pitch Shift stays
    // a host parameter, added to the patch's Coarse/Fine Tune)
    pitchAtt_   = std::make_unique<SliderAtt>(processor_.apvts, xt ? "oscPitch" : "pitchShift", pitchSlider_);
    timeAtt_    = std::make_unique<SliderAtt>(processor_.apvts, xt ? "oscTime" : "timeStretch", timeSlider_);
    formantAtt_ = std::make_unique<SliderAtt>(processor_.apvts, xt ? "oscFormant" : "formantShift", formantSlider_);
    mainKnobsXt_ = xt;
}

// The COSM rows show one section: its switch, type, Cutoff, Resonance, KF,
// the depth and Env A rows (level slots 24..31), the Dual, TB, Comb and Side
// Band Filters' rows and its LFO.
void VSynthEmuEditor::attachCosm(int k) {
    const juce::String pre = k ? "cosm2" : "cosm";
    cosmK_ = k;
    cosmAtt_.reset(); cosmFilterAtt_.reset(); cosmCutAtt_.reset(); cosmResoAtt_.reset(); cosmKfAtt_.reset();
    cosmTypeAtt_.reset(); cosmCutOwnAtt_.reset(); cosmResoOwnAtt_.reset();
    cosmTypeAtt_    = std::make_unique<ComboAtt>(processor_.apvts, pre + "Type", cosmTypeBox_);
    cosmCutOwnAtt_  = std::make_unique<SliderAtt>(processor_.apvts, pre + "CutoffOwn", cosmCutOwnSlider_);
    cosmResoOwnAtt_ = std::make_unique<SliderAtt>(processor_.apvts, pre + "ResoOwn", cosmResoOwnSlider_);
    clfoWaveAtt_.reset(); clfoRateAtt_.reset(); clfoSyncAtt_.reset();
    cosmAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, pre + "Switch", cosmBtn_);
    cosmFilterAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "Filter", cosmFilterBox_);
    cosmCutAtt_    = std::make_unique<SliderAtt>(processor_.apvts, pre + "Cutoff", cosmCutSlider_);
    cosmResoAtt_   = std::make_unique<SliderAtt>(processor_.apvts, pre + "Reso", cosmResoSlider_);
    cosmKfAtt_     = std::make_unique<SliderAtt>(processor_.apvts, pre + "CutoffKF", cosmKfSlider_);
    clfoWaveAtt_   = std::make_unique<ComboAtt>(processor_.apvts, pre + "LfoWave", clfoWaveBox_);
    clfoRateAtt_   = std::make_unique<SliderAtt>(processor_.apvts, pre + "LfoRate", clfoRateSlider_);
    clfoSyncAtt_   = std::make_unique<ButtonAtt>(processor_.apvts, pre + "LfoKeySync", clfoSyncBtn_);
    for (int i = 24; i < kLevelKnobs; ++i) {            // "cosm…" ids: the suffix after "cosm"
        levelAtts_[i].reset();
        levelAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + juce::String(kLevelIds[i][0] + 4), levelSliders_[i]);
    }
    dualTypeAtt_.reset();
    dualTypeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "DualType", dualTypeBox_);
    for (int i = 0; i < kDualKnobs; ++i) {
        dualAtts_[i].reset();
        dualAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + "Dual" + juce::String(i / 5 + 1) + kDualIds[i % 5][0], dualSliders_[i]);
    }
    for (int i = 0; i < kTbKnobs; ++i) {
        tbAtts_[i].reset();
        tbAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kTbIds[i][0], tbSliders_[i]);
    }
    combOctAtt_.reset();
    combOctAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "CombOctave", combOctBox_);
    for (int i = 0; i < kCombKnobs; ++i) {
        combAtts_[i].reset();
        combAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kCombIds[i][0], combSliders_[i]);
    }
    clfoLabel_.setText(k ? "LFO 4" : "LFO 3", juce::dontSendNotification);
    attachSideBand();
}

// The Side Band rows show the 1st's parameters, or the 2nd's when the type
// is the 2nd (its Q in the HPF's place, no Sub Detune).
void VSynthEmuEditor::attachSideBand() {
    const bool second = cosmTypeBox_.getSelectedItemIndex() == 6;
    const juce::String pre = juce::String(cosmK_ ? "cosm2" : "cosm") + (second ? "Sb2" : "Sb1");
    sbOctAtt_.reset();
    sbOctAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "Octave", sbOctBox_);
    for (int i = 0; i < kSbKnobs; ++i) {
        sbAtts_[i].reset();
        const bool none = second && i == 3;
        sbSliders_[i].setVisible(!none);
        sbLabels_[i].setVisible(!none);
        if (!none) sbAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + (second && i == 0 ? "Q" : kSbIds[i][0]), sbSliders_[i]);
    }
    sbLabels_[0].setText(second ? "Q" : "HPF", juce::dontSendNotification);
}

void VSynthEmuEditor::timerCallback() {
    if (timeAtt_ && processor_.hasRom() != mainKnobsXt_) attachMainKnobs(processor_.hasRom());
    statusLabel_.setText(processor_.getStatusText(), juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::textColourId, processor_.hasRom() ? juce::Colours::lightgrey : juce::Colours::orange);
}

void VSynthEmuEditor::styleSmall(juce::Label& l) {
    l.setFont(juce::Font(11.0f, juce::Font::bold));
    l.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    l.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(l);
}

void VSynthEmuEditor::openRomChooser() {
    romChooser_ = std::make_unique<juce::FileChooser>(
        "Choose the V-Synth XT firmware image (V-SynthXT.rom.2.bin)",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.bin");
    romChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            const auto f = fc.getResult();
            if (f.existsAsFile()) processor_.setRomFile(f);
            timerCallback();
        });
}

// ─── Painting ─────────────────────────────────────────────────────────────────

void VSynthEmuEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1A1A2E));

    // Title bar
    g.setColour(juce::Colour(0xFF0D0D20));
    g.fillRect(0, 0, getWidth(), 36);

    g.setColour(juce::Colour(0xFF00FF88));
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("V-SYNTH EMU", 0, 0, getWidth(), 36, juce::Justification::centred);

    // File strip background
    g.setColour(juce::Colour(0xFF111128));
    g.fillRect(0, 36, getWidth(), kFileStripH);

    g.setColour(juce::Colour(0xFF333355));
    g.drawLine(0.0f, 36.0f, float(getWidth()), 36.0f, 1.0f);
    g.drawLine(0.0f, float(36 + kFileStripH), float(getWidth()), float(36 + kFileStripH), 1.0f);

    // XT strip background
    g.setColour(juce::Colour(0xFF15152E));
    g.fillRect(0, 36 + kFileStripH, getWidth(), kXtStripH);
    g.setColour(juce::Colour(0xFF333355));
    g.drawLine(0.0f, float(36 + kFileStripH + kXtStripH), float(getWidth()), float(36 + kFileStripH + kXtStripH), 1.0f);

    // Algorithm strip background
    int algoTop = getHeight() - kAlgoStripH;
    g.setColour(juce::Colour(0xFF111128));
    g.fillRect(0, algoTop, getWidth(), kAlgoStripH);
    g.setColour(juce::Colour(0xFF333355));
    g.drawLine(0.0f, float(algoTop), float(getWidth()), float(algoTop), 1.0f);
}

// ─── Layout ───────────────────────────────────────────────────────────────────

void VSynthEmuEditor::resized() {
    const int titleH  = 36;
    const int algoTop = getHeight() - kAlgoStripH;

    // ── File strip ─────────────────────────────────────────────────────────
    auto fileStrip = juce::Rectangle<int>(0, titleH, getWidth(), kFileStripH).reduced(10, 8);
    loadButton_.setBounds(fileStrip.removeFromLeft(110));
    fileStrip.removeFromLeft(10);
    fileNameLabel_.setBounds(fileStrip);

    // ── XT strip ───────────────────────────────────────────────────────────
    auto xt = juce::Rectangle<int>(0, titleH + kFileStripH, getWidth(), kXtStripH).reduced(10, 6);
    auto row1 = xt.removeFromTop(26);
    romButton_.setBounds(row1.removeFromLeft(70));
    row1.removeFromLeft(10);
    statusLabel_.setBounds(row1);
    xt.removeFromTop(6);
    auto row2 = xt.removeFromTop(24);
    encodeLabel_.setBounds(row2.removeFromLeft(52));
    encodeBox_.setBounds(row2.removeFromLeft(120));
    row2.removeFromLeft(12);
    keyLabel_.setBounds(row2.removeFromLeft(30));
    keySlider_.setBounds(row2.removeFromLeft(110));
    row2.removeFromLeft(12);
    depthLabel_.setBounds(row2.removeFromLeft(44));
    depthSlider_.setBounds(row2);
    xt.removeFromTop(6);
    auto row3 = xt.removeFromTop(24);                // the OSC: Energy, Time Offset, Time KF, Formant KF
    const int sw = (row3.getWidth() - 46 - 38 - 80 - 50 - 46) / 3;   // the three sliders share what the labels leave
    energyLabel_.setBounds(row3.removeFromLeft(46));
    energySlider_.setBounds(row3.removeFromLeft(sw).withTrimmedRight(6));
    offsetLabel_.setBounds(row3.removeFromLeft(38));
    timeOffsetBox_.setBounds(row3.removeFromLeft(80).withTrimmedRight(6));
    oscTkfLabel_.setBounds(row3.removeFromLeft(50));
    oscTkfSlider_.setBounds(row3.removeFromLeft(sw).withTrimmedRight(6));
    oscFkfLabel_.setBounds(row3.removeFromLeft(46));
    oscFkfSlider_.setBounds(row3);
    xt.removeFromTop(6);
    auto row3l = xt.removeFromTop(24);               // Loop: switch, mode, region
    loopBtn_.setBounds(row3l.removeFromLeft(64));
    loopModeBox_.setBounds(row3l.removeFromLeft(110).withTrimmedRight(10));
    const int lh = row3l.getWidth() / 2;
    auto ls = row3l.removeFromLeft(lh);
    loopStartLabel_.setBounds(ls.removeFromLeft(38));
    loopStartSlider_.setBounds(ls.withTrimmedRight(8));
    loopEndLabel_.setBounds(row3l.removeFromLeft(32));
    loopEndSlider_.setBounds(row3l);
    xt.removeFromTop(6);
    auto row3p = xt.removeFromTop(24);               // Portamento: switch, mode, type, time, velocity sens
    portaBtn_.setBounds(row3p.removeFromLeft(64));
    portaModeBox_.setBounds(row3p.removeFromLeft(96).withTrimmedRight(8));
    portaTypeBox_.setBounds(row3p.removeFromLeft(80).withTrimmedRight(10));
    const int ph = row3p.getWidth() / 2;
    auto pt = row3p.removeFromLeft(ph);
    portaTimeLabel_.setBounds(pt.removeFromLeft(36));
    portaTimeSlider_.setBounds(pt.withTrimmedRight(8));
    portaSensLabel_.setBounds(row3p.removeFromLeft(40));
    portaSensSlider_.setBounds(row3p);
    auto row3k = xt.removeFromTop(24);               // Key assign: Mono/Poly, Legato, Octave Shift
    monoBox_.setBounds(row3k.removeFromLeft(96).withTrimmedRight(8));
    legatoBtn_.setBounds(row3k.removeFromLeft(80));
    octLabel_.setBounds(row3k.removeFromLeft(36));
    octSlider_.setBounds(row3k.removeFromLeft(200));
    xt.removeFromTop(6);
    auto row4 = xt.removeFromTop(24);                // amp envelope
    const int q = row4.getWidth() / 4;
    std::pair<juce::Label*, juce::Slider*> adsr[] = { { &attackLabel_, &attackSlider_ }, { &decayLabel_, &decaySlider_ },
                                                      { &sustainLabel_, &sustainSlider_ }, { &releaseLabel_, &releaseSlider_ } };
    for (auto& [l, sl] : adsr) {
        auto cell = row4.removeFromLeft(q);
        l->setBounds(cell.removeFromLeft(18));
        sl->setBounds(cell.withTrimmedRight(8));
    }
    xt.removeFromTop(6);
    auto row5 = xt.removeFromTop(24);                // velocity → time, Time KF
    std::pair<juce::Label*, juce::Slider*> sens[] = { { &aSensLabel_, &aSensSlider_ }, { &dSensLabel_, &dSensSlider_ },
                                                      { &rSensLabel_, &rSensSlider_ }, { &kfLabel_, &kfSlider_ } };
    for (auto& [l, sl] : sens) {
        auto cell = row5.removeFromLeft(q);
        l->setBounds(cell.removeFromLeft(38));
        sl->setBounds(cell.withTrimmedRight(8));
    }
    xt.removeFromTop(6);
    auto row6 = xt.removeFromTop(24);                // pitch envelope
    const int fifth = row6.getWidth() / 5;
    std::pair<juce::Label*, juce::Slider*> penv[] = { { &pAttackLabel_, &pAttackSlider_ }, { &pDecayLabel_, &pDecaySlider_ },
                                                      { &pSustainLabel_, &pSustainSlider_ }, { &pReleaseLabel_, &pReleaseSlider_ },
                                                      { &pDepthLabel_, &pDepthSlider_ } };
    for (auto& [l, sl] : penv) {
        auto cell = row6.removeFromLeft(fifth);
        l->setBounds(cell.removeFromLeft(30));
        sl->setBounds(cell.withTrimmedRight(6));
    }
    xt.removeFromTop(6);
    auto row6t = xt.removeFromTop(24);               // time envelope
    std::pair<juce::Label*, juce::Slider*> tenv[] = { { &tAttackLabel_, &tAttackSlider_ }, { &tDecayLabel_, &tDecaySlider_ },
                                                      { &tSustainLabel_, &tSustainSlider_ }, { &tReleaseLabel_, &tReleaseSlider_ },
                                                      { &tDepthLabel_, &tDepthSlider_ } };
    for (auto& [l, sl] : tenv) {
        auto cell = row6t.removeFromLeft(fifth);
        l->setBounds(cell.removeFromLeft(30));
        sl->setBounds(cell.withTrimmedRight(6));
    }
    xt.removeFromTop(6);
    auto row6f = xt.removeFromTop(24);               // formant envelope
    std::pair<juce::Label*, juce::Slider*> fenv[] = { { &fAttackLabel_, &fAttackSlider_ }, { &fDecayLabel_, &fDecaySlider_ },
                                                      { &fSustainLabel_, &fSustainSlider_ }, { &fReleaseLabel_, &fReleaseSlider_ },
                                                      { &fDepthLabel_, &fDepthSlider_ } };
    for (auto& [l, sl] : fenv) {
        auto cell = row6f.removeFromLeft(fifth);
        l->setBounds(cell.removeFromLeft(30));
        sl->setBounds(cell.withTrimmedRight(6));
    }
    xt.removeFromTop(6);
    auto row7 = xt.removeFromTop(24);                // LFO: waveform, rate, key sync
    lfoLabel_.setBounds(row7.removeFromLeft(30));
    lfoWaveBox_.setBounds(row7.removeFromLeft(80).withTrimmedRight(6));
    lfoRateLabel_.setBounds(row7.removeFromLeft(34));
    lfoRateSlider_.setBounds(row7.removeFromLeft(220).withTrimmedRight(6));
    lfoSyncBtn_.setBounds(row7);
    xt.removeFromTop(6);
    auto row8 = xt.removeFromTop(24);                // LFO: delay, fade, fade mode, offset
    lfoDelayLabel_.setBounds(row8.removeFromLeft(40));
    lfoDelaySlider_.setBounds(row8.removeFromLeft(130).withTrimmedRight(6));
    lfoFadeLabel_.setBounds(row8.removeFromLeft(34));
    lfoFadeSlider_.setBounds(row8.removeFromLeft(130).withTrimmedRight(6));
    lfoModeBox_.setBounds(row8.removeFromLeft(90).withTrimmedRight(6));
    lfoOffsetBox_.setBounds(row8.removeFromLeft(70));
    xt.removeFromTop(6);
    auto row9 = xt.removeFromTop(24);                // LFO depths: pitch, time, formant
    lfoDepthsLabel_.setBounds(row9.removeFromLeft(46));
    const int third = row9.getWidth() / 3;
    std::pair<juce::Label*, juce::Slider*> lfoDepths[] = { { &lfoDepthLabel_, &lfoDepthSlider_ }, { &lfoTimeLabel_, &lfoTimeSlider_ },
                                                           { &lfoFmtLabel_, &lfoFmtSlider_ } };
    for (auto& [l, sl] : lfoDepths) {
        auto cell = row9.removeFromLeft(third);
        l->setBounds(cell.removeFromLeft(34));
        sl->setBounds(cell.withTrimmedRight(6));
    }
    for (int r = 0; r < kLevelRows; ++r) {          // the OSC pitch, tuning, TVA and velocity rows
        xt.removeFromTop(6);
        auto row = xt.removeFromTop(24);
        levelRowLabels_[r].setBounds(row.removeFromLeft(56));
        const int quarter = row.getWidth() / 4;
        for (int i = 4 * r; i < 4 * r + 4; ++i) {
            auto cell = row.removeFromLeft(quarter);
            levelLabels_[i].setBounds(cell.removeFromLeft(r <= 1 || r == 4 || i >= 22 ? 46 : 34));
            levelSliders_[i].setBounds(cell.withTrimmedRight(6));
        }
        if (r == 5) {                                // COSM 1: switch, Filter Type, Cutoff, Resonance, KF
            xt.removeFromTop(6);
            auto rc = xt.removeFromTop(24);
            cosmSelBox_.setBounds(rc.removeFromLeft(90).withTrimmedRight(6));
            cosmBtn_.setBounds(rc.removeFromLeft(48));
            cosmFilterBox_.setBounds(rc.removeFromLeft(92).withTrimmedRight(6));
            const int w3 = rc.getWidth() / 3;
            std::pair<juce::Label*, juce::Slider*> cells[] = { { &cosmCutLabel_, &cosmCutSlider_ }, { &cosmResoLabel_, &cosmResoSlider_ },
                                                               { &cosmKfLabel_, &cosmKfSlider_ } };
            for (auto& [l, sl] : cells) {
                auto cell = rc.removeFromLeft(w3);
                l->setBounds(cell.removeFromLeft(28));
                sl->setBounds(cell.withTrimmedRight(4));
            }
        }
    }
    xt.removeFromTop(6);
    auto rd = xt.removeFromTop(24);                  // the COSM's type, the Dual Filter's type, the Dynamic TVF's Own Env depths
    cosmTypeLabel_.setBounds(rd.removeFromLeft(46));
    cosmTypeBox_.setBounds(rd.removeFromLeft(120).withTrimmedRight(8));
    dualTypeBox_.setBounds(rd.removeFromLeft(100).withTrimmedRight(8));
    const int half = rd.getWidth() / 2;
    auto rd1 = rd.removeFromLeft(half);
    cosmCutOwnLabel_.setBounds(rd1.removeFromLeft(46));
    cosmCutOwnSlider_.setBounds(rd1.withTrimmedRight(6));
    cosmResoOwnLabel_.setBounds(rd.removeFromLeft(46));
    cosmResoOwnSlider_.setBounds(rd);
    for (int r = 0; r < 2; ++r) {                    // the Dual Filter: Filter 1, Filter 2
        xt.removeFromTop(6);
        auto row = xt.removeFromTop(24);
        dualRowLabels_[r].setBounds(row.removeFromLeft(56));
        const int fifth = row.getWidth() / 5;
        for (int i = 5 * r; i < 5 * r + 5; ++i) {
            auto cell = row.removeFromLeft(fifth);
            dualLabels_[i].setBounds(cell.removeFromLeft(28));
            dualSliders_[i].setBounds(cell.withTrimmedRight(4));
        }
    }
    for (int r = 0; r < 2; ++r) {                    // the TB Filter: LPF (6), HPF (5)
        xt.removeFromTop(6);
        auto row = xt.removeFromTop(24);
        tbRowLabels_[r].setBounds(row.removeFromLeft(46));
        const int first = r ? 6 : 0, n = r ? 5 : 6, w = row.getWidth() / n;
        for (int i = first; i < first + n; ++i) {
            auto cell = row.removeFromLeft(w);
            const juce::String t = kTbIds[i][1];
            tbLabels_[i].setBounds(cell.removeFromLeft(t.length() >= 3 ? (t == "Gain" ? 32 : 27) : 20));
            tbSliders_[i].setBounds(cell.withTrimmedRight(3));
        }
    }
    for (int r = 0; r < 2; ++r) {                    // the Comb Filter: Octave + Detune, then Tone
        xt.removeFromTop(6);
        auto row = xt.removeFromTop(24);
        combRowLabels_[r].setBounds(row.removeFromLeft(46));
        if (r == 0) combOctBox_.setBounds(row.removeFromLeft(70).withTrimmedRight(6));
        else row.removeFromLeft(70);
        const int w = row.getWidth() / 3;
        for (int i = 3 * r; i < 3 * r + 3; ++i) {
            auto cell = row.removeFromLeft(w);
            combLabels_[i].setBounds(cell.removeFromLeft(34));
            combSliders_[i].setBounds(cell.withTrimmedRight(4));
        }
    }
    for (int r = 0; r < 3; ++r) {                    // the Side Band Filters: Octave + HPF/Q, LPF, KF, Sub; Detune; Width
        xt.removeFromTop(6);
        auto row = xt.removeFromTop(24);
        sbRowLabels_[r].setBounds(row.removeFromLeft(46));
        const int first = r == 0 ? 0 : r == 1 ? 4 : 8, n = r == 2 ? 5 : 4;
        if (r == 0) sbOctBox_.setBounds(row.removeFromLeft(62).withTrimmedRight(6));
        const int w = row.getWidth() / n;
        for (int i = first; i < first + n; ++i) {
            auto cell = row.removeFromLeft(w);
            sbLabels_[i].setBounds(cell.removeFromLeft(28));
            sbSliders_[i].setBounds(cell.withTrimmedRight(3));
        }
    }
    xt.removeFromTop(6);
    auto rl = xt.removeFromTop(24);                  // LFO 3: waveform, rate, key sync
    clfoLabel_.setBounds(rl.removeFromLeft(46));
    clfoWaveBox_.setBounds(rl.removeFromLeft(80).withTrimmedRight(6));
    clfoRateLabel_.setBounds(rl.removeFromLeft(34));
    clfoRateSlider_.setBounds(rl.removeFromLeft(220).withTrimmedRight(6));
    clfoSyncBtn_.setBounds(rl);

    // ── Knobs area ─────────────────────────────────────────────────────────
    const int knobTop = titleH + kFileStripH + kXtStripH + 8;
    auto knobArea = juce::Rectangle<int>(20, knobTop,
                                         getWidth() - 40,
                                         algoTop - knobTop - 36);
    const int knobW = knobArea.getWidth() / 3;

    auto layoutKnob = [](juce::Rectangle<int> a, juce::Slider& s, juce::Label& l) {
        l.setBounds(a.removeFromTop(20));
        a.reduce(8, 0);
        s.setBounds(a.removeFromTop(std::min(a.getHeight(), 130)));
    };

    layoutKnob(knobArea.removeFromLeft(knobW), pitchSlider_,   pitchLabel_);
    layoutKnob(knobArea.removeFromLeft(knobW), timeSlider_,    timeLabel_);
    layoutKnob(knobArea,                       formantSlider_, formantLabel_);

    // Robot button — centred below the knobs
    robotBtn_.setBounds(getWidth() / 2 - 44, algoTop - 30, 88, 22);

    // ── Algorithm strip ────────────────────────────────────────────────────
    auto algoStrip = juce::Rectangle<int>(0, algoTop, getWidth(), kAlgoStripH).reduced(12, 12);
    algoLabel_.setBounds(algoStrip.removeFromLeft(80));
    algoStrip.removeFromLeft(6);
    algoBox_.setBounds(algoStrip);
}

// ─── File chooser ────────────────────────────────────────────────────────────

void VSynthEmuEditor::openFileChooser() {
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Load audio file",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        processor_.getFormatManager().getWildcardForAllFormats());

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.existsAsFile())
            {
                processor_.loadFile(result);
                fileNameLabel_.setText(processor_.getLoadedFileName(),
                                       juce::dontSendNotification);
            }
        });
}

// ─── Combo box ───────────────────────────────────────────────────────────────

void VSynthEmuEditor::comboBoxChanged(juce::ComboBox* box) {
    if (box == &algoBox_) {
        using A = VSE::Algorithm;
        static const A map[] = { A::Passthrough, A::PhaseVocoder,
                                  A::SinusoidalPlusResidual, A::LPCSourceFilter,
                                  A::Hybrid };
        int idx = box->getSelectedId() - 1;
        if (idx >= 0 && idx < 5)
            processor_.setAlgorithm(map[idx]);
    }
}

// ─── Slider styling ──────────────────────────────────────────────────────────

void VSynthEmuEditor::styleSlider(juce::Slider& s, juce::Label& l) {
    s.setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour(0xFF00BFA5));
    s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF333355));
    s.setColour(juce::Slider::textBoxTextColourId,         juce::Colours::lightgrey);
    s.setColour(juce::Slider::textBoxBackgroundColourId,   juce::Colour(0xFF0D0D1A));

    l.setFont(juce::Font(11.0f, juce::Font::bold));
    l.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    l.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(s);
    addAndMakeVisible(l);
}
