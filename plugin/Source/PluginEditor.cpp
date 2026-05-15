#include "PluginEditor.h"

VSynthEmuEditor::VSynthEmuEditor(VSynthEmuProcessor& p)
    : AudioProcessorEditor(&p), processor_(p)
{
    setSize(520, 320);
    setResizable(true, true);
    setResizeLimits(400, 250, 900, 600);

    // Sliders
    styleSlider(pitchSlider_,   pitchLabel_);
    styleSlider(timeSlider_,    timeLabel_);
    styleSlider(formantSlider_, formantLabel_);

    pitchAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "pitchShift",   pitchSlider_);
    timeAtt_    = std::make_unique<SliderAtt>(processor_.apvts, "timeStretch",  timeSlider_);
    formantAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "formantShift", formantSlider_);
    robotAtt_   = std::make_unique<ButtonAtt>(processor_.apvts, "robot",        robotBtn_);

    addAndMakeVisible(robotBtn_);

    // Algorithm selector
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

void VSynthEmuEditor::styleSlider(juce::Slider& s, juce::Label& l) {
    s.setColour(juce::Slider::rotarySliderFillColourId,   juce::Colour(0xFF00BFA5));
    s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF333355));
    s.setColour(juce::Slider::textBoxTextColourId,         juce::Colours::lightgrey);
    s.setColour(juce::Slider::textBoxBackgroundColourId,   juce::Colour(0xFF0D0D1A));

    l.setFont(juce::Font(11.0f, juce::Font::bold));
    l.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    l.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(s);
    addAndMakeVisible(l);
}

void VSynthEmuEditor::paint(juce::Graphics& g) {
    // Dark background
    g.fillAll(juce::Colour(0xFF1A1A2E));

    // Title
    g.setColour(juce::Colour(0xFF00FF88));
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("V-SYNTH EMU", getLocalBounds().removeFromTop(36), juce::Justification::centred);

    g.setColour(juce::Colour(0xFF333355));
    g.drawLine(20, 38, getWidth() - 20, 38, 1.0f);

    // Algorithm section divider
    int algoY = getHeight() - 65;
    g.setColour(juce::Colour(0xFF222244));
    g.fillRect(0, algoY - 5, getWidth(), 70);
    g.setColour(juce::Colour(0xFF333355));
    g.drawLine(20, algoY - 5, getWidth() - 20, algoY - 5, 1.0f);
}

void VSynthEmuEditor::resized() {
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(40);  // title

    // Three knobs row
    auto knobArea = area.removeFromTop(190);
    int  knobW    = knobArea.getWidth() / 3;

    auto pitchArea   = knobArea.removeFromLeft(knobW);
    auto timeArea    = knobArea.removeFromLeft(knobW);
    auto formantArea = knobArea;

    auto layoutKnob = [](juce::Rectangle<int> a, juce::Slider& s, juce::Label& l) {
        l.setBounds(a.removeFromTop(20));
        a.reduce(10, 0);
        s.setBounds(a.removeFromTop(130));
    };

    layoutKnob(pitchArea,   pitchSlider_,   pitchLabel_);
    layoutKnob(timeArea,    timeSlider_,    timeLabel_);
    layoutKnob(formantArea, formantSlider_, formantLabel_);

    robotBtn_.setBounds(getWidth() / 2 - 40, getHeight() - 120, 80, 24);

    // Algorithm row
    int algoY = getHeight() - 58;
    algoLabel_.setBounds(20, algoY, 80, 22);
    algoBox_.setBounds(105, algoY, getWidth() - 125, 22);
}

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
