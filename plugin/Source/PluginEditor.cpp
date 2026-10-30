#include "PluginEditor.h"

static constexpr int kFileStripH = 44;   // height of the load-file strip at the top
static constexpr int kXtStripH   = 246;  // height of the XT strip below it
static constexpr int kAlgoStripH = 52;   // height of the algorithm strip at the bottom

VSynthEmuEditor::VSynthEmuEditor(VSynthEmuProcessor& p)
    : AudioProcessorEditor(&p), processor_(p)
{
    setSize(560, 610);
    setResizable(true, true);
    setResizeLimits(480, 400, 960, 760);

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
                              &aSensSlider_, &dSensSlider_, &rSensSlider_, &kfSlider_,
                              &pAttackSlider_, &pDecaySlider_, &pSustainSlider_, &pReleaseSlider_, &pDepthSlider_,
                              &lfoRateSlider_, &lfoDepthSlider_, &lfoDelaySlider_, &lfoFadeSlider_ }) {
        sl->setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl->setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        sl->setTextBoxStyle(sl->getTextBoxPosition(), false, 56, 20);
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
    for (juce::Label* l : { &encodeLabel_, &keyLabel_, &depthLabel_, &energyLabel_, &attackLabel_, &decayLabel_,
                            &sustainLabel_, &releaseLabel_, &aSensLabel_, &dSensLabel_, &rSensLabel_, &kfLabel_,
                            &pAttackLabel_, &pDecayLabel_, &pSustainLabel_, &pReleaseLabel_, &pDepthLabel_,
                            &lfoLabel_, &lfoRateLabel_, &lfoDepthLabel_, &lfoDelayLabel_, &lfoFadeLabel_ }) styleSmall(*l);
    timerCallback();
    startTimerHz(4);

    // ── Knobs ─────────────────────────────────────────────────────────────────
    styleSlider(pitchSlider_,   pitchLabel_);
    styleSlider(timeSlider_,    timeLabel_);
    styleSlider(formantSlider_, formantLabel_);

    pitchAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "pitchShift",   pitchSlider_);
    timeAtt_    = std::make_unique<SliderAtt>(processor_.apvts, "timeStretch",  timeSlider_);
    formantAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "formantShift", formantSlider_);
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

void VSynthEmuEditor::timerCallback() {
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
    auto row3 = xt.removeFromTop(24);
    energyLabel_.setBounds(row3.removeFromLeft(52));
    energySlider_.setBounds(row3.withWidth(row3.getWidth() / 2));
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
    auto row7 = xt.removeFromTop(24);                // LFO: waveform, rate, pitch depth, key sync
    lfoLabel_.setBounds(row7.removeFromLeft(30));
    lfoWaveBox_.setBounds(row7.removeFromLeft(80).withTrimmedRight(6));
    lfoRateLabel_.setBounds(row7.removeFromLeft(34));
    lfoRateSlider_.setBounds(row7.removeFromLeft(140).withTrimmedRight(6));
    lfoDepthLabel_.setBounds(row7.removeFromLeft(36));
    lfoDepthSlider_.setBounds(row7.removeFromLeft(140).withTrimmedRight(6));
    lfoSyncBtn_.setBounds(row7);
    xt.removeFromTop(6);
    auto row8 = xt.removeFromTop(24);                // LFO: delay, fade, fade mode, offset
    lfoDelayLabel_.setBounds(row8.removeFromLeft(40));
    lfoDelaySlider_.setBounds(row8.removeFromLeft(130).withTrimmedRight(6));
    lfoFadeLabel_.setBounds(row8.removeFromLeft(34));
    lfoFadeSlider_.setBounds(row8.removeFromLeft(130).withTrimmedRight(6));
    lfoModeBox_.setBounds(row8.removeFromLeft(90).withTrimmedRight(6));
    lfoOffsetBox_.setBounds(row8.removeFromLeft(70));

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
