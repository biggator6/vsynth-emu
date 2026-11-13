#include "PluginEditor.h"
#include "PluginEditorIds.h"
#include "PluginEditorEnvelopeView.h"
#include "XtBuildId.h"
#include <tuple>

using Ui::Page;
using Ui::Segmented;

#include "PluginEditorTabs.h"   // the tab bar, its own file from v195

VSynthEmuEditor::VSynthEmuEditor(VSynthEmuProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), fxPanel_(std::make_unique<FxPanel>(p))
{

    makeSourceControls();
    makeToneControls();
    makeCosmTypeRows();
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
    attachMainKnobs();

    robotBtn_.setColour(juce::ToggleButton::textColourId,     juce::Colours::lightblue);
    robotBtn_.setColour(juce::ToggleButton::tickColourId,     juce::Colour(0xFF00BFA5));
    addAndMakeVisible(robotBtn_);

    buildLayout();
    setSize(960, 600);
    setResizable(true, true);
    setResizeLimits(900, 600, 1400, 900);
}

VSynthEmuEditor::~VSynthEmuEditor() {
    stopTimer();
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
    Ui::drawText(g, "PERFORM  OSC " + juce::String(oscK_ + 1), { 18, top + 10, Ui::performW - 36, 16 },
                 { Ui::titleFont(), Ui::label });
}

// ─── Layout ───────────────────────────────────────────────────────────────────

void VSynthEmuEditor::resized() {
    if (tabBar_ == nullptr) return;                  // setSize() runs before buildLayout()
    // Header: the ROM at the right
    auto hdr = juce::Rectangle<int>(0, 0, getWidth(), Ui::headerH).reduced(16, 6);
    romButton_.setBounds(hdr.removeFromRight(70));
    hdr.removeFromRight(10);
    statusLabel_.setBounds(hdr.removeFromRight(170));
    hdr.removeFromRight(16);
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
