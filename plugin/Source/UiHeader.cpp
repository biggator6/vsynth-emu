// D's header (UiHeader.h; UI step 1)
#include "UiHeader.h"
#include "PluginParamIds.h"
#include "UiLook.h"
#include "UiParts.h"
#include "XtBuildId.h"

namespace {
// Where each part sits, left to right, in the drawing's units (UI_DESIGN.md, "The window")
constexpr int kLeft = 16, kModeX = 176, kModeW = 64, kPatchW = 400, kPatchH = 32, kBrowseW = 76, kPatchTagW = 56;
constexpr int kTempoX = 812, kTempoLabelW = 46, kTempoW = 56, kEffectsW = 84, kRomStateW = 96, kRomW = 60;
constexpr int kButtonH = 26;
constexpr auto kOpen = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
}

Header::Header(VSynthEmuProcessor& p)
    : processor_(p), effectsAtt_(p.apvts, Params::kFxOn, effects_) {
    Look::setTextLook(build_, Look::mono(10.0f), Look::muted, juce::Justification::centredLeft);
    build_.setText(juce::String(xtBuildId()) + "  " + xtBuildTime(), juce::dontSendNotification);
    Look::setTextLook(patchName_, Look::sans(13.0f, "Semibold"), Look::text, juce::Justification::centredLeft);
    Look::setTextLook(tempo_, Look::mono(11.0f), Look::text);
    Look::setTextLook(romState_, Look::sans(11.0f), Look::label, juce::Justification::centredRight);
    romState_.setMinimumHorizontalScale(0.5f);
    synthMode_.setToggleState(true, juce::dontSendNotification);
    vc2Mode_.setEnabled(false);
    vc2Mode_.setTooltip("The VC-2 mode isn't in yet");
    browse_.onClick = [this] { choosePatch(); };
    rom_.onClick = [this] { chooseRom(); };
    const std::initializer_list<juce::Component*> parts = { &build_, &patchName_, &tempo_, &romState_, &synthMode_,
                                                            &vc2Mode_, &browse_, &rom_, &effects_ };
    for (juce::Component* c : parts) addAndMakeVisible(c);
    refresh();
}

void Header::refresh() {
    const juce::String name = processor_.getPatchName();
    Look::showText(patchName_, name.isNotEmpty() ? name : juce::String("No patch loaded"),
                   name.isNotEmpty() ? Look::text : Look::muted);
    tempo_.setText(processor_.apvts.getParameter("patchTempo")->getCurrentValueAsText(), juce::dontSendNotification);
    Look::showText(romState_, processor_.getStatusText(), processor_.hasRom() ? Look::ready : Look::warning);
}

void Header::resized() {
    const int mid = getHeight() / 2;
    build_.setBounds(kLeft, 29, kModeX - kLeft - 8, 13);
    synthMode_.setBounds(kModeX, mid - 12, kModeW, 24);
    vc2Mode_.setBounds(kModeX + kModeW, mid - 12, kModeW, 24);
    patchBox_ = { (getWidth() - kPatchW) / 2, mid - kPatchH / 2, kPatchW, kPatchH };
    auto box = patchBox_.reduced(4, 0);
    browse_.setBounds(box.removeFromRight(kBrowseW).withSizeKeepingCentre(kBrowseW, kButtonH));
    box.removeFromLeft(kPatchTagW);
    patchName_.setBounds(box.withTrimmedRight(8));
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
    Ui::fillRounded(g, patchBox_.toFloat(), Look::panel, 6.0f);
    Ui::fillRounded(g, tempoBox_.toFloat(), Look::panel, 4.0f);
    Ui::drawText(g, "PATCH", patchBox_.withTrimmedLeft(12).withWidth(kPatchTagW), { Look::mono(11.0f), Look::muted });
    Ui::drawText(g, "Tempo", tempoBox_.translated(-kTempoLabelW, 0).withWidth(kTempoLabelW - 6),
                 { Look::sans(11.0f), Look::label, juce::Justification::centredRight });
}

void Header::choosePatch() {
    chooser_ = std::make_unique<juce::FileChooser>("Load a V-Synth patch", processor_.lastPatchFolder(), "*.p00");
    chooser_->launchAsync(kOpen, [this](const juce::FileChooser& fc) {
        const juce::File file = fc.getResult();
        if (file.existsAsFile()) processor_.loadPatch(file);
        refresh();
    });
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
