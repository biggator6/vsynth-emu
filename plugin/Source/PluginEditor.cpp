#include "PluginEditor.h"

static constexpr int kFileStripH = 44;   // height of the load-file strip at the top
static constexpr int kAlgoStripH = 52;   // height of the algorithm strip at the bottom

VSynthEmuEditor::VSynthEmuEditor(VSynthEmuProcessor& p)
    : AudioProcessorEditor(&p), processor_(p)
{
    setSize(520, 360);
    setResizable(true, true);
    setResizeLimits(420, 300, 900, 640);

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
    algoBox_.removeListener(this);
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

    // ── Knobs area ─────────────────────────────────────────────────────────
    auto knobArea = juce::Rectangle<int>(20, titleH + kFileStripH + 8,
                                         getWidth() - 40,
                                         algoTop - (titleH + kFileStripH + 8) - 36);
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
