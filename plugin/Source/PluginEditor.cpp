#include "PluginEditor.h"
#include "XtBuildId.h"
#include <tuple>

using Ui::Page;
using Ui::Segmented;

// ─── The tab bar ─────────────────────────────────────────────────────────────
class VSynthEmuEditor::TabBar : public juce::Component {
public:
    juce::StringArray names;
    int current = 0;
    std::function<void(int)> onChange;

    juce::Rectangle<int> tabBounds(int i) const {
        int x = Ui::pad;
        for (int k = 0; k < names.size(); ++k) {
            const int w = juce::Font(13.0f, juce::Font::bold).getStringWidth(names[k]) + 28;
            if (k == i) return { x, 0, w, getHeight() };
            x += w;
        }
        return {};
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(Ui::surfaceStrip);
        g.setColour(Ui::line);
        g.fillRect(0, getHeight() - 1, getWidth(), 1);
        for (int i = 0; i < names.size(); ++i) {
            const auto b = tabBounds(i);
            const bool on = i == current;
            g.setFont(juce::Font(13.0f, on ? juce::Font::bold : juce::Font::plain));
            g.setColour(on ? Ui::text : Ui::label);
            g.drawText(names[i], b, juce::Justification::centred, false);
            if (on) { g.setColour(Ui::accent); g.fillRect(b.getX(), getHeight() - 2, b.getWidth(), 2); }
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        for (int i = 0; i < names.size(); ++i)
            if (tabBounds(i).contains(e.getPosition())) { if (onChange) onChange(i); return; }
    }
};

// ─── An envelope's shape ─────────────────────────────────────────────────────
// Attack, decay and release as thirds of the width each scaled by their knob,
// sustain as the level they meet: a picture of the settings, not the
// envelope's real timing.
class VSynthEmuEditor::EnvelopeView : public juce::Component, private juce::Slider::Listener {
public:
    EnvelopeView(juce::Slider& a, juce::Slider& d, juce::Slider& s, juce::Slider& r) : sl_{ &a, &d, &s, &r } {
        for (auto* x : sl_) x->addListener(this);
    }
    ~EnvelopeView() override { for (auto* x : sl_) x->removeListener(this); }

    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat();
        g.setColour(Ui::surfaceField);
        g.fillRect(b);
        g.setColour(Ui::line);
        g.drawRect(b, 1.0f);
        const auto p = [this](int i) { return (float)sl_[i]->valueToProportionOfLength(sl_[i]->getValue()); };
        b = b.reduced(6.0f);
        const float third = b.getWidth() / 4.0f;
        const float x1 = b.getX() + p(0) * third, x2 = x1 + p(1) * third, x3 = b.getRight() - p(3) * third;
        const float ys = b.getBottom() - p(2) * b.getHeight();
        juce::Path path;
        path.startNewSubPath(b.getX(), b.getBottom());
        path.lineTo(x1, b.getY());
        path.lineTo(x2, ys);
        path.lineTo(juce::jmax(x2, x3), ys);
        path.lineTo(b.getRight(), b.getBottom());
        g.setColour(Ui::accent);
        g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
private:
    void sliderValueChanged(juce::Slider*) override { repaint(); }
    juce::Slider* sl_[4];
};


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
static const char* const kLevelRowNames[8] = { "OSC Pitch", "Tune", "OSC TVA", "O Env", "O Env", "Velocity", "TVF Mod", "COSM Env" };
// The patch TVA's rows (Level, KF, LFO Depth, Send; Pan, KF, LFO Depth), then LFO 5's knobs
static const char* const kTvaIds[10][2] = { { "tvaLevel", "Lvl" }, { "tvaLevelKF", "KF" }, { "tvaLevelLfo", "LFO" }, { "mfxSend", "Send" },
                                            { "tvaPan", "Pan" }, { "tvaPanKF", "KF" }, { "tvaPanLfo", "LFO" },
                                            { "tvaLfoRate", "Rate" }, { "tvaLfoDelay", "Delay" }, { "tvaLfoFade", "Fade" } };
// Which of those rows' knobs are the selected OSC's own (attachOsc): the OSC's
// pitch, TVA, amp envelope and velocity to level
static bool kLevelOsc(int i) { return i < 4 || (i >= 8 && i < 20) || i == 22 || i == 23; }
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
// The Resonator's rows (Size; Balance): the id suffixes after "cosm"/"cosm2", and labels
static const char* const kResIds[7][2] = { { "ResSize", "Size" }, { "ResSizeKF", "KF" }, { "ResSizeLfo", "LFO" }, { "ResSizeEnv", "Env" },
                                           { "ResBalance", "Bal" }, { "ResBalanceLfo", "LFO" }, { "ResBalanceEnv", "Env" } };
// The Frequency Shifter's rows (Depth; Balance): the id suffixes after "cosm"/"cosm2", and labels
static const char* const kFsIds[7][2] = { { "FsDepth", "Dep" }, { "FsDepthKF", "KF" }, { "FsDepthLfo", "LFO" }, { "FsDepthEnv", "Env" },
                                          { "FsBalance", "Bal" }, { "FsBalanceLfo", "LFO" }, { "FsBalanceEnv", "Env" } };
// Lo-Fi's rows (Bit, Rate, Balance and its depths; the Filter): the id suffixes after "cosm"/"cosm2", and labels
static const char* const kLofiIds[9][2] = { { "LofiBit", "Bit" }, { "LofiRate", "Rate" }, { "LofiBalance", "Bal" }, { "LofiBalanceLfo", "LFO" },
                                            { "LofiBalanceEnv", "Env" }, { "LofiFilter", "Filt" }, { "LofiFilterKF", "KF" },
                                            { "LofiFilterLfo", "LFO" }, { "LofiFilterEnv", "Env" } };
// The Compressor's row: the id suffixes after "cosm"/"cosm2", and labels
static const char* const kCompIds[3][2] = { { "CompAttack", "Atk" }, { "CompSustain", "Sus" }, { "CompLevel", "Lvl" } };
// Wave Shape's rows after its Type: Level, Drive and its depths; Tone, KF and depths
static const char* const kWsIds[8][2] = { { "WsLevel", "Lvl" }, { "WsDrive", "Drv" }, { "WsDriveLfo", "LFO" }, { "WsDriveEnv", "Env" },
                                          { "WsTone", "Tone" }, { "WsToneKF", "KF" }, { "WsToneLfo", "LFO" }, { "WsToneEnv", "Env" } };
// OD/Dist's, in the same places
static const char* const kOdIds[8][2] = { { "OdLevel", "Lvl" }, { "OdDrive", "Drv" }, { "OdDriveLfo", "LFO" }, { "OdDriveEnv", "Env" },
                                          { "OdTone", "Tone" }, { "OdToneKF", "KF" }, { "OdToneLfo", "LFO" }, { "OdToneEnv", "Env" } };
// The Amp Simulator's rows after its Type: Gain, its depths, Level; Bass, Middle, Treble
static const char* const kAmpIds[7][2] = { { "AmpGain", "Gain" }, { "AmpGainLfo", "LFO" }, { "AmpGainEnv", "Env" }, { "AmpLevel", "Lvl" },
                                           { "AmpBass", "Bass" }, { "AmpMiddle", "Mid" }, { "AmpTreble", "Treb" } };
// The Limiter's row after its Ratio
static const char* const kLimIds[4][2] = { { "LimThreshold", "Thr" }, { "LimAttack", "Atk" }, { "LimRelease", "Rel" }, { "LimLevel", "Lvl" } };

VSynthEmuEditor::VSynthEmuEditor(VSynthEmuProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), fxPanel_(std::make_unique<FxPanel>(p))
{

    // ── File strip ────────────────────────────────────────────────────────────
    loadButton_.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xFF00BFA5));
    loadButton_.setColour(juce::TextButton::textColourOffId,  juce::Colours::black);
    loadButton_.onClick = [this] { openFileChooser(); };
    addAndMakeVisible(loadButton_);
    patchButton_.onClick = [this] { openPatchChooser(); };
    addAndMakeVisible(patchButton_);

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
    buildLabel_.setText(juce::String(xtBuildId()) + "  " + xtBuildTime(), juce::dontSendNotification);
    buildLabel_.setFont(juce::Font(10.0f));
    buildLabel_.setColour(juce::Label::textColourId, Ui::label);
    buildLabel_.setBorderSize({ 0, 0, 0, 0 });
    addAndMakeVisible(buildLabel_);
    encodeBox_.addItemList({ "Auto", "SOLO", "BACKING", "ENSEMBLE", "LITE" }, 1);
    addAndMakeVisible(encodeBox_);
    encodeAtt_  = std::make_unique<ComboAtt>(processor_.apvts, "encodeType", encodeBox_);
    for (juce::Slider* sl : { &keySlider_, &depthSlider_, &energySlider_, &startSlider_, &attackSlider_, &decaySlider_, &sustainSlider_, &releaseSlider_,
                              &aSensSlider_, &dSensSlider_, &rSensSlider_, &kfSlider_, &oscTkfSlider_, &oscFkfSlider_,
                              &pAttackSlider_, &pDecaySlider_, &pSustainSlider_, &pReleaseSlider_, &pDepthSlider_,
                              &tAttackSlider_, &tDecaySlider_, &tSustainSlider_, &tReleaseSlider_, &tDepthSlider_,
                              &fAttackSlider_, &fDecaySlider_, &fSustainSlider_, &fReleaseSlider_, &fDepthSlider_,
                              &lfoRateSlider_, &lfoDepthSlider_, &lfoDelaySlider_, &lfoFadeSlider_, &lfoTimeSlider_, &lfoFmtSlider_,
                              &loopStartSlider_, &loopEndSlider_, &portaTimeSlider_, &portaSensSlider_, &octSlider_,
                              &patchTempoSlider_, &tripSlider_, &origTempoSlider_,
                              &cosmCutSlider_, &cosmResoSlider_, &cosmKfSlider_, &clfoRateSlider_, &cosmCutOwnSlider_,
                              &cosmResoOwnSlider_, &modLevelSlider_, &modAttackSlider_, &modReleaseSlider_ }) {
        Ui::styleValueSlider(*sl, sl == &keySlider_ ? 56 : sl == &startSlider_ ? 66 : 44);
        addAndMakeVisible(*sl);
    }
    keyAtt_     = std::make_unique<SliderAtt>(processor_.apvts, "rootKey",     keySlider_);
    depthAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "encodeDepth", depthSlider_);
    energyAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "energy",      energySlider_);
    startAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "startOffset", startSlider_);
    attackAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "ampAttack",   attackSlider_);
    decayAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "ampDecay",    decaySlider_);
    sustainAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "ampSustain",  sustainSlider_);
    releaseAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "ampRelease",  releaseSlider_);
    aSensAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "ampAttackSens",  aSensSlider_);
    dSensAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "ampDecaySens",   dSensSlider_);
    rSensAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "ampReleaseSens", rSensSlider_);
    kfAtt_      = std::make_unique<SliderAtt>(processor_.apvts, "ampTimeKF",      kfSlider_);
    oscTkfAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "oscTimeKF",    oscTkfSlider_);
    oscFkfAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "oscFormantKF", oscFkfSlider_);
    timeOffsetBox_.addItemList({ "BWD", "ZERO", "FWD" }, 1);
    addAndMakeVisible(timeOffsetBox_);
    timeOffsetAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "oscTimeOffset", timeOffsetBox_);
    oscSelBox_.addItemList({ "OSC 1", "OSC 2" }, 1);
    oscSelBox_.setSelectedId(1, juce::dontSendNotification);
    oscSelBox_.onChange = [this] { attachOsc(oscSelBox_.getSelectedId() == 2 ? 1 : 0); };
    oscOnBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    oscOnBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    addAndMakeVisible(oscSelBox_);
    addAndMakeVisible(oscOnBtn_);
    fxOnBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    fxOnBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    addAndMakeVisible(fxOnBtn_);
    fxOnAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor_.apvts, "fxOn", fxOnBtn_);
    addChildComponent(*fxPanel_);
    oscTypeBox_.addItemList({ "ANALOG", "PCM" }, 1);
    anaWaveBox_.addItemList({ "SAW", "SQUARE", "TRIANGLE", "SINE", "RAMP", "JUNO", "HQ-SAW", "HQ-SQUARE", "NOISE", "LA-SAW", "LA-SQUARE",
                              "SUPER-SAW", "FEEDBACK-OSC", "X-MOD" }, 1);
    for (int w = 0; w < 14; ++w) anaWaveBox_.setItemEnabled(w + 1, xtAnalogPlays(w));   // the waveforms the port plays
    addAndMakeVisible(oscTypeBox_);
    addAndMakeVisible(anaWaveBox_);
    oscTypeBox_.onChange = [this] { showOscType(); };
    {
        static const char* const names[kAnaKnobs] = { "Impact", "PW", "PW KF", "Fat", "Fat KF", "PW LFO", "PW Env", "Fat LFO", "Fat Env", "Detune", "Sub Oct", "Sub Lev" };
        for (int i = 0; i < kAnaKnobs; ++i) {
            juce::Slider& sl = anaSliders_[i];
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
            sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
            sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
            sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
            addAndMakeVisible(sl);
            anaLabels_[i].setText(names[i], juce::dontSendNotification);
            styleSmall(anaLabels_[i]);
        }
    }
    loopModeBox_.addItemList({ "One Shot", "One Way", "Alternate" }, 1);
    playbackBox_.addItemList({ "RETRIGGER", "LEGATO", "STEP", "EVENT" }, 1);
    addAndMakeVisible(playbackBox_);
    playbackAtt_  = std::make_unique<ComboAtt>(processor_.apvts, "playbackMode", playbackBox_);
    loopBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    loopBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    addAndMakeVisible(loopBtn_);
    addAndMakeVisible(loopModeBox_);
    loopAtt_      = std::make_unique<ButtonAtt>(processor_.apvts, "loop", loopBtn_);
    variBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    variBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    addAndMakeVisible(variBtn_);
    variAtt_      = std::make_unique<ButtonAtt>(processor_.apvts, "vari", variBtn_);
    loopModeAtt_  = std::make_unique<ComboAtt>(processor_.apvts, "loopMode", loopModeBox_);
    loopStartAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "loopStart", loopStartSlider_);
    loopEndAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "loopEnd", loopEndSlider_);
    portaModeBox_.addItemList({ "NORMAL", "LEGATO" }, 1);
    portaTypeBox_.addItemList({ "RATE", "TIME" }, 1);
    structBox_.addItemList({ "Type1", "Type2", "Type3" }, 1);
    structBox_.onChange = [this] {
        static const char* const order[3] = { "OSC 2, OSC 1, MOD, COSM 1, COSM 2", "OSC 2, OSC 1 > COSM 1, MOD, COSM 2", "OSC 2 > COSM 2, OSC 1 > COSM 1, MOD" };
        structOrderLabel_.setText(order[juce::jlimit(0, 2, structBox_.getSelectedItemIndex())], juce::dontSendNotification);
    };
    addAndMakeVisible(structLabel_);
    addAndMakeVisible(structBox_);
    addAndMakeVisible(structOrderLabel_);
    structAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "structure", structBox_);
    structBox_.onChange();
    modBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    modBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    tvaBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    tvaBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    addAndMakeVisible(tvaBtn_);
    tvaAtt_        = std::make_unique<ButtonAtt>(processor_.apvts, "tvaSwitch", tvaBtn_);
    modTypeBox_.addItemList({ "MIX", "RING", "FM", "ENV RING", "OSC SYNC" }, 1);
    addAndMakeVisible(modBtn_);
    addAndMakeVisible(modTypeBox_);
    modAtt_        = std::make_unique<ButtonAtt>(processor_.apvts, "modSwitch", modBtn_);
    modTypeAtt_    = std::make_unique<ComboAtt>(processor_.apvts, "modType", modTypeBox_);
    modLevelAtt_   = std::make_unique<SliderAtt>(processor_.apvts, "modLevel", modLevelSlider_);
    modAttackAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "modAttack", modAttackSlider_);
    modReleaseAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "modRelease", modReleaseSlider_);
    for (juce::ToggleButton* b : { &syncBtn_, &tripBtn_, &keepBtn_ }) {
        b->setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
        b->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(*b);
    }
    for (juce::Label* l : { &patchTempoLabel_, &tripLabel_, &origTempoLabel_, &clockLabel_ }) styleSmall(*l);
    clockBox_.addItemList({ "INTERNAL", "HOST" }, 1);
    addAndMakeVisible(clockBox_);
    clockAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor_.apvts, "clockSource", clockBox_);
    patchTempoAtt_ = std::make_unique<SliderAtt>(processor_.apvts, "patchTempo", patchTempoSlider_);
    tripAtt_       = std::make_unique<SliderAtt>(processor_.apvts, "timeTrip", tripSlider_);
    origTempoAtt_  = std::make_unique<SliderAtt>(processor_.apvts, "origTempo", origTempoSlider_);
    syncAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, "tempoSync", syncBtn_);
    tripBtnAtt_    = std::make_unique<ButtonAtt>(processor_.apvts, "timeTripSw", tripBtn_);
    keepAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, "beatKeep", keepBtn_);
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
    cosmTypeBox_.addItemList({ "TVF", "Dynamic TVF", "Dual Filter", "TB Filter", "Comb Filter", "1st Side Band", "2nd Side Band", "Resonator", "Frequency Shifter", "Lo-Fi", "Compressor", "Limiter", "Wave Shape", "OD/Dist", "Amp Simulator", "Speaker Simulator" }, 1);
    cosmTypeBox_.onChange = [this] { attachSideBand(); showCosmType(); };
    for (juce::Component* cpt : { (juce::Component*)&cosmSelBox_, (juce::Component*)&cosmBtn_, (juce::Component*)&cosmFilterBox_,
                                  (juce::Component*)&clfoWaveBox_, (juce::Component*)&clfoSyncBtn_, (juce::Component*)&cosmTypeBox_ })
        addAndMakeVisible(*cpt);
    cosmSelBox_.addItemList({ "COSM 1", "COSM 2" }, 1);
    cosmSelBox_.setSelectedId(1, juce::dontSendNotification);
    cosmSelBox_.onChange = [this] { attachCosm(cosmSelBox_.getSelectedId() == 2 ? 1 : 0); };
    for (int i = 0; i < kTvaKnobs; ++i) {
        juce::Slider& sl = tvaSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        tvaAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, kTvaIds[i][0], sl);
        tvaLabels_[i].setText(kTvaIds[i][1], juce::dontSendNotification);
        styleSmall(tvaLabels_[i]);
    }
    tvaRowLabels_[0].setText("TVA", juce::dontSendNotification);
    tvaRowLabels_[1].setText("TVA Pan", juce::dontSendNotification);
    tvaRowLabels_[2].setText("LFO 5", juce::dontSendNotification);
    for (juce::Label& l : tvaRowLabels_) styleSmall(l);
    tlfoWaveBox_.addItemList({ "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 1);
    tlfoModeBox_.addItemList({ "ON <<", "ON >>", "OFF <<", "OFF >>" }, 1);
    tlfoOffsetBox_.addItemList({ "-100", "-50", "0", "+50", "+100" }, 1);
    tlfoSyncBtn_.setColour(juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    tlfoSyncBtn_.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF00BFA5));
    for (juce::Component* cpt : { (juce::Component*)&tlfoWaveBox_, (juce::Component*)&tlfoModeBox_,
                                  (juce::Component*)&tlfoOffsetBox_, (juce::Component*)&tlfoSyncBtn_ }) addAndMakeVisible(*cpt);
    tlfoWaveAtt_   = std::make_unique<ComboAtt>(processor_.apvts, "tvaLfoWave", tlfoWaveBox_);
    tlfoModeAtt_   = std::make_unique<ComboAtt>(processor_.apvts, "tvaLfoFadeMode", tlfoModeBox_);
    tlfoOffsetAtt_ = std::make_unique<ComboAtt>(processor_.apvts, "tvaLfoOffset", tlfoOffsetBox_);
    tlfoSyncAtt_   = std::make_unique<ButtonAtt>(processor_.apvts, "tvaLfoKeySync", tlfoSyncBtn_);
    // The Matrix, after the patch's own knobs
    matrixLabel_.setText("Matrix", juce::dontSendNotification);
    assignLabel_.setText("Assign", juce::dontSendNotification);
    styleSmall(matrixLabel_);
    styleSmall(assignLabel_);
    for (int m = 0; m < 8; ++m) matrixSelBox_.addItem("Control " + juce::String(m + 1), m + 1);
    matrixSourceBox_.addItemList(VSynthEmuProcessor::matrixSourceNames(), 1);
    for (int j = 0; j < 2; ++j) {
        matrixDestBox_[j].addItemList(VSynthEmuProcessor::matrixDestNames(), 1);
        juce::Slider& sl = matrixSens_[j];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        addAndMakeVisible(matrixDestBox_[j]);
        matrixSensLabels_[j].setText("Sens", juce::dontSendNotification);
        styleSmall(matrixSensLabels_[j]);
    }
    for (int i = 0; i < VSynthEmuProcessor::kAssigns; ++i) assignSelBox_.addItem(VSynthEmuProcessor::assignName(i), i + 1);
    assignBox_.addItemList(VSynthEmuProcessor::assignValueNames(), 1);
    for (juce::ComboBox* b : { &matrixSelBox_, &matrixSourceBox_, &assignSelBox_, &assignBox_ }) addAndMakeVisible(*b);
    matrixSelBox_.onChange = [this] { attachMatrix(juce::jlimit(0, 7, matrixSelBox_.getSelectedItemIndex())); };
    assignSelBox_.onChange = [this] { attachAssign(juce::jlimit(0, VSynthEmuProcessor::kAssigns - 1, assignSelBox_.getSelectedItemIndex())); };
    matrixSelBox_.setSelectedId(1, juce::sendNotificationSync);
    assignSelBox_.setSelectedId(7, juce::sendNotificationSync);
    lfoWaveBox_.addItemList({ "SIN", "TRI", "SAW", "SQR", "RND", "TRP", "S&H", "CHAOS" }, 1);
    lfoModeBox_.addItemList({ "ON <<", "ON >>", "OFF <<", "OFF >>" }, 1);
    lfoOffsetBox_.addItemList({ "-100", "-50", "0", "+50", "+100" }, 1);
    for (juce::Component* cpt : { (juce::Component*)&lfoWaveBox_, (juce::Component*)&lfoModeBox_,
                                   (juce::Component*)&lfoOffsetBox_, (juce::Component*)&lfoSyncBtn_ }) addAndMakeVisible(*cpt);
    for (int i = 0; i < kLevelKnobs; ++i) {
        juce::Slider& sl = levelSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        if (!kLevelOsc(i)) levelAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, kLevelIds[i][0], sl);
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
    tvfLabel_.setText("TVF", juce::dontSendNotification);
    styleSmall(tvfLabel_);
    dualLabel_.setText("Dual", juce::dontSendNotification);
    styleSmall(dualLabel_);
    resTypeBox_.addItemList({ "1", "2", "3" }, 1);
    addAndMakeVisible(resTypeBox_);
    for (int i = 0; i < kResKnobs; ++i) {
        juce::Slider& sl = resSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        resLabels_[i].setText(kResIds[i][1], juce::dontSendNotification);
        styleSmall(resLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        resRowLabels_[r].setText(r ? "Res Bal" : "Res", juce::dontSendNotification);
        styleSmall(resRowLabels_[r]);
    }
    for (int i = 0; i < kFsKnobs; ++i) {
        juce::Slider& sl = fsSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        fsLabels_[i].setText(kFsIds[i][1], juce::dontSendNotification);
        styleSmall(fsLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        fsRowLabels_[r].setText(r ? "FS Bal" : "Shift", juce::dontSendNotification);
        styleSmall(fsRowLabels_[r]);
    }
    for (int i = 0; i < kLofiKnobs; ++i) {
        juce::Slider& sl = lofiSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        lofiLabels_[i].setText(kLofiIds[i][1], juce::dontSendNotification);
        styleSmall(lofiLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        lofiRowLabels_[r].setText(r ? "Lo-Fi F" : "Lo-Fi", juce::dontSendNotification);
        styleSmall(lofiRowLabels_[r]);
    }
    for (int i = 0; i < kCompKnobs; ++i) {
        juce::Slider& sl = compSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        compLabels_[i].setText(kCompIds[i][1], juce::dontSendNotification);
        styleSmall(compLabels_[i]);
    }
    compRowLabel_.setText("Comp", juce::dontSendNotification);
    styleSmall(compRowLabel_);
    limRatioBox_.addItemList({ "1.5:1", "2:1", "4:1", "100:1" }, 1);
    addAndMakeVisible(limRatioBox_);
    for (int i = 0; i < kLimKnobs; ++i) {
        juce::Slider& sl = limSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        limLabels_[i].setText(kLimIds[i][1], juce::dontSendNotification);
        styleSmall(limLabels_[i]);
    }
    limRowLabel_.setText("Limiter", juce::dontSendNotification);
    styleSmall(limRowLabel_);
    wsTypeBox_.addItemList({ "1", "2", "3", "4", "5", "6" }, 1);
    addAndMakeVisible(wsTypeBox_);
    for (int i = 0; i < kWsKnobs; ++i) {
        juce::Slider& sl = wsSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        wsLabels_[i].setText(kWsIds[i][1], juce::dontSendNotification);
        styleSmall(wsLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        wsRowLabels_[r].setText(r ? "WS Tone" : "W-Shape", juce::dontSendNotification);
        styleSmall(wsRowLabels_[r]);
    }
    odTypeBox_.addItemList({ "OD", "DS" }, 1);
    addAndMakeVisible(odTypeBox_);
    for (int i = 0; i < kWsKnobs; ++i) {
        juce::Slider& sl = odSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        odLabels_[i].setText(kOdIds[i][1], juce::dontSendNotification);
        styleSmall(odLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        odRowLabels_[r].setText(r ? "OD Tone" : "OD/Dist", juce::dontSendNotification);
        styleSmall(odRowLabels_[r]);
    }
    ampTypeBox_.addItemList({ "1", "2", "3" }, 1);
    addAndMakeVisible(ampTypeBox_);
    for (int i = 0; i < kAmpKnobs; ++i) {
        juce::Slider& sl = ampSliders_[i];
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 20);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::lightgrey);
        sl.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF0D0D1A));
        sl.setColour(juce::Slider::trackColourId, juce::Colour(0xFF00BFA5));
        addAndMakeVisible(sl);
        ampLabels_[i].setText(kAmpIds[i][1], juce::dontSendNotification);
        styleSmall(ampLabels_[i]);
    }
    for (int r = 0; r < 2; ++r) {
        ampRowLabels_[r].setText(r ? "Amp EQ" : "Amp", juce::dontSendNotification);
        styleSmall(ampRowLabels_[r]);
    }
    spkTypeBox_.addItemList({ "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12" }, 1);
    addAndMakeVisible(spkTypeBox_);
    spkRowLabel_.setText("Speaker", juce::dontSendNotification);
    styleSmall(spkRowLabel_);
    attachCosm(0);
    for (juce::Label* l : { &encodeLabel_, &keyLabel_, &depthLabel_, &energyLabel_, &startLabel_, &attackLabel_, &decayLabel_,
                            &sustainLabel_, &releaseLabel_, &aSensLabel_, &dSensLabel_, &rSensLabel_, &kfLabel_,
                            &pAttackLabel_, &pDecayLabel_, &pSustainLabel_, &pReleaseLabel_, &pDepthLabel_,
                            &tAttackLabel_, &tDecayLabel_, &tSustainLabel_, &tReleaseLabel_, &tDepthLabel_,
                            &fAttackLabel_, &fDecayLabel_, &fSustainLabel_, &fReleaseLabel_, &fDepthLabel_,
                            &offsetLabel_, &oscTkfLabel_, &oscFkfLabel_,
                            &lfoLabel_, &lfoRateLabel_, &lfoDepthLabel_, &lfoDelayLabel_, &lfoFadeLabel_,
                            &lfoDepthsLabel_, &lfoTimeLabel_, &lfoFmtLabel_, &loopStartLabel_, &loopEndLabel_,
                            &portaTimeLabel_, &portaSensLabel_, &octLabel_, &cosmCutLabel_, &cosmResoLabel_, &cosmKfLabel_,
                            &clfoLabel_, &clfoRateLabel_, &cosmTypeLabel_, &cosmCutOwnLabel_, &cosmResoOwnLabel_,
                            &anaWaveLabel_, &modLevelLabel_, &modAttackLabel_, &modReleaseLabel_, &structLabel_, &structOrderLabel_ }) styleSmall(*l);
    showCosmType();                                  // after styleSmall, which shows every label
    showOscType();
    timerCallback();
    startTimerHz(4);

    // ── Knobs ─────────────────────────────────────────────────────────────────
    styleSlider(pitchSlider_,   pitchLabel_);
    styleSlider(timeSlider_,    timeLabel_);
    styleSlider(formantSlider_, formantLabel_);

    attachOsc(0);
    attachMainKnobs(processor_.hasRom());

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

    buildLayout();
    setSize(960, 600);
    setResizable(true, true);
    setResizeLimits(900, 600, 1400, 900);
}

VSynthEmuEditor::~VSynthEmuEditor() {
    stopTimer();
    algoBox_.removeListener(this);
}

void VSynthEmuEditor::attachMainKnobs(bool xt) {
    pitchAtt_.reset();
    timeAtt_.reset();
    formantAtt_.reset();
    robotAtt_.reset();
    // with the XT, the selected OSC's Pitch, Time and Formant knobs and Robot
    // Voice (Pitch Shift stays a host parameter, added to the patch's Coarse/Fine Tune)
    auto id = [this](const char* name) { return VSynthEmuProcessor::oscParamId(oscK_, name); };
    pitchAtt_   = std::make_unique<SliderAtt>(processor_.apvts, xt ? id("oscPitch") : juce::String("pitchShift"), pitchSlider_);
    timeAtt_    = std::make_unique<SliderAtt>(processor_.apvts, xt ? id("oscTime") : juce::String("timeStretch"), timeSlider_);
    formantAtt_ = std::make_unique<SliderAtt>(processor_.apvts, xt ? id("oscFormant") : juce::String("formantShift"), formantSlider_);
    robotAtt_   = std::make_unique<ButtonAtt>(processor_.apvts, xt ? id("robot") : juce::String("robot"), robotBtn_);
    mainKnobsXt_ = xt;
}

// The COSM rows show one section: its switch, type, Cutoff, Resonance, KF,
// the depth and Env A rows (level slots 24..31), the Dual, TB, Comb and Side
// Band Filters', the Resonator's, the Frequency Shifter's, Lo-Fi's, the
// Compressor's, the Limiter's, Wave Shape's, OD/Dist's and the Amp and
// Speaker Simulators' rows and its LFO.
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
    resTypeAtt_.reset();
    resTypeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "ResType", resTypeBox_);
    for (int i = 0; i < kResKnobs; ++i) {
        resAtts_[i].reset();
        resAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kResIds[i][0], resSliders_[i]);
    }
    for (int i = 0; i < kFsKnobs; ++i) {
        fsAtts_[i].reset();
        fsAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kFsIds[i][0], fsSliders_[i]);
    }
    for (int i = 0; i < kLofiKnobs; ++i) {
        lofiAtts_[i].reset();
        lofiAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kLofiIds[i][0], lofiSliders_[i]);
    }
    for (int i = 0; i < kCompKnobs; ++i) {
        compAtts_[i].reset();
        compAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kCompIds[i][0], compSliders_[i]);
    }
    limRatioAtt_.reset();
    limRatioAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "LimRatio", limRatioBox_);
    for (int i = 0; i < kLimKnobs; ++i) {
        limAtts_[i].reset();
        limAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kLimIds[i][0], limSliders_[i]);
    }
    wsTypeAtt_.reset();
    wsTypeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "WsType", wsTypeBox_);
    for (int i = 0; i < kWsKnobs; ++i) {
        wsAtts_[i].reset();
        wsAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kWsIds[i][0], wsSliders_[i]);
    }
    odTypeAtt_.reset();
    odTypeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "OdType", odTypeBox_);
    for (int i = 0; i < kWsKnobs; ++i) {
        odAtts_[i].reset();
        odAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kOdIds[i][0], odSliders_[i]);
    }
    ampTypeAtt_.reset();
    ampTypeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "AmpType", ampTypeBox_);
    for (int i = 0; i < kAmpKnobs; ++i) {
        ampAtts_[i].reset();
        ampAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + kAmpIds[i][0], ampSliders_[i]);
    }
    spkTypeAtt_.reset();
    spkTypeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, pre + "SpkType", spkTypeBox_);
    clfoLabel_.setText(k ? "LFO 4" : "LFO 3", juce::dontSendNotification);
    attachSideBand();
    showCosmType();
}

// The selected OSC's own rows: its switch, type, wave (the Load button, the file
// name, Encode), Key and Depth, Energy and the VariPhrase knobs, Loop, analog
// knobs, pitch, TVA, amp envelope, velocity to level, pitch, time and formant
// envelopes and LFO; the PITCH, TIME and FORMANT knobs and Robot too.
void VSynthEmuEditor::attachOsc(int k) {
    oscK_ = k;
    auto id = [k](const char* name) { return VSynthEmuProcessor::oscParamId(k, name); };
    showFileName();
    oscOnAtt_.reset(); oscTypeAtt_.reset(); anaWaveAtt_.reset();
    oscOnAtt_ = std::make_unique<ButtonAtt>(processor_.apvts, id("oscSwitch"), oscOnBtn_);
    oscTypeAtt_ = std::make_unique<ComboAtt>(processor_.apvts, id("oscType"), oscTypeBox_);
    anaWaveAtt_ = std::make_unique<ComboAtt>(processor_.apvts, id("anaWave"), anaWaveBox_);
    encodeAtt_.reset(); keyAtt_.reset(); depthAtt_.reset(); energyAtt_.reset(); startAtt_.reset(); oscTkfAtt_.reset(); oscFkfAtt_.reset(); timeOffsetAtt_.reset();
    loopAtt_.reset(); loopModeAtt_.reset(); loopStartAtt_.reset(); loopEndAtt_.reset(); playbackAtt_.reset();
    origTempoAtt_.reset(); syncAtt_.reset(); tripBtnAtt_.reset(); keepAtt_.reset();
    origTempoAtt_  = std::make_unique<SliderAtt>(processor_.apvts, id("origTempo"), origTempoSlider_);
    syncAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, id("tempoSync"), syncBtn_);
    tripBtnAtt_    = std::make_unique<ButtonAtt>(processor_.apvts, id("timeTripSw"), tripBtn_);
    keepAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, id("beatKeep"), keepBtn_);
    encodeAtt_     = std::make_unique<ComboAtt>(processor_.apvts, id("encodeType"), encodeBox_);
    keyAtt_        = std::make_unique<SliderAtt>(processor_.apvts, id("rootKey"), keySlider_);
    depthAtt_      = std::make_unique<SliderAtt>(processor_.apvts, id("encodeDepth"), depthSlider_);
    energyAtt_     = std::make_unique<SliderAtt>(processor_.apvts, id("energy"), energySlider_);
    startAtt_      = std::make_unique<SliderAtt>(processor_.apvts, id("startOffset"), startSlider_);
    oscTkfAtt_     = std::make_unique<SliderAtt>(processor_.apvts, id("oscTimeKF"), oscTkfSlider_);
    oscFkfAtt_     = std::make_unique<SliderAtt>(processor_.apvts, id("oscFormantKF"), oscFkfSlider_);
    timeOffsetAtt_ = std::make_unique<ComboAtt>(processor_.apvts, id("oscTimeOffset"), timeOffsetBox_);
    loopAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, id("loop"), loopBtn_);
    variAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, id("vari"), variBtn_);
    loopModeAtt_   = std::make_unique<ComboAtt>(processor_.apvts, id("loopMode"), loopModeBox_);
    playbackAtt_   = std::make_unique<ComboAtt>(processor_.apvts, id("playbackMode"), playbackBox_);
    loopStartAtt_  = std::make_unique<SliderAtt>(processor_.apvts, id("loopStart"), loopStartSlider_);
    loopEndAtt_    = std::make_unique<SliderAtt>(processor_.apvts, id("loopEnd"), loopEndSlider_);
    static const char* const anaIds[kAnaKnobs] = { "anaImpact", "anaPw", "anaPwKF", "anaFat", "anaFatKF", "anaPwLfo", "anaPwEnv", "anaFatLfo", "anaFatEnv", "anaDetune", "anaSubOct", "anaSubLevel" };
    for (int i = 0; i < kAnaKnobs; ++i) {
        anaAtt_[i].reset();
        anaAtt_[i] = std::make_unique<SliderAtt>(processor_.apvts, id(anaIds[i]), anaSliders_[i]);
    }
    for (int i = 0; i < kLevelKnobs; ++i)
        if (kLevelOsc(i)) {
            levelAtts_[i].reset();
            levelAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, id(kLevelIds[i][0]), levelSliders_[i]);
        }
    const std::tuple<std::unique_ptr<SliderAtt>*, const char*, juce::Slider*> sliders[] = {
        { &pAttackAtt_, "pitchEnvAttack", &pAttackSlider_ }, { &pDecayAtt_, "pitchEnvDecay", &pDecaySlider_ }, { &pSustainAtt_, "pitchEnvSustain", &pSustainSlider_ },
        { &pReleaseAtt_, "pitchEnvRelease", &pReleaseSlider_ }, { &pDepthAtt_, "pitchEnvDepth", &pDepthSlider_ },
        { &tAttackAtt_, "timeEnvAttack", &tAttackSlider_ }, { &tDecayAtt_, "timeEnvDecay", &tDecaySlider_ }, { &tSustainAtt_, "timeEnvSustain", &tSustainSlider_ },
        { &tReleaseAtt_, "timeEnvRelease", &tReleaseSlider_ }, { &tDepthAtt_, "timeEnvDepth", &tDepthSlider_ },
        { &fAttackAtt_, "formantEnvAttack", &fAttackSlider_ }, { &fDecayAtt_, "formantEnvDecay", &fDecaySlider_ }, { &fSustainAtt_, "formantEnvSustain", &fSustainSlider_ },
        { &fReleaseAtt_, "formantEnvRelease", &fReleaseSlider_ }, { &fDepthAtt_, "formantEnvDepth", &fDepthSlider_ },
        { &lfoRateAtt_, "lfoRate", &lfoRateSlider_ }, { &lfoDepthAtt_, "lfoPitchDepth", &lfoDepthSlider_ }, { &lfoDelayAtt_, "lfoDelay", &lfoDelaySlider_ },
        { &lfoFadeAtt_, "lfoFade", &lfoFadeSlider_ }, { &lfoTimeAtt_, "lfoTimeDepth", &lfoTimeSlider_ }, { &lfoFmtAtt_, "lfoFormantDepth", &lfoFmtSlider_ } };
    for (const auto& [att, name, sl] : sliders) {
        att->reset();
        *att = std::make_unique<SliderAtt>(processor_.apvts, id(name), *sl);
    }
    lfoWaveAtt_.reset(); lfoModeAtt_.reset(); lfoOffsetAtt_.reset(); lfoSyncAtt_.reset();
    lfoWaveAtt_   = std::make_unique<ComboAtt>(processor_.apvts, id("lfoWave"), lfoWaveBox_);
    lfoModeAtt_   = std::make_unique<ComboAtt>(processor_.apvts, id("lfoFadeMode"), lfoModeBox_);
    lfoOffsetAtt_ = std::make_unique<ComboAtt>(processor_.apvts, id("lfoOffset"), lfoOffsetBox_);
    lfoSyncAtt_   = std::make_unique<ButtonAtt>(processor_.apvts, id("lfoKeySync"), lfoSyncBtn_);
    lfoLabel_.setText(k ? "LFO 2" : "LFO", juce::dontSendNotification);
    if (timeAtt_) attachMainKnobs(mainKnobsXt_);
    showOscType();
}

// ANALOG shows the analog oscillator's three rows where the PCM OSC's
// (Encode, Key and Depth; Energy and the VariPhrase knobs; Loop) are.
void VSynthEmuEditor::showOscType() {
    const bool ana = oscTypeBox_.getSelectedItemIndex() == 0;
    for (juce::Component* cpt : { (juce::Component*)&encodeLabel_, (juce::Component*)&encodeBox_, (juce::Component*)&keyLabel_,
                                  (juce::Component*)&keySlider_, (juce::Component*)&depthLabel_, (juce::Component*)&depthSlider_,
                                  (juce::Component*)&energyLabel_, (juce::Component*)&energySlider_, (juce::Component*)&startLabel_,
                                  (juce::Component*)&startSlider_, (juce::Component*)&playbackBox_, (juce::Component*)&offsetLabel_,
                                  (juce::Component*)&timeOffsetBox_, (juce::Component*)&oscTkfLabel_, (juce::Component*)&oscTkfSlider_,
                                  (juce::Component*)&oscFkfLabel_, (juce::Component*)&oscFkfSlider_, (juce::Component*)&loopBtn_, (juce::Component*)&variBtn_,
                                  (juce::Component*)&loopModeBox_, (juce::Component*)&loopStartLabel_, (juce::Component*)&loopStartSlider_,
                                  (juce::Component*)&loopEndLabel_, (juce::Component*)&loopEndSlider_, (juce::Component*)&syncBtn_,
                                  (juce::Component*)&tripBtn_, (juce::Component*)&keepBtn_, (juce::Component*)&origTempoLabel_,
                                  (juce::Component*)&origTempoSlider_ })
        cpt->setVisible(!ana);
    anaWaveLabel_.setVisible(ana);
    anaWaveBox_.setVisible(ana);
    for (int i = 0; i < kAnaKnobs; ++i) { anaLabels_[i].setVisible(ana); anaSliders_[i].setVisible(ana); }
    repaintPages();
}

// The type's own rows share one area: only the selected type's are shown
// (the TVF's Filter Type, Cutoff, Resonance, KF and depths for the TVF and
// Dynamic TVF, with the Own depths for the Dynamic TVF).
void VSynthEmuEditor::showCosmType() {
    const int t = cosmTypeBox_.getSelectedItemIndex();
    const bool tvf = t <= 1, dtvf = t == 1, dual = t == 2, tb = t == 3, comb = t == 4, sb = t == 5 || t == 6, res = t == 7, fs = t == 8, lofi = t == 9, comp = t == 10,
               lim = t == 11, ws = t == 12, od = t == 13, amp = t == 14, spk = t == 15;
    for (juce::Component* cpt : { (juce::Component*)&tvfLabel_, (juce::Component*)&cosmFilterBox_, (juce::Component*)&cosmCutLabel_,
                                  (juce::Component*)&cosmCutSlider_, (juce::Component*)&cosmResoLabel_, (juce::Component*)&cosmResoSlider_,
                                  (juce::Component*)&cosmKfLabel_, (juce::Component*)&cosmKfSlider_, (juce::Component*)&levelRowLabels_[6] })
        cpt->setVisible(tvf);
    for (int i = 24; i < 28; ++i) { levelLabels_[i].setVisible(tvf); levelSliders_[i].setVisible(tvf); }
    for (juce::Component* cpt : { (juce::Component*)&cosmCutOwnLabel_, (juce::Component*)&cosmCutOwnSlider_, (juce::Component*)&cosmResoOwnLabel_,
                                  (juce::Component*)&cosmResoOwnSlider_ })
        cpt->setVisible(dtvf);
    dualLabel_.setVisible(dual);
    dualTypeBox_.setVisible(dual);
    for (int i = 0; i < kDualKnobs; ++i) { dualLabels_[i].setVisible(dual); dualSliders_[i].setVisible(dual); }
    for (juce::Label& l : dualRowLabels_) l.setVisible(dual);
    for (int i = 0; i < kTbKnobs; ++i) { tbLabels_[i].setVisible(tb); tbSliders_[i].setVisible(tb); }
    for (juce::Label& l : tbRowLabels_) l.setVisible(tb);
    combOctBox_.setVisible(comb);
    for (int i = 0; i < kCombKnobs; ++i) { combLabels_[i].setVisible(comb); combSliders_[i].setVisible(comb); }
    for (juce::Label& l : combRowLabels_) l.setVisible(comb);
    sbOctBox_.setVisible(sb);
    for (int i = 0; i < kSbKnobs; ++i) {
        const bool on = sb && !(t == 6 && i == 3);   // the 2nd has no Sub Detune
        sbLabels_[i].setVisible(on);
        sbSliders_[i].setVisible(on);
    }
    for (juce::Label& l : sbRowLabels_) l.setVisible(sb);
    resTypeBox_.setVisible(res);
    for (int i = 0; i < kResKnobs; ++i) { resLabels_[i].setVisible(res); resSliders_[i].setVisible(res); }
    for (juce::Label& l : resRowLabels_) l.setVisible(res);
    for (int i = 0; i < kFsKnobs; ++i) { fsLabels_[i].setVisible(fs); fsSliders_[i].setVisible(fs); }
    for (juce::Label& l : fsRowLabels_) l.setVisible(fs);
    for (int i = 0; i < kLofiKnobs; ++i) { lofiLabels_[i].setVisible(lofi); lofiSliders_[i].setVisible(lofi); }
    for (juce::Label& l : lofiRowLabels_) l.setVisible(lofi);
    for (int i = 0; i < kCompKnobs; ++i) { compLabels_[i].setVisible(comp); compSliders_[i].setVisible(comp); }
    compRowLabel_.setVisible(comp);
    limRatioBox_.setVisible(lim);
    for (int i = 0; i < kLimKnobs; ++i) { limLabels_[i].setVisible(lim); limSliders_[i].setVisible(lim); }
    limRowLabel_.setVisible(lim);
    wsTypeBox_.setVisible(ws);
    for (int i = 0; i < kWsKnobs; ++i) { wsLabels_[i].setVisible(ws); wsSliders_[i].setVisible(ws); }
    for (juce::Label& l : wsRowLabels_) l.setVisible(ws);
    odTypeBox_.setVisible(od);
    for (int i = 0; i < kWsKnobs; ++i) { odLabels_[i].setVisible(od); odSliders_[i].setVisible(od); }
    for (juce::Label& l : odRowLabels_) l.setVisible(od);
    ampTypeBox_.setVisible(amp);
    for (int i = 0; i < kAmpKnobs; ++i) { ampLabels_[i].setVisible(amp); ampSliders_[i].setVisible(amp); }
    for (juce::Label& l : ampRowLabels_) l.setVisible(amp);
    spkTypeBox_.setVisible(spk);
    spkRowLabel_.setVisible(spk);
    repaintPages();
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
        if (!(second && i == 3))
            sbAtts_[i] = std::make_unique<SliderAtt>(processor_.apvts, pre + (second && i == 0 ? "Q" : kSbIds[i][0]), sbSliders_[i]);
    }
    sbLabels_[0].setText(second ? "Q" : "HPF", juce::dontSendNotification);
}

void VSynthEmuEditor::timerCallback() {
    if (timeAtt_ && processor_.hasRom() != mainKnobsXt_) attachMainKnobs(processor_.hasRom());
    showFileName();
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
    g.fillAll(Ui::surfaceApp);

    // Header: the wordmark
    g.setColour(Ui::surfaceTitlebar);
    g.fillRect(0, 0, getWidth(), Ui::headerH);
    g.setColour(Ui::title);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("V-SYNTH EMU", 16, 2, 200, Ui::headerH - 16, juce::Justification::centredLeft);

    // Source bar
    g.setColour(Ui::surfaceStrip);
    g.fillRect(0, Ui::headerH, getWidth(), Ui::sourceH);
    g.setColour(Ui::line);
    g.fillRect(0, Ui::headerH, getWidth(), 1);
    g.fillRect(0, Ui::headerH + Ui::sourceH - 1, getWidth(), 1);
    g.fillRect(fileNameLabel_.getRight() + 8, Ui::headerH + 14, 1, Ui::sourceH - 28);   // between the file and the OSC

    // The perform column
    const int top = Ui::headerH + Ui::sourceH;
    g.setColour(Ui::surfaceXt);
    g.fillRect(0, top, Ui::performW, getHeight() - top);
    g.setColour(Ui::line);
    g.fillRect(Ui::performW - 1, top, 1, getHeight() - top);
    juce::Font f(11.0f, juce::Font::bold);
    f.setExtraKerningFactor(0.08f);
    g.setFont(f);
    g.setColour(Ui::label);
    g.drawText(mainKnobsXt_ ? "PERFORM  OSC " + juce::String(oscK_ + 1) : juce::String("PERFORM"), 18, top + 10, Ui::performW - 36, 16, juce::Justification::centredLeft);
}

// ─── Layout ───────────────────────────────────────────────────────────────────

void VSynthEmuEditor::resized() {
    if (tabBar_ == nullptr) return;                  // setSize() runs before buildLayout()
    // Header: Algorithm and the ROM at the right
    auto hdr = juce::Rectangle<int>(0, 0, getWidth(), Ui::headerH).reduced(16, 6);
    romButton_.setBounds(hdr.removeFromRight(70));
    hdr.removeFromRight(10);
    statusLabel_.setBounds(hdr.removeFromRight(170));
    hdr.removeFromRight(16);
    algoBox_.setBounds(hdr.removeFromRight(230));
    hdr.removeFromRight(6);
    algoLabel_.setBounds(hdr.removeFromRight(72));
    hdr.removeFromRight(6);
    fxOnBtn_.setBounds(hdr.removeFromRight(76));
    buildLabel_.setBounds(16, Ui::headerH - 17, hdr.getRight() - 16, 13);     // under the wordmark

    // Source bar: Load, the file, the OSC selector, its switch and type
    auto src = juce::Rectangle<int>(0, Ui::headerH, getWidth(), Ui::sourceH).reduced(16, 13);
    loadButton_.setBounds(src.removeFromLeft(100));
    src.removeFromLeft(6);
    patchButton_.setBounds(src.removeFromLeft(100));
    src.removeFromLeft(10);
    oscTypeBox_.setBounds(src.removeFromRight(110).reduced(0, 2));
    src.removeFromRight(10);
    oscOnBtn_.setBounds(src.removeFromRight(54));
    src.removeFromRight(6);
    oscSeg_->setBounds(src.removeFromRight(140).reduced(0, 2));
    src.removeFromRight(24);
    fileNameLabel_.setBounds(src);

    // The perform column: Pitch, Time, Formant, Robot
    const int top = Ui::headerH + Ui::sourceH;
    auto col = juce::Rectangle<int>(0, top + 32, Ui::performW, getHeight() - top - 32);
    for (auto [s, l] : { std::pair<juce::Slider*, juce::Label*>{ &pitchSlider_, &pitchLabel_ },
                         { &timeSlider_, &timeLabel_ }, { &formantSlider_, &formantLabel_ } }) {
        l->setBounds(col.removeFromTop(16));
        s->setBounds(col.removeFromTop(112).withSizeKeepingCentre(160, 112));
        col.removeFromTop(10);
    }
    robotBtn_.setBounds(col.removeFromTop(22).withSizeKeepingCentre(88, 22));

    // The tabs
    tabBar_->setBounds(Ui::performW, top, getWidth() - Ui::performW, Ui::tabH);
    for (auto& p : pages_)
        p->setBounds(Ui::performW, top + Ui::tabH, getWidth() - Ui::performW, getHeight() - top - Ui::tabH);
    fxPanel_->setBounds(Ui::performW, top + Ui::tabH, getWidth() - Ui::performW, getHeight() - top - Ui::tabH);
}

void VSynthEmuEditor::showTab(int t) {
    tab_ = juce::jlimit(0, kEffectsTab, t);
    for (int i = 0; i < kPages; ++i) pages_[i]->setVisible(i == tab_);
    fxPanel_->setVisible(tab_ == kEffectsTab);
    tabBar_->current = tab_;
    tabBar_->repaint();
}

void VSynthEmuEditor::repaintPages() {
    if (tabBar_ == nullptr) return;                  // before buildLayout
    for (auto& p : pages_) p->repaint();
    repaint();                                       // the Perform column's title
}

juce::Label* VSynthEmuEditor::newLabel(const juce::String& text) {
    auto* l = extraLabels_.add(new juce::Label({}, text));
    styleSmall(*l);
    return l;
}

// The selected COSM type's rows (three at most), in the COSM tab's type card.
// The rows are the old layout's, wider: every type's sit in the same area
// and showCosmType shows one type's.
void VSynthEmuEditor::layoutCosmType(juce::Rectangle<int> area) {
    auto rowOf = [&](int r) { return area.withTrimmedTop(30 * r).withHeight(24); };
    {                                                // the TVF: Filter Type, Cutoff, Resonance, KF; the depths; the Own depths
        auto row = rowOf(0);
        tvfLabel_.setBounds(row.removeFromLeft(56));
        cosmFilterBox_.setBounds(row.removeFromLeft(92).withTrimmedRight(6));
        const int w3 = row.getWidth() / 3;
        std::pair<juce::Label*, juce::Slider*> cells[] = { { &cosmCutLabel_, &cosmCutSlider_ }, { &cosmResoLabel_, &cosmResoSlider_ },
                                                           { &cosmKfLabel_, &cosmKfSlider_ } };
        for (auto& [l, sl] : cells) {
            auto cell = row.removeFromLeft(w3);
            l->setBounds(cell.removeFromLeft(28));
            sl->setBounds(cell.withTrimmedRight(4));
        }
        row = rowOf(1);
        levelRowLabels_[6].setBounds(row.removeFromLeft(56));
        const int quarter = row.getWidth() / 4;
        for (int i = 24; i < 28; ++i) {
            auto cell = row.removeFromLeft(quarter);
            levelLabels_[i].setBounds(cell.removeFromLeft(46));
            levelSliders_[i].setBounds(cell.withTrimmedRight(6));
        }
        row = rowOf(2);
        row.removeFromLeft(56);
        auto half = row.removeFromLeft(row.getWidth() / 2);
        cosmCutOwnLabel_.setBounds(half.removeFromLeft(46));
        cosmCutOwnSlider_.setBounds(half.withTrimmedRight(6));
        cosmResoOwnLabel_.setBounds(row.removeFromLeft(46));
        cosmResoOwnSlider_.setBounds(row);
    }
    {                                                // the Dual Filter: its type, Filter 1, Filter 2
        auto row = rowOf(0);
        dualLabel_.setBounds(row.removeFromLeft(56));
        dualTypeBox_.setBounds(row.removeFromLeft(110).withTrimmedRight(8));
        for (int r = 0; r < 2; ++r) {
            row = rowOf(r + 1);
            dualRowLabels_[r].setBounds(row.removeFromLeft(56));
            const int fifth = row.getWidth() / 5;
            for (int i = 5 * r; i < 5 * r + 5; ++i) {
                auto cell = row.removeFromLeft(fifth);
                dualLabels_[i].setBounds(cell.removeFromLeft(28));
                dualSliders_[i].setBounds(cell.withTrimmedRight(4));
            }
        }
    }
    for (int r = 0; r < 2; ++r) {                    // the TB Filter: LPF (6), HPF (5)
        auto row = rowOf(r);
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
        auto row = rowOf(r);
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
        auto row = rowOf(r);
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
    for (int r = 0; r < 2; ++r) {                    // the Resonator: Type + Size, KF, LFO, Env; then Balance, LFO, Env
        auto row = rowOf(r);
        resRowLabels_[r].setBounds(row.removeFromLeft(46));
        if (r == 0) resTypeBox_.setBounds(row.removeFromLeft(62).withTrimmedRight(6));
        else row.removeFromLeft(62);
        const int first = r ? 4 : 0, n = r ? 3 : 4, w = row.getWidth() / n;
        for (int i = first; i < first + n; ++i) {
            auto cell = row.removeFromLeft(w);
            resLabels_[i].setBounds(cell.removeFromLeft(34));
            resSliders_[i].setBounds(cell.withTrimmedRight(4));
        }
    }
    for (int r = 0; r < 2; ++r) {                    // the Frequency Shifter: Depth, KF, LFO, Env; then Balance, LFO, Env
        auto row = rowOf(r);
        fsRowLabels_[r].setBounds(row.removeFromLeft(56));
        const int first = r ? 4 : 0, n = r ? 3 : 4, w = row.getWidth() / 4;
        for (int i = first; i < first + n; ++i) {
            auto cell = row.removeFromLeft(w);
            fsLabels_[i].setBounds(cell.removeFromLeft(34));
            fsSliders_[i].setBounds(cell.withTrimmedRight(4));
        }
    }
    for (int r = 0; r < 2; ++r) {                    // Lo-Fi: Bit, Rate, Balance, LFO, Env; then Filter, KF, LFO, Env
        auto row = rowOf(r);
        lofiRowLabels_[r].setBounds(row.removeFromLeft(56));
        const int first = r ? 5 : 0, n = r ? 4 : 5, w = row.getWidth() / 5;
        for (int i = first; i < first + n; ++i) {
            auto cell = row.removeFromLeft(w);
            lofiLabels_[i].setBounds(cell.removeFromLeft(30));
            lofiSliders_[i].setBounds(cell.withTrimmedRight(3));
        }
    }
    {                                                // the Compressor: Attack, Sustain, Level
        auto row = rowOf(0);
        compRowLabel_.setBounds(row.removeFromLeft(56));
        const int w = row.getWidth() / 3;
        for (int i = 0; i < kCompKnobs; ++i) {
            auto cell = row.removeFromLeft(w);
            compLabels_[i].setBounds(cell.removeFromLeft(34));
            compSliders_[i].setBounds(cell.withTrimmedRight(4));
        }
    }
    for (int r = 0; r < 2; ++r) {                    // the Amp Simulator: Type + Gain, LFO, Env, Level; then Bass, Middle, Treble
        auto row = rowOf(r);
        ampRowLabels_[r].setBounds(row.removeFromLeft(56));
        auto box = row.removeFromLeft(62);
        if (r == 0) ampTypeBox_.setBounds(box.withTrimmedRight(6));
        const int first = r ? 4 : 0, n = r ? 3 : 4, w = row.getWidth() / 4;
        for (int i = first; i < first + n; ++i) {
            auto cell = row.removeFromLeft(w);
            ampLabels_[i].setBounds(cell.removeFromLeft(32));
            ampSliders_[i].setBounds(cell.withTrimmedRight(3));
        }
    }
    {                                                // the Speaker Simulator: Speaker Type
        auto row = rowOf(0);
        spkRowLabel_.setBounds(row.removeFromLeft(56));
        spkTypeBox_.setBounds(row.removeFromLeft(70).withTrimmedRight(6));
    }
    {                                                // the Limiter: Ratio, Threshold, Attack, Release, Level
        auto row = rowOf(0);
        limRowLabel_.setBounds(row.removeFromLeft(56));
        limRatioBox_.setBounds(row.removeFromLeft(70).withTrimmedRight(6));
        const int w = row.getWidth() / kLimKnobs;
        for (int i = 0; i < kLimKnobs; ++i) {
            auto cell = row.removeFromLeft(w);
            limLabels_[i].setBounds(cell.removeFromLeft(30));
            limSliders_[i].setBounds(cell.withTrimmedRight(3));
        }
    }
    for (int r = 0; r < 2; ++r) {                    // Wave Shape and OD/Dist: Type + Level, Drive, LFO, Env; then Tone, KF, LFO, Env
        for (int t = 0; t < 2; ++t) {
            auto row = rowOf(r);
            (t ? odRowLabels_ : wsRowLabels_)[r].setBounds(row.removeFromLeft(56));
            auto box = row.removeFromLeft(62);
            if (r == 0) (t ? odTypeBox_ : wsTypeBox_).setBounds(box.withTrimmedRight(6));
            const int first = r ? 4 : 0, w = row.getWidth() / 4;
            for (int i = first; i < first + 4; ++i) {
                auto cell = row.removeFromLeft(w);
                (t ? odLabels_ : wsLabels_)[i].setBounds(cell.removeFromLeft(32));
                (t ? odSliders_ : wsSliders_)[i].setBounds(cell.withTrimmedRight(3));
            }
        }
    }
}

// ─── Building the tabs ───────────────────────────────────────────────────────
// Every control the constructor made is moved into a page here (or stays in
// the header, source bar or perform column); the rest of the old layout's
// row labels are dropped. The OSC and COSM selector combo boxes stay on the
// editor, hidden, as the segmented buttons' models (PluginTest finds them).
void VSynthEmuEditor::buildLayout() {
    // The segmented selectors drive the existing combo boxes, which keep
    // their own onChange work (attachOsc, attachCosm) and the parameters
    oscSeg_ = std::make_unique<Segmented>(juce::StringArray { "OSC 1", "OSC 2" }, 1001);
    oscSeg_->onChange = [this](int i) { oscSelBox_.setSelectedId(i + 1, juce::sendNotificationSync); };
    oscSelBox_.onChange = [this] { attachOsc(oscSelBox_.getSelectedId() == 2 ? 1 : 0); oscSeg_->setIndex(oscK_); };
    cosmSeg_ = std::make_unique<Segmented>(juce::StringArray { "COSM 1", "COSM 2" }, 1002);
    cosmSeg_->onChange = [this](int i) { cosmSelBox_.setSelectedId(i + 1, juce::sendNotificationSync); };
    cosmSelBox_.onChange = [this] { attachCosm(cosmSelBox_.getSelectedId() == 2 ? 1 : 0); cosmSeg_->setIndex(cosmK_); };
    monoSeg_ = std::make_unique<Segmented>(juce::StringArray { "MONO", "POLY" }, 1003);
    monoSeg_->onChange = [this](int i) { monoBox_.setSelectedItemIndex(i, juce::sendNotificationSync); };
    monoBox_.onChange = [this] { monoSeg_->setIndex(monoBox_.getSelectedItemIndex()); };
    monoSeg_->setIndex(monoBox_.getSelectedItemIndex());
    for (auto* seg : { oscSeg_.get(), cosmSeg_.get(), monoSeg_.get() }) seg->setVisible(true);   // pages adopt with visibility kept
    algoLabel_.setJustificationType(juce::Justification::centredRight);
    statusLabel_.setJustificationType(juce::Justification::centredRight);

    using Item = Page::Item;
    using Row = Page::Row;
    using Card = Page::Card;
    const auto pcm = [this] { return oscTypeBox_.getSelectedItemIndex() != 0; };

    // Friendlier names where the old rows were too narrow for them
    static const std::pair<int, const char*> levelNames[] = {
        { 0, "Coarse" }, { 1, "Fine" }, { 2, "Random" }, { 3, "Pitch KF" }, { 4, "Coarse" }, { 5, "Fine" }, { 6, "Bend up" }, { 7, "Bend down" },
        { 8, "Level" }, { 9, "Level KF" }, { 10, "LFO depth" }, { 11, "Wave gain" }, { 20, "Sensitivity" }, { 21, "Curve" },
        { 22, "Sensitivity" }, { 23, "Curve" } };
    for (auto [i, n] : levelNames) levelLabels_[i].setText(n, juce::dontSendNotification);
    static const char* const tvaNames[kTvaKnobs] = { "Level", "Level KF", "LFO depth", "MFX send", "Pan", "Pan KF", "LFO depth", "Rate", "Delay", "Fade" };
    for (int i = 0; i < kTvaKnobs; ++i) tvaLabels_[i].setText(tvaNames[i], juce::dontSendNotification);
    for (auto [l, n] : { std::pair<juce::Label*, const char*>{ &offsetLabel_, "Time offset" }, { &oscFkfLabel_, "Formant KF" },
                         { &origTempoLabel_, "Original BPM" }, { &portaTimeLabel_, "Time" }, { &portaSensLabel_, "Velocity sens" },
                         { &octLabel_, "Octave" }, { &tripLabel_, "Time Trip" }, { &lfoDepthLabel_, "Pitch depth" },
                         { &lfoTimeLabel_, "Time depth" }, { &lfoFmtLabel_, "Formant depth" }, { &modAttackLabel_, "Attack" },
                         { &modReleaseLabel_, "Release" }, { &structLabel_, "Type" }, { &matrixLabel_, "Control" },
                         { &assignLabel_, "Controller" }, { &cosmTypeLabel_, "Type" }, { &pitchLabel_, "Pitch" },
                         { &timeLabel_, "Time" }, { &formantLabel_, "Formant" }, { &pDepthLabel_, "Depth" },
                         { &tDepthLabel_, "Depth" }, { &fDepthLabel_, "Depth" } })
        l->setText(n, juce::dontSendNotification);
    for (int j = 0; j < 2; ++j) matrixSensLabels_[j].setText("Sensitivity " + juce::String(j + 1), juce::dontSendNotification);
    for (auto [l, n] : { std::pair<juce::Label*, const char*>{ &pAttackLabel_, "A" }, { &pDecayLabel_, "D" }, { &pSustainLabel_, "S" }, { &pReleaseLabel_, "R" },
                         { &tAttackLabel_, "A" }, { &tDecayLabel_, "D" }, { &tSustainLabel_, "S" }, { &tReleaseLabel_, "R" },
                         { &fAttackLabel_, "A" }, { &fDecayLabel_, "D" }, { &fSustainLabel_, "S" }, { &fReleaseLabel_, "R" } })
        l->setText(n, juce::dontSendNotification);
    for (juce::ToggleButton* b : { &loopBtn_, &portaBtn_, &modBtn_, &tvaBtn_ }) b->setButtonText("On");
    structOrderLabel_.setFont(juce::Font(12.0f));
    structOrderLabel_.setColour(juce::Label::textColourId, Ui::text);
    structOrderLabel_.setMinimumHorizontalScale(0.6f);
    for (juce::Slider* s : { &keySlider_ }) s->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 56, 22);

    for (auto& p : pages_) p = std::make_unique<Page>();

    // ── Oscillator ──
    {
        Page& p = *pages_[0];
        Card wave { "Wave" };
        wave.head = { { &variBtn_, 60 }, { &playbackBox_, 110 } };
        wave.rows = { lr(encodeLabel_, encodeBox_), lr(keyLabel_, keySlider_, 150), lr(depthLabel_, depthSlider_), lr(energyLabel_, energySlider_),
                      lr(offsetLabel_, timeOffsetBox_, 110), lr(oscTkfLabel_, oscTkfSlider_), lr(oscFkfLabel_, oscFkfSlider_), lr(startLabel_, startSlider_) };
        wave.shown = pcm;
        p.add(wave);
        Card loop { "Loop" };
        loop.head = { { &loopBtn_, 54 }, { &loopModeBox_, 110 } };
        loop.rows = { lr(loopStartLabel_, loopStartSlider_), lr(loopEndLabel_, loopEndSlider_) };
        loop.shown = pcm;
        p.add(loop);
        Card sync { "Tempo sync" };
        sync.head = { { &syncBtn_, 62 }, { &tripBtn_, 58 }, { &keepBtn_, 64 } };
        sync.rows = { lr(origTempoLabel_, origTempoSlider_) };
        sync.shown = pcm;
        p.add(sync);
        Card ana { "Analog oscillator" };
        ana.sameAs = 0;
        ana.rows = { lr(anaWaveLabel_, anaWaveBox_) };
        for (int i = 0; i < kAnaKnobs; i += 2)
            ana.rows.push_back(Row{ { { &anaLabels_[i], 50 }, { &anaSliders_[i], 0 }, { nullptr, 2 }, { &anaLabels_[i + 1], 50 }, { &anaSliders_[i + 1], 0 } } });
        ana.shown = [pcm] { return !pcm(); };
        p.add(ana);
        Card pitch { "Pitch" };
        pitch.col = 1;
        for (int i : { 0, 1, 2, 3 }) pitch.rows.push_back(rowOfLevel(i));
        p.add(pitch);
        Card level { "Level" };
        level.col = 1;
        for (int i : { 8, 9, 10, 11 }) level.rows.push_back(rowOfLevel(i));
        p.add(level);
        Card velo { "Velocity" };
        velo.col = 1;
        velo.rows = { rowOfLevel(22), rowOfLevel(23) };
        p.add(velo);
    }

    // ── Envelopes: each a shape and its knobs as bars ──
    {
        Page& p = *pages_[1];
        auto envCard = [this](const juce::String& title, std::function<juce::String()> titleFn, int col, juce::Component* headBtn,
                              std::vector<std::pair<juce::Label*, juce::Slider*>> knobs) {
            for (auto [l, s] : knobs) {
                s->setSliderStyle(juce::Slider::LinearBar);
                s->setColour(juce::Slider::trackColourId, Ui::accent.withAlpha(0.45f));
                s->setColour(juce::Slider::textBoxOutlineColourId, Ui::line);
                s->setColour(juce::Slider::textBoxTextColourId, Ui::text);
                l->setJustificationType(juce::Justification::centred);
                l->setBorderSize({ 1, 0, 1, 0 });            // a narrow cell: the default's 5 on each side cut "A-Sns" short
            }
            auto view = std::make_unique<EnvelopeView>(*knobs[0].second, *knobs[1].second, *knobs[2].second, *knobs[3].second);
            Card c { title };
            c.titleFn = std::move(titleFn);
            c.col = col;
            if (headBtn) c.head = { { headBtn, 54 } };
            c.customH = 72;
            EnvelopeView* v = view.get();
            v->setVisible(true);
            c.extra.push_back(v);
            for (auto [l, s] : knobs) { c.extra.push_back(l); c.extra.push_back(s); }
            c.custom = [v, knobs](juce::Rectangle<int> a) {
                v->setBounds(a.removeFromLeft(104).withTrimmedBottom(2));
                a.removeFromLeft(12);
                const int cw = (a.getWidth() - 4 * 4) / 5;
                for (size_t i = 0; i < knobs.size(); ++i) {
                    const int row = i < 4 || (knobs.size() == 5) ? 0 : 1, colIx = row == 0 ? (int)i : (int)i - 4;
                    juce::Rectangle<int> cell(a.getX() + colIx * (cw + 4), a.getY() + row * 38, cw, 34);
                    knobs[i].first->setBounds(cell.removeFromTop(14));
                    knobs[i].second->setBounds(cell);
                }
            };
            envViews_.push_back(std::move(view));
            return c;
        };
        p.add(envCard("Amp (TVA)", nullptr, 0, &tvaBtn_,
                      { { &attackLabel_, &attackSlider_ }, { &decayLabel_, &decaySlider_ }, { &sustainLabel_, &sustainSlider_ }, { &releaseLabel_, &releaseSlider_ },
                        { &aSensLabel_, &aSensSlider_ }, { &dSensLabel_, &dSensSlider_ }, { &rSensLabel_, &rSensSlider_ }, { &kfLabel_, &kfSlider_ } }));
        const auto ofOsc = [this](const juce::String& name) { return [this, name] { return name + " (OSC " + juce::String(oscK_ + 1) + ")"; }; };
        p.add(envCard("Pitch", ofOsc("Pitch"), 0, nullptr,
                      { { &pAttackLabel_, &pAttackSlider_ }, { &pDecayLabel_, &pDecaySlider_ }, { &pSustainLabel_, &pSustainSlider_ },
                        { &pReleaseLabel_, &pReleaseSlider_ }, { &pDepthLabel_, &pDepthSlider_ } }));
        p.add(envCard("Time", ofOsc("Time"), 0, nullptr,
                      { { &tAttackLabel_, &tAttackSlider_ }, { &tDecayLabel_, &tDecaySlider_ }, { &tSustainLabel_, &tSustainSlider_ },
                        { &tReleaseLabel_, &tReleaseSlider_ }, { &tDepthLabel_, &tDepthSlider_ } }));
        std::vector<std::pair<juce::Label*, juce::Slider*>> oscEnv, cosmEnv;
        for (int i = 12; i < 20; ++i) oscEnv.push_back({ &levelLabels_[i], &levelSliders_[i] });
        for (int i = 28; i < 32; ++i) cosmEnv.push_back({ &levelLabels_[i], &levelSliders_[i] });
        p.add(envCard("Oscillator", [this] { return "OSC " + juce::String(oscK_ + 1) + " Amp"; }, 1, nullptr, oscEnv));
        p.add(envCard("Formant", ofOsc("Formant"), 1, nullptr,
                      { { &fAttackLabel_, &fAttackSlider_ }, { &fDecayLabel_, &fDecaySlider_ }, { &fSustainLabel_, &fSustainSlider_ },
                        { &fReleaseLabel_, &fReleaseSlider_ }, { &fDepthLabel_, &fDepthSlider_ } }));
        p.add(envCard("COSM", [this] { return "COSM " + juce::String(cosmK_ + 1); }, 1, nullptr, cosmEnv));
    }

    // ── LFOs: the OSC's (LFO 1 or 2) and the patch TVA's (LFO 5) ──
    {
        Page& p = *pages_[2];
        Card osc { "LFO" };
        osc.titleFn = [this] { return oscK_ ? juce::String("LFO 2 (OSC 2)") : juce::String("LFO 1 (OSC 1)"); };
        osc.head = { { &lfoSyncBtn_, 84 } };
        osc.rows = { lr(*newLabel("Wave"), lfoWaveBox_), lr(lfoRateLabel_, lfoRateSlider_), lr(lfoDelayLabel_, lfoDelaySlider_),
                     lr(lfoFadeLabel_, lfoFadeSlider_),
                     Row{ { { newLabel("Fade mode"), Ui::labelW }, { &lfoModeBox_, 0 }, { &lfoOffsetBox_, 0 } } },
                     lr(lfoDepthLabel_, lfoDepthSlider_), lr(lfoTimeLabel_, lfoTimeSlider_), lr(lfoFmtLabel_, lfoFmtSlider_) };
        p.add(osc);
        Card amp { "Amp LFO (LFO 5)" };
        amp.col = 1;
        amp.head = { { &tlfoSyncBtn_, 84 } };
        amp.rows = { lr(*newLabel("Wave"), tlfoWaveBox_), lr(tvaLabels_[7], tvaSliders_[7]), lr(tvaLabels_[8], tvaSliders_[8]),
                     lr(tvaLabels_[9], tvaSliders_[9]),
                     Row{ { { newLabel("Fade mode"), Ui::labelW }, { &tlfoModeBox_, 0 }, { &tlfoOffsetBox_, 0 } } } };
        p.add(amp);
    }

    // ── COSM: which section, its type's rows, its LFO, MOD and the Structure ──
    {
        Page& p = *pages_[3];
        Card sel;
        sel.bare = true;
        sel.col = -1;
        sel.rows = { Row{ { { cosmSeg_.get(), 150 }, { &cosmBtn_, 50 }, { nullptr, 4 }, { &cosmTypeLabel_, 36 }, { &cosmTypeBox_, 220 }, { nullptr, 0 } } } };
        p.add(sel);
        Card type;
        type.titleFn = [this] { return "COSM " + juce::String(cosmK_ + 1) + "  " + cosmTypeBox_.getText(); };
        type.col = -1;
        type.customH = 3 * 30 - 6;
        type.custom = [this](juce::Rectangle<int> a) { layoutCosmType(a); };
        type.extra = { &tvfLabel_, &cosmFilterBox_, &cosmCutLabel_, &cosmCutSlider_, &cosmResoLabel_, &cosmResoSlider_, &cosmKfLabel_, &cosmKfSlider_,
                       &levelRowLabels_[6], &cosmCutOwnLabel_, &cosmCutOwnSlider_, &cosmResoOwnLabel_, &cosmResoOwnSlider_,
                       &dualLabel_, &dualTypeBox_, &combOctBox_, &sbOctBox_, &resTypeBox_, &compRowLabel_, &limRowLabel_, &limRatioBox_,
                       &wsTypeBox_, &odTypeBox_, &ampTypeBox_, &spkRowLabel_, &spkTypeBox_ };
        for (int i = 24; i < 28; ++i) { type.extra.push_back(&levelLabels_[i]); type.extra.push_back(&levelSliders_[i]); }
        auto addAll = [&type](juce::Label* labels, juce::Slider* sliders, int n) {
            for (int i = 0; i < n; ++i) {
                labels[i].setBorderSize({ 1, 3, 1, 0 });     // narrow cells, as the envelopes': the default's 10 squeezes "Reso", none touches the box before
                type.extra.push_back(&labels[i]);
                type.extra.push_back(&sliders[i]);
            }
        };
        for (juce::Label* l : { &cosmCutLabel_, &cosmResoLabel_, &cosmKfLabel_, &cosmCutOwnLabel_, &cosmResoOwnLabel_,
                                &levelLabels_[24], &levelLabels_[25], &levelLabels_[26], &levelLabels_[27] })
            l->setBorderSize({ 1, 3, 1, 0 });
        addAll(dualLabels_, dualSliders_, kDualKnobs);
        addAll(tbLabels_, tbSliders_, kTbKnobs);
        addAll(combLabels_, combSliders_, kCombKnobs);
        addAll(sbLabels_, sbSliders_, kSbKnobs);
        addAll(resLabels_, resSliders_, kResKnobs);
        addAll(fsLabels_, fsSliders_, kFsKnobs);
        addAll(lofiLabels_, lofiSliders_, kLofiKnobs);
        addAll(compLabels_, compSliders_, kCompKnobs);
        addAll(limLabels_, limSliders_, kLimKnobs);
        addAll(wsLabels_, wsSliders_, kWsKnobs);
        addAll(odLabels_, odSliders_, kWsKnobs);
        addAll(ampLabels_, ampSliders_, kAmpKnobs);
        for (juce::Label* rows : { dualRowLabels_, tbRowLabels_, combRowLabels_, resRowLabels_, fsRowLabels_, lofiRowLabels_, wsRowLabels_, odRowLabels_, ampRowLabels_ })
            for (int r = 0; r < 2; ++r) type.extra.push_back(&rows[r]);
        for (auto& l : sbRowLabels_) type.extra.push_back(&l);
        p.add(type);
        Card lfo { "LFO" };
        lfo.titleFn = [this] { return cosmK_ ? juce::String("LFO 4 (COSM 2)") : juce::String("LFO 3 (COSM 1)"); };
        lfo.head = { { &clfoSyncBtn_, 84 } };
        lfo.rows = { lr(*newLabel("Wave"), clfoWaveBox_), lr(clfoRateLabel_, clfoRateSlider_) };
        p.add(lfo);
        Card mod { "MOD" };
        mod.head = { { &modBtn_, 54 }, { &modTypeBox_, 110 } };
        mod.rows = { lr(modLevelLabel_, modLevelSlider_), lr(modAttackLabel_, modAttackSlider_), lr(modReleaseLabel_, modReleaseSlider_) };
        p.add(mod);
        Card st { "Structure" };
        st.col = 1;
        st.rows = { lr(structLabel_, structBox_, 110), Row{ { { &structOrderLabel_, 0 } } } };
        p.add(st);
    }

    // ── Amp & Pan: the patch TVA and velocity to level ──
    {
        Page& p = *pages_[4];
        Card lvl { "Patch level" };
        for (int i = 0; i < 4; ++i) lvl.rows.push_back(lr(tvaLabels_[i], tvaSliders_[i]));
        p.add(lvl);
        Card velo { "Velocity" };
        velo.rows = { rowOfLevel(20), rowOfLevel(21) };
        p.add(velo);
        Card pan { "Pan" };
        pan.col = 1;
        for (int i = 4; i < 7; ++i) pan.rows.push_back(lr(tvaLabels_[i], tvaSliders_[i]));
        p.add(pan);
    }

    buildPlayPage();   // PluginEditorPlay.cpp (v190)
    buildMsmPage();    // PluginEditorMsm.cpp (v193)
    buildMatrixPage(); // PluginEditorMatrix.cpp (v193)

    // What stays on the editor; everything the pages don't take is dropped, so a
    // new control must go in a card or here (TEST_UI_LAYOUT says if one didn't),
    // bar the two selector boxes, kept hidden
    const juce::Array<juce::Component*> chrome { &buildLabel_, &algoLabel_, &algoBox_, &statusLabel_, &romButton_, &loadButton_, &patchButton_, &fileNameLabel_,
                                                 &oscOnBtn_, &oscTypeBox_, &pitchSlider_, &pitchLabel_, &timeSlider_, &timeLabel_,
                                                 &formantSlider_, &formantLabel_, &robotBtn_, &oscSelBox_, &cosmSelBox_, &fxOnBtn_, fxPanel_.get() };
    for (auto& p : pages_) p->adopt();
    const juce::Array<juce::Component*> kids(getChildren());
    for (auto* c : kids)
        if (!chrome.contains(c)) removeChildComponent(c);
    oscSelBox_.setVisible(false);
    cosmSelBox_.setVisible(false);
    for (juce::Label* l : { &pitchLabel_, &timeLabel_, &formantLabel_ }) l->setJustificationType(juce::Justification::centred);

    tabBar_ = std::make_unique<TabBar>();
    tabBar_->names = { "Oscillator", "Envelopes", "LFOs", "COSM", "Amp & Pan", "Play", "MSM", "Matrix", "Effects" };
    tabBar_->onChange = [this](int t) { showTab(t); };
    tabBar_->setComponentID("tabs");                 // for PluginTest
    addAndMakeVisible(*tabBar_);
    addAndMakeVisible(*oscSeg_);
    for (auto& p : pages_) addChildComponent(*p);
    showTab(0);
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
                processor_.loadFile(result, oscK_);
                showFileName();
            }
        });
}

// The selected OSC's wave: Load loads into that OSC.
void VSynthEmuEditor::showFileName() {
    const juce::String name = processor_.hasFileLoaded(oscK_) ? processor_.getLoadedFileName(oscK_) : juce::String("No file loaded");
    juce::String text = oscK_ ? "OSC 2: " + name : name;
    // A patch's name first, and at the end what it asks for that isn't here
    if (processor_.getPatchName().isNotEmpty()) text = processor_.getPatchName() + " | " + text;
    if (processor_.getPatchNote().isNotEmpty()) text << " | not here: " << processor_.getPatchNote();
    fileNameLabel_.setText(text, juce::dontSendNotification);
}

void VSynthEmuEditor::openPatchChooser() {
    fileChooser_ = std::make_unique<juce::FileChooser>("Load a V-Synth patch", processor_.lastPatchFolder(), "*.p00");
    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (!file.existsAsFile()) return;
            processor_.loadPatch(file);
            showFileName();
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
