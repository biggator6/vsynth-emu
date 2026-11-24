// D's header (UiHeader.h; UI step 1; the modes, v453)
#include "UiHeader.h"
#include "PluginParamIds.h"
#include "PluginVc2Params.h"
#include "UiControls.h"
#include "UiLook.h"
#include "UiParts.h"
#include "UiVc2Mic.h"
#include "XtBuildId.h"

namespace {
// Where each part sits, left to right, in the drawing's units (UI_DESIGN.md, "The window")
constexpr int kLeft = 16, kModeX = 176, kModeW = 64, kPatchW = 400, kPatchH = 32;
constexpr int kTempoX = 812, kTempoLabelW = 46, kTempoW = 56, kEffectsW = 84, kRomStateW = 96, kRomW = 60;
constexpr int kButtonH = 26, kMicW = 84, kMicGap = 8;
constexpr int kModeGroup = 2002;   // the V-SYNTH/VC-2 pair's radio group
constexpr int kVc2Tempo = 0x13a;   // [FW] the VC-2's Patch Tempo
constexpr auto kOpen = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
}

Header::Header(VSynthEmuProcessor& p)
    : processor_(p), patch_(p), effectsAtt_(p.apvts, Params::kFxOn, effects_) {
    Look::setTextLook(build_, Look::mono(10.0f), Look::muted, juce::Justification::centredLeft);
    build_.setText(juce::String(xtBuildId()) + "  " + xtBuildTime(), juce::dontSendNotification);
    Look::setTextLook(tempo_, Look::mono(11.0f), Look::text);
    Look::setTextLook(romState_, Look::sans(11.0f), Look::label, juce::Justification::centredRight);
    romState_.setMinimumHorizontalScale(0.5f);
    for (auto [button, mode] : { std::pair { &synthMode_, PluginMode::vsynth }, { &vc2Mode_, PluginMode::vc2 } }) {
        button->setRadioGroupId(kModeGroup);
        button->onClick = [this, m = mode] { if (onMode) onMode(m); };
    }
    vc2Mode_.setTooltip("The VC-2 mode's pages; it plays nothing yet, until the VC-2 synth is in (step 4.3)");
    rom_.onClick = [this] { chooseRom(); };
    mic_.onClick = [this] { if (onMic) onMic(); };
    addChildComponent(mic_);
    const std::initializer_list<juce::Component*> parts = { &build_, &patch_, &tempo_, &romState_, &synthMode_,
                                                            &vc2Mode_, &rom_, &effects_ };
    for (juce::Component* c : parts) addAndMakeVisible(c);
    setMode(PluginMode::vsynth);
}

void Header::setMode(PluginMode mode) {
    mode_ = mode;
    synthMode_.setToggleState(mode == PluginMode::vsynth, juce::dontSendNotification);
    vc2Mode_.setToggleState(mode == PluginMode::vc2, juce::dontSendNotification);
    mic_.setVisible(mode == PluginMode::vc2);
    patch_.setMode(mode);
    resized();
    refresh();
}

void Header::refresh() {
    const bool vc2 = mode_ == PluginMode::vc2;
    patch_.refresh();
    const juce::String tempoId = vc2 ? vc2HostParamId(kVc2Tempo, 0, 0) : juce::String("patchTempo");
    tempo_.setText(Controls::currentText(processor_.apvts, tempoId), juce::dontSendNotification);
    Look::showText(romState_, processor_.getStatusText(), processor_.hasRom() ? Look::ready : Look::warning);
    if (!vc2) return;
    const bool heard = Vc2Mic::heard(processor_);   // the MIC chip: the vocoder heard as far as the editor sees, or why not
    mic_.setColour(juce::TextButton::textColourOffId, heard ? Look::ready : Look::warning);
    mic_.setTooltip(Vc2Mic::status(processor_));
}

void Header::resized() {
    const int mid = getHeight() / 2;
    build_.setBounds(kLeft, 29, kModeX - kLeft - 8, 13);
    synthMode_.setBounds(kModeX, mid - 12, kModeW, 24);
    vc2Mode_.setBounds(kModeX + kModeW, mid - 12, kModeW, 24);
    patchBox_ = { (getWidth() - kPatchW) / 2, mid - kPatchH / 2, kPatchW, kPatchH };
    if (mode_ == PluginMode::vc2) {   // the MIC chip at the patch box's right, the box narrower by it
        auto whole = patchBox_;
        mic_.setBounds(whole.removeFromRight(kMicW).withSizeKeepingCentre(kMicW, kButtonH));
        patchBox_ = whole.withTrimmedRight(kMicGap);
    }
    patch_.setBounds(patchBox_);
    tempoBox_ = { kTempoX + kTempoLabelW, mid - 12, kTempoW, 24 };
    tempo_.setBounds(tempoBox_);
    auto right = getLocalBounds().withLeft(tempoBox_.getRight() + 12).withTrimmedRight(Look::inset);
    effects_.setBounds(right.removeFromLeft(kEffectsW).withSizeKeepingCentre(kEffectsW, 24));
    rom_.setBounds(right.removeFromRight(kRomW).withSizeKeepingCentre(kRomW, kButtonH));
    right.removeFromRight(8);
    romState_.setBounds(right.removeFromRight(kRomStateW).withSizeKeepingCentre(kRomStateW, 20));
}

void Header::paint(juce::Graphics& g) {
    g.fillAll(Look::header);
    g.setColour(Look::divider);
    g.fillRect(getLocalBounds().removeFromBottom(1));
    Ui::drawText(g, "V-SYNTH EMU", { kLeft, 7, kModeX - kLeft, 22 }, { Look::sans(15.0f, "Bold", 0.12f), Look::text });
    Ui::fillRounded(g, tempoBox_.toFloat(), Look::panel, 4.0f);
    Ui::drawText(g, "Tempo", tempoBox_.translated(-kTempoLabelW, 0).withWidth(kTempoLabelW - 6),
                 { Look::sans(11.0f), Look::label, juce::Justification::centredRight });
}

void Header::chooseRom() {
    const juce::File home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    chooser_ = std::make_unique<juce::FileChooser>("Choose the V-Synth XT firmware image (V-SynthXT.rom.2.bin)", home,
                                                   "*.bin");
    chooser_->launchAsync(kOpen, [this](const juce::FileChooser& fc) {
        const juce::File file = fc.getResult();
        if (file.existsAsFile()) processor_.setRomFile(file);
        refresh();
    });
}
